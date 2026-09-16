#pragma once
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "sonic_internal_diagnostics.hpp"
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace sonic::scalar_writes {
using katana::runtime::DirectLinearMemoryGuard;
using katana::runtime::Memory;
using katana::runtime::MemoryPerformanceCounters;
using katana::runtime::NativePortImmutableWriteGuard;

// Only the authenticated NativePortAotServices constructor registers a pair.
// Its scalar observer does exactly guard.observe_write(event), and its batch
// observer has the same effect. No arbitrary Stable observer is admitted.
struct Binding {
    Memory* memory = nullptr;
    const NativePortImmutableWriteGuard* immutable = nullptr;
    std::uint64_t observer_generation = 0;
};
inline constinit thread_local std::array<Binding, 8> bindings{};
inline constinit thread_local const Binding* requested_capture = nullptr;

inline bool enabled() noexcept {
    static const bool value = [] {
        const char* p = std::getenv("SARECOMP_SCALAR_WRITES");
        return !p || std::strcmp(p, "0") != 0;
    }();
    return value && !diagnostics::runtime_checks_enabled();
}

inline void bind(Memory& memory, const NativePortImmutableWriteGuard& guard,
                 std::uint64_t observer_generation) noexcept {
    if (!enabled()) return;
    for (auto& slot : bindings) {
        if (slot.memory == &memory) return; // Duplicate registration fails closed.
    }
    for (auto& slot : bindings) {
        if (!slot.memory) {
            slot = {&memory, &guard, observer_generation};
            return;
        }
    }
    // An unregistered context simply uses the unchanged Memory operations.
}

inline void unbind(const Memory* memory,
                   const NativePortImmutableWriteGuard* guard) noexcept {
    for (auto& slot : bindings)
        if (slot.memory == memory && slot.immutable == guard) slot = {};
}

// Called only by the prepared copy of Memory::direct_linear_memory_guard.
// All its ordinary mapping, access-sink, watchpoint and write-permission checks
// run first. The usual public API still rejects writable guards with observers:
// this capability exists only during our synchronous, callback-free capture.
inline bool capture_admitted(const Memory* memory,
                             std::uint64_t observer_generation) noexcept {
    return requested_capture && requested_capture->memory == memory &&
           requested_capture->immutable &&
           requested_capture->observer_generation == observer_generation &&
           memory->guest_write_observer_pair_current(observer_generation);
}

class View final {
  public:
    View(Memory& memory, const NativePortImmutableWriteGuard* immutable) noexcept {
        if (!enabled() || !immutable) return;
        for (const auto& slot : bindings) {
            if (slot.memory != &memory || slot.immutable != immutable ||
                !memory.guest_write_observer_pair_current(slot.observer_generation))
                continue;
            const auto* previous = requested_capture;
            requested_capture = &slot;
            direct_ = memory.direct_linear_memory_guard(true);
            requested_capture = previous;
            if (!direct_ || !direct_.write_bytes) return;
            immutable_ = immutable;
            // Memory is non-const and owns these mutable accounting fields.
            // Keep the same counters as try_write_direct_linear_u8/u16/u32.
            counters_ = &const_cast<MemoryPerformanceCounters&>(memory.performance_counters());
            return;
        }
    }

    [[nodiscard]] bool available() const noexcept {
        return immutable_ && direct_.write_bytes && static_cast<bool>(direct_);
    }

    template<class T>
    [[nodiscard]] bool try_write(std::uint32_t translated_address, T value) const noexcept {
        static_assert(std::is_unsigned_v<T> &&
                      (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4));
        static_assert(std::endian::native == std::endian::little);
        std::uint32_t offset = 0;
        if (!immutable_ || !direct_.write_bytes ||
            !katana::runtime::direct_linear_guard_offset(
                direct_, translated_address, sizeof(T), offset) ||
            immutable_->tracks_address(translated_address, sizeof(T)))
            return false;
        // The exact current observer would return without side effects for
        // this range, regardless of source or bytes_changed. Do the store
        // immediately: this is not deferred write batching. A code/RO overlap,
        // stale mapping/observer or watched access always takes the old path.
        std::memcpy(direct_.write_bytes + offset, &value, sizeof(T));
        ++counters_->indexed_region_hits;
        ++counters_->unobserved_accesses;
        return true;
    }

  private:
    DirectLinearMemoryGuard direct_{};
    const NativePortImmutableWriteGuard* immutable_ = nullptr;
    MemoryPerformanceCounters* counters_ = nullptr;
};
} // namespace sonic::scalar_writes
