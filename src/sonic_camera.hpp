#pragma once
#include "katana/runtime/native_port.hpp"
#include "katana/runtime/native_port_platform.hpp"

namespace sonic::camera {
// Consume the same once-per-frame P1 snapshot as the original peripheral path.
// Never poll a second time. Only the explicitly enabled hidden collision test
// supplies a bounded P1 walk; normal input/movement mappings are untouched.
void sample_input(katana::runtime::NativePortContext&,
    katana::runtime::NativePortInputSnapshot&,bool suppressed) noexcept;
// Quicksave restores can retain all guest pointers and frame numbers.
void reset_timeline() noexcept;
void suppress_input() noexcept;
}
extern "C" katana::runtime::NativePortHookResult sonic_recompiled_camera_publish(
    katana::runtime::NativePortContext&) noexcept;
extern "C" katana::runtime::NativePortHookResult sonic_recompiled_camera_original_step(
    katana::runtime::NativePortContext&) noexcept;
