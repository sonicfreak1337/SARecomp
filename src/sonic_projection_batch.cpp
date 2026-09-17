#include "sonic_projection_batch.hpp"
#include <algorithm>
#include <cstring>
#include <xmmintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace sonic::projection_batch {
namespace detail {
bool calculate(const std::uint8_t*,std::size_t,const std::uint32_t*,
    const std::uint32_t*,Result&) noexcept;
}
namespace {
bool available() noexcept {
    static const bool value=[] {
#if defined(_MSC_VER)
        int v[4]{};__cpuid(v,0);if(v[0]<7)return false;
        __cpuidex(v,1,0);
        constexpr unsigned mask=(1u<<12)|(1u<<27)|(1u<<28);
        if((unsigned(v[2])&mask)!=mask || (_xgetbv(0)&6u)!=6u)return false;
        __cpuidex(v,7,0);return (unsigned(v[1])&(1u<<5))!=0;
#else
        if(__get_cpuid_max(0,nullptr)<7)return false;
        unsigned a,b,c,d;__cpuid(1,a,b,c,d);
        constexpr unsigned mask=(1u<<12)|(1u<<27)|(1u<<28);
        if((c&mask)!=mask)return false;
        unsigned lo,hi;__asm__ volatile("xgetbv":"=a"(lo),"=d"(hi):"c"(0));
        if((lo&6u)!=6u)return false;
        __cpuid_count(7,0,a,b,c,d);return (b&(1u<<5))!=0;
#endif
    }();return value;
}
bool bounded(std::uint32_t bits) noexcept {
    const auto magnitude=bits&0x7fffffffu;
    return magnitude==0 || (magnitude>=0x35800000u && magnitude<=0x49800000u);
}
struct PreserveHostStatus {
    unsigned incoming=_mm_getcsr();
    ~PreserveHostStatus(){_mm_setcsr(incoming);}
};
}
bool prepare(const katana::runtime::CpuState& cpu,std::uint32_t count,
    std::span<const std::uint8_t> points,Result& out,
    const katana::runtime::HostFpuExecutionEpoch&) {
    using namespace katana::runtime;
    const auto fpscr=cpu.read_fpscr();
    const auto even=(std::uint64_t(count)+1u)&~std::uint64_t(1u);
    if(!available() || !count || count>65536 || points.size()!=(even+1u)*12u ||
       cpu.fpu_disabled() || cpu.trap_pending || cpu.sleeping ||
       (fpscr&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) ||
       !(fpscr&fpscr_dn_mask) || !(fpscr&fpscr_flag_inexact_mask) ||
       (fpscr&fpscr_rounding_mode_mask)>1u)return false;
    for(unsigned f=4;f<8;++f)if(!bounded(cpu.fr[f]))return false;
    if((cpu.fr[14]&0x7fffffffu)>=0x7f800000u)return false;
    for(auto bits:cpu.xf)if((bits&0x7fffffffu)>=0x7f800000u)return false;
    out.transformed.resize(even+1u);
    out.positions.resize(even);out.clipped.resize(count);out.clip_count=0;
    // The arithmetic below can only set sticky Inexact, already set above:
    // admitted coordinates/scales lie in [2^-20,2^20] or are signed zero;
    // |z| is nonzero in that interval, hence its reciprocal is too. Products
    // are normal/zero within 2^60 and cancellation cannot reach underflow.
    // FTRV itself clears Cause without setting Flags in the retained owner.
    // Restore host status even on rejection before running the retained loop.
    const PreserveHostStatus host;
    return detail::calculate(points.data(),even,cpu.xf.data(),cpu.fr.data(),out);
}
void publish(katana::runtime::CpuState& cpu,const Result& out,
    std::span<std::uint8_t> output,std::span<std::uint8_t> clips) noexcept {
    for(std::size_t i=0;i<out.positions.size();++i)
        std::memcpy(output.data()+i*16u,out.positions[i].data(),12u);
    std::copy(out.clipped.begin(),out.clipped.end(),clips.begin());
    std::copy(out.read_ahead.begin(),out.read_ahead.end(),cpu.fr.begin());
    std::copy(out.last_second.begin(),out.last_second.end(),cpu.fr.begin()+8);
    cpu.r[13]+=out.clip_count;cpu.t=out.last_compare;
    // Preserve the final instruction's Cause bits with the existing helper;
    // all earlier arithmetic can only set the already-sticky Inexact flag.
    katana::runtime::fpu_binary(cpu,katana::runtime::FpuBinaryOperation::Add,4u,8u);
}
}
