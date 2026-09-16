#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sonic::dispatch {
// Raw addresses deliberately distinguish aliases and call/tail-jump state.
// Only a successful original preflight may produce a reusable plan.
struct TransferKey final {
    std::uint32_t source{}, callsite{}, target{}, continuation{};
    bool call{};
    bool operator==(const TransferKey&) const noexcept = default;
};

// Context backing is immutable for its lifetime. All mutable executable
// authorities participate here, including writes which quarantine code.
struct TransferEpoch final {
    std::array<const void*, 9> owners{};
    std::array<std::uint64_t, 10> generations{};
    bool static_ready{};
    bool operator==(const TransferEpoch&) const noexcept = default;
};

template<class Plan, std::size_t Capacity = 4096>
class PreparedTransfers final {
    static_assert(Capacity != 0 && (Capacity & (Capacity - 1)) == 0);
    struct Row { TransferKey key{}; Plan plan{}; std::uint64_t serial{}; };
    std::array<Row, Capacity> rows_{};
    TransferEpoch epoch_{};
    std::uint64_t serial_ = 1;
    bool epoch_valid_ = false;

    static std::size_t index(const TransferKey& key) noexcept {
        std::uint64_t hash = (static_cast<std::uint64_t>(key.source) << 32) | key.target;
        hash ^= (static_cast<std::uint64_t>(key.callsite) << 17) ^ key.continuation;
        hash ^= static_cast<std::uint64_t>(key.call);
        hash ^= hash >> 30;
        hash *= 0xbf58476d1ce4e5b9ULL;
        hash ^= hash >> 27;
        return static_cast<std::size_t>(hash) & (Capacity - 1);
    }
    void synchronize(const TransferEpoch& epoch) noexcept {
        if (!epoch_valid_ || !(epoch_ == epoch)) {
            reset();
            epoch_ = epoch;
            epoch_valid_ = true;
        }
    }
public:
    void reset() noexcept {
        epoch_valid_ = false;
        if (++serial_ == 0) {
            for (auto& row : rows_) row.serial = 0;
            serial_ = 1;
        }
    }
    const Plan* find(const TransferEpoch& epoch, const TransferKey& key) noexcept {
        synchronize(epoch);
        const auto& row = rows_[index(key)];
        return row.serial == serial_ && row.key == key ? &row.plan : nullptr;
    }
    void remember(const TransferEpoch& epoch, const TransferKey& key, const Plan& plan) noexcept {
        synchronize(epoch);
        rows_[index(key)] = {key, plan, serial_};
    }
};
} // namespace sonic::dispatch
