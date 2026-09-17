#include "experiment_controller.hpp"
#include <iostream>
#include <thread>
using namespace ioscope;
class FakeEngine:public WorkloadEngine {
public:
 std::atomic<int> calls{0};bool wait=false,fail=false;
 std::vector<std::string> seenDatasetIds;std::vector<bool> seenPrepare,seenCleanup;
 void ready(const std::string&)override{}
 Json metadata(const Json&,const std::string&,const std::string&)override{return {{"name","fake-adapter-test"},{"version","test"},{"sha256",std::string(64,'0')},{"argv",Json::array()}};}
 EngineResult execute(const Json&,const std::string&,const std::atomic<bool>& cancel,const std::function<std::optional<std::string>()>& watchdog,const std::function<void()>& running,const std::string& datasetId,bool prepareDataset,bool cleanupDataset)override{
  calls++;seenDatasetIds.push_back(datasetId);seenPrepare.push_back(prepareDataset);seenCleanup.push_back(cleanupDataset);
  running();while(wait&&!cancel.load())std::this_thread::sleep_for(std::chrono::milliseconds(10));EngineResult result;if(fail){result.state="failed";result.reason="Authored parser failure";return result;}if(cancel.load()){result.state="cancelled";result.reason="Test cancellation";}else if(auto reason=watchdog()){result.state="aborted";result.reason=reason;}else result.state="completed";return result;
 }
};
static Json make_definition(const Json& baseWorkload,const std::vector<int>& queueDepths,int settleSeconds){
 Json definition={{"schemaVersion","1.0.0"},{"definitionId","qd-sweep-test"},{"title","Queue depth sweep (test)"},
  {"question","How does concurrency affect storage?"},{"concept","Configured outstanding limit differs from observed queue."},
  {"variable","queueDepth"},{"hypothesis","Additional concurrency may improve utilization."},{"phases",Json::array()}};
 for(std::size_t i=0;i<queueDepths.size();i++){
  auto workload=baseWorkload;workload["queueDepth"]=queueDepths[i];workload["definitionId"]="qd-phase-"+std::to_string(i);
  definition["phases"].push_back({{"schemaVersion","1.0.0"},{"phaseId","qd"+std::to_string(queueDepths[i])},{"ordinal",static_cast<int>(i)},
   {"purpose","Observe queue depth "+std::to_string(queueDepths[i])},{"workload",workload},{"observe",Json::array({"storage.read.bytes_per_second"})},
   {"settleSeconds",settleSeconds}});
 }
 return definition;
}
static Json make_access_pass_definition(const Json& baseWorkload){
 Json definition={{"schemaVersion","1.0.0"},{"definitionId","access-pass-test"},{"title","First/repeated access (test)"},
  {"question","Does a repeated read differ from the first?"},{"concept","Preparation may warm cache; never claim genuinely cold."},
  {"variable","accessPass"},{"hypothesis","A repeated pass may read faster than the first."},{"phases",Json::array()}};
 const char* passes[]={"first","repeated"};
 for(int i=0;i<2;i++)definition["phases"].push_back({{"schemaVersion","1.0.0"},{"phaseId",std::string(passes[i])+"-pass"},{"ordinal",i},
  {"purpose",std::string("Observe the ")+passes[i]+" pass"},{"workload",baseWorkload},{"observe",Json::array({"storage.read.bytes_per_second"})},
  {"settleSeconds",0}});
 return definition;
}
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("Contracts directory required");const std::filesystem::path root(argv[1]);
 Contracts contracts(root);Store store(":memory:");FakeEngine engine;
 auto fixture=read_json(root/"fixtures/RunRecording.json");auto baseWorkload=fixture["metadata"]["workload"];
 baseWorkload["durationSeconds"]=1;baseWorkload["warmupSeconds"]=0;baseWorkload["cooldownSeconds"]=0;
 auto inventory=fixture["metadata"]["inventory"];const auto mapping=read_json(root/"flow-semantics.v1.json");
 for(const char* id:{"gpu0","vram0"})inventory["devices"].push_back({{"deviceId",id},{"kind",id==std::string("gpu0")?"gpu":"vram"},{"name","Fake test device"},{"metrics",Json::array()}});
 Resources resources;resources.diskTotal=100*GiB;resources.diskFree=60*GiB;resources.ramTotal=16*GiB;resources.ramAvailable=12*GiB;
 WorkloadController workloads(contracts,store,engine,inventory,mapping,[&]{return resources;},[&]{return RecordedFrame{fixture["samples"][0]["telemetry"],std::chrono::milliseconds(0)};});
 ExperimentController experiments(contracts,store,workloads,engine,[&]{return resources;});

 // Aggregate admission across phases, computed before any allocation.
 auto definition=make_definition(baseWorkload,{1,2,4},0);
 auto admitted=experiments.admission(definition);
 if(admitted["allowed"]!=true||admitted["phaseAdmissions"].size()!=3)throw std::runtime_error("Healthy experiment admission failed: "+admitted.dump());

 // A single denied phase denies the whole experiment; no side effects.
 resources.diskFree=1;
 auto denied=experiments.admission(definition);
 if(denied["allowed"]!=false||denied["reasons"].empty())throw std::runtime_error("Insufficient-disk experiment was admitted");
 resources.diskFree=60*GiB;

 // Aggregate wall-time budget (600s) is enforced before phase one, not per phase.
 auto longWorkload=baseWorkload;longWorkload["durationSeconds"]=60;longWorkload["warmupSeconds"]=5;longWorkload["cooldownSeconds"]=5;
 std::vector<int> eightPhases{1,2,4,8,16,32,16,8};
 auto longDefinition=make_definition(longWorkload,eightPhases,10);
 auto overBudget=experiments.admission(longDefinition);
 if(overBudget["allowed"]!=false||overBudget["totalWallSeconds"].get<int>()<=600)throw std::runtime_error("Over-budget experiment was admitted: "+overBudget.dump());

 // Full real sequencing: three phases, linked by one experimentExecutionId.
 Json request={{"schemaVersion","1.0.0"},{"requestId",std::string(32,'a')},{"definition",definition}};
 auto started=experiments.start(request);const std::string executionId=started["executionId"];
 auto wait=[&]{for(int i=0;i<500;i++){const auto value=experiments.snapshot();if(value["status"]=="completed"||value["status"]=="cancelled"||value["status"]=="aborted"||value["status"]=="failed")return value;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Experiment completion deadline: "+experiments.snapshot().dump());};
 auto finished=wait();
 if(finished["status"]!="completed"||finished["currentPhaseOrdinal"]!=3||engine.calls!=3)throw std::runtime_error("Sequenced experiment did not complete cleanly: "+finished.dump());
 for(std::size_t i=0;i<3;i++){
  const auto& phase=finished["phases"][i];if(phase["outcome"]!="completed"||phase["runId"].is_null())throw std::runtime_error("Phase evidence missing");
  const auto recording=Json::parse(store.get(phase["runId"].get<std::string>()));
  if(recording["metadata"]["experimentExecutionId"]!=executionId||recording["metadata"]["phaseId"]!=phase["phaseId"])throw std::runtime_error("Phase run not linked to its execution");
 }

 // Idempotent retry of the same request returns the same execution, no re-run.
 auto retried=experiments.start(request);
 if(retried["executionId"]!=executionId||engine.calls!=3)throw std::runtime_error("Repeated experiment request executed again");

 // First/repeated access: both phases share one dataset, keyed by the
 // execution, not each phase's own runId. Phase one prepares and keeps it;
 // phase two reuses and cleans it up.
 engine.calls=0;engine.seenDatasetIds.clear();engine.seenPrepare.clear();engine.seenCleanup.clear();
 auto accessPassDefinition=make_access_pass_definition(baseWorkload);
 Json accessPassRequest={{"schemaVersion","1.0.0"},{"requestId",std::string(32,'e')},{"definition",accessPassDefinition}};
 auto accessStarted=experiments.start(accessPassRequest);const std::string accessExecutionId=accessStarted["executionId"];
 auto accessFinished=wait();
 if(accessFinished["status"]!="completed"||engine.calls!=2)throw std::runtime_error("Access-pass experiment did not complete cleanly: "+accessFinished.dump());
 if(engine.seenDatasetIds[0]!=accessExecutionId||engine.seenDatasetIds[1]!=accessExecutionId)throw std::runtime_error("Access-pass phases did not share one dataset");
 if(engine.seenPrepare[0]!=true||engine.seenPrepare[1]!=false)throw std::runtime_error("Access-pass prepare flags wrong: first pass must prepare, repeated pass must reuse");
 if(engine.seenCleanup[0]!=false||engine.seenCleanup[1]!=true)throw std::runtime_error("Access-pass cleanup flags wrong: first pass must keep the dataset, repeated pass must clean it up");

 // 7E: capability-gated, no execution engine. Denied with a specific, honest
 // reason -- never silently accepted, never claiming GPU work is supported.
 auto gpuWorkload=baseWorkload;gpuWorkload["engine"]="gpu-pipeline";
 Json gpuDefinition={{"schemaVersion","1.0.0"},{"definitionId","gpu-pipeline-test"},{"title","Storage-to-GPU pipeline (test)"},
  {"question","Does staged host-to-device transfer add measurable overhead?"},{"concept","Explicit staged transfer, not DirectStorage."},
  {"variable","pipelineStage"},{"hypothesis","Host-to-device transfer adds a measurable, separately timed phase."},
  {"phases",Json::array({{{"schemaVersion","1.0.0"},{"phaseId","stage0"},{"ordinal",0},{"purpose","Observe host-to-device transfer"},
   {"workload",gpuWorkload},{"observe",Json::array({"storage.read.bytes_per_second"})},{"settleSeconds",0}}})}};
 auto gpuAdmission=experiments.admission(gpuDefinition);
 if(gpuAdmission["allowed"]!=false)throw std::runtime_error("GPU pipeline experiment was admitted without an execution engine");
 bool sawCapabilityReason=false;for(const auto& reason:gpuAdmission["reasons"])if(reason.get<std::string>().find("CUDA capability probe")!=std::string::npos)sawCapabilityReason=true;
 if(!sawCapabilityReason)throw std::runtime_error("GPU pipeline denial did not report the capability probe result: "+gpuAdmission["reasons"].dump());

 // Cancellation stops the current phase and every phase after it.
 engine.wait=true;engine.calls=0;
 auto cancelRequest=make_definition(baseWorkload,{1,2,4},0);
 Json startCancel={{"schemaVersion","1.0.0"},{"requestId",std::string(32,'b')},{"definition",cancelRequest}};
 experiments.start(startCancel);
 for(int i=0;i<200&&engine.calls.load()<1;i++)std::this_thread::sleep_for(std::chrono::milliseconds(5));
 experiments.cancel();
 auto cancelled=wait();
 if(cancelled["status"]!="cancelled"||engine.calls!=1)throw std::runtime_error("Cancellation did not stop future phases: "+cancelled.dump());
 engine.wait=false;

 // Recovery closes out an execution left "running" by an unclean shutdown.
 Json orphan={{"schemaVersion","1.0.0"},{"executionId",std::string(48,'c')},{"definitionId","orphan-test"},{"startedAt",utc_now()},
  {"endedAt",nullptr},{"status","running"},{"currentPhaseOrdinal",0},{"phases",Json::array({{{"phaseId","p0"},{"ordinal",0},{"runId",nullptr},{"outcome",nullptr}}})}};
 contracts.validate("ExperimentExecution",orphan);store.save_experiment(orphan);
 ExperimentController fresh(contracts,store,workloads,engine,[&]{return resources;});fresh.recover();
 const auto recovered=store.experiment(std::string(48,'c'));
 if(recovered["status"]!="interrupted"||recovered["endedAt"].is_null())throw std::runtime_error("Orphaned experiment was not recovered");

 std::cout<<"PASS: aggregate admission, budget rejection, sequenced real phases with linkage, idempotency, shared-dataset access-pass flags, cancellation and recovery\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
