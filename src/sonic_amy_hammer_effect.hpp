#pragma once
#include "katana/runtime/native_port.hpp"
#include "katana/runtime/runtime.hpp"
#include <array>

namespace sonic::amy_hammer_effect {
inline constexpr std::uint32_t entry = 0x8C0DF84Cu, size = 0xECu;
inline constexpr auto source_sha256 = "6848e8ae03d8ca34e4d1a6b3cb274912ba92e25b4bc3d6245f6dd557f5f97315";
inline constexpr std::array retained_entries{
    0x8C0DDD5Cu, 0x8C0DF804u, 0x8C63A8F8u, 0x8C0DF6A0u, 0x8C0986C6u};
struct RetainedCallBridge {
    void* context = nullptr;
    // Complete the real original call at PC, preserving its original PR.
    // False after mutation is fatal; this missing AOT entry has no fallback.
    bool (*invoke)(void*, katana::runtime::CpuState&, std::uint32_t) = nullptr;
};
// Complete PAL callback, including both cleanup branches and the display tail.
// No game-speed, animation-state, allocation or rendering substitution.
// Instruction/cycle accounting belongs to the native-hook dispatcher.
[[nodiscard]] katana::runtime::NativePortHookResult execute(
    katana::runtime::CpuState&, const RetainedCallBridge&);
} // namespace sonic::amy_hammer_effect
