#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sonic::dispatch {

// Only for pointers into the process-lifetime, immutable generated table.
// Admission, module ownership, generation and callable checks are not cached.
template<class Entry, std::size_t Capacity = 64>
class ImmutableSourceMemo {
    static_assert(Capacity && !(Capacity & (Capacity - 1)));
    struct Slot { std::uint32_t source = 0; const Entry* entry = nullptr; };
    std::array<Slot, Capacity> slots_{};
public:
    struct Statistics { std::uint64_t hits=0, misses=0, conflicts=0, missing=0; } statistics;
    static constexpr std::size_t slot_index(std::uint32_t source) noexcept {
        auto key=source>>1u; key^=key>>13u; key^=key>>7u;
        return key & (Capacity-1);
    }
    template<bool Diagnose = false, class Resolve>
    const Entry* find(std::uint32_t source, Resolve resolve) {
        auto& slot=slots_[slot_index(source)];
        if (slot.entry && slot.source==source) {
            if constexpr (Diagnose) ++statistics.hits;
            return slot.entry;
        }
        if constexpr (Diagnose) {
            ++statistics.misses;
            if (slot.entry) ++statistics.conflicts;
        }
        const Entry* entry=resolve(source); // Preserve original miss/exception behavior.
        if (entry) slot={source,entry}; // Never memoize negative results.
        else if constexpr (Diagnose) ++statistics.missing;
        return entry;
    }
    void reset() noexcept { slots_.fill({}); statistics={}; }
};
}
