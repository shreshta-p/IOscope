#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <map>
#include <cmath>
#include <stdexcept>
namespace ioscope {
using RecordJson=nlohmann::json;
inline void recording_require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
inline const RecordJson& evidence_metric(const RecordJson& frame,const RecordJson& reference){
 recording_require(frame["sequence"]==reference["sequence"],"Evidence sequence mismatch");
 const RecordJson* found=nullptr;
 for(const auto& metric:frame["measurements"])if(metric["deviceId"]==reference["deviceId"]&&metric["metricId"]==reference["metricId"]){recording_require(!found,"Ambiguous evidence scope");found=&metric;}
 recording_require(found&&(*found)["status"]=="available"&&!(*found)["value"].is_null()&&(*found)["ageMs"].get<double>()<=3000,"Unavailable evidence");return *found;
}
inline double bandwidth_activity(double value,const RecordJson& mapping){const auto scale=mapping["bandwidthScaleBytesPerSecond"].get<double>();return std::log2(1+std::min(value,scale)/1048576)/std::log2(1+scale/1048576);}
inline void validate_snapshot_flow(const RecordJson& flow,const RecordJson& frame,const RecordJson& mapping){
 std::set<std::string> paths;
 for(const auto& path:flow["paths"]){
  recording_require(paths.insert(path["pathId"].get<std::string>()).second,"Duplicate flow path");const double activity=path["activity"];
  recording_require(path["status"]=="active"?(activity>0&&!path["evidence"].empty()):activity==0,"Flow status/activity mismatch");
  for(const auto& evidence:path["evidence"])evidence_metric(frame,evidence);
  if(path["status"]=="unknown")continue;
  auto source=[&](const std::string& id)->const RecordJson&{for(const auto& reference:path["evidence"])if(reference["metricId"]==id)return evidence_metric(frame,reference);throw std::runtime_error("Flow source missing");};
  for(const auto& spec:mapping["paths"])if(spec["pathId"]==path["pathId"]){const auto& metric=source(spec["metricId"]);recording_require(metric["deviceId"]==(spec["pathId"]=="nvme-ram-read"?path["from"]:path["to"]),"Flow device mismatch");recording_require(std::abs(activity-bandwidth_activity(metric["value"],mapping))<1e-12,"Flow scaling mismatch");}
  for(const auto& spec:mapping["pipelinePaths"])if(spec["pathId"]==path["pathId"]){const auto& stage=source("pipeline.stage");const auto& metric=source(spec["metricId"]);const double expected=stage["value"]==spec["stage"]?(spec["scaling"]=="bandwidth"?bandwidth_activity(metric["value"],mapping):metric["value"].get<double>()/100):0;recording_require(std::abs(activity-expected)<1e-12,"Pipeline flow mismatch");}
 }
 const auto& queue=flow["queue"];if(!queue["observedAverage"].is_null()){bool matched=false;for(const auto& evidence:queue["evidence"])if(evidence["deviceId"]==queue["deviceId"]&&evidence["metricId"]=="storage.queue.average"){recording_require(evidence_metric(frame,evidence)["value"]==queue["observedAverage"],"Queue value mismatch");matched=true;}recording_require(matched,"Queue evidence missing");}
}
inline void validate_recording_links(const RecordJson& recording,const RecordJson& mapping){
 const auto& metadata=recording["metadata"];const auto& samples=recording["samples"];
 std::set<std::string> devices;for(const auto& device:metadata["inventory"]["devices"])recording_require(devices.insert(device["deviceId"].get<std::string>()).second,"Duplicate inventory device");
 std::map<unsigned long long,const RecordJson*> frames;std::string previousState;unsigned long long previousTime=0,previousSequence=0;bool first=true;
 const std::map<std::string,std::set<std::string>> transitions={{"validating",{"validating","preparing","cancelled","aborted","failed","interrupted"}},{"preparing",{"preparing","running","cancelled","aborted","failed","interrupted"}},{"running",{"running","stopping","completed","cancelled","aborted","failed","interrupted"}},{"stopping",{"stopping","completed","cancelled","aborted","failed","interrupted"}}};
 for(const auto& sample:samples){const auto& frame=sample["telemetry"];const auto sequence=frame["sequence"].get<unsigned long long>(),time=frame["elapsedUs"].get<unsigned long long>();const std::string state=sample["workloadStatus"]["state"];
  recording_require(sample["runId"]==metadata["runId"]&&frame["runId"]==metadata["runId"]&&sample["workloadStatus"]["runId"]==metadata["runId"]&&frame["origin"]==metadata["origin"],"Recording identity mismatch");
  recording_require(frame["sessionId"]==samples.front()["telemetry"]["sessionId"],"Recording session changed");
  if(!first){recording_require(sequence>previousSequence&&time>=previousTime,"Sample ordering mismatch");const auto transition=transitions.find(previousState);recording_require(transition==transitions.end()?state==previousState:transition->second.contains(state),"Workload transition mismatch");}
  first=false;previousSequence=sequence;previousTime=time;previousState=state;
  for(const auto& metric:frame["measurements"])recording_require(devices.contains(metric["deviceId"].get<std::string>()),"Unknown measurement device");
  for(const auto& path:sample["flow"]["paths"])recording_require(devices.contains(path["from"].get<std::string>())&&devices.contains(path["to"].get<std::string>()),"Unknown flow device");
  recording_require(devices.contains(sample["flow"]["queue"]["deviceId"].get<std::string>()),"Unknown queue device");
  recording_require(sample["flow"]["sequence"]==frame["sequence"]&&sample["flow"]["elapsedUs"]==frame["elapsedUs"]&&sample["workloadStatus"]["elapsedUs"]==frame["elapsedUs"],"Snapshot clock mismatch");
  validate_snapshot_flow(sample["flow"],frame,mapping);frames[sequence]=&frame;
  for(const char* kind:{"analyzerEvents","safetyEvents"})for(const auto& event:sample[kind]){recording_require(event["runId"]==metadata["runId"]&&event["elapsedUs"].get<unsigned long long>()<=time,"Event identity/time mismatch");for(const auto& reference:event["evidence"]){auto found=frames.find(reference["sequence"].get<unsigned long long>());recording_require(found!=frames.end(),"Event evidence missing");evidence_metric(*found->second,reference);}}
 }
 const std::set<std::string> terminal={"completed","cancelled","aborted","failed","interrupted"};
 recording_require(terminal.contains(metadata["outcome"].get<std::string>())?previousState==metadata["outcome"]:!terminal.contains(previousState),"Terminal outcome mismatch");
 if(recording.contains("config")){const auto& config=recording["config"];const auto interval=config["sampleIntervalMs"].get<unsigned long long>();const auto duration=config["durationSeconds"].get<unsigned long long>()*1000;recording_require(samples.size()==(duration+interval-1)/interval,"Simulation sample count mismatch");for(size_t i=0;i<samples.size();++i)recording_require(samples[i]["telemetry"]["sequence"]==i&&samples[i]["telemetry"]["elapsedUs"]==i*interval*1000,"Simulation clock mismatch");}
}
}
