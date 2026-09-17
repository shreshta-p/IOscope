#include "workload_controller.hpp"
#include <iostream>
#include <thread>
using namespace ioscope;
class FakeEngine:public WorkloadEngine {
public:
 std::atomic<int> calls{0};bool wait=false,fail=false;
 void ready()override{}
 Json metadata(const Json&,const std::string&)override{return {{"name","fake-adapter-test"},{"version","test"},{"sha256",std::string(64,'0')},{"argv",Json::array()}};}
 EngineResult execute(const Json&,const std::string&,const std::atomic<bool>& cancel,const std::function<std::optional<std::string>()>& watchdog,const std::function<void()>& running)override{
  calls++;running();while(wait&&!cancel.load())std::this_thread::sleep_for(std::chrono::milliseconds(10));EngineResult result;if(fail){result.state="failed";result.reason="Authored parser failure";result.diagnostic="Authored diagnostic artifact";return result;}if(cancel.load()){result.state="cancelled";result.reason="Test cancellation";}else if(auto reason=watchdog()){result.state="aborted";result.reason=reason;}else result.state="completed";return result;
 }
};
int main(int argc,char** argv){try{
 if(argc!=2&&argc!=3)throw std::runtime_error("Contracts directory required");const std::filesystem::path root(argv[1]);Contracts contracts(root);Store store(":memory:");FakeEngine engine;
 auto fixture=read_json(root/"fixtures/RunRecording.json"),workload=fixture["metadata"]["workload"],inventory=fixture["metadata"]["inventory"],mapping=read_json(root/"flow-semantics.v1.json");
 for(const char* id:{"gpu0","vram0"})inventory["devices"].push_back({{"deviceId",id},{"kind",id==std::string("gpu0")?"gpu":"vram"},{"name","Fake test device"},{"metrics",Json::array()}});
 Resources resources;resources.diskTotal=100*GiB;resources.diskFree=60*GiB;resources.ramTotal=16*GiB;resources.ramAvailable=12*GiB;
 WorkloadController controller(contracts,store,engine,inventory,mapping,[&]{return resources;},[&]{return RecordedFrame{fixture["samples"][0]["telemetry"],std::chrono::milliseconds(0)};});
 Json request={{"schemaVersion","1.0.0"},{"requestId",std::string(32,'a')},{"workload",workload}};
 resources.diskFree=1;if(controller.admission(workload)["allowed"]!=false)throw std::runtime_error("Low space admitted");bool denied=false;try{controller.start(request);}catch(const std::exception&){denied=true;}if(!denied||engine.calls||!store.pending().empty())throw std::runtime_error("Denied run had side effects");resources.diskFree=60*GiB;
 auto wait=[&]{for(int i=0;i<300;i++){const auto value=controller.snapshot();if(!value["recordingId"].is_null())return value;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Controller completion deadline: "+controller.snapshot().dump());};
 controller.start(request);auto completed=wait();auto recording=Json::parse(store.get(completed["recordingId"]));contracts.validate("RunRecording",recording);if(recording["metadata"]["outcome"]!="completed"||!store.pending().empty())throw std::runtime_error("Completion was not durable");
 if(controller.start(request)["recordingId"]!=completed["recordingId"]||engine.calls!=1)throw std::runtime_error("Repeated request executed again");
 auto changed=request;changed["workload"]["queueDepth"]=1;bool conflict=false;try{controller.start(changed);}catch(const std::exception&){conflict=true;}if(!conflict)throw std::runtime_error("Idempotency conflict accepted");
 engine.wait=true;request["requestId"]=std::string(32,'b');controller.start(request);controller.cancel();auto cancelled=wait();if(cancelled["status"]["state"]!="cancelled")throw std::runtime_error("Cancellation not saved");
 engine.wait=false;engine.fail=true;request["requestId"]=std::string(32,'d');controller.start(request);const auto failed=wait();const auto failure=Json::parse(store.get(failed["recordingId"]));contracts.validate("RunRecording",failure);if(failure["metadata"]["outcome"]!="failed"||!failure["metadata"]["summaries"].empty()||failure["metadata"]["artifacts"].size()!=1||store.artifact(failure["metadata"]["artifacts"][0]["artifactId"])["payload"]!="Authored diagnostic artifact")throw std::runtime_error("Failure lost diagnostic artifact");
 // Abrupt restart fixture: retain prior frame bytes and add an unavailable end.
 auto partial=recording;partial["metadata"]["runId"]="recovery-test";partial["metadata"]["outcome"]="running";partial["metadata"]["endedAt"]=nullptr;
 auto initial=native_sample(fixture["samples"][0]["telemetry"],"recovery-test",0,0,"preparing",nullptr,workload,mapping,std::chrono::milliseconds(0));store.begin_run(std::string(32,'c'),partial["metadata"],initial);
 controller.recover();const auto recovered=Json::parse(store.get(controller.snapshot()["recordingId"]));contracts.validate("RunRecording",recovered);if(recovered["samples"][0]!=initial||recovered["metadata"]["outcome"]!="interrupted"||recovered["samples"].back()["telemetry"]["measurements"][0]["value"]!=nullptr)throw std::runtime_error("Recovery changed evidence or fabricated a sample");
 if(argc==3){std::ofstream output(argv[2]);output<<Json::array({recording,Json::parse(store.get(cancelled["recordingId"])),failure,recovered}).dump(2);if(!output)throw std::runtime_error("Cannot export fake adapter validation fixtures");}
 std::cout<<"PASS: admission without side effects, durable completion, idempotency, cancellation and interrupted recovery\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
