#pragma once
#include "katana/runtime/fpu.hpp"
#include <array>
#include <span>
#include <vector>

namespace sonic::projection_batch {
struct Result {
    std::vector<std::array<std::uint32_t,4>> transformed;
    std::vector<std::array<std::uint32_t,3>> positions;
    std::vector<std::uint8_t> clipped;
    std::array<std::uint32_t,4> read_ahead{},last_second{};
    std::uint32_t clip_count=0;
    bool last_compare=false;
};
struct Counts {std::uint64_t calls=0,vertices=0,declined=0,verified=0;};
inline thread_local Counts counts{};
// Complete synchronous projection, including the padding/read-ahead point.
// Requires the caller's unchanged FPU epoch. No CPU or guest-RAM mutation on
// admission failure; Result is private scratch and may be partially filled.
bool prepare(const katana::runtime::CpuState&,std::uint32_t count,
    std::span<const std::uint8_t> points,Result&,
    const katana::runtime::HostFpuExecutionEpoch&);
// Only after prepare succeeds. The caller retains the ordinary RAM commit.
void publish(katana::runtime::CpuState&,const Result&,
    std::span<std::uint8_t> output,std::span<std::uint8_t> clips) noexcept;
}
