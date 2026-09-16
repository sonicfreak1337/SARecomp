#pragma once
#include "katana/runtime/runtime.hpp"
#include <span>
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::animation_hierarchy {
inline constexpr std::uint32_t entry=0x8C057B00u;
struct SourceSpan {std::uint32_t address;std::span<const std::uint8_t> bytes;};
[[nodiscard]] std::span<const SourceSpan> source_spans() noexcept;
// One node and its descendants, returning that node's sibling. The complete
// invocation is qualified before mutation; no guest dispatch inside the tree.
[[nodiscard]] bool try_execute(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*);
struct Statistics {std::uint64_t native_calls=0,original_calls=0,nodes=0,direct_write_calls=0;};
[[nodiscard]] const Statistics& statistics() noexcept;
// Internal comparison switch. No end-user setting or timing dependency.
[[nodiscard]] bool try_dispatch(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*);
}
