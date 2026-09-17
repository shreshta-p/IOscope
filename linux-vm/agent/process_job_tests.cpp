#include "process_job.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <unistd.h>
#include <climits>
int main(int argc,char** argv){try{
 if(argc>1&&std::string(argv[1])=="--echo"){auto values=nlohmann::json::array();for(int i=2;i<argc;i++)values.push_back(std::string(argv[i]));std::cout<<values.dump();return 0;}
 if(argc>1&&std::string(argv[1])=="--sleep"){struct timespec ts{15,0};nanosleep(&ts,nullptr);return 0;}
 if(argc>1&&std::string(argv[1])=="--flood"){std::string data(8192,'x');for(int i=0;i<2300;i++)std::cout<<data;return 0;}
 if(argc>1&&std::string(argv[1])=="--stderr-flood"){std::string data(8192,'e');for(int i=0;i<64;i++)std::cerr<<data;return 0;}
 if(argc>1&&std::string(argv[1])=="--crash"){_exit(73);}
 char self[PATH_MAX+1]{};const auto length=readlink("/proc/self/exe",self,PATH_MAX);if(length<=0)throw std::runtime_error("Test executable unavailable");
 const std::string path(self,static_cast<size_t>(length));std::atomic<bool> cancel=false;const auto safe=[]()->std::optional<std::string>{return std::nullopt;};
 auto result=ioscope::run_child_process("test-echo",path,{"--echo","","with spaces","/ending/","quoted\"argument"},cancel,std::chrono::seconds(5),safe);
 if(result.exitCode||nlohmann::json::parse(result.output)!=nlohmann::json::array({"","with spaces","/ending/","quoted\"argument"}))throw std::runtime_error("Argument roundtrip failed");
 cancel=true;const auto started=std::chrono::steady_clock::now();result=ioscope::run_child_process("test-cancel",path,{"--sleep"},cancel,std::chrono::seconds(5),safe);if(!result.cancelled||std::chrono::steady_clock::now()-started>std::chrono::seconds(3))throw std::runtime_error("Cancellation deadline failed");
 cancel=false;result=ioscope::run_child_process("test-timeout",path,{"--sleep"},cancel,std::chrono::milliseconds(100),safe);if(!result.aborted)throw std::runtime_error("Timeout not enforced");
 result=ioscope::run_child_process("test-flood",path,{"--flood"},cancel,std::chrono::seconds(5),safe);if(result.aborted!=std::optional<std::string>("Helper output limit exceeded")||result.output.size()!=16*1024*1024)throw std::runtime_error("Output cap not enforced");
 result=ioscope::run_child_process("test-stderr",path,{"--stderr-flood"},cancel,std::chrono::seconds(5),safe);if(result.aborted!=std::optional<std::string>("Helper output limit exceeded")||result.errors.size()!=64*1024||!result.output.empty())throw std::runtime_error("Stderr cap not enforced");
 result=ioscope::run_child_process("test-crash",path,{"--crash"},cancel,std::chrono::seconds(5),safe);if(result.exitCode!=73||result.cancelled||result.aborted)throw std::runtime_error("Child crash exit status lost");
 const auto checkWatchdog=[&](const std::function<std::optional<std::string>()>& watchdog,const std::string& expected){
  const auto began=std::chrono::steady_clock::now();
  auto stopped=ioscope::run_child_process("test-watchdog",path,{"--sleep"},cancel,std::chrono::seconds(5),watchdog);
  if(stopped.aborted!=std::optional<std::string>(expected)||stopped.cancelled||!stopped.exitCode||std::chrono::steady_clock::now()-began>std::chrono::seconds(3))throw std::runtime_error("Watchdog termination failed");
 };
 checkWatchdog([]()->std::optional<std::string>{return "Fake safety abort";},"Fake safety abort");
 checkWatchdog([]()->std::optional<std::string>{throw std::runtime_error("Fake watchdog error");},"Safety watchdog failed");
 checkWatchdog([]()->std::optional<std::string>{throw 42;},"Safety watchdog failed");
 std::cout<<"PASS: exact argument passing, cancellation, deadline, bounded stdout/stderr, crash exit, watchdog aborts and exceptions\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
