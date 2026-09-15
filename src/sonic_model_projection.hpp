#pragma once
#include "katana/runtime/runtime.hpp"
#include "katana/runtime/fpu.hpp"
#include <cstdint>
#include <span>

namespace sonic::model_projection {
// Research kernel, linked only by the EXCLUDE_FROM_ALL differential tests.
// Removed from the game after matched measurements found no useful gain.
// The admitted NINJA model-transform owner supplies FR4..7/14 and a stable
// XF matrix. Input/output are disjoint, bounded host spans; no RAM writes,
// dispatch, callbacks or mapping changes occur until this operation returns.
// Includes the retail pair loop's padding point and final read-ahead point.
// The matching host FPU epoch must remain live throughout this synchronous
// operation. Returns false before mutation for unsupported arithmetic/spans.
// On success only FR, FPSCR, R13, T, XYZ output and clip bytes are changed.
// The caller retains padding words and publishes its existing RAM transaction.
bool try_execute(katana::runtime::CpuState& cpu, std::uint32_t point_count,
                 std::span<const std::uint8_t> points,
                 std::span<std::uint8_t> output,
                 std::span<std::uint8_t> clipped,
                 const katana::runtime::HostFpuExecutionEpoch& epoch) noexcept;
}
