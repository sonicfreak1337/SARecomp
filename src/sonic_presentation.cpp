#include "sonic_presentation.hpp"
#include "sonic_configuration_lock.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <array>
#include <atomic>
#include <mutex>
#include <sstream>
#define NOMINMAX
#include <windows.h>

namespace sonic::presentation {
namespace {
Settings current;
std::mutex settings_mutex;
std::atomic<std::uint64_t> settings_revision{1};
constexpr std::array text_languages{"japanese","english","french","spanish","german"};
constexpr std::array voice_languages{"japanese","english"};
constexpr std::array window_modes{"windowed","borderless","fullscreen"};
bool full_width_color_plane(const katana::runtime::NativePortDrawPacket& packet) noexcept {
    using namespace katana::runtime;
    // A screen-space color plane spanning the authored viewport is an
    // overlay, independent of the scene/callback that submitted it. Restrict
    // this to the existing Interface producer contract: world geometry,
    // textured pictures/movies and HUD carriers never enter this inference.
    if(packet.mesh || packet.texture || packet.texture_stage!=NativePortTextureStage::Disabled ||
       !packet.indices.empty() || packet.vertex_space!=NativePortVertexSpace::PvrScreenReciprocal)
        return false;
    const auto count=packet.vertices.size();
    if(!((count==4 && packet.topology==NativePortPrimitiveTopology::TriangleStrip) ||
         (count==6 && packet.topology==NativePortPrimitiveTopology::TriangleList)))return false;
    constexpr std::array screen{2.0f/640,0.0f,0.0f,0.0f,0.0f,-2.0f/480,0.0f,0.0f,
                               0.0f,0.0f,1.0f,0.0f,-1.0f,1.0f,0.0f,1.0f};
    if(packet.transform.values!=screen)return false;
    auto left=packet.vertices[0].position[0],right=left;
    auto top=packet.vertices[0].position[1],bottom=top;
    const auto depth=packet.vertices[0].position[2];
    for(const auto& vertex:packet.vertices){
        const auto& p=vertex.position;
        if(!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]) || p[2]!=depth)
            return false;
        left=std::min(left,p[0]);right=std::max(right,p[0]);
        top=std::min(top,p[1]);bottom=std::max(bottom,p[1]);
    }
    // Includes full-width cinematic bars and vertical wipes. Local panels
    // and partial-width shapes retain their original size and placement.
    if(left>0 || left< -1 || right<640 || right>641 || top< -1 || bottom>481 || bottom<=top)
        return false;
    std::array<unsigned,6> corners{};unsigned all=0;
    for(unsigned i=0;i<count;++i){
        const auto& p=packet.vertices[i].position;
        if((p[0]!=left && p[0]!=right) || (p[1]!=top && p[1]!=bottom))return false;
        corners[i]=(p[0]==right?1u:0u)|(p[1]==bottom?2u:0u);all|=1u<<corners[i];
    }
    if(all!=15)return false;
    const auto first=(1u<<corners[0])|(1u<<corners[1])|(1u<<corners[2]);
    const auto second=count==4?((1u<<corners[1])|(1u<<corners[2])|(1u<<corners[3])):
                              ((1u<<corners[3])|(1u<<corners[4])|(1u<<corners[5]));
    const auto shared=first&second;
    // The triangles must meet at the rectangle's diagonal, not overlap.
    return (shared==9u || shared==6u) && first!=shared && second!=shared;
}
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
    if(value.schema_version!=2 || value.active_profile.empty() || value.active_profile.size()>64 ||
       value.active_profile.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")!=std::string::npos)
        throw std::runtime_error("Unsupported settings version or profile identifier");
#define SONIC_SETTING(name,initial,minimum,maximum) if(value.name<minimum || value.name>maximum) throw std::runtime_error("Invalid setting: " #name);
#include "sonic_settings_fields.inc"
#undef SONIC_SETTING
    for(const auto& binding:value.bindings)
        if(binding.key>255 || binding.alternate>255 || binding.mouse>5 || binding.pad>0xffff)
            throw std::runtime_error("Invalid input binding");
    for(auto action:{input::Action::Confirm,input::Action::Cancel,input::Action::Settings}) {
        const auto& b=value.bindings[unsigned(action)];
        if(!b.key && !b.alternate && !b.mouse && !b.pad)throw std::runtime_error("A menu action cannot be entirely unbound");
    }
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
const Settings& settings() noexcept {
    thread_local Settings cached;
    thread_local std::uint64_t generation=0;
    if(generation!=settings_revision.load(std::memory_order_acquire)) {
        std::lock_guard guard(settings_mutex);cached=current;
        generation=settings_revision.load(std::memory_order_relaxed);
    }
    return cached;
}
std::uint64_t revision() noexcept {return settings_revision.load(std::memory_order_acquire);}
void validate_settings(const Settings& value){validate(value);}
bool needs_restart(const Settings& a,const Settings& b) noexcept {
    return a.width!=b.width || a.height!=b.height || a.render_percent!=b.render_percent ||
        a.renderer!=b.renderer || a.widescreen!=b.widescreen || a.window_mode!=b.window_mode || a.active_profile!=b.active_profile || a.vsync!=b.vsync;
}
void apply_live(const Settings& value) {
    validate(value);std::lock_guard guard(settings_mutex);
    const auto active_vsync=current.vsync;
#define SONIC_SETTING(name,initial,minimum,maximum) current.name=value.name;
#include "sonic_settings_fields.inc"
#undef SONIC_SETTING
    current.vsync=active_vsync;
    current.camera_style=value.camera_style;current.bindings=value.bindings;
    current.text_language=value.text_language;current.voice_language=value.voice_language;current.subtitles=value.subtitles;
    current.presentation_fps=value.presentation_fps;
    settings_revision.fetch_add(1,std::memory_order_release);
}
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
        else if (key == "render_percent") {
            const auto scale=number(value);
            // Migrate the retired experimental supersampling range safely.
            selected.render_percent=scale>100 && scale<=200?100:scale;
        }
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
        else if(key=="schema_version") {if(number(value)>2)throw std::runtime_error("Configuration comes from a newer version");}
        else if(key=="active_profile")selected.active_profile=std::string(value);
        else if(key=="hud_scale" || key=="hud_margin_x" || key=="hud_margin_y" || key=="interpolation") {
            // Retired experimental keys: accept old INIs without enabling them.
            (void)number(value);
        }
#define SONIC_SETTING(name,initial,minimum,maximum) else if(key==#name)selected.name=number(value);
#include "sonic_settings_fields.inc"
#undef SONIC_SETTING
        else if(key.starts_with("bind_")) {
            const auto name=key.substr(5);
            const auto found=std::find(input::action_names.begin(),input::action_names.end(),name);
            if(found==input::action_names.end())throw std::runtime_error("Unknown input action");
            auto fields=std::string(value);std::replace(fields.begin(),fields.end(),',',' ');
            std::istringstream values(fields);input::Binding binding;std::string trailing;
            if(!(values>>binding.key>>binding.alternate>>binding.mouse>>binding.pad) || (values>>trailing))
                throw std::runtime_error("Invalid input binding format");
            selected.bindings[found-input::action_names.begin()]=binding;
        } else throw std::runtime_error("Unknown Sonic display setting");
    }
    validate(selected);
    return selected;
}
void save_settings(const std::filesystem::path& path,const Settings& value) {
    validate(value);
    ConfigurationLock configuration_guard(path);
    auto temporary=path; temporary+=L".pending";
    {
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        output<<"# Sonic Adventure: Recompiled configuration.\nschema_version=2\n"
            <<"setup_complete="<<value.setup_complete<<"\nmode="<<(value.widescreen?"widescreen":"original")
            <<"\nwidth="<<value.width<<"\nheight="<<value.height<<"\nrender_percent="<<value.render_percent
            <<"\nrenderer="<<rendering::name(value.renderer)<<"\npresentation_fps="<<value.presentation_fps
            <<"\nwindow_mode="<<window_modes[unsigned(value.window_mode)]
            <<"\ncamera_style="<<camera::name(value.camera_style)
            <<"\ntext_language="<<(value.text_language<0?"game":text_languages[value.text_language])
            <<"\nvoice_language="<<(value.voice_language<0?"game":voice_languages[value.voice_language])
            <<"\nsubtitles="<<(value.subtitles<0?"game":value.subtitles?"on":"off")
            <<"\nactive_profile="<<value.active_profile<<'\n';
#define SONIC_SETTING(name,initial,minimum,maximum) output<<#name "="<<value.name<<'\n';
#include "sonic_settings_fields.inc"
#undef SONIC_SETTING
        for(unsigned i=0;i<input::action_count;++i) {
            const auto& b=value.bindings[i];output<<"bind_"<<input::action_names[i]<<'='<<b.key<<','<<b.alternate<<','<<b.mouse<<','<<b.pad<<'\n';
        }
        output.flush();
        if (!output) throw std::runtime_error("Sonic configuration could not be written");
    }
    if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Sonic configuration could not be published");
}
void initialize(const std::filesystem::path& executable) {
    {std::lock_guard guard(settings_mutex);current=read_settings(configuration_path(executable));settings_revision.fetch_add(1,std::memory_order_release);}
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
    const auto& current=settings();
    config.title = "Sonic Adventure: Recompiled [EXPERIMENTAL]";
    config.output_extent = {current.width,current.height};
    if(current.vsync)config.synchronize_present=current.vsync==1;
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
    const auto& current=settings();
    return current.widescreen ? (4.0f*current.height)/(3.0f*current.width) : 1.0f;
}
float extra_horizontal_pixels() noexcept {
    const auto& current=settings();
    return current.widescreen ? (480.0f*current.width/current.height-640.0f)*0.5f : 0.0f;
}
void apply(katana::runtime::NativePortDrawPacket& packet, Role role) noexcept {
    const auto& current=settings();
    if(role==Role::World && packet.texture && current.anisotropy>1){
        packet.sampler.filter=katana::runtime::NativePortTextureFilter::Anisotropic;
        packet.sampler.maximum_anisotropy=current.anisotropy;
    }
    if (!current.widescreen) return;
    if(role==Role::Interface && full_width_color_plane(packet))role=Role::Fullscreen;
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
