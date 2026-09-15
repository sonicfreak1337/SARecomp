// Isolated arithmetic comparison, not a frame-rate predictor. Also establishes
// that the hardware branch itself accepts and exactly matches normal inputs.
#include "sonic_fpu_body.hpp"
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <xmmintrin.h>

namespace kernel {
using namespace katana::runtime;
using SingleBinaryResult = sonic::fpu_body::detail::SingleBinaryResult;
// Same host-control expression as the authenticated fpu.cpp helper.
unsigned requested_host_control(unsigned current, std::uint8_t rounding) noexcept {
    return (current & ~(0x6000u | 0x40u | 0x8000u | 0x3Fu | 0x1F80u)) |
        (rounding == 1u ? 0x6000u : 0u) | 0x1F80u;
}
#define SONIC_FPU_RUNTIME_INLINE inline __attribute__((always_inline))
#include "sonic_hardware_fpu.inc"
#undef SONIC_FPU_RUNTIME_INLINE

using Op = FpuBinaryOperation;
bool integer(std::uint32_t n, std::uint32_t m, Op op, std::uint8_t rm, SingleBinaryResult& result) {
    using namespace sonic::fpu_body::detail;
    switch(op) {
    case Op::Add: return try_normal_single_sum(n,m,false,rm,result);
    case Op::Subtract: return try_normal_single_sum(n,m,true,rm,result);
    case Op::Multiply: return try_normal_single_product(n,m,rm,result);
    case Op::Divide: return try_normal_single_quotient(n,m,rm,result);
    }
    return false;
}
struct Input {std::uint32_t n,m;};
std::array<Input,1024> inputs;
using Fn = bool(*)(std::uint32_t,std::uint32_t,Op,std::uint8_t,SingleBinaryResult&);
struct Result {double milliseconds; std::uint64_t fingerprint;};
Result measure(Fn function, Op op, unsigned round) {
    // A volatile function pointer prevents folding or hoisting a repeated call.
    Fn volatile call=function;
    const auto begin=std::chrono::steady_clock::now();
    std::uint64_t fingerprint=0;
    for(unsigned i=0;i<262144;++i) {
        const auto& in=inputs[i&1023u];
        SingleBinaryResult out;
        if(!call(in.n,in.m,op,round&1u,out)) throw std::runtime_error("unexpected normal-input rejection");
        fingerprint+=out.bits; fingerprint^=out.causes;
    }
    return {std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count(),fingerprint};
}
}
int main() {
    using namespace kernel;
    const unsigned saved=_mm_getcsr();
    struct Restore {unsigned value;~Restore(){_mm_setcsr(value);}} restore{saved};
    try {
        std::uint32_t rng=0xADB27C31u;
        const auto random=[&]{rng^=rng<<13; rng^=rng>>17; rng^=rng<<5;return rng;};
        for(auto& in:inputs) {
            const auto n=random(),m=random();
            in.n=(n&0x807FFFFFu)|((120u+(n%15u))<<23u);
            in.m=(m&0x807FFFFFu)|((120u+(m%15u))<<23u);
        }
        unsigned accepted=0;
        for(auto op:{Op::Add,Op::Subtract,Op::Multiply,Op::Divide})
            for(unsigned rm=0;rm<4;++rm) for(const auto& in:inputs) {
                SingleBinaryResult a,b;
                const auto ambient=0x1F80u | ((rm&3u)<<13u) | 0x8040u | 0x25u;
                _mm_setcsr(ambient);
                if(!integer(in.n,in.m,op,rm,a) ||
                   !sonic_try_hardware_single(in.n,in.m,op,rm,b) ||
                   a.bits!=b.bits || a.causes!=b.causes || _mm_getcsr()!=ambient)
                    throw std::runtime_error("hardware branch differential mismatch");
                ++accepted;
            }
        _mm_setcsr(0x1F80u);
        std::cout<<"SONIC_HARDWARE_KERNEL_EXACT accepted="<<accepted<<" host_state=exact\n";
        for(auto op:{Op::Add,Op::Subtract,Op::Multiply,Op::Divide}) for(unsigned round=0;round<4;++round) {
            Result a,b;
            if(round&1u) {b=measure(sonic_try_hardware_single,op,round);a=measure(integer,op,round);}
            else {a=measure(integer,op,round);b=measure(sonic_try_hardware_single,op,round);}
            if(a.fingerprint!=b.fingerprint) throw std::runtime_error("benchmark fingerprint mismatch");
            std::cout<<"SONIC_HARDWARE_KERNEL op="<<unsigned(op)<<" round="<<round
                <<" integer_ms="<<a.milliseconds<<" hardware_ms="<<b.milliseconds
                <<" ratio="<<a.milliseconds/b.milliseconds<<" checksum="<<a.fingerprint<<'\n';
        }
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
