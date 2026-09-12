#pragma once
#include "katana/runtime/native_port.hpp"
#include "katana/runtime/native_port_platform.hpp"

namespace sonic::camera {
// Consume the same once-per-frame P1 snapshot as the original peripheral path.
// Never poll a second time or change player movement/button mappings.
void sample_input(katana::runtime::NativePortContext&,
    const katana::runtime::NativePortInputSnapshot&,bool suppressed) noexcept;
// Quicksave restores can retain all guest pointers and frame numbers.
void reset_timeline() noexcept;
void suppress_input() noexcept;
}
extern "C" katana::runtime::NativePortHookResult sonic_recompiled_camera_publish(
    katana::runtime::NativePortContext&) noexcept;
