#pragma once
#include <windows.h>
#include <utility>
#include <stdexcept>
#include <string>
namespace ioscope {
class Handle{
 HANDLE handle_=nullptr;
public:
 Handle()=default;explicit Handle(HANDLE value):handle_(value){}
 ~Handle(){reset();}Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
 Handle(Handle&& other)noexcept:handle_(std::exchange(other.handle_,nullptr)){}
 Handle& operator=(Handle&& other)noexcept{if(this!=&other){reset();handle_=std::exchange(other.handle_,nullptr);}return *this;}
 void reset(HANDLE value=nullptr)noexcept{if(handle_&&handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);handle_=value;}
 HANDLE get()const{return handle_;}explicit operator bool()const{return handle_&&handle_!=INVALID_HANDLE_VALUE;}
};
inline void win_require(bool condition,const char* operation){if(!condition)throw std::runtime_error(std::string(operation)+" (Windows error "+std::to_string(GetLastError())+")");}
inline std::string utf8(const std::wstring& value){if(value.empty())return {};const int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);win_require(count>0,"Encode UTF-8");std::string output(count,'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),output.data(),count,nullptr,nullptr);return output;}
}
