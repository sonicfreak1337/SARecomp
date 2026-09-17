#pragma once
#include "katana/runtime/runtime.hpp"
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::matrix_stack {
struct BulkCounts {
    std::uint64_t pushes=0,pops=0,saved=0,loaded=0;
};
inline thread_local BulkCounts bulk_counts{};
// Internal opt-in; preserves the product observer and entire leaf admission.
bool bulk_enabled() noexcept;
// Complete PAL byte spans, including their inline literal islands.
inline constexpr std::uint32_t push_entry=0x8C639BB0u, push_size=0x80u;
inline constexpr std::uint32_t pop_entry=0x8C639AD8u, pop_size=0x40u;
inline constexpr auto push_source_sha256 =
    "b1a24af68d7a51cc4ffebc58b54082563beb4add63eb4eeef54663967a0ffb10";
inline constexpr auto pop_source_sha256 =
    "a3ff7b35d7be9f1ac1dce0209af71beca602344d478d8cec77901ccf7598cc62";
// False precedes every guest/observer/memory-metric mutation. True completes
// the entire leaf at PR, including both null and nonnull matrix variants and
// the full-stack/zero-remainder returns. Dispatch selects push/pop by CPU.PC;
// every other entry declines. Push capacity is signed; Pop uses the original
// unsigned wrapped subtraction, not a repaired underflow check.
// No CPU stack is touched. Scalar MOVCA/FMOV events keep
// their original sources and order; native-hook accounting is caller-owned.
// batch_stores is an opt-in experiment: Push may batch each 18-store matrix
// save, flushing before its following guest read/control transfer. It requires
// a contract-compliant event-only observer with the SDK batch interface; absent
// admission, the same ordered scalar stores execute. Pop remains scalar because
// its stores are separated by guest reads. No batch is constructed when false.
// staged_groups optionally accumulates Save groups containing staged stores,
// only after they have flushed. This includes SDK scalar replay and does NOT
// assert batch-observer admission or optimized commit. Declines, scalar-only
// execution, Pop and full-stack returns leave the caller's count unchanged.
// The counter is caller-owned host storage, separate from CPU and guest RAM.
[[nodiscard]] bool try_execute(katana::runtime::CpuState& cpu,
    const katana::runtime::NativePortImmutableWriteGuard* immutable_guard,
    bool batch_stores=false,
    std::uint64_t* staged_groups=nullptr);
} // namespace sonic::matrix_stack
