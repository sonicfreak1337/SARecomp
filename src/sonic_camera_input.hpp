#pragma once
#include <algorithm>
#include <cstdint>

namespace sonic::camera {
struct RawStick {std::int16_t x=0,y=0;};
inline std::int16_t joystick_axis(std::uint32_t value,std::uint32_t low,std::uint32_t high) {
    if (high<=low) return 0;
    const auto clamped=std::clamp<std::uint64_t>(value,low,high)-low;
    return std::int16_t(std::int32_t(clamped*65535/(std::uint64_t(high)-low))-32768);
}
inline RawStick dualsense_right_stick(std::uint32_t z,std::uint32_t r,
    std::uint32_t z_low,std::uint32_t z_high,std::uint32_t r_low,std::uint32_t r_high) {
    // Sony's right stick is HID Z/Rz (WinMM Z/R). U/V are the independent
    // trigger axes: their released zero must never become full camera input.
    return {joystick_axis(z,z_low,z_high),std::int16_t(std::clamp(-std::int32_t(joystick_axis(r,r_low,r_high)),-32768,32767))};
}
}
