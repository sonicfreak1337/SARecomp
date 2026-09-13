#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <stdexcept>
#include <vector>

namespace sonic::texture {

// Host-only candidate selection, never a cache of publication validity. The
// owning state invalidates this at every list/key mutation. Positions, rather
// than pointers, survive reserve/reallocation; state reset resets the index.
class DescriptorCandidateIndex final {
public:
    enum class Source : std::uint8_t { Pvm, Binding };
    struct Candidate final {
        std::uint32_t backing_address;
        Source source;
        std::size_t owner;
        std::size_t ordinal;
        bool operator==(const Candidate&) const = default;
    };

    void invalidate() noexcept {
        ++revision_; // Unsigned wrap cannot revive ready_ or old candidates.
        ready_ = false;
    }

    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] bool ready() const noexcept { return ready_; }

    // Enumeration must supply ALL host candidates, including currently stale
    // or conflicting claims. Admission failure keeps the original linear path.
    template<class Enumerate>
    [[nodiscard]] bool prepare(Enumerate&& enumerate) {
        if (ready_) return true;
        entries_.clear();
        try {
            enumerate([&](std::uint32_t key, Source source,
                          std::size_t owner, std::size_t ordinal) {
                entries_.push_back({key, source, owner, ordinal});
            });
            std::sort(entries_.begin(), entries_.end(), [](const auto& a, const auto& b) {
                if (a.backing_address != b.backing_address)
                    return a.backing_address < b.backing_address;
                if (a.source != b.source) return a.source < b.source;
                if (a.owner != b.owner) return a.owner < b.owner;
                return a.ordinal < b.ordinal;
            });
            ready_ = true;
        } catch (const std::bad_alloc&) {
            entries_.clear();
        } catch (const std::length_error&) {
            entries_.clear();
        }
        return ready_;
    }

    [[nodiscard]] std::span<const Candidate> candidates(std::uint32_t key) const noexcept {
        if (!ready_) return {};
        const auto first = std::lower_bound(entries_.begin(), entries_.end(), key,
            [](const auto& item, auto value) { return item.backing_address < value; });
        const auto last = std::upper_bound(first, entries_.end(), key,
            [](auto value, const auto& item) { return value < item.backing_address; });
        return {first, last};
    }

private:
    std::vector<Candidate> entries_;
    std::uint64_t revision_ = 0u;
    bool ready_ = false;
};

} // namespace sonic::texture
