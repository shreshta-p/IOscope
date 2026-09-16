#pragma once
#include <windows.h>
#include <shlobj.h>
#include <bcrypt.h>
#include <filesystem>
#include <array>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace ioscope {
inline std::string random_id(){std::array<unsigned char,24> bytes{};if(BCryptGenRandom(nullptr,bytes.data(),static_cast<ULONG>(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("Random generator unavailable");const char* hex="0123456789abcdef";std::string out;for(auto b:bytes){out+=hex[b>>4];out+=hex[b&15];}return out;}
inline std::string utc_now(){SYSTEMTIME t;GetSystemTime(&t);char out[32];sprintf_s(out,"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);return out;}
inline std::filesystem::path local_directory(){PWSTR raw=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&raw)))throw std::runtime_error("Local app data unavailable");std::filesystem::path directory(raw);CoTaskMemFree(raw);directory/="IOscope";std::filesystem::create_directories(directory);
 // MSIX hosts may redirect LocalAppData without a filesystem reparse point.
 // Resolve the actual directory first; all subsequent ownership checks use it.
 for(auto ancestor=directory;!ancestor.empty();){const DWORD attributes=GetFileAttributesW(ancestor.c_str());if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Application data has a missing or reparse ancestor");const auto parent=ancestor.parent_path();if(parent==ancestor)break;ancestor=parent;}
 HANDLE handle=CreateFileW(directory.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(handle==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot resolve application data directory");wchar_t resolved[32768]{};const DWORD count=GetFinalPathNameByHandleW(handle,resolved,32768,FILE_NAME_NORMALIZED);CloseHandle(handle);if(!count||count>=32768)throw std::runtime_error("Application data path resolution failed");std::wstring finalPath(resolved);if(finalPath.rfind(L"\\\\?\\",0)==0)finalPath=finalPath.substr(4);if(finalPath.rfind(L"\\\\",0)==0||finalPath.rfind(L"UNC\\",0)==0)throw std::runtime_error("Application data must use a local volume");return std::filesystem::path(finalPath);
}
}
