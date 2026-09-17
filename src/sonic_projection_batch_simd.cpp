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
        alignas(16) std::uint32_t axes[3][4];
        for(unsigned lane=0;lane<4;++lane){
            const auto index=base+lane<=even?base+lane:even;
            for(unsigned axis=0;axis<3;++axis)axes[axis][lane]=out.transformed[index][axis];
        }
        __m128 transformed[3];
        for(unsigned row=0;row<3;++row){
            transformed[row]=_mm_castsi128_ps(_mm_load_si128(reinterpret_cast<const __m128i*>(axes[row])));
            if(!bounded(transformed[row],row==2))return false;
        }
        const auto inverse=_mm_div_ps(one,transformed[2]);
        const auto px=_mm_mul_ps(_mm_mul_ps(transformed[0],sx),inverse);
        const auto py=_mm_mul_ps(_mm_mul_ps(transformed[1],sy),inverse);
        const auto x=_mm_add_ps(px,cx),y=_mm_add_ps(py,cy);
        const auto visible=unsigned(_mm_movemask_ps(_mm_cmpgt_ps(transformed[2],near)));
        alignas(16) std::uint32_t raw[3][4],depth[4],xs[4],ys[4],before_x[4];
        for(unsigned axis=0;axis<3;++axis)_mm_store_si128(reinterpret_cast<__m128i*>(raw[axis]),_mm_castps_si128(transformed[axis]));
        _mm_store_si128(reinterpret_cast<__m128i*>(depth),_mm_castps_si128(inverse));
        _mm_store_si128(reinterpret_cast<__m128i*>(xs),_mm_castps_si128(x));
        _mm_store_si128(reinterpret_cast<__m128i*>(ys),_mm_castps_si128(y));
        _mm_store_si128(reinterpret_cast<__m128i*>(before_x),_mm_castps_si128(px));
        for(unsigned lane=0;lane<4 && base+lane<=even;++lane){
            const auto i=base+lane;
            if(i==even){out.read_ahead={raw[0][lane],raw[1][lane],raw[2][lane],depth[lane]};continue;}
            out.positions[i]={xs[lane],ys[lane],depth[lane]};
            const bool clipped=(visible&(1u<<lane))==0;
            out.clip_count+=unsigned(clipped);
            if(i<out.clipped.size())out.clipped[i]=std::uint8_t(clipped);
            if(i+1u==even){
                out.last_second={before_x[lane],ys[lane],raw[2][lane],depth[lane]};
                out.last_compare=!clipped;
            }
        }
    }
    return true;
}
}
