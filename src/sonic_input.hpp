#pragma once
#include "sonic_input_bindings.hpp"
#include "katana/runtime/native_port_platform.hpp"
#include <array>
#include <string>
namespace sonic::input {
struct Snapshot {
    std::array<bool,256> keys{};
    std::array<bool,6> mouse{};
    std::uint32_t pad=0;
    float move_x=0,move_y=0,look_x=0,look_y=0;
    float mouse_dx=0,mouse_dy=0;
    int cursor_x=0,cursor_y=0,wheel=0;
    unsigned client_width=0,client_height=0;
    bool focused=false,connected=false,connection_changed=false;
    GlyphStyle glyphs=GlyphStyle::Keyboard;
};
bool held(const Snapshot&,Action,const Bindings&) noexcept;
float axis_deadzone(float value,unsigned percent) noexcept;
// Receives Win32 events on the render/window thread. Never injects OS input.
void window_created(void* window);
bool window_message(void* window,unsigned message,std::uintptr_t word,std::intptr_t data) noexcept;
Snapshot sample(const katana::runtime::NativePortInputSnapshot&,bool modal=false);
bool window_focused() noexcept;
// Host dialogs poll real devices without consuming or recording guest input.
katana::runtime::NativePortInputSnapshot poll_host(katana::runtime::NativePortPlatformServices&);
bool host_poll_active() noexcept;
void transform(katana::runtime::NativePortInputSnapshot&,const Snapshot&,bool suppressed);
void set_modal(bool) noexcept;
void set_camera_active(bool) noexcept;
std::array<float,2> consume_mouse_look() noexcept;
void note_controller(unsigned slot,bool sony,bool connected) noexcept;
GlyphStyle resolved_glyph_style() noexcept;
void set_replay(bool) noexcept;
bool replay() noexcept;
std::wstring binding_name(const Binding&,GlyphStyle);
void test_snapshot(const Snapshot*); // accepted only in explicit hidden tests
}
