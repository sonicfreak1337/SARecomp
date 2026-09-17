#include "sonic_palette_batch.hpp"
#include "sonic_native_cpu_policy.hpp"
#include "katana/runtime/fpu.hpp"
#include <cstdlib>
#include <cstring>
#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#elif defined(__x86_64__)
#include <cpuid.h>
#endif

namespace sonic::palette_batch {
namespace detail {
void calculate_avx2(const std::uint8_t*,std::size_t,const std::uint32_t*,
    std::uint32_t,std::uint32_t*,std::uint32_t*) noexcept;
}
namespace {
bool available() noexcept {
    static const bool value=[] {
#if defined(_MSC_VER) && defined(_M_X64)
        int v[4]{}; __cpuid(v,0); if(v[0]<7)return false;
        __cpuidex(v,1,0);
        constexpr unsigned mask=(1u<<12)|(1u<<27)|(1u<<28);
        if((unsigned(v[2])&mask)!=mask || (_xgetbv(0)&6u)!=6u)return false;
        __cpuidex(v,7,0);return (unsigned(v[1])&(1u<<5))!=0;
#elif defined(__x86_64__)
        if(__get_cpuid_max(0,nullptr)<7)return false;
        unsigned a=0,b=0,c=0,d=0;__cpuid(1,a,b,c,d);
        constexpr unsigned mask=(1u<<12)|(1u<<27)|(1u<<28);
        if((c&mask)!=mask)return false;
        unsigned lo=0,hi=0;__asm__ volatile("xgetbv":"=a"(lo),"=d"(hi):"c"(0));
        if((lo&6u)!=6u)return false;
        __cpuid_count(7,0,a,b,c,d);return (b&(1u<<5))!=0;
#else
        return false;
#endif
    }();return value;
}
}
bool enabled() noexcept {
    static const bool value=native_cpu::enabled("SARECOMP_NATIVE_PALETTE_BATCH");
    return value;
}
bool prepare(const katana::runtime::CpuState& cpu,const std::uint8_t* normals,
    std::size_t count,const std::uint32_t* light,std::uint32_t scale,Result& out) {
    using namespace katana::runtime;
    const auto fpscr=cpu.read_fpscr();
    const auto magnitude=scale&0x7fffffffu;
    if(!available() || !normals || !light || count<2 || count>65536 ||
       (fpscr&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) ||
       !(fpscr&fpscr_dn_mask) || !(fpscr&fpscr_flag_inexact_mask) ||
       (fpscr&fpscr_rounding_mode_mask)>1 ||
       magnitude<0x3f800000u || magnitude>0x43800000u || light[3]!=0)return false;
    for(unsigned i=0;i<3;++i)if((light[i]&0x7fffffffu)>0x41800000u)return false;
    for(std::size_t i=0;i<count*3;++i){
        std::uint32_t bits;std::memcpy(&bits,normals+i*4,4);
        if((bits&0x7fffffffu)>0x41800000u)return false;
    }
    // |dot| <= 768, 1 <= |scale| <= 256. DN-flushed dot operands make
    // multiply normal or zero; cancellation around |scale| >= 1 cannot
    // underflow. FMUL/FADD can set only Inexact, which is already sticky.
    // FIPR and FTRC clear Cause without setting Flags in the retained runtime.
    out.scaled.resize(count);out.integers.resize(count);
    const HostFpuExecutionEpoch epoch(cpu);
    detail::calculate_avx2(normals,count,light,scale,out.scaled.data(),out.integers.data());
    return true;
}
}
