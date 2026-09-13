#pragma once
#include "katana/runtime/runtime.hpp"
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::vertex_normals {
inline constexpr auto source_sha256 =
    "bffbfdedd2721c7829b7cc35e82bc34190040b4703b131fafcdbf06df802907e";
// Optional complete PAL leaf 8C0563AC..8C056651. False changes no guest state,
// RAM, callbacks or memory counters. True returns at PR with exact registers,
// FPSCR, stack residues and ordered guest stores. Instruction/cycle accounting
// remains the native-hook caller's policy, as for sonic::palette_lighting.
// Pass the live immutable guard belonging to the caller's AOT services.
[[nodiscard]] bool try_execute(katana::runtime::CpuState& cpu,
    const katana::runtime::NativePortImmutableWriteGuard* immutable_guard);
} // namespace sonic::vertex_normals
