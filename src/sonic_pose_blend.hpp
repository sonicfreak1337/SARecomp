#pragma once
#include "katana/runtime/runtime.hpp"
#include <span>
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::pose_blend {
inline constexpr std::uint32_t entry=0x8C0417C8u;
struct SourceSpan {std::uint32_t address;std::span<const std::uint8_t> bytes;};
struct Statistics {std::uint64_t native_calls=0,original_calls=0,direct_write_calls=0;};
[[nodiscard]] std::span<const SourceSpan> source_spans() noexcept;
[[nodiscard]] const Statistics& statistics() noexcept;
[[nodiscard]] bool try_execute(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*);
[[nodiscard]] bool try_dispatch(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*);
}
