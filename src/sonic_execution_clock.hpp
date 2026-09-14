#pragma once
#include <cstdint>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <cerrno>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#endif
namespace sonic::performance {
struct ExecutionClock {
    std::uint32_t thread_id=0;
    std::uint64_t thread_cpu_100ns=0,process_cpu_100ns=0,thread_cycles=0;
    bool thread_valid=false,process_valid=false,cycles_valid=false;
};
// Diagnostic gameplay samples only, not per instruction or ordinary frame.
// CPU time excludes waiting. Cycle counts are never converted to wall time.
inline ExecutionClock execution_clock() noexcept {
    ExecutionClock result;
#if defined(_WIN32)
    const auto saved_error=GetLastError();result.thread_id=GetCurrentThreadId();
    FILETIME created{},exited{},kernel{},user{};
    const auto count=[](FILETIME value){return (std::uint64_t(value.dwHighDateTime)<<32)|value.dwLowDateTime;};
    if(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)){
        result.thread_valid=true;result.thread_cpu_100ns=count(kernel)+count(user);
    }
    if(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)){
        result.process_valid=true;result.process_cpu_100ns=count(kernel)+count(user);
    }
    ULONG64 cycles=0;
    if(QueryThreadCycleTime(GetCurrentThread(),&cycles)){
        result.cycles_valid=true;result.thread_cycles=cycles;
    }
    SetLastError(saved_error);
#elif defined(__linux__)
    const int saved_errno=errno;
    const auto tid=syscall(SYS_gettid);
    if(tid>0)result.thread_id=static_cast<std::uint32_t>(tid);
    const auto count=[](const timespec& value){return std::uint64_t(value.tv_sec)*10'000'000+std::uint64_t(value.tv_nsec)/100;};
    timespec value{};
    if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)==0){
        result.thread_valid=true;result.thread_cpu_100ns=count(value);
    }
    if(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&value)==0){
        result.process_valid=true;result.process_cpu_100ns=count(value);
    }
    // No synthetic TSC-derived cycle count: VM scheduling and CPU frequency
    // are separate from the actual CPU-time clocks above.
    errno=saved_errno;
#endif
    return result;
}
}
