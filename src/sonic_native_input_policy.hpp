#pragma once

#include <cstdint>

namespace sonic_native_private {

struct SonicNativeInputEdges final {
    std::uint32_t buttons = 0u;
    std::uint32_t pressed = 0u;
    std::uint32_t released = 0u;
    std::uint32_t previous = 0u;
};

// Keep the native title projection identical to the shared host-controller
// default: XInput's 8-bit trigger is first widened by 257, then the default
// 7,710/65,535 deadzone is removed and the remaining range is rescaled.  The
// PAL camera treats every non-zero peripheral trigger word as active, so raw
// hardware noise must not leak into the fixed title records.
[[nodiscard]] constexpr std::uint16_t sonic_native_peripheral_trigger(
    const std::uint8_t raw_trigger) noexcept {
    constexpr std::uint32_t deadzone = 30u;
    if (raw_trigger <= deadzone) return 0u;
    return static_cast<std::uint16_t>(
        ((static_cast<std::uint32_t>(raw_trigger) - deadzone) * 0xFFu) /
        (0xFFu - deadzone));
}

// One host snapshot may feed more than one original title input-service
// boundary. Each boundary still republishes the SDK edge fields: the first
// observes the transition, while a later boundary sees current == previous
// and therefore cannot expose the same rising edge twice.
[[nodiscard]] constexpr SonicNativeInputEdges sonic_native_input_edges(
    const std::uint32_t current,
    std::uint32_t previous,
    const bool suppress_edges) noexcept {
    if (suppress_edges) previous = current;
    return {
        current,
        current & ~previous,
        previous & ~current,
        current,
    };
}

struct SonicNativeTitleEdges final {
    std::uint32_t buttons = 0u;
    std::uint32_t pressed = 0u;
};

// Exact title projection used by the resident four-slot helper: analog
// directions contribute their own edge against the previous projected mask,
// while digital edges arrive from the SDK-facing record.
[[nodiscard]] constexpr SonicNativeTitleEdges sonic_native_title_edges(
    const std::uint32_t raw_buttons,
    const std::uint32_t raw_pressed,
    const std::uint32_t previous_buttons,
    const std::uint32_t analog_buttons,
    const bool suppress_edges) noexcept {
    return {
        analog_buttons | raw_buttons,
        suppress_edges
            ? 0u
            : ((~previous_buttons & analog_buttons) | raw_pressed),
    };
}

} // namespace sonic_native_private
