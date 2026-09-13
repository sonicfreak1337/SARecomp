#pragma once
#include "katana/runtime/runtime.hpp"
#include <span>
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::collision_candidates {
inline constexpr std::uint32_t entry=0x8C029B00u,size=0xB6Cu;
inline constexpr auto source_sha256="744ca095c47e07d9b87ed43cf54e013c45349cd472852cf44cee85dd7b946832";
struct SourceSpan { std::uint32_t address; std::span<const std::uint8_t> bytes; };
[[nodiscard]] std::span<const SourceSpan> source_spans() noexcept;
struct RetainedCallBridge {
    void* context=nullptr;
    // Only the three original angle wrappers, X/Y rotation and 88-byte copier.
    // No unrelated guest callbacks. PC=target and original PR already set.
    bool (*invoke)(void*,katana::runtime::CpuState&,std::uint32_t target)=nullptr;
};
// False only before ALL guest/observer mutation. Debug drawing, unsafe aliases,
// invalid matrix stacks and non-RAM contexts take untouched Original.
// Post-call contract failure throws and MUST become Abort, never fallback.
// The original 16-contact exit intentionally retains its unmatched matrix push.
// Native-hook caller owns instruction/cycle/provenance bookkeeping.
[[nodiscard]] bool try_execute(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*,const RetainedCallBridge&);
} // namespace sonic::collision_candidates
