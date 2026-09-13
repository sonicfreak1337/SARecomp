#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sonic::diagnostics {

// Private observation only. The first records retain event order; counters
// continue for the whole fixture after the fixed buffer fills. No allocation,
// guest writes, instruction hooks or per-event logging on the measured path.
enum class UpdateTimingEvent : std::uint32_t {
    TasksCompleted, TailSample, Elapsed, Wait, ImageBoundary
};

struct UpdateTimingRecord {
    std::uint64_t nanoseconds{}, frame{};
    std::uint32_t pr{}, caller{}, iteration{}, phase{}, delta{}, ready{}, release{}, value{}, threshold{};
    UpdateTimingEvent event{};
};

struct UpdateTimingProbe {
    std::array<UpdateTimingRecord, 4096> records{};
    std::size_t size{};
    std::uint64_t tasks{}, tails{}, waits{}, elapsed{}, elapsed_extra{}, boundaries{};
    std::uint64_t main_tasks{}, alternate_tasks{}, setups{}, unreadable{}, dropped{};
};

} // namespace sonic::diagnostics
