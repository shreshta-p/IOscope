#pragma once
#include "win_handle.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include <optional>
#include <chrono>
namespace ioscope {
inline std::wstring quote_argument(const std::wstring& value){
 std::wstring result=L"\"";size_t slashes=0;
 for(wchar_t character:value){if(character==L'\\'){++slashes;continue;}if(character==L'"'){result.append(slashes*2+1,L'\\');result+=character;}else{result.append(slashes,L'\\');result+=character;}slashes=0;}
 result.append(slashes*2,L'\\');result+=L'"';return result;
}
struct ChildResult{DWORD exitCode=1;std::string output,errors;bool cancelled=false;std::optional<std::string> aborted;};
inline ChildResult run_child_job(const std::wstring& executable,const std::vector<std::wstring>& arguments,
 const std::atomic<bool>& cancelled,std::chrono::milliseconds timeout,
 const std::function<std::optional<std::string>()>& watchdog,HANDLE gracefulStop=nullptr,HANDLE ownedInput=nullptr){
 Handle job(CreateJobObjectW(nullptr,nullptr));win_require(static_cast<bool>(job),"Create job");
 JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_ACTIVE_PROCESS|JOB_OBJECT_LIMIT_PROCESS_MEMORY;limits.BasicLimitInformation.ActiveProcessLimit=1;limits.ProcessMemoryLimit=256*1024*1024;
 win_require(SetInformationJobObject(job.get(),JobObjectExtendedLimitInformation,&limits,sizeof(limits))!=0,"Set job limits");
 SECURITY_ATTRIBUTES attributes{sizeof(attributes),nullptr,TRUE};HANDLE rawRead=nullptr,rawWrite=nullptr;
 win_require(CreatePipe(&rawRead,&rawWrite,&attributes,256*1024)!=0,"Create stdout pipe");Handle outputRead(rawRead),outputWrite(rawWrite);
 win_require(CreatePipe(&rawRead,&rawWrite,&attributes,64*1024)!=0,"Create stderr pipe");Handle errorRead(rawRead),errorWrite(rawWrite);
 win_require(SetHandleInformation(outputRead.get(),HANDLE_FLAG_INHERIT,0)!=0&&SetHandleInformation(errorRead.get(),HANDLE_FLAG_INHERIT,0)!=0,"Protect parent pipe handles");
 Handle input;
 if(ownedInput){HANDLE duplicate=nullptr;win_require(DuplicateHandle(GetCurrentProcess(),ownedInput,GetCurrentProcess(),&duplicate,0,TRUE,DUPLICATE_SAME_ACCESS)!=0,"Duplicate owned preparation handle");input.reset(duplicate);}
 else input.reset(CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&attributes,OPEN_EXISTING,0,nullptr));win_require(static_cast<bool>(input),"Open helper input");
 SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);std::vector<unsigned char> buffer(bytes);
 auto list=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());win_require(InitializeProcThreadAttributeList(list,1,0,&bytes)!=0,"Initialize handle inheritance");
 struct AttributeCleanup{PPROC_THREAD_ATTRIBUTE_LIST list;~AttributeCleanup(){DeleteProcThreadAttributeList(list);}} cleanup{list};
 HANDLE inherited[]={outputWrite.get(),errorWrite.get(),input.get()};win_require(UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)!=0,"Restrict inherited handles");
 STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;startup.StartupInfo.hStdOutput=outputWrite.get();startup.StartupInfo.hStdError=errorWrite.get();startup.StartupInfo.hStdInput=input.get();startup.lpAttributeList=list;
 std::wstring command=quote_argument(executable);for(const auto& argument:arguments)command+=L" "+quote_argument(argument);
 PROCESS_INFORMATION process{};win_require(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_SUSPENDED|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&process)!=0,"Create suspended helper");
 Handle processHandle(process.hProcess),threadHandle(process.hThread);
 ChildResult result;std::atomic<bool> overflow=false,readerFailed=false,stopReaders=false;
 std::thread stdoutReader,stderrReader;
 // Install before assignment/resume or thread creation: every exception must kill
 // the child before joining any reader that was successfully started.
 struct ChildCleanup{
  Handle& job;HANDLE process;std::atomic<bool>& stop;std::thread& output;std::thread& errors;
  ~ChildCleanup(){
   if(WaitForSingleObject(process,0)!=WAIT_OBJECT_0){TerminateJobObject(job.get(),1);TerminateProcess(process,1);}
   job.reset(); // KILL_ON_JOB_CLOSE also covers a failed explicit termination.
   WaitForSingleObject(process,2000);stop.store(true);
   if(output.joinable())output.join();if(errors.joinable())errors.join();
  }
 } childCleanup{job,processHandle.get(),stopReaders,stdoutReader,stderrReader};
 win_require(AssignProcessToJobObject(job.get(),processHandle.get())!=0,"Helper job assignment");
 win_require(ResumeThread(threadHandle.get())!=static_cast<DWORD>(-1),"Helper resume");
 outputWrite.reset();errorWrite.reset();input.reset();threadHandle.reset();
 auto read=[&](HANDLE handle,std::string& target,size_t maximum)noexcept{
  try{
   char data[8192];
   while(!stopReaders.load()){
    DWORD available=0,count=0;
    if(!PeekNamedPipe(handle,nullptr,0,nullptr,&available,nullptr)){
     if(GetLastError()!=ERROR_BROKEN_PIPE)readerFailed.store(true);break;
    }
    if(!available){Sleep(1);continue;}
    // Only this reader consumes the pipe; reading available bytes cannot wait
    // for a future write. Cleanup can therefore stop readers without hanging.
    const DWORD requested=available<sizeof(data)?available:static_cast<DWORD>(sizeof(data));
    if(!ReadFile(handle,data,requested,&count,nullptr)){
     if(GetLastError()!=ERROR_BROKEN_PIPE)readerFailed.store(true);break;
    }
    const size_t room=maximum-target.size(),keep=count<room?count:room;
    target.append(data,keep);if(keep<count)overflow.store(true);
   }
  }catch(...){readerFailed.store(true);}
 };
 stdoutReader=std::thread(read,outputRead.get(),std::ref(result.output),16*1024*1024);
 stderrReader=std::thread(read,errorRead.get(),std::ref(result.errors),64*1024);
 const auto started=std::chrono::steady_clock::now();auto checked=started;std::optional<std::chrono::steady_clock::time_point> stopping;
 while(true){
  const DWORD waited=WaitForSingleObject(processHandle.get(),50);
  if(waited==WAIT_OBJECT_0)break;
  win_require(waited==WAIT_TIMEOUT,"Wait for helper");
  const auto now=std::chrono::steady_clock::now();
  if(!stopping){
   if(cancelled.load()){result.cancelled=true;stopping=now;}
   else if(overflow.load()){result.aborted="Helper output limit exceeded";stopping=now;}
   else if(readerFailed.load()){result.aborted="Helper output reader failed";stopping=now;}
   else if(now-started>timeout){result.aborted="Helper deadline exceeded";stopping=now;}
   else if(now-checked>=std::chrono::seconds(1)){checked=now;try{result.aborted=watchdog();}catch(...){result.aborted="Safety watchdog failed";}if(result.aborted)stopping=now;}
   if(stopping&&gracefulStop)SetEvent(gracefulStop);
  }
  if(stopping&&(!gracefulStop||now-*stopping>=std::chrono::milliseconds(1500)))win_require(TerminateJobObject(job.get(),1)!=0,"Terminate helper job");
 }
 win_require(GetExitCodeProcess(processHandle.get(),&result.exitCode)!=0,"Read helper exit code");stdoutReader.join();stderrReader.join();
 if(overflow.load())result.aborted="Helper output limit exceeded";
 else if(readerFailed.load())result.aborted="Helper output reader failed";
 return result;
}
}
