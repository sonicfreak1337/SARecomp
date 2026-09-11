#pragma once
#include "katana/runtime/fpu.hpp"
#include <array>
#include <bit>
#include <cstdint>

namespace sonic::color {
using Color = std::array<float, 4u>; // RGBA at the native boundary.
using ArgbWords = std::array<std::uint32_t, 4u>;

// SDK 620A72 and 6385F0 load these as raw FMOV words. Unlike geometry,
// material colors may contain infinities/NaNs until the TA color boundary.
template<class Reader>
bool read_constant_argb(Reader& reader, std::uint32_t control, ArgbWords& words) {
    words.fill(0u);
    if ((control & 0x30u) == 0u) return true;
    for (std::uint32_t i = 0u; i < words.size(); ++i)
        if (!reader.u32(0x8C88F5A8u + i * 4u, words[i])) return false;
    return true;
}

inline Color apply_constant_argb(katana::runtime::CpuState& fpu,
                                std::uint32_t control,
                                const ArgbWords& words, Color face) {
    for (std::uint32_t source = 0u; source < 4u; ++source) {
        const auto component = (source + 3u) % 4u;
        if ((control & 0x10u) != 0u) {
            face[component] = std::bit_cast<float>(words[source]);
        } else if ((control & 0x20u) != 0u) {
            fpu.fr[0u] = std::bit_cast<std::uint32_t>(face[component]);
            fpu.fr[1u] = words[source];
            katana::runtime::fpu_binary(fpu,
                katana::runtime::FpuBinaryOperation::Add, 1u, 0u);
            face[component] = std::bit_cast<float>(fpu.fr[0u]);
        }
    }
    return face;
}

inline bool has_nonfinite(const Color& color) noexcept {
    for (const auto value : color)
        if ((std::bit_cast<std::uint32_t>(value) & 0x7F800000u) == 0x7F800000u)
            return true;
    return false;
}

constexpr std::uint32_t ta_float_color_byte(std::uint32_t bits) noexcept {
    // Flycast ta_vtx.cpp: upper-16-bit table, then saturation/truncation.
    const auto value = std::bit_cast<float>(bits & 0xFFFF0000u);
    if (value != value) return 255u;
    if (value <= 0.0f) return 0u;
    if (value >= 1.0f) return 255u;
    return static_cast<std::uint32_t>(value * 255.0f);
}

inline Color intensity_header_color(const Color& face, std::uint32_t packed_intensity) {
    Color result{};
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        auto byte = ta_float_color_byte(std::bit_cast<std::uint32_t>(face[i]));
        // The native lighting cache already stores bytes. Quantizing byte/255
        // through the float table again would turn e.g. 128 into 127.
        if (i < 3u)
            byte = byte * ((packed_intensity >> ((2u-i)*8u)) & 255u) / 256u;
        result[i] = static_cast<float>(byte) / 255.0f;
    }
    return result;
}

inline Color lit_vertex_color(katana::runtime::CpuState& fpu,
                              const Color& face, const Color& light) {
    Color result{};
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        auto bits = std::bit_cast<std::uint32_t>(face[i]);
        if (i < 3u) {
            fpu.fr[0u] = bits;
            fpu.fr[1u] = std::bit_cast<std::uint32_t>(light[i]);
            katana::runtime::fpu_binary(fpu,
                katana::runtime::FpuBinaryOperation::Multiply, 1u, 0u);
            bits = fpu.fr[0u];
        }
        result[i] = static_cast<float>(ta_float_color_byte(bits)) / 255.0f;
    }
    return result;
}
} // namespace sonic::color
