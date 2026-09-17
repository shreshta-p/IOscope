#pragma once
// Linux replacement for windows/agent/workload_engine.hpp (baseline commit 280ced5).
// Same WorkloadEngine interface and EngineResult shape; FioEngine replaces
// DiskSpdEngine. The RunMetadata.engine.sha256 field is still populated (the schema
// requires it, additionalProperties:false forbids a differently-named field), but its
// trust model differs from Windows: Windows compares against one hardcoded expected
// hash for a specific pinned release; here it's the actual hash of whatever fio
// binary is installed, recorded for provenance/audit, with trust instead coming from
// apt's package signing plus the runtime `fio --version` pin check in fio.hpp. Not
// the same guarantee — recorded here rather than left implicit.
#include "fio.hpp"
#include "fio_json.hpp"
#include "scratch.hpp"
#include "process_job.hpp"
namespace ioscope {
struct EngineResult {std::string state="failed";std::optional<std::string> reason; nlohmann::json summaries=nlohmann::json::array();std::string xml,diagnostic;std::optional<bool> cleaned;};
class WorkloadEngine {
public:
 virtual ~WorkloadEngine()=default;
 // Any leftover scratch entry normally means an orphaned run from an unclean
 // shutdown -- refuse until recovery. expectedDatasetId names the one entry a
 // first/repeated access experiment's earlier phase deliberately kept (see
 // Scratch::keep()), so its later phase doesn't misread its own dataset as
 // an orphan.
 virtual void ready(const std::string& expectedDatasetId)=0;
 virtual nlohmann::json metadata(const nlohmann::json& workload,const std::string& runId,const std::string& datasetId)=0;
 // datasetId names the on-disk scratch dataset (normally == runId, one dataset
 // per run); prepareDataset/cleanupDataset default to today's single-run
 // behavior (create fresh, clean up after). docs/07-EXPERIMENT-SPEC.md's
 // first/repeated access experiment sets these so two phases share one
 // dataset: phase one creates and keeps it, phase two reuses and cleans up.
 virtual EngineResult execute(const nlohmann::json& workload,const std::string& runId,const std::atomic<bool>& cancel,
  const std::function<std::optional<std::string>()>& watchdog,const std::function<void()>& running,
  const std::string& datasetId,bool prepareDataset,bool cleanupDataset)=0;
};
class FioEngine final:public WorkloadEngine {
 std::filesystem::path executable_;
 static std::filesystem::path preparation_path(){
  char self[4096]{};const auto count=readlink("/proc/self/exe",self,sizeof(self)-1);posix_require(count>0,"Resolve preparation helper");
  return std::filesystem::path(std::string(self,static_cast<size_t>(count))).parent_path()/"ioscope_prepare";
 }
public:
 explicit FioEngine(std::filesystem::path executable):executable_(std::move(executable)){}
 void ready(const std::string& expectedDatasetId)override{
  verify_fio(executable_);
  posix_require(std::filesystem::is_regular_file(preparation_path()),"Preparation helper is unavailable; rebuild the native application");
  const auto root=local_directory()/"scratch";
  if(std::filesystem::exists(root)){reject_symlink_ancestors(root);
   for(const auto& entry:std::filesystem::directory_iterator(root))
    if(entry.path().filename().string()!=expectedDatasetId)throw std::runtime_error("Owned scratch contains an unfinished run; inspect recovery before starting another workload");
  }
 }
 nlohmann::json metadata(const nlohmann::json& workload,const std::string&,const std::string& datasetId)override{
  auto argv=nlohmann::json::array();for(const auto& arg:fio_arguments(workload,local_directory()/"scratch"/datasetId/"data.bin"))argv.push_back(arg);
  Fd file(open(executable_.c_str(),O_RDONLY));posix_require(static_cast<bool>(file),"Open fio for hashing");
  return {{"name","fio"},{"version",fio_pinned_version},{"sha256",hash_fd(file.get())},{"argv",argv}};
 }
 EngineResult execute(const nlohmann::json& workload,const std::string& id,const std::atomic<bool>& cancel,
  const std::function<std::optional<std::string>()>& watchdog,const std::function<void()>& running,
  const std::string& datasetId,bool prepareDataset,bool cleanupDataset)override{
  EngineResult result;std::unique_ptr<Scratch> scratch;
  try{
   verify_fio(executable_);posix_require(std::filesystem::is_regular_file(preparation_path()),"Preparation helper is unavailable");
   if(cancel.load()){result.state="cancelled";result.reason="Cancelled before preparation";return result;}
   if(auto reason=watchdog()){result.state="aborted";result.reason=reason;return result;}
   const auto bytes=workload.at("workingSetBytes").get<unsigned long long>();
   if(prepareDataset){
    scratch=std::make_unique<Scratch>(datasetId,ScratchMode::Create);const auto preparation=preparation_path();
    const auto prepared=run_child_process(id+"-prepare",preparation.string(),{datasetId,std::to_string(bytes)},cancel,std::chrono::seconds(bytes/(32*MiB)+15),watchdog,scratch->preparation_fd());
    result.diagnostic=prepared.errors;
    if(prepared.cancelled)throw std::runtime_error("Preparation cancelled");
    if(prepared.aborted){result.state="aborted";result.reason=prepared.aborted;auto cleanup=scratch->cleanup();result.cleaned=!cleanup;if(cleanup){result.state="failed";result.reason=cleanup;}return result;}
    if(prepared.exitCode)throw std::runtime_error("Preparation failed: "+prepared.errors.substr(0,300));
    if(cancel.load())throw std::runtime_error("Preparation cancelled");
   }else{
    scratch=std::make_unique<Scratch>(datasetId,ScratchMode::Reuse);
    posix_require(scratch->size()>=bytes,"Reused dataset is smaller than the requested working set");
   }
   running();
   const auto seconds=workload.at("durationSeconds").get<unsigned>()+workload.at("warmupSeconds").get<unsigned>()+workload.at("cooldownSeconds").get<unsigned>();
   const auto child=run_child_process(id+"-fio",executable_.string(),fio_arguments(workload,scratch->path()),cancel,std::chrono::seconds(seconds+5),watchdog);
   result.xml=child.output;result.diagnostic=child.errors;
   if(child.cancelled||cancel.load()){result.state="cancelled";result.reason="User requested cancellation";}
   else if(child.aborted){result.state="aborted";result.reason=child.aborted;}
   else if(child.exitCode){result.reason="fio exited with code "+std::to_string(child.exitCode)+": "+child.errors.substr(0,300);}
   else {result.summaries=disk_summaries(parse_fio_json(child.output));result.state="completed";}
  }catch(const std::exception& error){result.state=cancel.load()?"cancelled":"failed";result.reason=std::string(error.what()).substr(0,512);result.diagnostic=(result.diagnostic+"\n"+*result.reason).substr(0,65536);}
  if(scratch){if(cleanupDataset){auto reason=scratch->cleanup();result.cleaned=!reason;if(reason){result.state="failed";result.reason=("Owned scratch cleanup failed: "+*reason).substr(0,512);}}else{scratch->keep();result.cleaned=true;}}
  return result;
 }
};
}
