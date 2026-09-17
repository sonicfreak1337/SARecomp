#include "sonic_projection_batch.hpp"
#include <immintrin.h>
#include <cstring>

namespace sonic::projection_batch::detail {
namespace {
__m128 flush(__m128i bits) noexcept {
    const auto zero=_mm_cmpeq_epi32(_mm_and_si128(bits,_mm_set1_epi32(0x7f800000)),_mm_setzero_si128());
    return _mm_castsi128_ps(_mm_andnot_si128(_mm_and_si128(zero,_mm_set1_epi32(0x7fffffff)),bits));
}
bool finite(__m128i bits) noexcept {
    const auto exponent=_mm_set1_epi32(0x7f800000);
    return !_mm_movemask_epi8(_mm_cmpeq_epi32(_mm_and_si128(bits,exponent),exponent));
}
bool bounded(__m128 v,bool nonzero=false) noexcept {
    const auto magnitude=_mm_and_si128(_mm_castps_si128(v),_mm_set1_epi32(0x7fffffff));
    auto valid=_mm_andnot_si128(_mm_cmpgt_epi32(magnitude,_mm_set1_epi32(0x49800000)),
        _mm_cmpgt_epi32(magnitude,_mm_set1_epi32(0x357fffff)));
    if(!nonzero)valid=_mm_or_si128(valid,_mm_cmpeq_epi32(magnitude,_mm_setzero_si128()));
    return _mm_movemask_epi8(valid)==0xffff;
}
__m128 splat(std::uint32_t bits) noexcept {return flush(_mm_set1_epi32(int(bits)));}
}
// CPUID/OSXSAVE admission is in a separate baseline-ISA translation unit.
// Capture the matrix once; transform complete points in retained FTRV order,
// then project four vertices together. No guest register publication or
// per-operation mode tests inside the model loop.
bool calculate(const std::uint8_t* points,std::size_t even,const std::uint32_t* matrix,
    const std::uint32_t* fr,Result& out) noexcept {
    __m128 m[4];
    for(unsigned col=0;col<4;++col)m[col]=flush(_mm_loadu_si128(reinterpret_cast<const __m128i*>(matrix+col*4u)));
    const auto one=_mm_set1_ps(1.0f),sx=splat(fr[6]),sy=splat(fr[7]);
    const auto cx=splat(fr[4]),cy=splat(fr[5]),near=splat(fr[14]);
    // Finish FTRV for the entire model before hardware projection. The
    // retained bounded FMUL/FADD/FDIV helpers use integer arithmetic and do
    // not change host exception flags between these transforms. Retaining
    // that order also avoids a known TCG FMA/status-dependent low-bit change.
    for(std::size_t index=0;index<=even;++index){
        alignas(16) std::uint32_t input[4]{0,0,0,0x3f800000u};
        std::memcpy(input,points+index*12u,12u);
        const auto bits=_mm_load_si128(reinterpret_cast<const __m128i*>(input));
        if(!finite(bits))return false;
        const auto v=flush(bits);
        auto value=_mm_mul_ps(m[0],_mm_shuffle_ps(v,v,_MM_SHUFFLE(0,0,0,0)));
        value=_mm_fmadd_ps(m[1],_mm_shuffle_ps(v,v,_MM_SHUFFLE(1,1,1,1)),value);
        value=_mm_fmadd_ps(m[2],_mm_shuffle_ps(v,v,_MM_SHUFFLE(2,2,2,2)),value);
        value=_mm_fmadd_ps(m[3],one,value);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out.transformed[index].data()),
            _mm_castps_si128(flush(_mm_castps_si128(value))));
    }
    for(std::size_t base=0;base<=even;base+=4){
        // Transpose four whole records in registers. Previously each quad
        // scattered twelve scalar words onto the stack and gathered them
        // again, followed by a second scalar scatter of projected results.
        const auto load=[&](std::size_t i){
            return _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(
                out.transformed[i<=even?i:even].data())));
        };
        auto tx=load(base),ty=load(base+1),tz=load(base+2),tw=load(base+3);
        _MM_TRANSPOSE4_PS(tx,ty,tz,tw);
        if(!bounded(tx) || !bounded(ty) || !bounded(tz,true))return false;
        const auto inverse=_mm_div_ps(one,tz);
        const auto px=_mm_mul_ps(_mm_mul_ps(tx,sx),inverse);
        const auto py=_mm_mul_ps(_mm_mul_ps(ty,sy),inverse);
        const auto x=_mm_add_ps(px,cx),y=_mm_add_ps(py,cy);
        const auto visible=unsigned(_mm_movemask_ps(_mm_cmpgt_ps(tz,near)));
        auto r0=x,r1=y,r2=inverse,r3=_mm_setzero_ps();
        _MM_TRANSPOSE4_PS(r0,r1,r2,r3);
        const auto remaining=even-base;
        if(remaining>=4){
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out.positions[base].data()),_mm_castps_si128(r0));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out.positions[base+1].data()),_mm_castps_si128(r1));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out.positions[base+2].data()),_mm_castps_si128(r2));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out.positions[base+3].data()),_mm_castps_si128(r3));
        }else if(remaining==2){
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out.positions[base].data()),_mm_castps_si128(r0));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out.positions[base+1].data()),_mm_castps_si128(r1));
        }
        const unsigned lanes=remaining>=4?4u:unsigned(remaining);
        const unsigned clipped=(~visible)&((1u<<lanes)-1u);
        // Include the even padding vertex in R13, but never in the title's
        // actual-point clip array. The read-ahead vertex has neither effect.
        out.clip_count+=(clipped&1u)+((clipped>>1)&1u)+((clipped>>2)&1u)+((clipped>>3)&1u);
        for(unsigned lane=0;lane<lanes && base+lane<out.clipped.size();++lane)
            out.clipped[base+lane]=std::uint8_t((clipped>>lane)&1u);
        if(remaining<=4){
            alignas(16) std::uint32_t depth[4],before_x[4];
            _mm_store_si128(reinterpret_cast<__m128i*>(depth),_mm_castps_si128(inverse));
            if(remaining){
                _mm_store_si128(reinterpret_cast<__m128i*>(before_x),_mm_castps_si128(px));
                const auto last=even-1;
                out.last_second={before_x[remaining-1],out.positions[last][1],
                    out.transformed[last][2],depth[remaining-1]};
                out.last_compare=(visible&(1u<<(remaining-1)))!=0;
            }
            if(remaining<4){
                const auto& last=out.transformed[even];
                out.read_ahead={last[0],last[1],last[2],depth[remaining]};
            }
        }
    }
    return true;
}
}
