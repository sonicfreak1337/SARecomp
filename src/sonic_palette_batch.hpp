#pragma once
#include "katana/runtime/runtime.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sonic::palette_batch {
struct Result {
    std::vector<std::uint32_t> scaled;
    std::vector<std::uint32_t> integers;
};
struct Counts { std::uint64_t calls=0, vertices=0, declined=0, closed_loops=0; };
inline thread_local Counts counts{};
bool enabled() noexcept;
// No guest mutation. Only bounded finite arithmetic with sticky Inexact already
// set is admitted. Other values retain the complete original palette loop.
bool prepare(const katana::runtime::CpuState& cpu, const std::uint8_t* normals,
    std::size_t count, const std::uint32_t* light, std::uint32_t scale, Result& out);
}
