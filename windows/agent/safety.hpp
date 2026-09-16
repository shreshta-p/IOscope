#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
namespace ioscope {
constexpr std::uint64_t MiB=1024*1024,GiB=1024*MiB;
struct Resources {
 std::uint64_t diskTotal=0,diskFree=0,ramTotal=0,ramAvailable=0;
 std::optional<double> cpuTemperature,gpuTemperature,ssdTemperature;
 bool staleAdmittedSensor=false;
};
struct Admission {
 std::vector<std::string> reasons;
 std::uint64_t diskReserve=0,ramReserve=0,diskAllocationBytes=0,bufferBytes=0,writeBytes=0,rateBytesPerSecond=0;
 bool restrictedThermals=true;
 bool allowed()const{return reasons.empty();}
};
inline std::uint64_t offered_rate(const std::string& intensity){return intensity=="light"?32*MiB:intensity=="moderate"?128*MiB:intensity=="high"?256*MiB:0;}
inline Admission admit(const nlohmann::json& workload,const Resources& resources,std::uint64_t alreadyWritten=0){
 Admission result;
 // Spare OS headroom is not a workload allocation. Fixed floors avoid scaling
 // the admission requirement with unrelated installed disk/RAM capacity.
 result.diskReserve=2*GiB;
 result.ramReserve=2*GiB;
 auto deny=[&](const char* text){result.reasons.emplace_back(text);};
 const auto bytes=workload.at("workingSetBytes").get<std::uint64_t>();
 const auto block=workload.at("blockBytes").get<std::uint64_t>();
 const auto queue=workload.at("queueDepth").get<std::uint64_t>();
 const auto duration=workload.at("durationSeconds").get<std::uint64_t>();
 const auto warmup=workload.at("warmupSeconds").get<std::uint64_t>();
 const auto cooldown=workload.at("cooldownSeconds").get<std::uint64_t>();
 const auto read=workload.at("readPercent").get<std::uint64_t>();
 result.rateBytesPerSecond=offered_rate(workload.at("intensity"));
 if(bytes<64*MiB||bytes>4*GiB||queue<1||queue>32||duration<1||duration>60||warmup>5||cooldown>5||read>100||!result.rateBytesPerSecond||
  (block!=4096&&block!=16384&&block!=65536&&block!=262144&&block!=1048576)){
  deny("Workload exceeds validated control bounds");return result;
 }
 if(workload.at("engine")!="diskspd")deny("Requested engine is not available");
 if(workload.at("pattern")!="random"&&workload.at("pattern")!="sequential")deny("Unknown access pattern");
 if(workload.at("cacheMode")!="buffered"&&workload.at("cacheMode")!="unbuffered")deny("Unknown cache mode");
 if(bytes%block)deny("Working set must align to block size");
 result.bufferBytes=256*MiB+MiB+block*queue;
 result.diskAllocationBytes=bytes+64*MiB; // Owned target plus artifact/journal allowance.
 // A requested mix is not a hard upper bound on writes in a finite interval.
 result.writeBytes=bytes+(read==100?0:result.rateBytesPerSecond*(duration+warmup+cooldown))+block*queue+64*MiB;
 if(resources.diskTotal==0||resources.diskFree<result.diskReserve||resources.diskFree-result.diskReserve<result.diskAllocationBytes)deny("Insufficient disk space for the test file, recording allowance and 2 GiB spare space");
 if(resources.ramTotal==0||resources.ramAvailable<result.ramReserve||resources.ramAvailable-result.ramReserve<result.bufferBytes)deny("Insufficient available RAM after the required reserve");
 if(alreadyWritten>8*GiB||result.writeBytes>8*GiB-alreadyWritten)deny("Preparation and offered writes exceed the cumulative 8 GiB budget");
 result.restrictedThermals=!resources.cpuTemperature||!resources.ssdTemperature;
 if(result.restrictedThermals&&(workload.at("intensity")!="light"||duration>15))deny("Missing CPU/SSD temperatures require light intensity and at most 15 seconds");
 const auto hot=[](std::optional<double> value,double warning){return value&&(!std::isfinite(*value)||*value<0||*value>=warning);};
 if(hot(resources.cpuTemperature,85)||hot(resources.gpuTemperature,80)||hot(resources.ssdTemperature,65))deny("Temperature is invalid or at the warning threshold");
 if(resources.staleAdmittedSensor)deny("A sensor used for admission is stale");
 return result;
}
inline std::optional<std::string> runtime_breach(const Resources& current,const Admission& admission,const Resources& admitted){
 if(current.diskFree<admission.diskReserve)return "Disk reserve breached";
 if(current.ramAvailable<admission.ramReserve)return "RAM reserve breached";
 if(current.staleAdmittedSensor||(admitted.cpuTemperature&&!current.cpuTemperature)||(admitted.gpuTemperature&&!current.gpuTemperature)||(admitted.ssdTemperature&&!current.ssdTemperature))return "Admitted temperature sensor became unavailable";
 for(const auto& item:{std::make_pair(current.cpuTemperature,85.0),std::make_pair(current.gpuTemperature,80.0),std::make_pair(current.ssdTemperature,65.0)})if(item.first&&(!std::isfinite(*item.first)||*item.first<0||*item.first>=item.second))return "Temperature warning threshold reached";
 return std::nullopt;
}
}
