#include "../src/sonic_native_input_policy.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {

void require(const bool condition, const char* const message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::exit(1);
}

} // namespace

int main() {
    constexpr std::uint32_t jump = 1u << 2u;

    static_assert(
        sonic_native_private::sonic_native_peripheral_trigger(0u) == 0u);
    static_assert(
        sonic_native_private::sonic_native_peripheral_trigger(30u) == 0u);
    static_assert(
        sonic_native_private::sonic_native_peripheral_trigger(31u) == 1u);
    static_assert(
        sonic_native_private::sonic_native_peripheral_trigger(128u) == 111u);
    static_assert(
        sonic_native_private::sonic_native_peripheral_trigger(255u) == 255u);
    for (std::uint32_t raw = 0u; raw <= 0xFFu; ++raw) {
        constexpr std::uint32_t host_deadzone = 7'710u;
        const auto widened = raw * 257u;
        const auto expected =
            widened <= host_deadzone
                ? 0u
                : ((widened - host_deadzone) * 0xFFu) /
                      (65'535u - host_deadzone);
        require(
            sonic_native_private::sonic_native_peripheral_trigger(
                static_cast<std::uint8_t>(raw)) == expected,
            "native peripheral trigger diverged from the host default");
    }

    const auto first = sonic_native_private::sonic_native_input_edges(
        jump, 0u, false);
    const auto first_title = sonic_native_private::sonic_native_title_edges(
        first.buttons, first.pressed, 0u, 0u, false);
    require(first_title.buttons == jump && first_title.pressed == jump,
            "first title boundary lost the jump edge");

    const auto repeated = sonic_native_private::sonic_native_input_edges(
        jump, first.previous, false);
    const auto repeated_title =
        sonic_native_private::sonic_native_title_edges(
            repeated.buttons, repeated.pressed, first_title.buttons, 0u,
            false);
    require(repeated_title.buttons == jump && repeated_title.pressed == 0u,
            "same snapshot produced a second jump edge");

    const auto released = sonic_native_private::sonic_native_input_edges(
        0u, repeated.previous, false);
    require(released.pressed == 0u && released.released == jump,
            "release edge was not preserved");

    const auto second_press = sonic_native_private::sonic_native_input_edges(
        jump, released.previous, false);
    require(second_press.pressed == jump,
            "a real later press did not produce a new edge");

    const auto suppressed = sonic_native_private::sonic_native_input_edges(
        jump, 0u, true);
    require(suppressed.buttons == jump && suppressed.pressed == 0u &&
                suppressed.released == 0u,
            "edge suppression changed held state");

    constexpr std::uint32_t analog_right = 1u << 13u;
    const auto suppressed_title =
        sonic_native_private::sonic_native_title_edges(
            0u, 0u, 0u, analog_right, true);
    require(suppressed_title.buttons == analog_right &&
                suppressed_title.pressed == 0u,
            "edge suppression leaked an analog title edge");

    return 0;
}
