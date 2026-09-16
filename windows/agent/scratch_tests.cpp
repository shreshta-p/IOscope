#include "scratch.hpp"
#include <fstream>
#include "process_job.hpp"
#include <process.h>
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--orphan"){ioscope::Scratch scratch(argv[2]);std::atomic<bool> cancel=false;scratch.initialize(4096,cancel,[]{return std::optional<std::string>{};});_exit(99);}

 const auto id=ioscope::random_id();std::filesystem::path path;
 {ioscope::Scratch scratch(id);path=scratch.path();if(!std::filesystem::exists(path)||path.wstring().find(L"OneDrive")!=std::wstring::npos)throw std::runtime_error("Invalid scratch placement");std::atomic<bool> cancelled=false;scratch.initialize(4096,cancelled,[]{return std::optional<std::string>{};});if(std::filesystem::file_size(path)!=4096)throw std::runtime_error("Initialization failed");
  bool duplicate=false;try{ioscope::Scratch second(id);}catch(const std::exception&){duplicate=true;}if(!duplicate)throw std::runtime_error("Existing ownership overwritten");
  {ioscope::Handle helper(CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));if(!helper)throw std::runtime_error("DiskSpd sharing mode cannot open target");}
  if(DeleteFileW(path.c_str()))throw std::runtime_error("Active target allowed deletion");
  if(scratch.cleanup())throw std::runtime_error("Explicit cleanup failed");if(scratch.cleanup())throw std::runtime_error("Cleanup is not idempotent");}
 if(std::filesystem::exists(path)||std::filesystem::exists(path.parent_path()))throw std::runtime_error("Owned files leaked");
 bool traversal=false;try{ioscope::Scratch bad("../outside");}catch(const std::exception&){traversal=true;}if(!traversal)throw std::runtime_error("Traversal accepted");
 {ioscope::Scratch scratch(ioscope::random_id());path=scratch.path();std::atomic<bool> cancel=true;bool stopped=false;try{scratch.initialize(4096,cancel,[]{return std::optional<std::string>{};});}catch(const std::exception&){stopped=true;}if(!stopped||std::filesystem::file_size(path)!=0)throw std::runtime_error("Cancelled preparation wrote data");}
 if(std::filesystem::exists(path))throw std::runtime_error("Cancelled scratch leaked");
 {ioscope::Scratch scratch(ioscope::random_id());wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,32768);const auto preparation=std::filesystem::path(module).parent_path()/"ioscope_prepare.exe";const auto ownId=scratch.path().parent_path().filename().wstring();std::atomic<bool> stop=false;
  const auto prepared=ioscope::run_child_job(preparation.wstring(),{ownId,L"4096"},stop,std::chrono::seconds(3),[]{return std::optional<std::string>{};},nullptr,scratch.preparation_handle());if(prepared.exitCode||std::filesystem::file_size(scratch.path())!=4096)throw std::runtime_error("Isolated preparation failed: "+prepared.errors);
 }
 const auto orphanId=ioscope::random_id();wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);std::atomic<bool> cancel=false;
 const auto child=ioscope::run_child_job(executable,{L"--orphan",std::wstring(orphanId.begin(),orphanId.end())},cancel,std::chrono::seconds(5),[]{return std::optional<std::string>{};});if(child.exitCode!=99)throw std::runtime_error("Orphan fixture did not exit abruptly");
 const auto orphan=ioscope::local_directory()/"scratch"/orphanId;const auto manifestPath=orphan/"ownership.json";nlohmann::json manifest;{std::ifstream input(manifestPath);input>>manifest;}const auto original=manifest;manifest["fileIndexLow"]=manifest["fileIndexLow"].get<unsigned long long>()+1;{std::ofstream output(manifestPath);output<<manifest;}
 bool mismatch=false;try{ioscope::recover_scratch(orphanId);}catch(const std::exception&){mismatch=true;}if(!mismatch||!std::filesystem::exists(orphan/"data.bin")||!std::filesystem::exists(manifestPath))throw std::runtime_error("Recovery deleted mismatched ownership");
 {std::ofstream output(manifestPath);output<<original;}ioscope::recover_scratch(orphanId);if(std::filesystem::exists(orphan))throw std::runtime_error("Verified orphan cleanup failed");
 std::cout<<"PASS: private local scratch, exclusive ownership, cancellation, idempotent handle-based cleanup, traversal rejection\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
