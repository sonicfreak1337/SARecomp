#pragma once
#include "katana/runtime/fpu.hpp"
#include <array>
#include <bit>
#include <cstdint>

namespace sonic::model_uv {
inline std::array<float,2> decode(std::int16_t u,std::int16_t v,
                                bool title_basic,
                                const katana::runtime::CpuState& cpu) noexcept {
    if (!title_basic)
        return {static_cast<float>(u)/256.0f,static_cast<float>(v)/256.0f};
    // PAL 037990/037ADC/037CDC load FR11 from 038F10, then FLOAT/FMUL
    // the signed NJS_TEX coordinates. This title-local factor is deliberately
    // different from the resident SDK's 1/256. BG_BEACH closes at 5*255;
    // dividing it by 256 samples a different column at the cylinder join.
    // Keep the literal and FPSCR rounding, including negative/tiled UVs.
    const katana::runtime::HostFpuExecutionEpoch epoch(cpu);
    constexpr auto scale=std::bit_cast<float>(0x3B808083u);
    return {static_cast<float>(u)*scale,static_cast<float>(v)*scale};
}
}
