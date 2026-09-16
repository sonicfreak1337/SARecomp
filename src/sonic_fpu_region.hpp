#pragma once
#include "katana/runtime/fpu.hpp"
#include <bit>
#include <cassert>
#include <cstdint>
#include <emmintrin.h>

#if defined(_MSC_VER)
#define SONIC_REGION_INLINE __forceinline
#else
#define SONIC_REGION_INLINE inline __attribute__((always_inline))
#endif

namespace sonic::fpu_region {
using namespace katana::runtime;
struct Result { std::uint32_t bits=0, causes=0; };

// Caller owns a matching SDK epoch: RN/RTZ, exceptions masked, DAZ/FTZ off.
// Host status may change INSIDE the closed region. The SDK epoch restores it
// at the boundary. This helper must never replace the general runtime API.
template<FpuBinaryOperation Op>
SONIC_REGION_INLINE bool try_binary(std::uint32_t n,std::uint32_t m,Result& out) noexcept {
    const auto an=n&0x7FFFFFFFu,am=m&0x7FFFFFFFu;
    if(an>=0x7F800000u || am>=0x7F800000u ||
       (an!=0 && an<0x00800000u) || (am!=0 && am<0x00800000u)) return false;
    if constexpr(Op==FpuBinaryOperation::Divide) if(am==0) return false;
    const auto x=_mm_set_ss(std::bit_cast<float>(n));
    const auto y=_mm_set_ss(std::bit_cast<float>(m));
    __m128 z;
    if constexpr(Op==FpuBinaryOperation::Add) z=_mm_add_ss(x,y);
    else if constexpr(Op==FpuBinaryOperation::Subtract) z=_mm_sub_ss(x,y);
    else if constexpr(Op==FpuBinaryOperation::Multiply) z=_mm_mul_ss(x,y);
    else {
        static_assert(Op==FpuBinaryOperation::Divide);
        z=_mm_div_ss(x,y);
    }
    const auto bits=std::bit_cast<std::uint32_t>(_mm_cvtss_f32(z));
    const auto magnitude=bits&0x7FFFFFFFu,exponent=magnitude>>23u;
    if(magnitude!=0 && (exponent<2 || exponent>252)) return false;

    const auto dx=_mm_cvtps_pd(x),dy=_mm_cvtps_pd(y),dz=_mm_cvtps_pd(z);
    bool exact;
    if constexpr(Op==FpuBinaryOperation::Multiply) {
        // Two binary32 significands have an exact <=48-bit binary64 product.
        exact=_mm_comieq_sd(_mm_mul_sd(dx,dy),dz)!=0;
    } else if constexpr(Op==FpuBinaryOperation::Divide) {
        // Compare result * divisor with dividend, not a rounded double divide.
        exact=_mm_comieq_sd(_mm_mul_sd(dz,dy),dx)!=0;
    } else {
        const auto en=an>>23u,em=am>>23u;
        const auto gap=en>em?en-em:em-en;
        // At larger exponent gaps the nonzero small term necessarily makes
        // the binary32 sum inexact; it could disappear even from binary64.
        if(an!=0 && am!=0 && gap>24) exact=false;
        else if constexpr(Op==FpuBinaryOperation::Add)
            exact=_mm_comieq_sd(_mm_add_sd(dx,dy),dz)!=0;
        else exact=_mm_comieq_sd(_mm_sub_sd(dx,dy),dz)!=0;
    }
    // Rounded-to-zero underflow belongs to the retained DN/flag behavior.
    if(magnitude==0 && !exact) return false;
    out={bits,exact?0u:fpscr_cause_inexact_mask};
    return true;
}

// Only for authenticated, callback-free FPU runs admitted by the existing AOT
// epoch predicate. Instruction legality/accounting remains the caller's work.
// No memory/service/dispatch calls or FP-mode changes may cross this lifetime.
// General runtime operations inside the region keep their original fallbacks.
class BinaryRegion final {
public:
    BinaryRegion(CpuState& cpu,const HostFpuExecutionEpoch&) noexcept:cpu_(cpu) {
        assert(admitted(cpu));
    }
    static bool admitted(const CpuState& cpu) noexcept {
        return (cpu.fpscr&(fpscr_pr_mask|fpscr_exception_enable_mask|fpscr_dn_mask))==fpscr_dn_mask &&
               (cpu.fpscr&fpscr_rounding_mode_mask)<=1u;
    }
    BinaryRegion(const BinaryRegion&)=delete;
    BinaryRegion& operator=(const BinaryRegion&)=delete;
    template<FpuBinaryOperation Op,unsigned Source,unsigned Destination>
    SONIC_REGION_INLINE bool binary() noexcept {
        static_assert(Source<16 && Destination<16);
        assert(admitted(cpu_));
        Result result;
        if(try_binary<Op>(cpu_.fr[Destination],cpu_.fr[Source],result)) {
            cpu_.fpscr=(cpu_.fpscr&~fpscr_cause_mask)|result.causes|
                       ((result.causes>>10u)&fpscr_flag_mask);
            cpu_.fr[Destination]=result.bits;
            return true;
        }
        fpu_binary(cpu_,Op,Source,Destination);
        return false;
    }
private:
    CpuState& cpu_;
};
} // namespace sonic::fpu_region
#undef SONIC_REGION_INLINE
