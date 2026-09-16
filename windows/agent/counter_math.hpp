#pragma once
#include <optional>
#include <cstdint>
namespace ioscope {
struct CpuTimes{std::uint64_t idle,kernel,user;};
inline std::optional<double> cpu_percent(CpuTimes now,CpuTimes previous){
 if(now.idle<previous.idle||now.kernel<previous.kernel||now.user<previous.user)return std::nullopt;
 const double total=static_cast<double>(now.kernel-previous.kernel)+static_cast<double>(now.user-previous.user);
 const double idle=static_cast<double>(now.idle-previous.idle);
 if(total<=0||idle>total)return std::nullopt;
 return 100*(1-idle/total);
}
}
