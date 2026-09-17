#pragma once
// Linux replacement for windows/agent/system_resources.hpp (baseline commit 280ced5).
// Same Resources shape and same temperature-evidence scan over a TelemetryFrame;
// only the raw disk/RAM reads change (statvfs/meminfo instead of Win32 calls).
#include "safety.hpp"
#include "platform.hpp"
#include "posix_handle.hpp"
#include "linux_proc.hpp"
#include <sys/statvfs.h>
namespace ioscope {
inline Resources system_resources(const nlohmann::json& frame,std::chrono::milliseconds frameAge){
 Resources result;struct statvfs info{};posix_require(statvfs(local_directory().c_str(),&info)==0,"Read scratch volume space");
 result.diskTotal=static_cast<std::uint64_t>(info.f_frsize)*info.f_blocks;
 result.diskFree=static_cast<std::uint64_t>(info.f_frsize)*info.f_bavail;
 const auto total=meminfo_field("MemTotal:"),available=meminfo_field("MemAvailable:");
 posix_require(total.has_value()&&available.has_value(),"Read available memory");
 result.ramTotal=static_cast<std::uint64_t>(*total);result.ramAvailable=static_cast<std::uint64_t>(*available);
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
