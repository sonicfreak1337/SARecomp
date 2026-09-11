#pragma once
#include "katana/runtime/runtime.hpp"

namespace sonic::presentation {
// Complete identity-bound retail leaves. Only the two X comparisons receive
// expanded host operands; shared guest bounds and vertical tests stay original.
void basic_model_cull(katana::runtime::CpuState& cpu, float horizontal_extra);
void draw_sphere_cull(katana::runtime::CpuState& cpu, float horizontal_extra);
bool draw_sphere_caller(std::uint32_t return_pc) noexcept;
}
