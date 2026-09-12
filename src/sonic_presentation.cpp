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
#include <array>
#define NOMINMAX
#include <windows.h>

namespace sonic::presentation {
namespace {
Settings current;
constexpr std::array text_languages{"japanese","english","french","spanish","german"};
constexpr std::array voice_languages{"japanese","english"};
constexpr std::array window_modes{"windowed","borderless","fullscreen"};
template<std::size_t N> int language(std::string_view value,const std::array<const char*,N>& values) {
    if (value=="game") return -1;
    for (std::size_t i=0;i<N;++i) if (value==values[i]) return int(i);
    throw std::runtime_error("Unsupported language setting");
}
void validate(const Settings& value) {
    if (value.width<640 || value.width>7680 || value.height<480 || value.height>4320 ||
        value.render_percent<25 || value.render_percent>100 ||
        (value.widescreen && value.width*3<value.height*4) ||
        value.presentation_fps<30 || value.presentation_fps>144 ||
        value.text_language < -1 || value.text_language > 4 ||
        value.voice_language < -1 || value.voice_language > 1 ||
        value.subtitles < -1 || value.subtitles > 1 || unsigned(value.window_mode)>2 ||
        unsigned(value.renderer)>1 || unsigned(value.camera_style)>1)
        throw std::runtime_error("Sonic settings outside supported range");
}
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
std::filesystem::path configuration_path(const std::filesystem::path& executable) {
    auto path = executable.parent_path() / "sonic-display.ini";
    if (const char* override_path = std::getenv("SARECOMP_DISPLAY_CONFIG");
        override_path && *override_path) path = override_path;
    return path;
}
Settings read_settings(const std::filesystem::path& path) {
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
        } else if (key == "renderer") {
            if (value != "d3d11" && value != "vulkan")
                throw std::runtime_error("Sonic renderer must be d3d11 or vulkan");
            selected.renderer = value == "vulkan" ? rendering::Renderer::Vulkan : rendering::Renderer::D3D11;
        } else if (key == "width") selected.width = number(value);
        else if (key == "camera_style") {
            if (value != "original" && value != "recompiled")
                throw std::runtime_error("Camera style must be original or recompiled");
            selected.camera_style = value == "recompiled" ? camera::Style::Recompiled : camera::Style::Original;
        }
        else if (key == "height") selected.height = number(value);
        else if (key == "render_percent") selected.render_percent = number(value);
        else if (key == "presentation_fps") selected.presentation_fps = number(value);
        else if (key == "setup_complete") {
            if (value!="0" && value!="1") throw std::runtime_error("Invalid setup flag");
            selected.setup_complete=value=="1";
        } else if (key == "window_mode") {
            auto found=std::find(window_modes.begin(),window_modes.end(),value);
            if (found==window_modes.end()) throw std::runtime_error("Invalid window mode");
            selected.window_mode=rendering::WindowMode(found-window_modes.begin());
        } else if (key == "text_language") selected.text_language=language(value,text_languages);
        else if (key == "voice_language") selected.voice_language=language(value,voice_languages);
        else if (key == "subtitles") {
            if (value!="game" && value!="on" && value!="off") throw std::runtime_error("Invalid subtitles setting");
            selected.subtitles=value=="game"?-1:value=="on"?1:0;
        }
        else throw std::runtime_error("Unknown Sonic display setting");
    }
    validate(selected);
    return selected;
}
void save_settings(const std::filesystem::path& path,const Settings& value) {
    validate(value);
    auto temporary=path; temporary+=L".pending";
    {
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        output<<"# Sonic Adventure: Recompiled configuration. Applied on next launch.\n"
            <<"setup_complete="<<value.setup_complete<<"\nmode="<<(value.widescreen?"widescreen":"original")
            <<"\nwidth="<<value.width<<"\nheight="<<value.height<<"\nrender_percent="<<value.render_percent
            <<"\nrenderer="<<rendering::name(value.renderer)<<"\npresentation_fps="<<value.presentation_fps
            <<"\nwindow_mode="<<window_modes[unsigned(value.window_mode)]
            <<"\ncamera_style="<<camera::name(value.camera_style)
            <<"\ntext_language="<<(value.text_language<0?"game":text_languages[value.text_language])
            <<"\nvoice_language="<<(value.voice_language<0?"game":voice_languages[value.voice_language])
            <<"\nsubtitles="<<(value.subtitles<0?"game":value.subtitles?"on":"off")<<'\n';
        output.flush();
        if (!output) throw std::runtime_error("Sonic configuration could not be written");
    }
    if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Sonic configuration could not be published");
}
void initialize(const std::filesystem::path& executable) {
    current = read_settings(configuration_path(executable));
    rendering::selected_renderer = current.renderer;
    rendering::selected_window_mode = current.window_mode;
    std::cerr << "SONIC_PRESENTATION mode=" << (current.widescreen ? "hor-plus" : "original")
              << " output=" << current.width << 'x' << current.height
              << " renderer=" << rendering::name(current.renderer)
              << " camera=" << camera::name(current.camera_style)
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
