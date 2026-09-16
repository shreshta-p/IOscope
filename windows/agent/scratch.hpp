#pragma once
#include "platform.hpp"
#include "win_handle.hpp"
#include "json_input.hpp"
#include <sddl.h>
#include <aclapi.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <thread>
#include <iostream>
#include <vector>
#include <functional>
#include <optional>
namespace ioscope {
class PrivateSecurity {
 PSECURITY_DESCRIPTOR descriptor_=nullptr;
 std::vector<unsigned char> userSid_;
public:
 SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),nullptr,FALSE};
 PrivateSecurity(){Handle token;HANDLE raw=nullptr;win_require(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&raw)!=0,"Read current identity");token.reset(raw);DWORD bytes=0;GetTokenInformation(token.get(),TokenUser,nullptr,0,&bytes);std::vector<unsigned char> buffer(bytes);win_require(GetTokenInformation(token.get(),TokenUser,buffer.data(),bytes,&bytes)!=0,"Read user SID");const auto sourceSid=reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid;userSid_.resize(GetLengthSid(sourceSid));win_require(CopySid(static_cast<DWORD>(userSid_.size()),userSid_.data(),sourceSid)!=0,"Copy user identity");LPWSTR sid=nullptr;win_require(ConvertSidToStringSidW(sourceSid,&sid)!=0,"Format user SID");std::wstring text=L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;"+std::wstring(sid)+L")";LocalFree(sid);win_require(ConvertStringSecurityDescriptorToSecurityDescriptorW(text.c_str(),SDDL_REVISION_1,&descriptor_,nullptr)!=0,"Create private ACL");attributes.lpSecurityDescriptor=descriptor_;}
 ~PrivateSecurity(){if(descriptor_)LocalFree(descriptor_);}PrivateSecurity(const PrivateSecurity&)=delete;
 void protect(HANDLE directory){PSID owner=nullptr;PSECURITY_DESCRIPTOR existing=nullptr;win_require(GetSecurityInfo(directory,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION,&owner,nullptr,nullptr,nullptr,&existing)==ERROR_SUCCESS,"Read scratch owner");const bool sameUser=EqualSid(owner,userSid_.data())!=0;LocalFree(existing);win_require(sameUser,"Scratch owner differs from current user");BOOL present=FALSE,defaulted=FALSE;PACL acl=nullptr;win_require(GetSecurityDescriptorDacl(descriptor_,&present,&acl,&defaulted)!=0&&present,"Read private ACL");win_require(SetSecurityInfo(directory,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,nullptr,nullptr,acl,nullptr)==ERROR_SUCCESS,"Protect scratch ACL");}
};
inline void reject_reparse_ancestors(const std::filesystem::path& path){
 win_require(path.is_absolute()&&path.native().rfind(L"\\\\",0)!=0,"Require local absolute path");
 for(auto current=path;!current.empty();){const auto attributes=GetFileAttributesW(current.c_str());win_require(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT)==0,"Reject missing or reparse ancestor");const auto parent=current.parent_path();if(parent==current)break;current=parent;}
}
inline void verify_target(HANDLE handle,const std::filesystem::path& expected){
 BY_HANDLE_FILE_INFORMATION info{};win_require(GetFileInformationByHandle(handle,&info)!=0&&info.nNumberOfLinks==1&&(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))==0,"Require regular owned file with one link");
 wchar_t path[32768]{};const auto count=GetFinalPathNameByHandleW(handle,path,32768,FILE_NAME_NORMALIZED);win_require(count>0&&count<32768,"Resolve opened target");std::wstring actual(path);if(actual.rfind(L"\\\\?\\",0)==0)actual=actual.substr(4);if(_wcsicmp(actual.c_str(),expected.c_str())!=0)throw std::runtime_error("Opened file location mismatch: actual="+utf8(actual)+" expected="+utf8(expected.wstring()));
}
class Scratch {
 std::filesystem::path directory_,target_;Handle directoryHandle_,targetHandle_,manifestHandle_;bool cleaned_=false,identityKnown_=false;BY_HANDLE_FILE_INFORMATION identity_{};
 static void erase_handle(Handle& handle){if(!handle)return;FILE_DISPOSITION_INFO disposition{TRUE};win_require(SetFileInformationByHandle(handle.get(),FileDispositionInfo,&disposition,sizeof(disposition))!=0,"Delete owned file by handle");handle.reset();}
public:
 explicit Scratch(const std::string& runId){try{
  win_require(runId.size()==48&&runId.find_first_not_of("0123456789abcdef")==std::string::npos,"Invalid scratch ownership ID");PrivateSecurity security;
  const auto parent=local_directory();reject_reparse_ancestors(parent);const auto root=parent/L"scratch";
  if(!CreateDirectoryW(root.c_str(),&security.attributes))win_require(GetLastError()==ERROR_ALREADY_EXISTS,"Create scratch root");reject_reparse_ancestors(root);
  Handle rootHandle(CreateFileW(root.c_str(),READ_CONTROL|WRITE_DAC|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));win_require(static_cast<bool>(rootHandle),"Open private scratch root");security.protect(rootHandle.get());
  directory_=root/std::wstring(runId.begin(),runId.end());win_require(CreateDirectoryW(directory_.c_str(),&security.attributes)!=0,"Create unique run directory");
  directoryHandle_.reset(CreateFileW(directory_.c_str(),DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));win_require(static_cast<bool>(directoryHandle_),"Hold owned directory");
  // DiskSpd shares read/write but not delete. Hold no DELETE access while the
  // helper runs; this handle still denies replacement or deletion of its target.
  target_=directory_/L"data.bin";targetHandle_.reset(CreateFileW(target_.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security.attributes,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));win_require(static_cast<bool>(targetHandle_),"Exclusively create target");verify_target(targetHandle_.get(),target_);
  BY_HANDLE_FILE_INFORMATION info{};win_require(GetFileInformationByHandle(targetHandle_.get(),&info)!=0,"Read owned file identity");
  identity_=info;identityKnown_=true;
  const auto manifest=nlohmann::json({{"schemaVersion","1.0.0"},{"runId",runId},{"volumeSerial",info.dwVolumeSerialNumber},{"fileIndexHigh",info.nFileIndexHigh},{"fileIndexLow",info.nFileIndexLow},{"createdAt",utc_now()}}).dump();
  const auto manifestPath=directory_/L"ownership.json";manifestHandle_.reset(CreateFileW(manifestPath.c_str(),GENERIC_WRITE|DELETE,FILE_SHARE_READ,&security.attributes,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));win_require(static_cast<bool>(manifestHandle_),"Create ownership manifest");verify_target(manifestHandle_.get(),manifestPath);DWORD written=0;win_require(WriteFile(manifestHandle_.get(),manifest.data(),static_cast<DWORD>(manifest.size()),&written,nullptr)!=0&&written==manifest.size()&&FlushFileBuffers(manifestHandle_.get())!=0,"Persist ownership manifest");
 }catch(...){cleanup();throw;}}
 ~Scratch(){auto error=cleanup();if(error)std::cerr<<"Scratch cleanup requires attention: "<<*error<<"\n";}
 Scratch(const Scratch&)=delete;Scratch& operator=(const Scratch&)=delete;
 const std::filesystem::path& path()const{return target_;}
 HANDLE preparation_handle()const{return targetHandle_.get();}
 void initialize(std::uint64_t size,const std::atomic<bool>& cancel,const std::function<std::optional<std::string>()>& watchdog){
  initialize_handle(targetHandle_.get(),size,cancel,watchdog);
 }
 // Synchronous writes/flush run only in the isolated preparation process in
 // production. The parent remains free to enforce cancellation and watchdogs.
 static void initialize_handle(HANDLE target,std::uint64_t size,const std::atomic<bool>& cancel,const std::function<std::optional<std::string>()>& watchdog){
  win_require(size>0&&size<=4ULL*1024*1024*1024,"Preparation size exceeds hard limit");
  std::vector<unsigned char> data(1024*1024);std::uint64_t random=0x42d14783;const auto start=std::chrono::steady_clock::now();auto checked=start;
  for(std::uint64_t offset=0;offset<size;){if(cancel.load())throw std::runtime_error("Preparation cancelled");const auto now=std::chrono::steady_clock::now();if(now-checked>=std::chrono::seconds(1)){checked=now;auto reason=watchdog();if(reason)throw std::runtime_error(*reason);}
   for(auto& value:data){random^=random<<13;random^=random>>7;random^=random<<17;value=static_cast<unsigned char>(random);}
   const auto count=static_cast<DWORD>(std::min<std::uint64_t>(data.size(),size-offset));DWORD written=0;win_require(WriteFile(target,data.data(),count,&written,nullptr)!=0&&written==count,"Initialize owned target");offset+=written;
   while(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<static_cast<double>(offset)/(32*1024*1024)){if(cancel.load())throw std::runtime_error("Preparation cancelled");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
  }
  // Initialization deliberately may populate the page cache; no cold-cache claim.
  win_require(FlushFileBuffers(target)!=0,"Flush initialized target");
 }
 std::optional<std::string> cleanup()noexcept{if(cleaned_)return std::nullopt;try{
  if(targetHandle_||identityKnown_){win_require(identityKnown_,"Target identity unknown; preserve scratch for inspection");targetHandle_.reset();
   Handle deletion(CreateFileW(target_.c_str(),DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));win_require(static_cast<bool>(deletion),"Reopen owned target for cleanup");verify_target(deletion.get(),target_);BY_HANDLE_FILE_INFORMATION current{};win_require(GetFileInformationByHandle(deletion.get(),&current)!=0&&current.dwVolumeSerialNumber==identity_.dwVolumeSerialNumber&&current.nFileIndexHigh==identity_.nFileIndexHigh&&current.nFileIndexLow==identity_.nFileIndexLow,"Target identity changed; preserve replacement");erase_handle(deletion);identityKnown_=false;
  }
  erase_handle(manifestHandle_);erase_handle(directoryHandle_);cleaned_=true;return std::nullopt;
 }catch(const std::exception& error){return error.what();}}
};
// Called only while the agent's exclusive application lock is held. Recovery
// accepts an ownership ID, never a caller-selected filesystem path.
inline void recover_scratch(const std::string& runId){
 win_require(runId.size()==48&&runId.find_first_not_of("0123456789abcdef")==std::string::npos,"Invalid recovery ownership ID");
 const auto directory=local_directory()/"scratch"/runId;reject_reparse_ancestors(directory);PrivateSecurity security;
 Handle dir(CreateFileW(directory.c_str(),DELETE|READ_CONTROL|WRITE_DAC|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));win_require(static_cast<bool>(dir),"Lock orphan directory");security.protect(dir.get());
 for(const auto& entry:std::filesystem::directory_iterator(directory)){const auto name=entry.path().filename();win_require(name==L"data.bin"||name==L"ownership.json","Unknown orphan contents; preserve for inspection");}
 Handle manifest(CreateFileW((directory/"ownership.json").c_str(),GENERIC_READ|DELETE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));win_require(static_cast<bool>(manifest),"Open orphan manifest");verify_target(manifest.get(),directory/"ownership.json");
 LARGE_INTEGER size{};win_require(GetFileSizeEx(manifest.get(),&size)!=0&&size.QuadPart>0&&size.QuadPart<=4096,"Bound orphan manifest");std::string content(static_cast<size_t>(size.QuadPart),'\0');DWORD read=0;win_require(ReadFile(manifest.get(),content.data(),static_cast<DWORD>(content.size()),&read,nullptr)!=0&&read==content.size(),"Read orphan manifest");const auto owner=parse_input(content);win_require(owner.at("schemaVersion")=="1.0.0"&&owner.at("runId")==runId,"Orphan ownership manifest mismatch");
 Handle target(CreateFileW((directory/"data.bin").c_str(),DELETE|FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));win_require(static_cast<bool>(target),"Open orphan target");verify_target(target.get(),directory/"data.bin");BY_HANDLE_FILE_INFORMATION identity{};
 win_require(GetFileInformationByHandle(target.get(),&identity)!=0&&owner.at("volumeSerial")==identity.dwVolumeSerialNumber&&owner.at("fileIndexHigh")==identity.nFileIndexHigh&&owner.at("fileIndexLow")==identity.nFileIndexLow,"Orphan file identity mismatch; preserve replacement");
 const auto erase=[](Handle& handle){FILE_DISPOSITION_INFO disposition{TRUE};win_require(SetFileInformationByHandle(handle.get(),FileDispositionInfo,&disposition,sizeof(disposition))!=0,"Delete verified orphan by handle");handle.reset();};
 erase(target);erase(manifest);erase(dir);
}
}
