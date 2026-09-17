#pragma once
#include "workload_engine.hpp"
#include "native_recording.hpp"
#include "store.hpp"
#include <mutex>
namespace ioscope {
struct RecordedFrame {Json frame;std::chrono::milliseconds age{0};};
class WorkloadController {
 Contracts& contracts_;Store& store_;WorkloadEngine& engine_;Json inventory_,mapping_;
 std::function<Resources()> resources_;std::function<RecordedFrame()> frame_;
 std::mutex mutex_;std::thread worker_;std::atomic<bool> cancel_{false};bool busy_=false;
 Json snapshot_={{"schemaVersion","1.0.0"},{"status",nullptr},{"workload",nullptr},{"recordingId",nullptr}};
 static Json request_snapshot(const Json& request){return {{"schemaVersion","1.0.0"},{"status",request["status"]},{"workload",request["definition"]},{"recordingId",request["recordingId"]}};}
 Json sample(const std::string& id,unsigned long long sequence,unsigned long long elapsed,const std::string& state,const Json& reason,const Json& workload){auto observed=frame_();auto result=native_sample(observed.frame,id,sequence,elapsed,state,reason,workload,mapping_,observed.age);contracts_.validate("ReplaySample",result);return result;}
public:
 WorkloadController(Contracts& contracts,Store& store,WorkloadEngine& engine,Json inventory,Json mapping,
 std::function<Resources()> resources,std::function<RecordedFrame()> frame):contracts_(contracts),store_(store),engine_(engine),inventory_(std::move(inventory)),mapping_(std::move(mapping)),resources_(std::move(resources)),frame_(std::move(frame)){}
 ~WorkloadController(){cancel_.store(true);if(worker_.joinable())worker_.join();}
 Json admission(const Json& workload){contracts_.validate("WorkloadDefinition",workload);const auto resources=resources_();auto result=admit(workload,resources);bool ready=false;
  try{engine_.ready();ready=true;}catch(const std::exception& error){result.reasons.push_back(std::string(error.what()).substr(0,512));}
  {std::lock_guard<std::mutex> lock(mutex_);if(busy_)result.reasons.push_back("A workload is already active");}
  if(!store_.pending().empty())result.reasons.push_back("An unfinished run journal requires recovery");
  Json value={{"schemaVersion","1.0.0"},{"workload",workload},{"allowed",result.allowed()},{"reasons",result.reasons},{"diskFreeBytes",resources.diskFree},{"diskReserveBytes",result.diskReserve},{"diskAllocationBytes",result.diskAllocationBytes},{"ramAvailableBytes",resources.ramAvailable},{"ramReserveBytes",result.ramReserve},{"bufferBytes",result.bufferBytes},{"plannedWriteBytes",result.writeBytes},{"rateBytesPerSecond",result.rateBytesPerSecond/1000*1000},{"restrictedThermals",result.restrictedThermals},{"engineReady",ready},{"engineVersion",fio_pinned_version}};contracts_.validate("WorkloadAdmission",value);return value;
 }
 Json snapshot(){std::lock_guard<std::mutex> lock(mutex_);return snapshot_;}
 Json cancel(){std::lock_guard<std::mutex> lock(mutex_);if(busy_)cancel_.store(true);return snapshot_;}
 Json start(const Json& request){
  contracts_.validate("StartWorkloadRequest",request);const std::string requestId=request["requestId"];const auto workload=request["workload"];
  std::lock_guard<std::mutex> lock(mutex_);
  if(auto prior=store_.request(requestId);!prior.is_null()){if(prior["definition"]!=workload)throw std::runtime_error("Request ID already belongs to different controls");return request_snapshot(prior);}
  if(busy_)throw std::runtime_error("A workload is already active");if(worker_.joinable())worker_.join();
  engine_.ready();const auto resources=resources_();const auto approved=admit(workload,resources);if(!approved.allowed())throw std::runtime_error(approved.reasons.front());
  const auto id=random_id();Json metadata={{"schemaVersion","1.0.0"},{"runId",id},{"origin","live"},{"startedAt",utc_now()},{"endedAt",nullptr},{"inventory",inventory_},{"capabilities",Json::array()},{"workload",workload},{"experimentExecutionId",nullptr},{"phaseId",nullptr},{"engine",engine_.metadata(workload,id)},{"mappingVersion","1.0.0"},{"analyzerVersion","1.0.0"},{"simulatorVersion",nullptr},{"seed",nullptr},{"outcome","running"},{"abortReason",nullptr},{"uiMode","measurement"},{"summaries",Json::array()},{"artifacts",Json::array()}};
  const auto started=std::chrono::steady_clock::now();auto initial=sample(id,0,0,"preparing",nullptr,workload);initial["safetyEvents"].push_back({{"schemaVersion","1.0.0"},{"eventId",random_id()},{"runId",id},{"elapsedUs",0},{"ruleId","native-admission"},{"action","admit"},{"message","Native reserve, thermal coverage and cumulative write checks passed"},{"evidence",Json::array()}});for(const auto& device:inventory_["devices"]){auto capabilities=Json::array();for(const auto& metric:initial["telemetry"]["measurements"])if(metric["deviceId"]==device["deviceId"])capabilities.push_back({{"metricId",metric["metricId"]},{"status",metric["status"]},{"requiresElevation",false},{"reason",metric["reason"]}});metadata["capabilities"].push_back({{"schemaVersion","1.0.0"},{"deviceId",device["deviceId"]},{"capabilities",capabilities}});}
  contracts_.validate("RunRecording",{{"schemaVersion","1.0.0"},{"metadata",metadata},{"samples",Json::array({initial})}});
  store_.begin_run(requestId,metadata,initial);cancel_.store(false);busy_=true;snapshot_={{"schemaVersion","1.0.0"},{"status",initial["workloadStatus"]},{"workload",workload},{"recordingId",nullptr}};
  try{worker_=std::thread([this,metadata,initial,workload,id,resources,approved,started]()mutable noexcept{
   try{auto samples=Json::array({initial});std::string state="preparing";unsigned long long sequence=1;auto last=started;std::optional<std::string> safetyReason;
   auto append=[&](const std::string& next,const Json& reason,bool persist=true){const auto now=std::chrono::steady_clock::now();auto value=sample(id,sequence++,static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::microseconds>(now-started).count()),next,reason,workload);if(persist)store_.append_sample(id,value);samples.push_back(value);state=next;last=now;if(persist){std::lock_guard<std::mutex> lock(mutex_);snapshot_["status"]=value["workloadStatus"];}};
    auto watchdog=[&]()->std::optional<std::string>{safetyReason=runtime_breach(resources_(),approved,resources);if(std::chrono::steady_clock::now()-last>=std::chrono::seconds(1))append(state,nullptr);return safetyReason;};
    auto result=engine_.execute(workload,id,cancel_,watchdog,[&]{append("running",nullptr);});
    if(safetyReason&&result.state!="cancelled"){result.state="aborted";result.reason=safetyReason;}
    append(result.state,result.reason?Json(*result.reason):Json(nullptr),false);metadata["outcome"]=result.state;metadata["abortReason"]=result.reason?Json(*result.reason):Json(nullptr);metadata["endedAt"]=utc_now();metadata["summaries"]=result.summaries;
    auto safetyEvent=[&](const std::string& action,const std::string& message){samples.back()["safetyEvents"].push_back({{"schemaVersion","1.0.0"},{"eventId",random_id()},{"runId",id},{"elapsedUs",samples.back()["telemetry"]["elapsedUs"]},{"ruleId","native-lifecycle"},{"action",action},{"message",message},{"evidence",Json::array()}});};
    if(result.state=="aborted")safetyEvent("abort",result.reason.value_or("Native watchdog aborted the run"));
    if(result.cleaned)safetyEvent(*result.cleaned?"cleanup":"cleanup_failed",*result.cleaned?"Owned target and manifest cleanup completed":"Owned scratch cleanup requires attention");
    auto artifacts=Json::array();if(!result.xml.empty()){Json artifact={{"artifactId",random_id()},{"kind","fio-json"},{"sha256",hash_bytes(result.xml)}};metadata["artifacts"].push_back(artifact);artifact["payload"]=result.xml;artifacts.push_back(artifact);}
    if(!result.diagnostic.empty()){Json artifact={{"artifactId",random_id()},{"kind","diagnostic"},{"sha256",hash_bytes(result.diagnostic)}};metadata["artifacts"].push_back(artifact);artifact["payload"]=result.diagnostic;artifacts.push_back(artifact);}
    Json recording={{"schemaVersion","1.0.0"},{"metadata",metadata},{"samples",samples}};contracts_.validate("RunRecording",recording);const auto saved=store_.finish_run(recording,artifacts);
    std::lock_guard<std::mutex> lock(mutex_);snapshot_["status"]=samples.back()["workloadStatus"];snapshot_["recordingId"]=saved;busy_=false;
   }catch(...){std::lock_guard<std::mutex> lock(mutex_);busy_=false;try{snapshot_["status"]["state"]="failed";snapshot_["status"]["reason"]="Recording incomplete; journal retained for recovery";}catch(...){/* Preserve the durable journal even under allocation failure. */}}
  });}catch(...){busy_=false;throw;}
  return snapshot_;
 }
 // Recovery adds an unavailable terminal snapshot. Its elapsed time is the last
 // known clock value, not a claimed crash time; previous evidence is untouched.
 void recover(){for(auto recording:store_.pending()){
  auto& metadata=recording["metadata"];auto& samples=recording["samples"];const auto previous=samples.back();const auto state=previous["workloadStatus"]["state"].get<std::string>();
  if(state!="completed"&&state!="cancelled"&&state!="aborted"&&state!="failed"&&state!="interrupted"){
   auto frame=previous["telemetry"];for(auto& metric:frame["measurements"]){metric["value"]=nullptr;metric["status"]="unavailable";metric["reason"]="Agent restarted; no measurement at interruption";}
   samples.push_back(native_sample(frame,metadata["runId"],frame["sequence"].get<unsigned long long>()+1,frame["elapsedUs"],"interrupted","Agent restarted; interruption time is unknown",metadata["workload"],mapping_,std::chrono::milliseconds(0)));
  }
  metadata["outcome"]=samples.back()["workloadStatus"]["state"];metadata["abortReason"]=samples.back()["workloadStatus"]["reason"];metadata["endedAt"]=utc_now();
  contracts_.validate("RunRecording",recording);const auto id=store_.finish_run(recording);snapshot_={{"schemaVersion","1.0.0"},{"status",samples.back()["workloadStatus"]},{"workload",metadata["workload"]},{"recordingId",id}};
 }}
};
}
