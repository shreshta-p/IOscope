#pragma once
#include "safety.hpp"
#include "platform.hpp"
#include "win_handle.hpp"
namespace ioscope {
inline Resources system_resources(const nlohmann::json& frame,std::chrono::milliseconds frameAge){
 Resources result;ULARGE_INTEGER available{},total{},free{};
 win_require(GetDiskFreeSpaceExW(local_directory().c_str(),&available,&total,&free)!=0,"Read scratch volume space");
 result.diskTotal=total.QuadPart;result.diskFree=available.QuadPart;
 MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);win_require(GlobalMemoryStatusEx(&memory)!=0,"Read available memory");result.ramTotal=memory.ullTotalPhys;result.ramAvailable=memory.ullAvailPhys;
 for(const auto& metric:frame.at("measurements")){
  if(metric.at("status")!="available"||metric.at("value").is_null())continue;
  const auto id=metric.at("metricId").get<std::string>();
  if(id!="cpu.temperature"&&id!="gpu.temperature"&&id!="storage.temperature")continue;
  if(metric.at("ageMs").get<double>()+static_cast<double>(frameAge.count())>3000){result.staleAdmittedSensor=true;continue;}
  const auto value=metric.at("value").get<double>();if(id=="cpu.temperature")result.cpuTemperature=value;else if(id=="gpu.temperature")result.gpuTemperature=value;else result.ssdTemperature=value;
 }
 return result;
}
}
