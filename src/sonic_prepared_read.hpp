#pragma once
#include "katana/runtime/memory.hpp"
#include <cstdint>
#include <utility>
namespace sonic::memory {
struct PreloadedRead32 {
    std::uint32_t value=0;
    bool valid=false;
};
// Restricted to exact ordinary MOV.L and scalar FMOV envelopes verified by
// the preparer. FMOV may preload only with FD clear and SZ clear; its original
// FPU fault checks and paired-read path remain at their original positions.
// A successful guard read has no observer/callback. Between it and assignment,
// only ExplicitGuestInstructionAttempt's noexcept CPU bookkeeping may run.
// This performs the one real read and its original counters at admission;
// faults, MMIO, observer calls and miss-triggered scheduler flushes stay in
// the original path. Never retain this value across a call or a write.
template<class Guard,class Translate>
inline PreloadedRead32 preload_read32(const Guard& guard,std::uint32_t address,
                                     Translate&& translate) noexcept {
    PreloadedRead32 result;std::uint32_t direct=0;
    result.valid=translate(address,direct) &&
        katana::runtime::direct_linear_guard_read_u32(guard,direct,result.value);
    return result;
}
template<class Fallback>
inline std::uint32_t consume_preloaded32(const PreloadedRead32& prepared,
                                        Fallback&& original_read) {
    return prepared.valid ? prepared.value : std::forward<Fallback>(original_read)();
}
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
