#pragma once
// Linux replacement for windows/agent/process_job.hpp (baseline commit 280ced5).
// Same contract: launch one owned child, bound its output, and guarantee it cannot
// outlive this function even under an exception. Windows uses a kill-on-close Job
// Object; Linux uses a delegated cgroup v2 scope with cgroup.kill (atomic, immune to
// PID-reuse/re-parenting races), falling back to process-group signals if the cgroup
// tree isn't delegated to this user (see ../docs/ADAPTER-BOUNDARIES.md).
#include "posix_handle.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include <optional>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <cerrno>
namespace ioscope {
struct ChildResult{int exitCode=1;std::string output,errors;bool cancelled=false;std::optional<std::string> aborted;};
inline std::optional<std::filesystem::path> cgroup_scope_base(){
 const auto uid=getuid();
 for(const auto& suffix:{"/app.slice",""}){
  std::filesystem::path candidate("/sys/fs/cgroup/user.slice/user-"+std::to_string(uid)+".slice/user@"+std::to_string(uid)+".service"+suffix);
  std::error_code error;if(std::filesystem::exists(candidate,error)&&access(candidate.c_str(),W_OK)==0)return candidate;
 }
 return std::nullopt;
}
class CgroupScope{
 std::filesystem::path path_;bool created_=false;
public:
 explicit CgroupScope(const std::string& scopeId){
  if(auto base=cgroup_scope_base()){
   auto candidate=*base/("ioscope-"+scopeId);
   if(mkdir(candidate.c_str(),0755)==0){path_=candidate;created_=true;}
  }
 }
 ~CgroupScope(){remove();}
 CgroupScope(const CgroupScope&)=delete;CgroupScope& operator=(const CgroupScope&)=delete;
 bool active()const{return created_;}
 // Called from the parent right after fork(), never from the child between fork()
 // and exec(): std::ofstream allocates and may take locks, which is unsafe in a
 // forked child of a multithreaded process before the child has called exec.
 bool add(pid_t pid)const{if(!created_)return false;std::ofstream procs(path_/"cgroup.procs");if(!procs)return false;procs<<pid;return static_cast<bool>(procs);}
 void kill()const{if(!created_)return;std::ofstream file(path_/"cgroup.kill");if(file)file<<"1";}
 void remove(){
  if(!created_)return;
  for(int attempt=0;attempt<20;++attempt){if(rmdir(path_.c_str())==0){created_=false;return;}std::this_thread::sleep_for(std::chrono::milliseconds(50));}
 }
};
inline std::vector<char*> to_argv(const std::string& executable,const std::vector<std::string>& arguments){
 std::vector<char*> argv;argv.push_back(const_cast<char*>(executable.c_str()));
 for(const auto& arg:arguments)argv.push_back(const_cast<char*>(arg.c_str()));
 argv.push_back(nullptr);return argv;
}
inline void drain_pipe(int fd,std::string& target,size_t maximum,std::atomic<bool>& overflow,std::atomic<bool>& failed)noexcept{
 try{
  char data[8192];
  while(true){
   const ssize_t count=::read(fd,data,sizeof(data));
   if(count<0){if(errno==EINTR)continue;failed.store(true);break;}
   if(count==0)break;
   const size_t room=maximum-target.size(),keep=static_cast<size_t>(count)<room?static_cast<size_t>(count):room;
   target.append(data,keep);if(keep<static_cast<size_t>(count))overflow.store(true);
  }
 }catch(...){failed.store(true);}
}
// gracefulStop: SIGTERM is sent first and the child is given up to 1500ms to exit
// before a hard SIGKILL/cgroup.kill escalation, mirroring the Windows 1500ms grace
// window before TerminateJobObject. scopeId names the cgroup (falls back to plain
// process-group signaling if cgroup delegation isn't available for this user).
inline ChildResult run_child_process(const std::string& scopeId,const std::string& executable,const std::vector<std::string>& arguments,
 const std::atomic<bool>& cancelled,std::chrono::milliseconds timeout,
 const std::function<std::optional<std::string>()>& watchdog,int ownedInputFd=-1){
 CgroupScope scope(scopeId);
 int outPipe[2],errPipe[2];posix_require(pipe(outPipe)==0,"Create stdout pipe");posix_require(pipe(errPipe)==0,"Create stderr pipe");
 // Build argv before fork(): allocating between fork() and exec() in the child is
 // unsafe in a multithreaded process (see CgroupScope::add's comment for the same
 // reason applied to iostream use).
 auto argv=to_argv(executable,arguments);
 const pid_t pid=fork();posix_require(pid>=0,"Fork helper");
 if(pid==0){
  setpgid(0,0);
  dup2(outPipe[1],1);dup2(errPipe[1],2);
  close(outPipe[0]);close(outPipe[1]);close(errPipe[0]);close(errPipe[1]);
  if(ownedInputFd>=0)dup2(ownedInputFd,0);else{const int null=open("/dev/null",O_RDONLY);if(null>=0)dup2(null,0);}
  execv(executable.c_str(),argv.data());
  _exit(127);
 }
 if(scope.active())scope.add(pid);
 close(outPipe[1]);close(errPipe[1]);if(ownedInputFd>=0)close(ownedInputFd);
 Fd outputRead(outPipe[0]),errorRead(errPipe[0]);
 ChildResult result;std::atomic<bool> overflow=false,readerFailed=false;
 std::thread stdoutReader(drain_pipe,outputRead.get(),std::ref(result.output),16*1024*1024,std::ref(overflow),std::ref(readerFailed));
 std::thread stderrReader(drain_pipe,errorRead.get(),std::ref(result.errors),64*1024,std::ref(overflow),std::ref(readerFailed));
 const auto started=std::chrono::steady_clock::now();auto checked=started;std::optional<std::chrono::steady_clock::time_point> stopping;bool termSent=false;
 int status=0;
 while(true){
  const pid_t waited=waitpid(pid,&status,WNOHANG);
  if(waited==pid)break;
  posix_require(waited==0,"Wait for helper");
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const auto now=std::chrono::steady_clock::now();
  if(!stopping){
   if(cancelled.load()){result.cancelled=true;stopping=now;}
   else if(overflow.load()){result.aborted="Helper output limit exceeded";stopping=now;}
   else if(readerFailed.load()){result.aborted="Helper output reader failed";stopping=now;}
   else if(now-started>timeout){result.aborted="Helper deadline exceeded";stopping=now;}
   else if(now-checked>=std::chrono::seconds(1)){checked=now;try{result.aborted=watchdog();}catch(...){result.aborted="Safety watchdog failed";}if(result.aborted)stopping=now;}
  }
  if(stopping&&!termSent){kill(-pid,SIGTERM);termSent=true;}
  if(stopping&&now-*stopping>=std::chrono::milliseconds(1500)){if(scope.active())scope.kill();else kill(-pid,SIGKILL);}
 }
 stdoutReader.join();stderrReader.join();
 result.exitCode=WIFEXITED(status)?WEXITSTATUS(status):(WIFSIGNALED(status)?128+WTERMSIG(status):1);
 if(overflow.load())result.aborted="Helper output limit exceeded";
 else if(readerFailed.load())result.aborted="Helper output reader failed";
 return result;
}
}
