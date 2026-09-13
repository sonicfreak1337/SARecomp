#pragma once
#include <cstddef>

namespace sonic::contour {
// Retail PAL64FA28 (untextured) and64FBA8 (textured) turn a convex clipped
// contour into a strip by consuming its front/back alternately. This is
// different from treating the incoming contour as an already ordered strip.
// Caller supplies output<count, with count>=3.
[[nodiscard]] constexpr std::size_t strip_index(std::size_t output, std::size_t count) noexcept {
    return (output & 1u) ? count - 1u - output / 2u : output / 2u;
}
}
