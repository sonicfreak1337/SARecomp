#pragma once
#include "katana/runtime/native_port_graphics.hpp"
#include "renderer/renderer_selection.hpp"
#include <filesystem>

namespace sonic::presentation {
enum class Role { Interface, World, Fullscreen, HudLeft, HudRight };
struct Settings {
    rendering::Renderer renderer = rendering::Renderer::D3D11;
    bool widescreen = false;
    unsigned width = 1920;
    unsigned height = 1080;
    unsigned render_percent = 100;
    unsigned presentation_fps = 144;
    rendering::WindowMode window_mode = rendering::WindowMode::Windowed;
    int text_language = -1;
    int voice_language = -1;
    int subtitles = -1;
    bool setup_complete = false;
};
std::filesystem::path configuration_path(const std::filesystem::path& executable);
Settings read_settings(const std::filesystem::path& path);
void save_settings(const std::filesystem::path& path,const Settings& value);
// Initialized once before the host/worker is created. Never guest state.
void initialize(const std::filesystem::path& executable);
const Settings& settings() noexcept;
void configure(katana::runtime::NativePortGraphicsConfig& config);
float horizontal_scale() noexcept;
float extra_horizontal_pixels() noexcept;
void apply(katana::runtime::NativePortDrawPacket& packet, Role role) noexcept;
}
