#pragma once
// Shared /proc and /sys readers used by both telemetry.hpp and system_resources.hpp.
#include <fstream>
#include <sstream>
#include <string>
#include <optional>
namespace ioscope {
inline std::optional<double> meminfo_field(const std::string& key){
 std::ifstream file("/proc/meminfo");std::string line;
 while(std::getline(file,line)){if(line.rfind(key,0)==0){std::istringstream stream(line.substr(key.size()));double kib=0;if(stream>>kib)return kib*1024.0;}}
 return std::nullopt;
}
}
