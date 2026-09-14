#pragma once
#include "katana/runtime/runtime.hpp"
#include <array>
#include <span>
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::motion_sampling {
inline constexpr std::array<std::uint32_t,4u> entries{0x8C0400A0u,0x8C0400F8u,0x8C040150u,0x8C0401A8u};
struct SourceSpan {std::uint32_t address;std::span<const std::uint8_t> bytes;};
[[nodiscard]] std::span<const SourceSpan> source_spans() noexcept;
// Complete sampling and register-only SRT closure. A false return is always
// before mutation; exceptions after admission must abort, never restart Original.
[[nodiscard]] bool try_execute(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*);
}
