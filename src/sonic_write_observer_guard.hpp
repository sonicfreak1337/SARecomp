#pragma once
#include "katana/runtime/memory.hpp"

namespace sonic::write_observer {
// A permission hint, never a writable pointer or a complete access proof.
// Every consumer must still resolve the address through this exact guard.
// Memory changes its direct generation for every observer/lookup/mapping
// change, so a stale hint cannot admit a store through the guard.
struct Guard final : katana::runtime::DirectLinearMemoryGuard {
    bool permits_observed_writes = false;
};

[[nodiscard]] inline Guard capture(const katana::runtime::Memory& memory) noexcept {
    Guard result;
    static_cast<katana::runtime::DirectLinearMemoryGuard&>(result) =
        memory.direct_linear_memory_guard(false);
    result.permits_observed_writes =
        memory.guest_write_observer_allows_prevalidated_linear_writes();
    return result;
}
} // namespace sonic::write_observer
