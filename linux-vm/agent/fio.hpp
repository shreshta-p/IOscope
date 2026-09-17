#pragma once
// Linux replacement for windows/agent/diskspd.hpp (baseline commit 280ced5).
// fio, not DiskSpd, is the pinned Linux workload dependency (L2 of the port plan).
// Windows pins an exact DiskSpd release by SHA-256 of a downloaded binary; fio here
// is installed from the distribution package manager and pinned by its reported
// version string instead (apt itself signs and verifies the package). This is a
// different trust model, not a weaker one — record it, don't claim they're equivalent.
//
// Option mapping is explicit, not a claim of DiskSpd-equivalent numeric behavior
// (see ../docs/PORT-PLAN.md L2: "Do not claim numerical equivalence with DiskSpd
// based only on matching options"). Notable divergence: fio has no built-in
// unmeasured "cooldown" tail the way DiskSpd's -C does; cooldownSeconds here only
// widens the process deadline, it does not add trailing unmeasured I/O.
#include "safety.hpp"
#include "posix_handle.hpp"
#include "process_job.hpp"
#include "sha256.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
namespace ioscope {
inline constexpr const char* fio_pinned_version="fio-3.41";
inline std::string run_capture(const std::string& executable,const std::vector<std::string>& arguments){
 int pipefd[2];posix_require(pipe(pipefd)==0,"Create version-check pipe");
 const pid_t pid=fork();posix_require(pid>=0,"Fork version check");
 if(pid==0){dup2(pipefd[1],1);close(pipefd[0]);close(pipefd[1]);const int null=open("/dev/null",O_WRONLY);if(null>=0)dup2(null,2);auto argv=to_argv(executable,arguments);execv(executable.c_str(),argv.data());_exit(127);}
 close(pipefd[1]);std::string output;char buffer[4096];ssize_t count;
 while((count=read(pipefd[0],buffer,sizeof(buffer)))>0)output.append(buffer,static_cast<size_t>(count));
 close(pipefd[0]);int status=0;waitpid(pid,&status,0);
 return output;
}
inline void verify_fio(const std::filesystem::path& executable){
 posix_require(std::filesystem::is_regular_file(executable),"fio executable not found (install the pinned fio package first)");
 const auto output=run_capture(executable.string(),{"--version"});
 if(output.find(fio_pinned_version)==std::string::npos)throw std::runtime_error("fio version does not match the pinned "+std::string(fio_pinned_version));
}
inline std::vector<std::string> fio_arguments(const nlohmann::json& workload,const std::filesystem::path& target){
 const auto blockBytes=workload.at("blockBytes").get<unsigned long long>();
 const auto queueDepth=workload.at("queueDepth").get<unsigned long long>();
 const auto readPercent=workload.at("readPercent").get<unsigned long long>();
 const auto pattern=workload.at("pattern").get<std::string>();
 const auto duration=workload.at("durationSeconds").get<unsigned long long>();
 const auto warmup=workload.at("warmupSeconds").get<unsigned long long>();
 const auto cacheMode=workload.at("cacheMode").get<std::string>();
 const auto workingSet=workload.at("workingSetBytes").get<unsigned long long>();
 const auto rate=offered_rate(workload.at("intensity"));if(!rate)throw std::runtime_error("Unknown intensity");
 const bool random=pattern=="random";
 std::string rw;
 if(readPercent==100)rw=random?"randread":"read";
 else if(readPercent==0)rw=random?"randwrite":"write";
 else rw=random?"randrw":"rw";
 std::vector<std::string> args={
  "--name=ioscope-run",
  "--filename="+target.string(),
  "--allow_file_create=0",
  "--rw="+rw,
  "--bs="+std::to_string(blockBytes),
  "--iodepth="+std::to_string(queueDepth),
  "--ioengine=io_uring",
  "--direct="+std::string(cacheMode=="unbuffered"?"1":"0"),
  "--size="+std::to_string(workingSet),
  "--runtime="+std::to_string(duration),
  "--ramp_time="+std::to_string(warmup),
  "--time_based",
  "--rate="+std::to_string(rate),
  "--group_reporting",
  "--output-format=json",
 };
 if(readPercent!=0&&readPercent!=100)args.push_back("--rwmixread="+std::to_string(readPercent));
 return args;
}
}
