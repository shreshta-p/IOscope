#pragma once
// Phase 8 (docs/10-ANALYZER-SPEC.md): deterministic, versioned rules over
// normalized samples emitting immutable AnalyzerEvent records -- observation,
// evidence references, interpretation, confidence. Missing/stale metrics
// suppress the rules that depend on them; nothing is ever guessed. No Windows
// equivalent exists yet (same reason as experiment_controller.hpp).
//
// Rules run against ReplaySample.telemetry only, over a bounded rolling
// window this class owns -- no access to the full run's sample history is
// needed. Each rule is transition-only-emits-once with a 10s per-(rule,
// device) debounce, matching spec: "Event transition emits once, recovery
// emits once; 10s per-rule/device debounce."
#include "contracts.hpp"
#include "platform.hpp"
#include <deque>
#include <map>
#include <optional>
#include <chrono>
namespace ioscope {
inline constexpr const char* analyzer_rule_version="1.0.0";
class Analyzer {
 struct RuleState{bool active=false;std::chrono::steady_clock::time_point lastFired{};};
 std::map<std::string,RuleState> state_;
 std::deque<Json> window_;
 static constexpr std::size_t kWindow=10;
 static std::optional<double> metric(const Json& frame,const std::string& deviceId,const std::string& metricId){
  for(const auto& m:frame.at("measurements"))
   if(m.at("deviceId")==deviceId&&m.at("metricId")==metricId&&m.at("status")=="available")return m.at("value").get<double>();
  return std::nullopt;
 }
 static double mean(const std::deque<Json>& frames,std::size_t from,std::size_t count,const std::string& device,const std::string& metricId,bool& allPresent){
  double total=0;std::size_t seen=0;
  for(std::size_t i=from;i<from+count&&i<frames.size();i++){auto v=metric(frames[i],device,metricId);if(!v){allPresent=false;return 0;}total+=*v;seen++;}
  allPresent=seen==count;return seen?total/static_cast<double>(seen):0;
 }
 // Emits at most one event per rule/device per call: a transition into the
 // condition (debounced 10s) or a transition out of it (recovery, no debounce
 // -- spec debounces re-firing the same anomaly, not clearing it promptly).
 std::optional<Json> transition(const std::string& ruleId,const std::string& deviceId,bool conditionMet,
  const std::string& observation,const std::string& interpretation,const std::string& confidence,
  const Json& evidence,std::chrono::steady_clock::time_point now){
  auto& state=state_[ruleId+"/"+deviceId];
  if(conditionMet&&!state.active){
   if(now-state.lastFired<std::chrono::seconds(10)&&state.lastFired.time_since_epoch().count()!=0)return std::nullopt;
   state.active=true;state.lastFired=now;
   return Json{{"schemaVersion","1.0.0"},{"eventId",random_id()},{"runId",nullptr},{"elapsedUs",0},{"ruleId",ruleId},
    {"ruleVersion",analyzer_rule_version},{"observation",observation},{"interpretation",interpretation},
    {"confidence",confidence},{"simpleExplanation",nullptr},{"evidence",evidence}};
  }
  if(!conditionMet&&state.active){
   state.active=false;
   return Json{{"schemaVersion","1.0.0"},{"eventId",random_id()},{"runId",nullptr},{"elapsedUs",0},{"ruleId",ruleId},
    {"ruleVersion",analyzer_rule_version},{"observation","Condition previously observed by "+ruleId+" is no longer present."},
    {"interpretation","Recovery: the earlier observation no longer holds."},{"confidence","high"},{"simpleExplanation",nullptr},
    {"evidence",evidence}};
  }
  return std::nullopt;
 }
public:
 void reset(){state_.clear();window_.clear();}
 // Called once per sample, after native_sample() builds the frame and before
 // contracts validation. Returns zero or more new events for this sample.
 Json evaluate(const Json& frame,unsigned long long sequence,std::chrono::steady_clock::time_point now){
  window_.push_back(frame);if(window_.size()>kWindow)window_.pop_front();
  Json events=Json::array();
  const auto add=[&](std::optional<Json> event){if(event){(*event)["elapsedUs"]=frame.at("elapsedUs");events.push_back(*event);}};
  // Memory pressure: available/total <10% for 5 samples.
  if(window_.size()>=5){
   bool present=true;const auto& recent=window_;const auto start=recent.size()-5;
   bool pressureAll=true;
   for(std::size_t i=start;i<recent.size();i++){
    auto available=metric(recent[i],"ram0","ram.available");auto total=metric(recent[i],"ram0","ram.total");
    if(!available||!total||*total<=0){present=false;break;}
    if(*available / *total>=0.10)pressureAll=false;
   }
   if(present)add(transition("memory-pressure","ram0",pressureAll,
    "Available RAM has been below 10% of total for 5 consecutive samples.",
    "May indicate memory pressure.","medium",
    Json::array({{{"sequence",sequence},{"deviceId","ram0"},{"metricId","ram.available"}}}),now));
  }
  // Queue pressure: last-5 mean queue exceeds first-5 by >=2 AND mean latency rises >=25%, over 10 samples of nvme0.
  if(window_.size()>=10){
   bool queuePresent=true,latencyPresent=true;
   const double firstQueue=mean(window_,0,5,"nvme0","storage.queue.average",queuePresent);
   const double lastQueue=mean(window_,5,5,"nvme0","storage.queue.average",queuePresent);
   const double firstLatency=mean(window_,0,5,"nvme0","storage.latency.mean",latencyPresent);
   const double lastLatency=mean(window_,5,5,"nvme0","storage.latency.mean",latencyPresent);
   if(queuePresent&&latencyPresent&&firstLatency>0){
    const bool met=(lastQueue-firstQueue>=2.0)&&((lastLatency-firstLatency)/firstLatency>=0.25);
    add(transition("queue-pressure","nvme0",met,
     "Mean storage queue depth over the last 5 samples exceeds the first 5 by 2 or more, with mean latency up 25% or more.",
     "Consistent with increased outstanding storage activity; not a causal or workload-only claim.","medium",
     Json::array({{{"sequence",sequence},{"deviceId","nvme0"},{"metricId","storage.queue.average"}},
      {{"sequence",sequence},{"deviceId","nvme0"},{"metricId","storage.latency.mean"}}}),now));
   }
  }
  // Thermal warning: mirrors safety.hpp's own warning thresholds (85/80/65), never diagnoses throttling from temperature alone.
  for(const auto& [deviceId,metricId,warning]:{std::tuple{std::string("cpu0"),std::string("cpu.temperature"),85.0},
   std::tuple{std::string("gpu0"),std::string("gpu.temperature"),80.0},std::tuple{std::string("nvme0"),std::string("storage.temperature"),65.0}}){
   auto value=metric(frame,deviceId,metricId);
   if(value)add(transition("thermal-warning",deviceId,*value>=warning,
    deviceId+" temperature has reached the safety warning threshold.",
    "Mirrors the native safety warning threshold; not a throttling diagnosis. Throttling requires a reliable throttle flag or clock/power evidence.",
    "high",Json::array({{{"sequence",sequence},{"deviceId",deviceId},{"metricId",metricId}}}),now));
  }
  // GPU transfer phase: an explicit instrumented phase marker, never inferred from disk activity.
  // No GPU pipeline execution engine exists on this platform (see gpu_capability.hpp) -- this
  // metric is therefore always unavailable here, and this rule correctly never fires.
  {
   auto stage=metric(frame,"gpu0","pipeline.stage");
   if(stage)add(transition("gpu-transfer-phase","gpu0",true,
    "An instrumented GPU pipeline phase marker is active.","A storage-to-GPU pipeline stage is in progress.","high",
    Json::array({{{"sequence",sequence},{"deviceId","gpu0"},{"metricId","pipeline.stage"}}}),now));
  }
  // VRAM pressure: used/total >=90% for 5 samples, suppressed if either metric unavailable.
  if(window_.size()>=5){
   bool present=true;const auto start=window_.size()-5;bool pressureAll=true;
   for(std::size_t i=start;i<window_.size();i++){
    auto used=metric(window_[i],"vram0","vram.used");auto total=metric(window_[i],"vram0","vram.total");
    if(!used||!total||*total<=0){present=false;break;}
    if(*used / *total<0.90)pressureAll=false;
   }
   if(present)add(transition("vram-pressure","vram0",pressureAll,
    "VRAM used has been at or above 90% of total for 5 consecutive samples.",
    "May indicate device memory pressure.","medium",
    Json::array({{{"sequence",sequence},{"deviceId","vram0"},{"metricId","vram.used"}}}),now));
  }
  return events;
 }
 // Completion anomaly: checked once, at run completion, not per sample --
 // "expected terminal result missing or nonzero engine exit." Evidence must
 // cite a genuinely available measurement in the terminal frame; there is no
 // metric guaranteed present on every device inventory (a minimal fixture may
 // carry only one), so this picks whichever measurement the frame actually
 // has available rather than assuming a specific one exists. If truly none
 // are available, no event is emitted -- an honest claim needs real evidence,
 // never a fabricated reference.
 static Json completion_anomaly(unsigned long long sequence,const Json& frame,bool anomalous,const std::string& detail){
  if(!anomalous)return nullptr;
  for(const auto& m:frame.at("measurements"))if(m.at("status")=="available"){
   Json evidence=Json::array({{{"sequence",sequence},{"deviceId",m.at("deviceId")},{"metricId",m.at("metricId")}}});
   return Json{{"schemaVersion","1.0.0"},{"eventId",random_id()},{"runId",nullptr},{"elapsedUs",0},{"ruleId","completion-anomaly"},
    {"ruleVersion",analyzer_rule_version},{"observation",detail},
    {"interpretation","The engine's terminal result did not match what a normal completion produces."},
    {"confidence","high"},{"simpleExplanation",nullptr},{"evidence",evidence}};
  }
  return nullptr;
 }
};
}
