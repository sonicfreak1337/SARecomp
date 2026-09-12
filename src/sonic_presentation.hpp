#pragma once
#include "katana/runtime/native_port_graphics.hpp"
#include "renderer/renderer_selection.hpp"
#include "sonic_camera_style.hpp"
#include "sonic_input_bindings.hpp"
#include <filesystem>
#include <string>

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
    camera::Style camera_style = camera::Style::Original;
    int text_language = -1;
    int voice_language = -1;
    int subtitles = -1;
    bool setup_complete = false;
    unsigned schema_version = 2;
    std::string active_profile = "default";
    input::Bindings bindings=input::default_bindings;
    // Render-interpolation prototype retired on 2026-09-12. Keep its code
    // available for reference, but no setting or environment can enable it.
    static constexpr unsigned interpolation = 0;
#define SONIC_SETTING(name,initial,minimum,maximum) unsigned name=initial;
#include "sonic_settings_fields.inc"
#undef SONIC_SETTING
    bool operator==(const Settings&)const=default;
};
std::filesystem::path configuration_path(const std::filesystem::path& executable);
Settings read_settings(const std::filesystem::path& path);
void save_settings(const std::filesystem::path& path,const Settings& value);
// Initialized once before the host/worker is created. Never guest state.
void initialize(const std::filesystem::path& executable);
const Settings& settings() noexcept;
std::uint64_t revision() noexcept;
void validate_settings(const Settings&);
bool needs_restart(const Settings&,const Settings&) noexcept;
// Publish only live fields. Display/profile choices remain staged until restart.
void apply_live(const Settings&);
void configure(katana::runtime::NativePortGraphicsConfig& config);
float horizontal_scale() noexcept;
float extra_horizontal_pixels() noexcept;
void apply(katana::runtime::NativePortDrawPacket& packet, Role role) noexcept;
}
