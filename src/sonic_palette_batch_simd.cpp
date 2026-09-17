#include <immintrin.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sonic::palette_batch::detail {
namespace {
__m128 flush(__m128i bits) noexcept {
    const auto zero=_mm_cmpeq_epi32(_mm_and_si128(bits,_mm_set1_epi32(0x7f800000)),_mm_setzero_si128());
    return _mm_castsi128_ps(_mm_andnot_si128(_mm_and_si128(zero,_mm_set1_epi32(0x7fffffff)),bits));
}
__m128 axis(const std::uint8_t* data,std::size_t start,std::size_t count,unsigned coordinate) noexcept {
    std::uint32_t bits[4]{};
    for(unsigned lane=0;lane<4 && start+lane<count;++lane)
        std::memcpy(bits+lane,data+((start+lane)*3+coordinate)*4,4);
    return flush(_mm_loadu_si128(reinterpret_cast<const __m128i*>(bits)));
}
}
// Separate AVX2/FMA translation unit; caller checks CPUID and OSXSAVE. Each
// lane is one normal, retaining the ordered multiply + three FMAs of FIPR.
void calculate_avx2(const std::uint8_t* normals,std::size_t count,const std::uint32_t* light,
    std::uint32_t scale,std::uint32_t* scaled,std::uint32_t* integers) noexcept {
    const auto x=flush(_mm_set1_epi32(int(light[0])));
    const auto y=flush(_mm_set1_epi32(int(light[1])));
    const auto z=flush(_mm_set1_epi32(int(light[2])));
    const auto zero=_mm_setzero_ps(),one=_mm_set1_ps(1.0f);
    const auto factor=_mm_castsi128_ps(_mm_set1_epi32(int(scale)));
    for(std::size_t base=0;base<count;base+=4){
        auto dot=_mm_mul_ps(x,axis(normals,base,count,0));
        dot=_mm_fmadd_ps(y,axis(normals,base,count,1),dot);
        dot=_mm_fmadd_ps(z,axis(normals,base,count,2),dot);
        dot=_mm_fmadd_ps(zero,one,dot);
        const auto product=_mm_mul_ps(flush(_mm_castps_si128(dot)),factor);
        const auto value=_mm_add_ps(product,factor);
        const auto integral=_mm_cvttps_epi32(value);
        alignas(16) std::uint32_t values[4],converted[4];
        _mm_store_si128(reinterpret_cast<__m128i*>(values),_mm_castps_si128(value));
        _mm_store_si128(reinterpret_cast<__m128i*>(converted),integral);
        const auto lanes=count-base<4?count-base:4;
        std::memcpy(scaled+base,values,lanes*4);
        std::memcpy(integers+base,converted,lanes*4);
    }
}
}
