#pragma once
#include "katana/runtime/runtime.hpp"
#include <span>
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::near_collision {
inline constexpr std::uint32_t entry=0x8C028BFEu, size=0x246u;
inline constexpr auto source_sha256="1039a254e1a32bfd40deedc30c20cb7e4cb8c082732c926160ba58a95c6992c8";
struct SourceSpan { std::uint32_t address; std::span<const std::uint8_t> bytes; };
[[nodiscard]] std::span<const SourceSpan> source_spans() noexcept;
// Complete NEAR-POLY, its eligibility producer and RAM-only SDK closure.
// False is permitted only before any guest mutation. Unsupported debug/service
// paths retain Original; a failure after admission throws and must become Abort.
[[nodiscard]] bool try_execute(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*);
}
