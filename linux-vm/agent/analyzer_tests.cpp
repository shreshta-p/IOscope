#include "analyzer.hpp"
#include <iostream>
using namespace ioscope;
static Json measurement(const std::string& device,const std::string& metricId,double value,bool available=true){
 return {{"deviceId",device},{"metricId",metricId},{"status",available?"available":"unavailable"},{"value",available?Json(value):Json(nullptr)}};
}
static Json frame(std::initializer_list<Json> measurements,unsigned long long elapsedUs=0){
 return {{"elapsedUs",elapsedUs},{"measurements",Json(measurements)}};
}
static Json ram_frame(double availableRatio){return frame({measurement("ram0","ram.available",availableRatio*16.0),measurement("ram0","ram.total",16.0)});}
static bool has_rule(const Json& events,const std::string& ruleId){for(const auto& e:events)if(e["ruleId"]==ruleId)return true;return false;}
// Feeds n frames and returns whether ruleId fired on any of them.
static bool any_fired(Analyzer& analyzer,const std::string& ruleId,const Json& sampleFrame,int n,unsigned long long& sequence,std::chrono::steady_clock::time_point now){
 bool fired=false;for(int i=0;i<n;i++){auto events=analyzer.evaluate(sampleFrame,sequence++,now);if(has_rule(events,ruleId))fired=true;}return fired;
}
int main(){try{
 auto now=std::chrono::steady_clock::now();
 // Memory pressure: negative below threshold (5 healthy samples never fire).
 {Analyzer analyzer;unsigned long long seq=0;
  if(any_fired(analyzer,"memory-pressure",ram_frame(0.50),5,seq,now))throw std::runtime_error("Memory pressure fired above threshold");
 }
 // Positive: fires exactly once on the 5th consecutive low sample, not earlier.
 {Analyzer analyzer;unsigned long long seq=0;int fires=0;
  for(int i=0;i<4;i++){auto events=analyzer.evaluate(ram_frame(0.05),seq++,now);if(has_rule(events,"memory-pressure"))throw std::runtime_error("Memory pressure fired before 5 consecutive samples");}
  auto fifth=analyzer.evaluate(ram_frame(0.05),seq++,now);
  if(!has_rule(fifth,"memory-pressure"))throw std::runtime_error("Memory pressure did not fire on the 5th consecutive low sample");
  for(const auto& e:fifth)if(e["ruleId"]=="memory-pressure"){
   if(e["evidence"][0]["deviceId"]!="ram0"||e["evidence"][0]["metricId"]!="ram.available")throw std::runtime_error("Wrong evidence device/metric");
   if(!e["runId"].is_null())throw std::runtime_error("Analyzer should leave runId for the caller to fill in");
   fires++;
  }
  // Stays active (no re-fire) while the condition persists.
  for(int i=0;i<3;i++){auto events=analyzer.evaluate(ram_frame(0.05),seq++,now);if(has_rule(events,"memory-pressure"))fires++;}
  if(fires!=1)throw std::runtime_error("Memory pressure re-fired while sustained, expected exactly one transition event");
  // Recovery fires exactly once when the condition clears.
  auto recovered=analyzer.evaluate(ram_frame(0.50),seq++,now);
  if(!has_rule(recovered,"memory-pressure"))throw std::runtime_error("Memory pressure recovery did not fire");
 }
 // Debounce: re-triggering within 10s of the last firing is suppressed; after 10s it fires again.
 {Analyzer analyzer;unsigned long long seq=0;
  for(int i=0;i<5;i++)analyzer.evaluate(ram_frame(0.05),seq++,now); // fires once at sample 5
  analyzer.evaluate(ram_frame(0.50),seq++,now); // recovers
  bool suppressed=any_fired(analyzer,"memory-pressure",ram_frame(0.05),5,seq,now+std::chrono::seconds(3));
  if(suppressed)throw std::runtime_error("Debounce failed to suppress a re-trigger within 10s");
  analyzer.evaluate(ram_frame(0.50),seq++,now+std::chrono::seconds(3)); // recover from the suppressed episode
  bool refired=any_fired(analyzer,"memory-pressure",ram_frame(0.05),5,seq,now+std::chrono::seconds(15));
  if(!refired)throw std::runtime_error("Rule did not re-fire once the debounce window elapsed");
 }
 // Queue pressure: needs both queue rise >=2 and latency rise >=25% over 10 samples; negative when only one holds.
 {Analyzer analyzer;unsigned long long seq=0;
  for(int i=0;i<5;i++)analyzer.evaluate(frame({measurement("nvme0","storage.queue.average",1),measurement("nvme0","storage.latency.mean",1)}),seq++,now);
  bool fired=any_fired(analyzer,"queue-pressure",frame({measurement("nvme0","storage.queue.average",5),measurement("nvme0","storage.latency.mean",1.05)}),5,seq,now);
  if(fired)throw std::runtime_error("Queue pressure fired on queue rise alone, without a matching latency rise");
 }
 {Analyzer analyzer;unsigned long long seq=0;
  for(int i=0;i<5;i++)analyzer.evaluate(frame({measurement("nvme0","storage.queue.average",1),measurement("nvme0","storage.latency.mean",1)}),seq++,now);
  bool fired=any_fired(analyzer,"queue-pressure",frame({measurement("nvme0","storage.queue.average",5),measurement("nvme0","storage.latency.mean",2)}),5,seq,now);
  if(!fired)throw std::runtime_error("Queue pressure did not fire when both queue and latency crossed thresholds");
 }
 // Thermal warning: mirrors the 85/80/65 safety thresholds; suppressed entirely when the sensor is unavailable.
 {Analyzer analyzer;
  auto below=analyzer.evaluate(frame({measurement("cpu0","cpu.temperature",70)}),0,now);
  if(has_rule(below,"thermal-warning"))throw std::runtime_error("Thermal warning fired below threshold");
  auto above=analyzer.evaluate(frame({measurement("cpu0","cpu.temperature",90)}),1,now);
  if(!has_rule(above,"thermal-warning"))throw std::runtime_error("Thermal warning did not fire at/above threshold");
  auto unavailable=analyzer.evaluate(frame({measurement("cpu0","cpu.temperature",0,false)}),2,now);
  if(has_rule(unavailable,"thermal-warning"))throw std::runtime_error("Thermal warning fired on an unavailable sensor");
 }
 // GPU transfer phase and VRAM pressure: correctly never fire when the metric is simply absent (no GPU in this VM).
 {Analyzer analyzer;unsigned long long seq=0;
  bool fired=any_fired(analyzer,"vram-pressure",frame({measurement("vram0","vram.used",0,false),measurement("vram0","vram.total",0,false)}),10,seq,now)||
   any_fired(analyzer,"gpu-transfer-phase",frame({measurement("gpu0","pipeline.stage",0,false)}),1,seq,now);
  if(fired)throw std::runtime_error("GPU-dependent rule fired without the metric present");
 }
 // VRAM pressure: positive when the metric genuinely is present and crosses 90%.
 {Analyzer analyzer;unsigned long long seq=0;
  for(int i=0;i<4;i++)analyzer.evaluate(frame({measurement("vram0","vram.used",7.5),measurement("vram0","vram.total",8)}),seq++,now);
  bool fired=any_fired(analyzer,"vram-pressure",frame({measurement("vram0","vram.used",7.5),measurement("vram0","vram.total",8)}),1,seq,now);
  if(!fired)throw std::runtime_error("VRAM pressure did not fire at/above 90%");
 }
 // GPU transfer phase: positive when the instrumented marker genuinely is present.
 {Analyzer analyzer;
  auto events=analyzer.evaluate(frame({measurement("gpu0","pipeline.stage",1)}),0,now);
  if(!has_rule(events,"gpu-transfer-phase"))throw std::runtime_error("GPU transfer phase did not fire when the marker is present");
 }
 // Completion anomaly: positive for a failed engine result, negative for a normal completion,
 // and correctly withheld (not fabricated) when the terminal frame has no available evidence at all.
 {
  auto terminalFrame=frame({measurement("cpu0","cpu.utilization",42)});
  auto anomalous=Analyzer::completion_anomaly(5,terminalFrame,true,"The workload engine did not complete normally: exit 1");
  if(anomalous.is_null()||anomalous["ruleId"]!="completion-anomaly")throw std::runtime_error("Completion anomaly did not build an event");
  if(anomalous["evidence"][0]["deviceId"]!="cpu0"||anomalous["evidence"][0]["metricId"]!="cpu.utilization")throw std::runtime_error("Completion anomaly cited the wrong evidence");
  auto normal=Analyzer::completion_anomaly(5,terminalFrame,false,"");
  if(!normal.is_null())throw std::runtime_error("Completion anomaly fired for a normal completion");
  auto emptyFrame=frame({measurement("cpu0","cpu.utilization",0,false)});
  auto withheld=Analyzer::completion_anomaly(5,emptyFrame,true,"anomalous but no evidence available");
  if(!withheld.is_null())throw std::runtime_error("Completion anomaly fabricated evidence when nothing was available");
 }
 std::cout<<"PASS: memory/queue/thermal/VRAM pressure rules, GPU-metric suppression, debounce, recovery and completion anomaly\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
