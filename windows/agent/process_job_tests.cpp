#include "process_job.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
int wmain(int argc,wchar_t** argv){try{
 if(argc>1&&std::wstring(argv[1])==L"--echo"){auto values=nlohmann::json::array();for(int i=2;i<argc;i++)values.push_back(ioscope::utf8(argv[i]));std::cout<<values.dump();return 0;}
 if(argc>1&&std::wstring(argv[1])==L"--sleep"){Sleep(15000);return 0;}
 if(argc>1&&std::wstring(argv[1])==L"--flood"){std::string data(8192,'x');for(int i=0;i<2300;i++)std::cout<<data;return 0;}
 if(argc>1&&std::wstring(argv[1])==L"--stderr-flood"){std::string data(8192,'e');for(int i=0;i<64;i++)std::cerr<<data;return 0;}
 if(argc>1&&std::wstring(argv[1])==L"--crash"){TerminateProcess(GetCurrentProcess(),73);return 74;}
 wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);std::atomic<bool> cancel=false;const auto safe=[]()->std::optional<std::string>{return std::nullopt;};
 auto result=ioscope::run_child_job(path,{L"--echo",L"",L"with spaces",L"C:\\ending\\",L"quoted\"argument"},cancel,std::chrono::seconds(5),safe);
 if(result.exitCode||nlohmann::json::parse(result.output)!=nlohmann::json::array({"","with spaces","C:\\ending\\","quoted\"argument"}))throw std::runtime_error("Argument roundtrip failed");
 cancel=true;const auto started=std::chrono::steady_clock::now();result=ioscope::run_child_job(path,{L"--sleep"},cancel,std::chrono::seconds(5),safe);if(!result.cancelled||std::chrono::steady_clock::now()-started>std::chrono::seconds(2))throw std::runtime_error("Cancellation deadline failed");
 cancel=false;result=ioscope::run_child_job(path,{L"--sleep"},cancel,std::chrono::milliseconds(100),safe);if(!result.aborted)throw std::runtime_error("Timeout not enforced");
 result=ioscope::run_child_job(path,{L"--flood"},cancel,std::chrono::seconds(5),safe);if(result.aborted!=std::optional<std::string>("Helper output limit exceeded")||result.output.size()!=16*1024*1024)throw std::runtime_error("Output cap not enforced");
 result=ioscope::run_child_job(path,{L"--stderr-flood"},cancel,std::chrono::seconds(5),safe);if(result.aborted!=std::optional<std::string>("Helper output limit exceeded")||result.errors.size()!=64*1024||!result.output.empty())throw std::runtime_error("Stderr cap not enforced");
 result=ioscope::run_child_job(path,{L"--crash"},cancel,std::chrono::seconds(5),safe);if(result.exitCode!=73||result.cancelled||result.aborted)throw std::runtime_error("Child crash exit status lost");
 const auto checkWatchdog=[&](const std::function<std::optional<std::string>()>& watchdog,const std::string& expected){
  const auto began=std::chrono::steady_clock::now();
  auto stopped=ioscope::run_child_job(path,{L"--sleep"},cancel,std::chrono::seconds(5),watchdog);
  if(stopped.aborted!=std::optional<std::string>(expected)||stopped.cancelled||!stopped.exitCode||std::chrono::steady_clock::now()-began>std::chrono::seconds(3))throw std::runtime_error("Watchdog termination failed");
 };
 checkWatchdog([]()->std::optional<std::string>{return "Fake safety abort";},"Fake safety abort");
 checkWatchdog([]()->std::optional<std::string>{throw std::runtime_error("Fake watchdog error");},"Safety watchdog failed");
 checkWatchdog([]()->std::optional<std::string>{throw 42;},"Safety watchdog failed");
 ioscope::Handle stopEvent(CreateEventW(nullptr,TRUE,FALSE,nullptr));ioscope::win_require(static_cast<bool>(stopEvent),"Create fake graceful-stop event");
 cancel=true;const auto gracefulBegan=std::chrono::steady_clock::now();
 result=ioscope::run_child_job(path,{L"--sleep"},cancel,std::chrono::seconds(5),safe,stopEvent.get());
 const auto gracefulElapsed=std::chrono::steady_clock::now()-gracefulBegan;
 if(!result.cancelled||WaitForSingleObject(stopEvent.get(),0)!=WAIT_OBJECT_0||gracefulElapsed<std::chrono::milliseconds(1500)||gracefulElapsed>std::chrono::seconds(2))throw std::runtime_error("Graceful-stop fallback deadline failed");
 std::cout<<"PASS: exact argument quoting, cancellation, graceful fallback, deadline, bounded stdout/stderr, crash exit, watchdog aborts and exceptions\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
