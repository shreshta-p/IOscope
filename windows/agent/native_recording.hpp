#pragma once
#include "contracts.hpp"
#include "platform.hpp"
namespace ioscope {
// A recorder clock is independent of the collector clock. Copied measurements
// retain their true age; stale values become unavailable, never fresh evidence.
inline Json native_sample(Json frame,const std::string& runId,unsigned long long sequence,unsigned long long elapsedUs,
 const std::string& state,const Json& reason,const Json& workload,const Json& mapping,std::chrono::milliseconds age){
 frame["sessionId"]="run-"+runId;frame["runId"]=runId;frame["sequence"]=sequence;frame["elapsedUs"]=elapsedUs;frame["capturedAt"]=utc_now();
 for(auto& metric:frame["measurements"]){metric["ageMs"]=std::min(9007199254740991.0,metric["ageMs"].get<double>()+static_cast<double>(age.count()));if(metric["status"]=="available"&&metric["ageMs"].get<double>()>3000){metric["value"]=nullptr;metric["status"]="unavailable";metric["reason"]="Collector sample is stale";}}
 const auto reference=[&](const char* id)->Json{return {{"sequence",sequence},{"deviceId","nvme0"},{"metricId",id}};};
 const auto metric=[&](const char* id)->const Json*{for(const auto& value:frame["measurements"])if(value["metricId"]==id&&value["deviceId"]=="nvme0"&&value["scope"]=="system"&&value["status"]=="available"&&!value["value"].is_null())return &value;return nullptr;};
 auto paths=Json::array();
 for(const auto& spec:mapping["paths"]){const std::string id=spec["metricId"];const auto* source=metric(id.c_str());const double activity=source?bandwidth_activity((*source)["value"],mapping):0;const bool read=spec["pathId"]=="nvme-ram-read";paths.push_back({{"pathId",spec["pathId"]},{"from",read?"nvme0":"ram0"},{"to",read?"ram0":"nvme0"},{"representation","aggregated-conceptual"},{"status",!source?"unknown":activity>0?"active":"idle"},{"activity",activity},{"direction","forward"},{"evidence",source?Json::array({reference(id.c_str())}):Json::array()}});}
 for(const auto& spec:mapping["pipelinePaths"])paths.push_back({{"pathId",spec["pathId"]},{"from",spec["from"]},{"to",spec["to"]},{"representation","aggregated-conceptual"},{"status","unknown"},{"activity",0},{"direction","forward"},{"evidence",Json::array()}});
 const auto* queue=metric("storage.queue.average");
 Json flow={{"schemaVersion","1.0.0"},{"mappingVersion","1.0.0"},{"sequence",sequence},{"elapsedUs",elapsedUs},{"paths",paths},{"queue",{{"deviceId","nvme0"},{"representation","aggregated"},{"configuredLimit",workload["queueDepth"]},{"observedAverage",queue?(*queue)["value"]:Json(nullptr)},{"evidence",queue?Json::array({reference("storage.queue.average")}):Json::array()}}}};
 return {{"schemaVersion","1.0.0"},{"runId",runId},{"phaseId",nullptr},{"telemetry",frame},{"flow",flow},{"workloadStatus",{{"schemaVersion","1.0.0"},{"runId",runId},{"state",state},{"elapsedUs",elapsedUs},{"progress",state=="completed"?1:0},{"reason",reason}}},{"analyzerEvents",Json::array()},{"safetyEvents",Json::array()}};
}
}
