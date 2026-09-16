#pragma once

#include "katana/runtime/code_address_inline.hpp"

// Port-local experiment. The original interval/ABI helpers remain available
// for unmodified units and for pages containing a mapping boundary.
namespace sonic::code_address_pages {
inline constexpr unsigned shift = 16u;
inline constexpr unsigned count = 1u << (32u - shift);
inline constexpr std::uint32_t fallback = 0xFFFFFFFFu;
extern constinit thread_local std::uint32_t forward[count];
extern constinit thread_local std::uint32_t reverse[count];

[[nodiscard]] inline std::uint32_t relocate(std::uint32_t address) noexcept {
    const auto delta = forward[address >> shift];
    return delta != fallback ? address + delta
                             : katana::runtime::relocate_code_address(address);
}
[[nodiscard]] inline std::uint32_t unrelocate(std::uint32_t address) noexcept {
    const auto delta = reverse[address >> shift];
    return delta != fallback ? address + delta
                             : katana::runtime::unrelocate_code_address(address);
}
} // namespace sonic::code_address_pages
