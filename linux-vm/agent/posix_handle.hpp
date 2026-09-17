#pragma once
// Linux replacement for windows/agent/win_handle.hpp (baseline commit 280ced5).
// Same shape: an RAII OS-handle wrapper plus a throwing "require" helper.
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <string>
#include <stdexcept>
#include <utility>
namespace ioscope {
class Fd{
 int fd_=-1;
public:
 Fd()=default;explicit Fd(int value):fd_(value){}
 ~Fd(){reset();}Fd(const Fd&)=delete;Fd& operator=(const Fd&)=delete;
 Fd(Fd&& other)noexcept:fd_(std::exchange(other.fd_,-1)){}
 Fd& operator=(Fd&& other)noexcept{if(this!=&other){reset();fd_=std::exchange(other.fd_,-1);}return *this;}
 void reset(int value=-1)noexcept{if(fd_>=0)close(fd_);fd_=value;}
 int get()const{return fd_;}explicit operator bool()const{return fd_>=0;}
};
inline void posix_require(bool condition,const char* operation){if(!condition)throw std::runtime_error(std::string(operation)+" ("+std::strerror(errno)+")");}
}
