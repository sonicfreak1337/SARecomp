#pragma once
#include "katana/runtime/memory.hpp"
#include <cstdint>
#include <utility>
namespace sonic::memory {
struct PreparedRead32 {
    std::uint32_t guest_address=0,direct_address=0;
    bool valid=false;
};
// Preparation performs no load or accounting. Its caller supplies the exact
// original architectural translation and keeps the original miss/flush path.
template<class Guard,class Translate>
inline PreparedRead32 prepare_read32(const Guard& guard,std::uint32_t address,Translate&& translate) noexcept {
    PreparedRead32 result;result.guest_address=address;std::uint32_t ignored=0;
    result.valid=translate(address,result.direct_address) &&
        katana::runtime::direct_linear_guard_offset(guard,result.direct_address,4,ignored);
    return result;
}
// Only within a proven ordinary instruction envelope: no callback or SR/MMU
// mutation may intervene. The SDK still owns live-generation validation,
// alignment, bounds, the actual load and both successful-read counters.
template<class Guard,class Fallback>
inline std::uint32_t read_prepared32(const Guard& guard,const PreparedRead32& prepared,
    std::uint32_t current_address,Fallback&& original_read) {
    std::uint32_t value=0;
    if(prepared.valid && prepared.guest_address==current_address &&
        katana::runtime::direct_linear_guard_read_u32(guard,prepared.direct_address,value))return value;
    return std::forward<Fallback>(original_read)();
}
}
