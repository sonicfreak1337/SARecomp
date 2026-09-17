#pragma once

#include "katana/runtime/runtime.hpp"
#include <span>

namespace sonic::collision_memory { class Access; }

namespace katana::runtime { class NativePortImmutableWriteGuard; }

namespace sonic::collision_math {
inline constexpr std::uint32_t cross_entry = 0x8C027360u, cross_size = 0x42u;
inline constexpr auto cross_source_sha256 =
    "ab64ed43a74ef8ae8bc802e9a03ac1ff70c19370a74186906ecf5878a3149dd8";
inline constexpr std::uint32_t length_entry = 0x8C63A69Cu, length_size = 0x10u;
inline constexpr auto length_source_sha256 =
    "184ec57b105022cf5a5df589f52fed8dc017109b7c0ff5626bfaf6c31c5a39dd";
inline constexpr std::uint32_t normalize_entry = 0x8C63A88Cu, normalize_size = 0x20u;
inline constexpr auto normalize_source_sha256 =
    "91bc28ff6fe7b04c8d5178dd3b7e8ee411224556da83d770225895326e61376b";

// Optional whole PAL leaves, selected by PC. False precedes every guest-state,
// RAM and observer mutation; true returns at PR with architectural end state.
// The non-null immutable guard must be the live guard of the caller's services.
// Instruction/cycle/provenance accounting remains the native-hook caller policy.
// Cross: (R5-R4) x (R6-R4) into R7. Length: R4 vector -> FR0. Normalize:
// R4 vector in place, retaining the original FSRRA-derived FR0 length result.
// All arithmetic uses retained SH4 FPU helpers, including original operand order.
// All input words are latched before the first store, so data aliasing is legal;
// code/immutable outputs, diagnostics and exceptional FPU modes are declined.
// P0 requires MMUCR.AT=0 and NoMmu; P1/P2 bypass translation. No RAM mirrors.
[[nodiscard]] bool try_execute(
    katana::runtime::CpuState& cpu,
    const katana::runtime::NativePortImmutableWriteGuard* immutable_guard);
// Internal closed-child entry, only for the reviewed triangle/contact owners.
// Caller proves the exact three source bodies, admitted FPU mode/epoch and the
// complete writable footprints. Access authenticates the product observer; an
// arbitrary observer cannot use this entry. Operands are checked before any
// mutation. Caller must discard Access before external/retained calls, prove
// their reviewed closure and revalidate before recapture. No generic call cache.
struct ClosedWriteRange { std::uint32_t address,size; };
[[nodiscard]] bool try_execute_closed(katana::runtime::CpuState&,std::uint32_t target,
    collision_memory::Access&,bool p0,std::span<const ClosedWriteRange> writes);
} // namespace sonic::collision_math
