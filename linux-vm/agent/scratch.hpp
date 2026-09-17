#pragma once
// Linux replacement for windows/agent/scratch.hpp (baseline commit 280ced5).
// Same contract: an exclusively agent-owned scratch file per run, symlink-ancestor
// rejection, identity verification before cleanup, and orphan recovery gated on a
// signed ownership manifest. POSIX permissions/fstat replace ACLs/BY_HANDLE_FILE_INFORMATION.
//
// Linux-specific addition (not present in the Windows version, see
// ../docs/ADAPTER-BOUNDARIES.md): reject scratch on tmpfs/overlay/network/vboxsf
// filesystems before ever opening a target file there, so a disk-I/O trial can never
// silently become a RAM-backed or shared-folder copy.
#include "platform.hpp"
#include "posix_handle.hpp"
#include "json_input.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>
#include <functional>
namespace ioscope {
inline std::string mount_fstype(const std::filesystem::path& path){
 const auto target=std::filesystem::weakly_canonical(path).string();
 std::ifstream mounts("/proc/mounts");std::string line,bestMount,bestType;
 while(std::getline(mounts,line)){
  std::istringstream stream(line);std::string device,mountPoint,fstype;
  if(!(stream>>device>>mountPoint>>fstype))continue;
  if(target.rfind(mountPoint,0)==0&&mountPoint.size()>bestMount.size()){bestMount=mountPoint;bestType=fstype;}
 }
 return bestType;
}
inline void require_durable_filesystem(const std::filesystem::path& path){
 static const std::vector<std::string> disallowed={"tmpfs","overlay","overlayfs","nfs","nfs4","cifs","smb3","9p","vboxsf","fuse.vboxsf","ramfs"};
 const auto fstype=mount_fstype(path);
 for(const auto& bad:disallowed)if(fstype==bad)throw std::runtime_error("Scratch filesystem '"+fstype+"' is not a durable local disk (RAM-backed or shared-folder); refusing to admit a disk workload there");
}
// Create: today's exclusive per-run target (O_CREAT|O_EXCL). Reuse: opens a
// dataset a prior phase created and deliberately left behind (Scratch::keep()),
// verifying its identity against that phase's own ownership manifest rather
// than writing a new one -- for docs/07-EXPERIMENT-SPEC.md's first/repeated
// access experiment, where two phases must read the exact same on-disk bytes,
// not two independently-prepared-but-similar files.
enum class ScratchMode{Create,Reuse};
class Scratch {
 std::filesystem::path directory_,target_;Fd directoryFd_,targetFd_,manifestFd_;bool cleaned_=false,identityKnown_=false;struct stat identity_{};
public:
 explicit Scratch(const std::string& datasetId,ScratchMode mode=ScratchMode::Create){try{
  posix_require(datasetId.size()==48&&datasetId.find_first_not_of("0123456789abcdef")==std::string::npos,"Invalid scratch ownership ID");
  const auto parent=local_directory();reject_symlink_ancestors(parent);
  const auto root=parent/"scratch";
  if(mkdir(root.c_str(),0700)!=0)posix_require(errno==EEXIST,"Create scratch root");
  reject_symlink_ancestors(root);require_durable_filesystem(root);
  directory_=root/datasetId;target_=directory_/"data.bin";
  if(mode==ScratchMode::Create){
   posix_require(mkdir(directory_.c_str(),0700)==0,"Create unique run directory");
   directoryFd_.reset(open(directory_.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW));posix_require(static_cast<bool>(directoryFd_),"Hold owned directory");
   targetFd_.reset(open(target_.c_str(),O_CREAT|O_EXCL|O_RDWR|O_NOFOLLOW,0600));posix_require(static_cast<bool>(targetFd_),"Exclusively create target");
   struct stat info{};posix_require(fstat(targetFd_.get(),&info)==0&&S_ISREG(info.st_mode)&&info.st_nlink==1,"Require regular owned file with one link");
   identity_=info;identityKnown_=true;
   const auto manifest=nlohmann::json({{"schemaVersion","1.0.0"},{"runId",datasetId},{"device",static_cast<std::uint64_t>(info.st_dev)},{"inode",static_cast<std::uint64_t>(info.st_ino)},{"createdAt",utc_now()}}).dump();
   const auto manifestPath=directory_/"ownership.json";
   manifestFd_.reset(open(manifestPath.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW,0600));posix_require(static_cast<bool>(manifestFd_),"Create ownership manifest");
   const auto written=write(manifestFd_.get(),manifest.data(),manifest.size());
   posix_require(written==static_cast<ssize_t>(manifest.size())&&fsync(manifestFd_.get())==0,"Persist ownership manifest");
  }else{
   reject_symlink_ancestors(directory_);
   directoryFd_.reset(open(directory_.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW));posix_require(static_cast<bool>(directoryFd_),"Open existing dataset directory");
   targetFd_.reset(open(target_.c_str(),O_RDWR|O_NOFOLLOW));posix_require(static_cast<bool>(targetFd_),"Open existing dataset target");
   struct stat info{};posix_require(fstat(targetFd_.get(),&info)==0&&S_ISREG(info.st_mode)&&info.st_nlink==1,"Require regular owned file with one link");
   std::ifstream manifestStream(directory_/"ownership.json");std::ostringstream buffer;buffer<<manifestStream.rdbuf();
   const auto owner=parse_input(buffer.str());
   posix_require(owner.at("schemaVersion")=="1.0.0"&&owner.at("device").get<std::uint64_t>()==static_cast<std::uint64_t>(info.st_dev)&&owner.at("inode").get<std::uint64_t>()==static_cast<std::uint64_t>(info.st_ino),"Reused dataset identity does not match ownership manifest");
   identity_=info;identityKnown_=true;
  }
 }catch(...){cleanup();throw;}}
 ~Scratch(){auto error=cleanup();if(error)std::cerr<<"Scratch cleanup requires attention: "<<*error<<"\n";}
 Scratch(const Scratch&)=delete;Scratch& operator=(const Scratch&)=delete;
 const std::filesystem::path& path()const{return target_;}
 int preparation_fd()const{return targetFd_.get();}
 std::uintmax_t size()const{return identity_.st_size;}
 // Deliberately skip real cleanup: a later phase (docs/07-EXPERIMENT-SPEC.md's
 // first/repeated access experiment) still owns and will clean up this dataset.
 void keep(){cleaned_=true;}
 void initialize(std::uint64_t size,const std::atomic<bool>& cancel,const std::function<std::optional<std::string>()>& watchdog){
  initialize_fd(targetFd_.get(),size,cancel,watchdog);
 }
 static void initialize_fd(int target,std::uint64_t size,const std::atomic<bool>& cancel,const std::function<std::optional<std::string>()>& watchdog){
  posix_require(size>0&&size<=4ULL*1024*1024*1024,"Preparation size exceeds hard limit");
  std::vector<unsigned char> data(1024*1024);std::uint64_t random=0x42d14783;const auto start=std::chrono::steady_clock::now();auto checked=start;
  for(std::uint64_t offset=0;offset<size;){
   if(cancel.load())throw std::runtime_error("Preparation cancelled");
   const auto now=std::chrono::steady_clock::now();if(now-checked>=std::chrono::seconds(1)){checked=now;auto reason=watchdog();if(reason)throw std::runtime_error(*reason);}
   for(auto& value:data){random^=random<<13;random^=random>>7;random^=random<<17;value=static_cast<unsigned char>(random);}
   const auto count=std::min<std::uint64_t>(data.size(),size-offset);
   ssize_t written=0;while(written<static_cast<ssize_t>(count)){const auto result=write(target,data.data()+written,count-static_cast<size_t>(written));posix_require(result>0,"Initialize owned target");written+=result;}
   offset+=count;
   while(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<static_cast<double>(offset)/(32*1024*1024)){if(cancel.load())throw std::runtime_error("Preparation cancelled");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
  }
  // Initialization deliberately may populate the page cache; no cold-cache claim.
  posix_require(fsync(target)==0,"Flush initialized target");
 }
 std::optional<std::string> cleanup()noexcept{if(cleaned_)return std::nullopt;try{
  if(targetFd_||identityKnown_){posix_require(identityKnown_,"Target identity unknown; preserve scratch for inspection");targetFd_.reset();
   Fd deletion(open(target_.c_str(),O_RDONLY|O_NOFOLLOW));posix_require(static_cast<bool>(deletion),"Reopen owned target for cleanup");
   struct stat current{};posix_require(fstat(deletion.get(),&current)==0&&current.st_dev==identity_.st_dev&&current.st_ino==identity_.st_ino,"Target identity changed; preserve replacement");
   deletion.reset();posix_require(unlink(target_.c_str())==0,"Delete owned target");identityKnown_=false;
  }
  manifestFd_.reset();if(!directory_.empty())posix_require(unlink((directory_/"ownership.json").c_str())==0||errno==ENOENT,"Delete ownership manifest");
  directoryFd_.reset();if(!directory_.empty())posix_require(rmdir(directory_.c_str())==0,"Remove owned run directory");
  cleaned_=true;return std::nullopt;
 }catch(const std::exception& error){return error.what();}}
};
// Called only while the agent's exclusive application lock is held. Recovery accepts
// an ownership ID, never a caller-selected filesystem path.
inline void recover_scratch(const std::string& runId){
 posix_require(runId.size()==48&&runId.find_first_not_of("0123456789abcdef")==std::string::npos,"Invalid recovery ownership ID");
 const auto directory=local_directory()/"scratch"/runId;reject_symlink_ancestors(directory);
 for(const auto& entry:std::filesystem::directory_iterator(directory)){const auto name=entry.path().filename();posix_require(name=="data.bin"||name=="ownership.json","Unknown orphan contents; preserve for inspection");}
 std::ifstream manifestStream(directory/"ownership.json",std::ios::binary);posix_require(static_cast<bool>(manifestStream),"Open orphan manifest");
 std::ostringstream buffer;buffer<<manifestStream.rdbuf();const auto content=buffer.str();
 posix_require(content.size()>0&&content.size()<=4096,"Bound orphan manifest");
 const auto owner=parse_input(content);posix_require(owner.at("schemaVersion")=="1.0.0"&&owner.at("runId")==runId,"Orphan ownership manifest mismatch");
 Fd target(open((directory/"data.bin").c_str(),O_RDONLY|O_NOFOLLOW));posix_require(static_cast<bool>(target),"Open orphan target");
 struct stat identity{};posix_require(fstat(target.get(),&identity)==0&&owner.at("device").get<std::uint64_t>()==static_cast<std::uint64_t>(identity.st_dev)&&owner.at("inode").get<std::uint64_t>()==static_cast<std::uint64_t>(identity.st_ino),"Orphan file identity mismatch; preserve replacement");
 target.reset();
 posix_require(unlink((directory/"data.bin").c_str())==0,"Delete verified orphan");
 posix_require(unlink((directory/"ownership.json").c_str())==0,"Delete orphan manifest");
 posix_require(rmdir(directory.c_str())==0,"Remove orphan directory");
}
}
