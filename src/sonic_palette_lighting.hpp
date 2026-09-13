#pragma once

#include "katana/runtime/runtime.hpp"

namespace katana::runtime { class NativePortImmutableWriteGuard; }

namespace sonic::palette_lighting {

inline constexpr auto source_sha256 =
    "6033d3d4b0c9821d221d54c2bc3e78477df900a59c56208fc0a8bddfc518084c";

// Optional complete PAL leaf 8C037350..8C03745F. False preserves all guest
// state and RAM. True returns at PR with the original architectural end state.
// Guest instruction/cycle accounting remains the native-hook caller's policy.
// No stack is used by this leaf. Admission deliberately excludes exceptional
// FPU modes, diagnostic observers and overlapping source/destination storage.
// P0 is limited to the first physical 16 MiB RAM window with MMUCR.AT clear
// and no bound MMU translation; P1/P2 aliases remain valid with MMU enabled.
// The non-null guard must be the live guard of the caller's AOT services.
[[nodiscard]] bool try_execute(
    katana::runtime::CpuState& cpu,
    const katana::runtime::NativePortImmutableWriteGuard* immutable_guard);

} // namespace sonic::palette_lighting
