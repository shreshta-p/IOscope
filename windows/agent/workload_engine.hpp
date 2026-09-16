#pragma once
#include "diskspd.hpp"
#include "diskspd_xml.hpp"
#include "scratch.hpp"
#include "process_job.hpp"
namespace ioscope {
struct EngineResult {std::string state="failed";std::optional<std::string> reason; nlohmann::json summaries=nlohmann::json::array();std::string xml,diagnostic;std::optional<bool> cleaned;};
class WorkloadEngine {
public:
 virtual ~WorkloadEngine()=default;
 virtual void ready()=0;
 virtual nlohmann::json metadata(const nlohmann::json& workload,const std::string& runId)=0;
 virtual EngineResult execute(const nlohmann::json& workload,const std::string& runId,const std::atomic<bool>& cancel,
  const std::function<std::optional<std::string>()>& watchdog,const std::function<void()>& running)=0;
};
class DiskSpdEngine final:public WorkloadEngine {
 std::filesystem::path executable_;
 static std::wstring event_name(const std::string& id){return L"Local\\IOscope-stop-"+std::wstring(id.begin(),id.end());}
 static std::filesystem::path preparation_path(){wchar_t path[32768]{};const auto count=GetModuleFileNameW(nullptr,path,32768);win_require(count>0&&count<32768,"Resolve preparation helper");return std::filesystem::path(path).parent_path()/"ioscope_prepare.exe";}
 static Handle preparation_lock(){const auto path=preparation_path();Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));win_require(static_cast<bool>(file),"Preparation helper is unavailable; rebuild the native application");verify_target(file.get(),path);return file;}
public:
 explicit DiskSpdEngine(std::filesystem::path executable):executable_(std::move(executable)){}
 void ready()override{auto verified=verify_diskspd(executable_);auto preparation=preparation_lock();const auto root=local_directory()/"scratch";if(std::filesystem::exists(root)){reject_reparse_ancestors(root);if(std::filesystem::directory_iterator(root)!=std::filesystem::directory_iterator())throw std::runtime_error("Owned scratch contains an unfinished run; inspect recovery before starting another workload");}}
 nlohmann::json metadata(const nlohmann::json& workload,const std::string& id)override{
  auto argv=nlohmann::json::array();for(const auto& arg:diskspd_arguments(workload,local_directory()/"scratch"/id/"data.bin",event_name(id)))argv.push_back(utf8(arg));
  return {{"name","diskspd"},{"version",diskspd_version},{"sha256",diskspd_sha256},{"argv",argv}};
 }
 EngineResult execute(const nlohmann::json& workload,const std::string& id,const std::atomic<bool>& cancel,
  const std::function<std::optional<std::string>()>& watchdog,const std::function<void()>& running)override{
  EngineResult result;std::unique_ptr<Scratch> scratch;
  try{
   auto executableLock=verify_diskspd(executable_);auto preparationLock=preparation_lock();PrivateSecurity security;
   Handle stop(CreateEventW(&security.attributes,TRUE,FALSE,event_name(id).c_str()));win_require(static_cast<bool>(stop)&&GetLastError()!=ERROR_ALREADY_EXISTS,"Create private stop event");
   if(cancel.load()){result.state="cancelled";result.reason="Cancelled before preparation";return result;}
   if(auto reason=watchdog()){result.state="aborted";result.reason=reason;return result;}
   scratch=std::make_unique<Scratch>(id);const auto preparation=preparation_path();
   const auto bytes=workload.at("workingSetBytes").get<unsigned long long>();const auto prepared=run_child_job(preparation.wstring(),{std::wstring(id.begin(),id.end()),std::to_wstring(bytes)},cancel,std::chrono::seconds(bytes/(32*MiB)+15),watchdog,nullptr,scratch->preparation_handle());
   result.diagnostic=prepared.errors;
   if(prepared.cancelled)throw std::runtime_error("Preparation cancelled");if(prepared.aborted){result.state="aborted";result.reason=prepared.aborted;auto cleanup=scratch->cleanup();result.cleaned=!cleanup;if(cleanup){result.state="failed";result.reason=cleanup;}return result;}if(prepared.exitCode)throw std::runtime_error("Preparation failed: "+prepared.errors.substr(0,300));
   if(cancel.load())throw std::runtime_error("Preparation cancelled");
   running();const auto seconds=workload.at("durationSeconds").get<unsigned>()+workload.at("warmupSeconds").get<unsigned>()+workload.at("cooldownSeconds").get<unsigned>();
   const auto child=run_child_job(executable_.wstring(),diskspd_arguments(workload,scratch->path(),event_name(id)),cancel,std::chrono::seconds(seconds+5),watchdog,stop.get());
   result.xml=child.output;result.diagnostic=child.errors;
   if(child.cancelled||cancel.load()){result.state="cancelled";result.reason="User requested cancellation";}
   else if(child.aborted){result.state="aborted";result.reason=child.aborted;}
   else if(child.exitCode){result.reason="DiskSpd exited with code "+std::to_string(child.exitCode)+": "+child.errors.substr(0,300);}
   else {result.summaries=disk_summaries(parse_diskspd_xml(child.output,workload.at("blockBytes")));result.xml=child.output;result.state="completed";}
  }catch(const std::exception& error){result.state=cancel.load()?"cancelled":"failed";result.reason=std::string(error.what()).substr(0,512);result.diagnostic=(result.diagnostic+"\n"+*result.reason).substr(0,65536);}
  if(scratch){auto reason=scratch->cleanup();result.cleaned=!reason;if(reason){result.state="failed";result.reason=("Owned scratch cleanup failed: "+*reason).substr(0,512);}}
  return result;
 }
};
}
