#pragma once
namespace sonic::camera {
enum class Style { Original, Recompiled };
inline constexpr const char* name(Style value) noexcept {
    return value == Style::Recompiled ? "recompiled" : "original";
}
}
