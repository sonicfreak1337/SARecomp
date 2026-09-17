#pragma once
#include "katana/runtime/runtime.hpp"
#include <span>
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::render_context {
inline constexpr std::uint32_t capture_entry=0x8C605CECu,commit_entry=0x8C605D4Au;
struct SourceSpan {std::uint32_t address;std::span<const std::uint8_t> bytes;};
std::span<const SourceSpan> source_spans() noexcept;
bool try_execute(katana::runtime::CpuState&,const katana::runtime::NativePortImmutableWriteGuard*);
bool try_dispatch(katana::runtime::CpuState&,const katana::runtime::NativePortImmutableWriteGuard*);
struct Statistics {std::uint64_t captures=0,commits=0,fallbacks=0,direct_calls=0;};
const Statistics& statistics() noexcept;
}
