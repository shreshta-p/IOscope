#pragma once
// Linux replacement for windows/agent/platform.hpp (baseline commit 280ced5).
// Same three functions (random_id, utc_now, local_directory), same contract:
// local_directory() returns a validated, agent-owned, non-symlinked directory
// with no local-volume/UNC concept on Linux, so that check is dropped.
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <filesystem>
#include <string>
#include <stdexcept>
namespace ioscope {
inline std::string random_id(){unsigned char bytes[24]{};if(getrandom(bytes,sizeof(bytes),0)!=static_cast<ssize_t>(sizeof(bytes)))throw std::runtime_error("Random generator unavailable");const char* hex="0123456789abcdef";std::string out;for(auto b:bytes){out+=hex[b>>4];out+=hex[b&15];}return out;}
inline std::string utc_now(){struct timespec ts{};clock_gettime(CLOCK_REALTIME,&ts);struct tm parts{};gmtime_r(&ts.tv_sec,&parts);char out[32];std::snprintf(out,sizeof(out),"%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",parts.tm_year+1900,parts.tm_mon+1,parts.tm_mday,parts.tm_hour,parts.tm_min,parts.tm_sec,ts.tv_nsec/1000000);return out;}
// Reject a missing or symlinked ancestor at every level, mirroring the reparse-point
// ancestor check in the Windows platform.hpp/security.hpp. A symlinked ancestor could
// redirect agent-owned scratch outside the validated directory after the check passes.
inline void reject_symlink_ancestors(const std::filesystem::path& path){
 if(!path.is_absolute())throw std::runtime_error("Require local absolute path");
 for(auto current=path;!current.empty();){struct stat info{};if(lstat(current.c_str(),&info)!=0)throw std::runtime_error("Missing ancestor: "+current.string());if(S_ISLNK(info.st_mode))throw std::runtime_error("Reject symlinked ancestor: "+current.string());const auto parent=current.parent_path();if(parent==current)break;current=parent;}
}
inline std::filesystem::path local_directory(){
 const char* xdg=std::getenv("XDG_DATA_HOME");
 std::filesystem::path base;
 if(xdg&&*xdg)base=xdg;
 else{const char* home=std::getenv("HOME");if(!home||!*home)throw std::runtime_error("HOME is not set; cannot resolve local data directory");base=std::filesystem::path(home)/".local"/"share";}
 if(!base.is_absolute())throw std::runtime_error("XDG_DATA_HOME/HOME must be an absolute path");
 std::filesystem::create_directories(base);
 reject_symlink_ancestors(base);
 const auto directory=base/"ioscope";
 if(!std::filesystem::exists(directory)){if(mkdir(directory.c_str(),0700)!=0&&errno!=EEXIST)throw std::runtime_error("Cannot create local data directory");}
 reject_symlink_ancestors(directory);
 struct stat info{};if(lstat(directory.c_str(),&info)!=0||!S_ISDIR(info.st_mode))throw std::runtime_error("Local data path is not a directory");
 if(info.st_uid!=getuid())throw std::runtime_error("Local data directory is not owned by the current user");
 return directory;
}
}
