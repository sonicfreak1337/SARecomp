#pragma once
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

namespace sonic::render_completion {
// One generation belongs to one guest lifetime. Only the producer reserves
// and acknowledges tickets; only the render consumer publishes completion.
// Queued tickets retain their generation across restore, but cannot notify
// a replacement guest. No GPU fence or guest register access lives here.
struct Generation final {
    std::atomic<std::uint64_t> completed{0};
    std::uint64_t submitted = 0, dispatched = 0;
};
struct Ticket final {
    std::shared_ptr<Generation> generation;
    std::uint64_t sequence = 0;
    explicit operator bool() const noexcept { return generation != nullptr; }
    [[nodiscard]] bool complete() const noexcept {
        if (!generation || sequence == 0) return false;
        auto previous = sequence - 1;
        // A failed, duplicated or out-of-order command must never manufacture
        // an earlier completion by simply raising a high-water counter.
        return generation->completed.compare_exchange_strong(
            previous, sequence, std::memory_order_release, std::memory_order_relaxed);
    }
};
struct Counters final { std::uint64_t submitted = 0, completed = 0, dispatched = 0; };
class Channel final {
public:
    [[nodiscard]] Ticket reserve() {
        if (!generation_) generation_ = std::make_shared<Generation>();
        if (generation_->submitted == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("sonic-render-completion-sequence");
        return {generation_, ++generation_->submitted};
    }
    [[nodiscard]] Counters counters() const noexcept {
        if (!generation_) return {};
        return {generation_->submitted,
            generation_->completed.load(std::memory_order_acquire), generation_->dispatched};
    }
    [[nodiscard]] bool acknowledge() noexcept {
        const auto value = counters();
        if (value.dispatched >= value.completed) return false;
        ++generation_->dispatched;
        return true;
    }
private:
    std::shared_ptr<Generation> generation_;
};

// An explicit, producer-thread scope authenticates the origin of Present.
// Host Options, decoded videos and output repeats have no guest ticket.
inline thread_local Channel* submitting = nullptr;
class Submission final {
public:
    explicit Submission(Channel* channel) noexcept : previous_(submitting) { submitting = channel; }
    ~Submission() { submitting = previous_; }
    Submission(const Submission&) = delete;
    Submission& operator=(const Submission&) = delete;
private:
    Channel* previous_;
};
[[nodiscard]] inline Ticket capture() { return submitting ? submitting->reserve() : Ticket{}; }
} // namespace sonic::render_completion
