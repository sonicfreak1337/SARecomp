#include "sonic_presentation.hpp"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sonic::presentation {
namespace {
Settings current;
std::string_view trim(std::string_view value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == value.npos) return {};
    return value.substr(begin, value.find_last_not_of(" \t\r\n") - begin + 1);
}
unsigned number(std::string_view value) {
    unsigned n = 0;
    const auto result = std::from_chars(value.data(), value.data()+value.size(), n);
    if (result.ec != std::errc{} || result.ptr != value.data()+value.size())
        throw std::runtime_error("Invalid Sonic display setting");
    return n;
}
}
const Settings& settings() noexcept { return current; }
void initialize(const std::filesystem::path& executable) {
    auto path = executable.parent_path() / "sonic-display.ini";
    if (const char* override_path = std::getenv("SARECOMP_DISPLAY_CONFIG");
        override_path && *override_path) path = override_path;
    std::ifstream input(path);
    if (!input && std::filesystem::exists(path))
        throw std::runtime_error("Cannot read Sonic display settings");
    Settings selected;
    for (std::string row; std::getline(input, row);) {
        const auto line = trim(row);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        const auto equal = line.find('=');
        if (equal == line.npos) throw std::runtime_error("Invalid Sonic display setting");
        const auto key = trim(line.substr(0,equal));
        const auto value = trim(line.substr(equal+1));
        if (key == "mode") {
            if (value != "original" && value != "widescreen")
                throw std::runtime_error("Sonic display mode must be original or widescreen");
            selected.widescreen = value == "widescreen";
        } else if (key == "width") selected.width = number(value);
        else if (key == "height") selected.height = number(value);
        else if (key == "render_percent") selected.render_percent = number(value);
        else throw std::runtime_error("Unknown Sonic display setting");
    }
    if (selected.width < 640 || selected.width > 7680 ||
        selected.height < 480 || selected.height > 4320 ||
        selected.render_percent < 25 || selected.render_percent > 100 ||
        (selected.widescreen && selected.width * 3 < selected.height * 4))
        throw std::runtime_error("Sonic display settings outside supported range");
    current = selected;
    std::cerr << "SONIC_PRESENTATION mode=" << (current.widescreen ? "hor-plus" : "original")
              << " output=" << current.width << 'x' << current.height
              << " render_percent=" << current.render_percent
              << " x_scale=" << horizontal_scale() << " timing=unchanged\n";
}
void configure(katana::runtime::NativePortGraphicsConfig& config) {
    config.title = "Sonic Adventure: Recompiled [EXPERIMENTAL]";
    config.output_extent = {current.width,current.height};
    // Use an integer multiple of the reduced ratio. Even odd requested sizes
    // cannot silently give render/camera/output three different aspects.
    const auto divisor = std::gcd(current.width,current.height);
    const auto multiple = std::max(1u,(divisor*current.render_percent+50u)/100u);
    config.render_extent = {current.width/divisor*multiple,current.height/divisor*multiple};
    if (current.widescreen) {
        config.ui_viewport.policy = katana::runtime::NativePortViewportPolicy::FullRender;
        // All UI owners compensate projection and anchor groups separately.
        config.explicit_camera_aspect = {current.width,current.height};
        config.camera_aspect_policy = katana::runtime::NativePortCameraAspectPolicy::Explicit;
    }
    // The pinned SDK resizes only its swapchain. Keep the complete layout
    // atomic by applying settings at startup, until port-local live resize.
    config.resizable = false;
}
float horizontal_scale() noexcept {
    return current.widescreen ? (4.0f*current.height)/(3.0f*current.width) : 1.0f;
}
float extra_horizontal_pixels() noexcept {
    return current.widescreen ? (480.0f*current.width/current.height-640.0f)*0.5f : 0.0f;
}
void apply(katana::runtime::NativePortDrawPacket& packet, Role role) noexcept {
    if (!current.widescreen) return;
    if (role == Role::Fullscreen) {
        packet.viewport = katana::runtime::NativePortViewportTarget::Game;
        return;
    }
    if (role == Role::World) packet.viewport = katana::runtime::NativePortViewportTarget::Game;
    const auto scale = horizontal_scale();
    const float anchor = role == Role::HudLeft ? scale-1.0f :
                         role == Role::HudRight ? 1.0f-scale : 0.0f;
    // row-vector X' = scale*X + anchor*W. Apply to both homogeneous
    // world models and authored screen-space sprites without touching Y/Z/W,
    // normals, UVs, mesh identity or any guest matrix.
    for (unsigned row = 0; row < 4; ++row)
        packet.transform.values[row*4] = packet.transform.values[row*4]*scale +
                                        packet.transform.values[row*4+3]*anchor;
}
}
