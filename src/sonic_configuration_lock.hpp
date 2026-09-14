#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include "linux/sonic_file_lock.hpp"
#endif
#include <cwctype>
#include <filesystem>
#include <stdexcept>
namespace sonic::presentation {
#ifdef _WIN32
class ConfigurationLock {
    HANDLE mutex_=nullptr;
public:
    explicit ConfigurationLock(const std::filesystem::path& path,DWORD timeout=2000){
        std::uint64_t hash=14695981039346656037ull;
        for(auto c:std::filesystem::absolute(path).lexically_normal().wstring()){hash^=std::uint64_t(std::towlower(c));hash*=1099511628211ull;}
        const auto name=L"Local\\SARecomp-configuration-"+std::to_wstring(hash);
        mutex_=CreateMutexW(nullptr,FALSE,name.c_str());if(!mutex_)throw std::runtime_error("configuration-lock-create");
        const auto waited=WaitForSingleObject(mutex_,timeout);
        if(waited!=WAIT_OBJECT_0&&waited!=WAIT_ABANDONED){CloseHandle(mutex_);mutex_=nullptr;throw std::runtime_error("configuration-in-use");}
    }
    ~ConfigurationLock(){if(mutex_){ReleaseMutex(mutex_);CloseHandle(mutex_);}}
    ConfigurationLock(const ConfigurationLock&)=delete;
    ConfigurationLock& operator=(const ConfigurationLock&)=delete;
};
#else
using ConfigurationLock=linux_host::FileLock;
#endif
}
