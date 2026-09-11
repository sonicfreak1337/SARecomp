#pragma once
#include "katana/runtime/native_port_graphics.hpp"
#include <filesystem>

namespace sonic::presentation {
enum class Role { Interface, World, Fullscreen, HudLeft, HudRight };
struct Settings {
    bool widescreen = false;
    unsigned width = 1920;
    unsigned height = 1080;
    unsigned render_percent = 100;
};
// Initialized once before the host/worker is created. Never guest state.
void initialize(const std::filesystem::path& executable);
const Settings& settings() noexcept;
void configure(katana::runtime::NativePortGraphicsConfig& config);
float horizontal_scale() noexcept;
float extra_horizontal_pixels() noexcept;
void apply(katana::runtime::NativePortDrawPacket& packet, Role role) noexcept;
}
