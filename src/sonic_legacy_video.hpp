#pragma once
#include "katana/runtime/native_port.hpp"

// Source-bound ADVERTISE TV-mode owner. PC output is configured by the host.
extern "C" katana::runtime::NativePortHookResult sonic_legacy_video_mode_disabled(
    katana::runtime::NativePortContext&) noexcept;
