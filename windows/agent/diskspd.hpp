#pragma once
#include "safety.hpp"
#include "win_handle.hpp"
#include <filesystem>
#include <bcrypt.h>
#include <vector>
namespace ioscope {
inline constexpr const char* diskspd_version="2.3";
inline constexpr const char* diskspd_sha256="dd4e57e1e8ccaf5d6437938f8aab7f17e9a1e6d8fba8a093006b7cadf16faea2";
inline std::string hash_bytes(const std::string& bytes){
 BCRYPT_ALG_HANDLE algorithm=nullptr;win_require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"Open artifact hash provider");unsigned char digest[32]{};
 const auto status=BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),static_cast<ULONG>(bytes.size()),digest,sizeof(digest));BCryptCloseAlgorithmProvider(algorithm,0);win_require(status>=0,"Hash artifact");
 const char* digits="0123456789abcdef";std::string result;for(auto byte:digest){result+=digits[byte>>4];result+=digits[byte&15];}return result;
}
inline std::string hash_file(HANDLE file){
 BCRYPT_ALG_HANDLE algorithm=nullptr;win_require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"Open SHA256 provider");
 struct AlgorithmCleanup{BCRYPT_ALG_HANDLE value;~AlgorithmCleanup(){BCryptCloseAlgorithmProvider(value,0);}} algorithmCleanup{algorithm};
 DWORD length=0,received=0;win_require(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&received,0)>=0,"Read hash size");std::vector<unsigned char> object(length);BCRYPT_HASH_HANDLE hash=nullptr;win_require(BCryptCreateHash(algorithm,&hash,object.data(),length,nullptr,0,0)>=0,"Create SHA256 state");
 struct HashCleanup{BCRYPT_HASH_HANDLE value;~HashCleanup(){BCryptDestroyHash(value);}} hashCleanup{hash};
 LARGE_INTEGER zero{};win_require(SetFilePointerEx(file,zero,nullptr,FILE_BEGIN)!=0,"Rewind helper");unsigned char buffer[65536];DWORD count=0;while(true){win_require(ReadFile(file,buffer,sizeof(buffer),&count,nullptr)!=0,"Read helper for hash");if(!count)break;win_require(BCryptHashData(hash,buffer,count,0)>=0,"Hash helper");}
 unsigned char digest[32]{};win_require(BCryptFinishHash(hash,digest,sizeof(digest),0)>=0,"Finish SHA256");const char* digits="0123456789abcdef";std::string result;for(unsigned char byte:digest){result+=digits[byte>>4];result+=digits[byte&15];}return result;
}
inline Handle verify_diskspd(const std::filesystem::path& path){
 Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr));win_require(static_cast<bool>(file),"Open pinned DiskSpd (run dependency setup first)");BY_HANDLE_FILE_INFORMATION information{};win_require(GetFileInformationByHandle(file.get(),&information)!=0&&(information.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))==0,"Require regular helper executable");if(hash_file(file.get())!=diskspd_sha256)throw std::runtime_error("DiskSpd executable hash does not match the pinned release");return file;
}
inline std::vector<std::wstring> diskspd_arguments(const nlohmann::json& workload,const std::filesystem::path& target,const std::wstring& stopEvent){
 const auto number=[&](const char* field){return std::to_wstring(workload.at(field).get<unsigned long long>());};
 const auto rate=offered_rate(workload.at("intensity"));if(!rate)throw std::runtime_error("Unknown intensity");
 return {L"-t1",L"-n",L"-bsn",L"-L",L"-Rxml",L"-Z1M",L"-b"+number("blockBytes"),L"-o"+number("queueDepth"),L"-w"+std::to_wstring(100-workload.at("readPercent").get<unsigned>()),L"-d"+number("durationSeconds"),L"-W"+number("warmupSeconds"),L"-C"+number("cooldownSeconds"),L"-g"+std::to_wstring(rate/1000),workload.at("cacheMode")=="buffered"?L"-Sb":L"-Su",(workload.at("pattern")=="random"?L"-r":L"-s")+number("blockBytes"),L"-yp"+stopEvent,target.wstring()};
}
}
