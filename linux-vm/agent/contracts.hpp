#pragma once
#include <nlohmann/json.hpp>
#include <jsoncons/json.hpp>
#include <jsoncons_ext/jsonschema/jsonschema.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include "recording_validation.hpp"
namespace ioscope {
using Json=nlohmann::json;
inline Json read_json(const std::filesystem::path& file){std::ifstream stream(file);if(!stream)throw std::runtime_error("Cannot read contract file");Json value;stream>>value;return value;}
class Contracts{
  Json domain_,catalog_,mapping_;
  std::map<std::string,std::shared_ptr<jsoncons::jsonschema::json_schema<jsoncons::json>>> compiled_;
  std::mutex mutex_;
  static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
  void metrics(const Json& list,const std::string& origin){std::set<std::string> seen;for(const auto& m:list){auto id=m.at("metricId").get<std::string>();require(catalog_.contains(id),"Unknown metric");const auto& def=catalog_[id];require(m["unit"]==def["unit"],"Invalid metric unit");auto provenance=m["provenance"];require(origin=="simulated"?provenance=="simulated":(provenance=="measured"||provenance=="derived"),"Invalid provenance");require(seen.insert(m["deviceId"].get<std::string>()+"/"+id+"/"+m["scope"].get<std::string>()).second,"Duplicate metric");if(!m["value"].is_null()){double v=m["value"];require(std::isfinite(v)&&v>=def["min"].get<double>()&&(def["max"].is_null()||v<=def["max"].get<double>()),"Metric bounds");}}}
public:
 explicit Contracts(const std::filesystem::path& directory):domain_(read_json(directory/"v1"/"domain.schema.json")){mapping_=read_json(directory/"flow-semantics.v1.json");const auto catalog=read_json(directory/"metric-catalog.v1.json");for(const auto& m:catalog["metrics"])catalog_[m["metricId"].get<std::string>()]=m;}
 void validate(const std::string& name,const Json& value){
   std::lock_guard<std::mutex> lock(mutex_);
   require(domain_["$defs"].contains(name),"Unknown domain contract");
   if(!compiled_.contains(name)){auto root=domain_;root["$ref"]="#/$defs/"+name;auto parsed=jsoncons::json::parse(root.dump());auto result=jsoncons::jsonschema::make_json_schema(parsed,jsoncons::jsonschema::evaluation_options{}.require_format_validation(true));compiled_[name]=std::make_shared<jsoncons::jsonschema::json_schema<jsoncons::json>>(std::move(result));}
   require(compiled_[name]->is_valid(jsoncons::json::parse(value.dump())),"Schema validation failed");
   if(name=="TelemetryFrame")metrics(value["measurements"],value["origin"]);
   const auto kind=name=="Recording"?(value.contains("config")?"SimulationRecording":"RunRecording"):name;
   if(kind=="SimulationRecording"||kind=="RunRecording"){
     validate_recording_links(value,mapping_);const auto& metadata=value["metadata"];if(kind=="SimulationRecording")require(metadata["origin"]=="simulated"&&metadata["seed"]==value["config"]["seed"],"Origin/seed mismatch");else require(metadata["origin"]=="live"&&metadata["seed"].is_null()&&metadata["simulatorVersion"].is_null()&&metadata["inventory"]["platform"]=="windows","Native origin mismatch");
     std::set<std::string> devices;for(auto& d:metadata["inventory"]["devices"])devices.insert(d["deviceId"]);
     require((metadata["outcome"]=="running")==metadata["endedAt"].is_null(),"Terminal timestamp mismatch");
     require(metadata["outcome"]=="running"||metadata["outcome"]=="completed"||!metadata["abortReason"].is_null(),"Abnormal outcome lacks reason");
     metrics(metadata["summaries"],metadata["origin"]);for(const auto& metric:metadata["summaries"])require(devices.contains(metric["deviceId"]),"Unknown summary device");
     for(const auto& device:metadata["inventory"]["devices"])metrics(device["metrics"],metadata["origin"]);
     for(const auto& capability:metadata["capabilities"])require(devices.contains(capability["deviceId"]),"Unknown capability device");
     long long previous=-1;std::string run=metadata["runId"];
     for(const auto& s:value["samples"]){const auto& f=s["telemetry"];require(f["origin"]==metadata["origin"]&&f["runId"]==run&&s["runId"]==run,"Run identity mismatch");metrics(f["measurements"],metadata["origin"]);long long sequence=f["sequence"];require(sequence>previous,"Invalid sample order");previous=sequence;require(s["flow"]["sequence"]==f["sequence"]&&s["flow"]["elapsedUs"]==f["elapsedUs"]&&s["workloadStatus"]["elapsedUs"]==f["elapsedUs"],"Snapshot clocks differ");for(auto& m:f["measurements"])require(devices.contains(m["deviceId"]),"Unknown device");}

   }
 }
};
}
