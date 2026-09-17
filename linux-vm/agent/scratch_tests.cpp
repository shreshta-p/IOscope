#include "scratch.hpp"
#include "process_job.hpp"
#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <climits>
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--orphan"){ioscope::Scratch scratch(argv[2]);std::atomic<bool> cancel=false;scratch.initialize(4096,cancel,[]{return std::optional<std::string>{};});_exit(99);}

 const auto id=ioscope::random_id();std::filesystem::path path;
 {ioscope::Scratch scratch(id);path=scratch.path();if(!std::filesystem::exists(path))throw std::runtime_error("Invalid scratch placement");std::atomic<bool> cancelled=false;scratch.initialize(4096,cancelled,[]{return std::optional<std::string>{};});if(std::filesystem::file_size(path)!=4096)throw std::runtime_error("Initialization failed");
  bool duplicate=false;try{ioscope::Scratch second(id);}catch(const std::exception&){duplicate=true;}if(!duplicate)throw std::runtime_error("Existing ownership overwritten");
  {ioscope::Fd helper(open(path.c_str(),O_RDWR));if(!helper)throw std::runtime_error("fio-equivalent sharing mode cannot open target");}
  if(scratch.cleanup())throw std::runtime_error("Explicit cleanup failed");if(scratch.cleanup())throw std::runtime_error("Cleanup is not idempotent");}
 if(std::filesystem::exists(path)||std::filesystem::exists(path.parent_path()))throw std::runtime_error("Owned files leaked");
 bool traversal=false;try{ioscope::Scratch bad("../outside");}catch(const std::exception&){traversal=true;}if(!traversal)throw std::runtime_error("Traversal accepted");
 {ioscope::Scratch scratch(ioscope::random_id());path=scratch.path();std::atomic<bool> cancel=true;bool stopped=false;try{scratch.initialize(4096,cancel,[]{return std::optional<std::string>{};});}catch(const std::exception&){stopped=true;}if(!stopped||std::filesystem::file_size(path)!=0)throw std::runtime_error("Cancelled preparation wrote data");}
 if(std::filesystem::exists(path))throw std::runtime_error("Cancelled scratch leaked");
 char self[PATH_MAX+1]{};const auto length=readlink("/proc/self/exe",self,PATH_MAX);if(length<=0)throw std::runtime_error("Test executable unavailable");
 const std::string executable(self,static_cast<size_t>(length));
 {ioscope::Scratch scratch(ioscope::random_id());const auto preparation=std::filesystem::path(executable).parent_path()/"ioscope_prepare";const auto ownId=scratch.path().parent_path().filename().string();std::atomic<bool> stop=false;
  const auto prepared=ioscope::run_child_process("scratch-test-prepare",preparation.string(),{ownId,"4096"},stop,std::chrono::seconds(3),[]{return std::optional<std::string>{};},scratch.preparation_fd());if(prepared.exitCode||std::filesystem::file_size(scratch.path())!=4096)throw std::runtime_error("Isolated preparation failed: "+prepared.errors);
 }
 // First/repeated access (docs/07-EXPERIMENT-SPEC.md): one phase prepares and
 // keeps a dataset, a later phase reuses the exact same on-disk bytes.
 const auto sharedId=ioscope::random_id();std::filesystem::path sharedPath;
 {ioscope::Scratch first(sharedId);sharedPath=first.path();std::atomic<bool> cancelled=false;first.initialize(8192,cancelled,[]{return std::optional<std::string>{};});first.keep();}
 if(!std::filesystem::exists(sharedPath))throw std::runtime_error("Kept dataset was deleted");
 {ioscope::Scratch reused(sharedId,ioscope::ScratchMode::Reuse);if(reused.path()!=sharedPath||reused.size()!=8192)throw std::runtime_error("Reused dataset identity or size mismatch");
  if(reused.cleanup())throw std::runtime_error("Reused dataset cleanup failed");}
 if(std::filesystem::exists(sharedPath)||std::filesystem::exists(sharedPath.parent_path()))throw std::runtime_error("Reused dataset not cleaned up");
 bool missing=false;try{ioscope::Scratch(ioscope::random_id(),ioscope::ScratchMode::Reuse);}catch(const std::exception&){missing=true;}if(!missing)throw std::runtime_error("Reuse of a nonexistent dataset accepted");
 const auto tamperedId=ioscope::random_id();
 {ioscope::Scratch tampered(tamperedId);std::atomic<bool> cancelled=false;tampered.initialize(4096,cancelled,[]{return std::optional<std::string>{};});tampered.keep();}
 const auto tamperedManifestPath=ioscope::local_directory()/"scratch"/tamperedId/"ownership.json";nlohmann::json tamperedManifest;{std::ifstream input(tamperedManifestPath);input>>tamperedManifest;}const auto untamperedManifest=tamperedManifest;
 tamperedManifest["inode"]=tamperedManifest["inode"].get<unsigned long long>()+1;{std::ofstream output(tamperedManifestPath);output<<tamperedManifest;}
 bool tamperRejected=false;try{ioscope::Scratch(tamperedId,ioscope::ScratchMode::Reuse);}catch(const std::exception&){tamperRejected=true;}if(!tamperRejected)throw std::runtime_error("Reuse accepted a dataset with a tampered ownership manifest");
 {std::ofstream output(tamperedManifestPath);output<<untamperedManifest;}ioscope::recover_scratch(tamperedId);

 const auto orphanId=ioscope::random_id();std::atomic<bool> cancel=false;
 const auto child=ioscope::run_child_process("scratch-test-orphan",executable,{"--orphan",orphanId},cancel,std::chrono::seconds(5),[]{return std::optional<std::string>{};});if(child.exitCode!=99)throw std::runtime_error("Orphan fixture did not exit abruptly: "+child.errors);
 const auto orphan=ioscope::local_directory()/"scratch"/orphanId;const auto manifestPath=orphan/"ownership.json";nlohmann::json manifest;{std::ifstream input(manifestPath);input>>manifest;}const auto original=manifest;manifest["inode"]=manifest["inode"].get<unsigned long long>()+1;{std::ofstream output(manifestPath);output<<manifest;}
 bool mismatch=false;try{ioscope::recover_scratch(orphanId);}catch(const std::exception&){mismatch=true;}if(!mismatch||!std::filesystem::exists(orphan/"data.bin")||!std::filesystem::exists(manifestPath))throw std::runtime_error("Recovery deleted mismatched ownership");
 {std::ofstream output(manifestPath);output<<original;}ioscope::recover_scratch(orphanId);if(std::filesystem::exists(orphan))throw std::runtime_error("Verified orphan cleanup failed");
 std::cout<<"PASS: private local scratch, exclusive ownership, cancellation, idempotent identity-based cleanup, traversal rejection, dataset reuse and tamper rejection\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
