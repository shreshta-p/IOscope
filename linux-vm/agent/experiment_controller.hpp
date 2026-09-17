#pragma once
// Phase 7 (docs/07-EXPERIMENT-SPEC.md): serial controlled experiments. No
// Windows equivalent exists yet (Windows' own Phase 6 hardware gate is still
// outstanding, so Phase 7+ isn't implemented there either, per AGENTS.md's
// "do not implement later phases while their prerequisite gates fail").
//
// Reuses the existing single-run WorkloadController phase-by-phase, exactly as
// docs/plans/PHASE-6-WORKLOADS.md specifies ("each phase uses the same admitted
// controller") -- this gets the one-experiment/one-workload-at-a-time invariant
// for free from WorkloadController::busy_ and Store's journal_runs single-row
// constraint, with no new run-concurrency logic needed here.
#include "workload_controller.hpp"
#include <thread>
#include <atomic>
#include <chrono>
namespace ioscope {
class ExperimentController {
 Contracts& contracts_;Store& store_;WorkloadController& workload_;WorkloadEngine& engine_;
 std::function<Resources()> resources_;
 std::mutex mutex_;std::thread worker_;std::atomic<bool> cancel_{false};bool busy_=false;
 Json snapshot_=nullptr;
 // Aggregate every phase's admission before allocating anything ("reject before
 // allocating" per spec) -- does not call admission()/lock mutex_ itself, so
 // start() can reuse this logic while already holding the lock.
 struct Plan{std::vector<std::string> reasons;std::uint64_t totalWriteBytes=0,totalWallSeconds=0;Json phaseAdmissions=Json::array();bool allowed()const{return reasons.empty();}};
 Plan plan(const Json& definition,const Resources& resources)const{
  Plan result;std::uint64_t written=0;
  for(const auto& phase:definition.at("phases")){
   const auto& workload=phase.at("workload");auto admitted=admit(workload,resources,written);
   Json value={{"schemaVersion","1.0.0"},{"workload",workload},{"allowed",admitted.allowed()},{"reasons",admitted.reasons},
    {"diskFreeBytes",resources.diskFree},{"diskReserveBytes",admitted.diskReserve},{"diskAllocationBytes",admitted.diskAllocationBytes},
    {"ramAvailableBytes",resources.ramAvailable},{"ramReserveBytes",admitted.ramReserve},{"bufferBytes",admitted.bufferBytes},
    {"plannedWriteBytes",admitted.writeBytes},{"rateBytesPerSecond",admitted.rateBytesPerSecond/1000*1000},
    {"restrictedThermals",admitted.restrictedThermals},{"engineReady",true},{"engineVersion",fio_pinned_version}};
   contracts_.validate("WorkloadAdmission",value);result.phaseAdmissions.push_back(value);
   const std::string phaseId=phase.at("phaseId");
   for(const auto& reason:admitted.reasons)result.reasons.push_back(phaseId+": "+reason);
   written+=admitted.writeBytes;
   result.totalWallSeconds+=workload.at("durationSeconds").get<std::uint64_t>()+workload.at("warmupSeconds").get<std::uint64_t>()+workload.at("cooldownSeconds").get<std::uint64_t>()+phase.at("settleSeconds").get<std::uint64_t>();
  }
  result.totalWriteBytes=written;
  // The last phase has no following phase to settle before, matching start()'s
  // worker loop, which only sleeps settleSeconds when a next phase exists.
  if(!definition.at("phases").empty())result.totalWallSeconds-=definition.at("phases").back().at("settleSeconds").get<std::uint64_t>();
  if(result.totalWallSeconds>600)result.reasons.push_back("Experiment exceeds the 600 second wall-time budget, including settling");
  return result;
 }
public:
 ExperimentController(Contracts& contracts,Store& store,WorkloadController& workload,WorkloadEngine& engine,std::function<Resources()> resources)
 :contracts_(contracts),store_(store),workload_(workload),engine_(engine),resources_(std::move(resources)){}
 ~ExperimentController(){cancel_.store(true);if(worker_.joinable())worker_.join();}
 Json admission(const Json& definition){
  contracts_.validate("ExperimentDefinition",definition);
  const auto resources=resources_();auto result=plan(definition,resources);
  bool ready=false;try{engine_.ready();ready=true;}catch(const std::exception& error){result.reasons.push_back(std::string(error.what()).substr(0,512));}
  {std::lock_guard<std::mutex> lock(mutex_);if(busy_)result.reasons.push_back("An experiment is already active");}
  if(!store_.pending().empty())result.reasons.push_back("An unfinished run journal requires recovery");
  Json value={{"schemaVersion","1.0.0"},{"definition",definition},{"allowed",result.allowed()},{"reasons",result.reasons},
   {"totalWriteBytes",result.totalWriteBytes},{"totalWallSeconds",result.totalWallSeconds},{"engineReady",ready},
   {"engineVersion",fio_pinned_version},{"phaseAdmissions",result.phaseAdmissions}};
  contracts_.validate("ExperimentAdmission",value);return value;
 }
 Json snapshot(){std::lock_guard<std::mutex> lock(mutex_);return snapshot_;}
 // Cancels the in-progress phase (via the shared WorkloadController) and stops
 // any phase not yet started -- "stop cancels current and future phases".
 Json cancel(){std::lock_guard<std::mutex> lock(mutex_);if(busy_){cancel_.store(true);workload_.cancel();}return snapshot_;}
 Json start(const Json& request){
  contracts_.validate("StartExperimentRequest",request);const std::string requestId=request["requestId"];const auto definition=request["definition"];
  std::lock_guard<std::mutex> lock(mutex_);
  if(auto prior=store_.experiment_request(requestId);!prior.is_null()){
   if(prior["definition"]!=definition)throw std::runtime_error("Request ID already belongs to different experiment controls");
   return store_.experiment(prior["executionId"].get<std::string>());
  }
  if(busy_)throw std::runtime_error("An experiment is already active");if(worker_.joinable())worker_.join();
  engine_.ready();const auto resources=resources_();const auto approved=plan(definition,resources);
  if(!approved.allowed())throw std::runtime_error(approved.reasons.front());
  const auto executionId=random_id();
  Json execution={{"schemaVersion","1.0.0"},{"executionId",executionId},{"definitionId",definition.at("definitionId")},
   {"startedAt",utc_now()},{"endedAt",nullptr},{"status","running"},{"currentPhaseOrdinal",0},{"phases",Json::array()}};
  for(const auto& phase:definition.at("phases"))execution["phases"].push_back({{"phaseId",phase.at("phaseId")},{"ordinal",phase.at("ordinal")},{"runId",nullptr},{"outcome",nullptr}});
  contracts_.validate("ExperimentExecution",execution);
  store_.begin_experiment_request(requestId,definition,executionId);store_.save_experiment(execution);
  cancel_.store(false);busy_=true;snapshot_=execution;
  try{worker_=std::thread([this,definition,execution,executionId]()mutable noexcept{
   try{
    std::string finalStatus="completed";const auto& phases=definition.at("phases");
    for(std::size_t i=0;i<phases.size();i++){
     if(cancel_.load()){finalStatus="cancelled";break;}
     const auto& phase=phases[i];
     Json startRequest={{"schemaVersion","1.0.0"},{"requestId",random_id()},{"workload",phase.at("workload")}};
     workload_.start(startRequest,executionId,phase.at("phaseId"));
     Json phaseSnapshot;while(true){phaseSnapshot=workload_.snapshot();if(!phaseSnapshot["recordingId"].is_null())break;std::this_thread::sleep_for(std::chrono::milliseconds(50));}
     const std::string recordingId=phaseSnapshot["recordingId"];const auto recording=Json::parse(store_.get(recordingId));const std::string outcome=recording["metadata"]["outcome"];
     execution["phases"][i]["runId"]=recordingId;execution["phases"][i]["outcome"]=outcome;execution["currentPhaseOrdinal"]=static_cast<int>(i)+1;
     contracts_.validate("ExperimentExecution",execution);store_.save_experiment(execution);{std::lock_guard<std::mutex> lock(mutex_);snapshot_=execution;}
     if(outcome!="completed"){finalStatus=outcome=="cancelled"?"cancelled":outcome=="aborted"?"aborted":"failed";break;}
     if(i+1<phases.size()){
      const int settle=phase.at("settleSeconds").get<int>();bool cancelledDuringSettle=false;
      // Settling has no benchmark samples in its statistics: sleep only, no sampling.
      for(int tick=0;tick<settle*10;tick++){if(cancel_.load()){cancelledDuringSettle=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(100));}
      if(cancelledDuringSettle){finalStatus="cancelled";break;}
     }
    }
    execution["status"]=finalStatus;execution["endedAt"]=utc_now();
    contracts_.validate("ExperimentExecution",execution);store_.save_experiment(execution);
    std::lock_guard<std::mutex> lock(mutex_);snapshot_=execution;busy_=false;
   }catch(...){
    std::lock_guard<std::mutex> lock(mutex_);busy_=false;
    try{execution["status"]="failed";execution["endedAt"]=utc_now();store_.save_experiment(execution);snapshot_=execution;}catch(...){/* Preserve the durable journal even under allocation failure. */}
   }
  });}catch(...){busy_=false;throw;}
  return snapshot_;
 }
 // Recovery closes out any execution left "running" by an unclean shutdown as
 // "interrupted", mirroring WorkloadController::recover(). The in-flight
 // phase's own RunRecording is separately recovered by WorkloadController::
 // recover() (unchanged) and keeps its correct experimentExecutionId/phaseId;
 // it just won't be reflected in this execution's `phases[]` index, since the
 // crash happened before this class could record its outcome here -- an
 // honest gap, not fabricated data.
 void recover(){for(auto execution:store_.running_experiments()){
  execution["status"]="interrupted";execution["endedAt"]=utc_now();
  contracts_.validate("ExperimentExecution",execution);store_.save_experiment(execution);
  std::lock_guard<std::mutex> lock(mutex_);snapshot_=execution;
 }}
};
}
