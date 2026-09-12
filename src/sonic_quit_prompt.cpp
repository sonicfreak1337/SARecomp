#define NOMINMAX
#include <windows.h>
#include "sonic_quit_prompt.hpp"
#include "sonic_presentation.hpp"
#include "sonic_language.hpp"
#include "katana/runtime/runtime.hpp"
#include "katana/runtime/native_port_content.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>

namespace sonic::quit_prompt {
namespace {
using namespace katana::runtime;
std::uint32_t word(CpuState& cpu,std::uint32_t p) {return cpu.memory.read_u32(canonical_physical_address(p));}
bool pointer(std::uint32_t p) noexcept {
    const auto physical=canonical_physical_address(p);
    return (p&3u)==0 && physical>=0x0C010000u && physical<=0x0CFFFF80u;
}
bool environment(const char* key,const char* expected="1") noexcept {
    const auto* value=std::getenv(key);return value && std::string_view(value)==expected;
}
bool background() noexcept {static const bool value=environment("KATANA_PORT_BACKGROUND_TEST");return value;}
int test_mode() noexcept {
    static const int value=background()?(environment("SARECOMP_QUIT_TEST","controller")?1:
        environment("SARECOMP_QUIT_TEST","keyboard")?2:0):0;
    return value;
}
struct State {
    Prompt prompt;
    NativePortTextureHandle texture;
    const void* owner=nullptr;
    std::uint64_t connection=0,frame=0;
    unsigned test_wait=0,test_modal=0,test_cancelled=0;
};
thread_local State state;
bool bound(NativePortContext& context) {
    if(!context.loaded_aot || !context.loaded_aot->validate_bound_entry(0x8C9001A0u))return false;
    const auto entry=context.loaded_aot->active_entry_for_address(0x8C9001A0u);
    return entry && entry->active && entry->lifecycle_generation!=0 &&
        entry->module_sha256=="sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5" &&
        canonical_physical_address(entry->runtime_start)==0x0C900000u &&
        entry->module_size==0xE4648u && entry->source_offset==0x1A0u;
}
Screen screen(NativePortContext& context) noexcept {
    try {
        if(!context.cpu || word(*context.cpu,0x8C7608B0u)!=11u)return Screen::None;
        return read_screen(*context.cpu,bound(context));
    }catch(...){return Screen::None;}
}
bool focused() noexcept {
    if(test_mode())return true;
    if(background())return false;
    DWORD process=0;const auto window=GetForegroundWindow();
    if(window)GetWindowThreadProcessId(window,&process);
    return process==GetCurrentProcessId();
}
Buttons buttons(const NativePortInputSnapshot& input,bool focus) noexcept {
    if(!focus)return {};
    const auto& pad=input.gamepads[0];
    const auto mask=pad.connected?pad.buttons:0u;
    Buttons result{bool(mask&native_port_gamepad_button_mask(NativePortGamepadButton::B)),
                   bool(mask&native_port_gamepad_button_mask(NativePortGamepadButton::A))};
    if(!background()) {
        result.back|=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0;
        // Alt+Enter remains a display command, not confirmation.
        result.accept|=(GetAsyncKeyState(VK_RETURN)&0x8000)!=0 && (GetAsyncKeyState(VK_MENU)&0x8000)==0;
    }
    return result;
}
void neutralize(NativePortInputSnapshot& input) noexcept {
    for(auto& pad:input.gamepads){const bool connected=pad.connected;pad={};pad.connected=connected;}
}
void connection(const NativePortInputSnapshot& input) noexcept {
    if(input.connection_generation!=state.connection){state.connection=input.connection_generation;state.prompt.disarm();}
}
}

Screen read_screen(CpuState& cpu,bool advertise_bound) noexcept {
    try {
        if(!advertise_bound || word(cpu,0x8C7608B0u)!=11u || word(cpu,0x8C960AF4u)!=1u)return Screen::None;
        const auto controller=word(cpu,0x8C960AE8u);
        if(!pointer(controller))return Screen::None;
        const auto work=word(cpu,controller+0x2Cu);
        if(!pointer(work))return Screen::None;
        const auto current=word(cpu,work+4u);
        if((current!=6u && current!=7u) || word(cpu,work+12u)!=current || word(cpu,work+20u)!=0u ||
           cpu.memory.read_u16(canonical_physical_address(work+28u))!=0u ||
           cpu.memory.read_u8(canonical_physical_address(work+30u))!=0u)return Screen::None;
        const auto task=word(cpu,current==6u?0x8C9645D8u:0x8C9645DCu);
        if(!pointer(task) || word(cpu,task+0x10u)!=(current==6u?0x8C909380u:0x8C909A50u))return Screen::None;
        const auto tw=word(cpu,task+0x2Cu);
        if(!pointer(tw) || word(cpu,tw)!=2u)return Screen::None;
        // +36 delays original Start, not title visibility. Waiting for 180
        // can miss the entire ready interval when the BGM requests a demo.
        if(current==6u)return word(cpu,tw+24u)==2u?Screen::PressStart:Screen::None;
        return word(cpu,tw+28u)==1u?Screen::MainMenu:Screen::None;
    }catch(...){return Screen::None;}
}
int effective_language(CpuState& cpu,int preference) noexcept {
    if(preference>=0 && preference<5)return preference;
    try {const auto value=word(cpu,sonic::language::text_global);if(value<5)return int(value);}catch(...){}
    return 1;
}
void sample_input(NativePortContext& context,NativePortInputSnapshot& input,bool suppressed) noexcept {
    if(state.owner!=context.cpu || context.frame_index<state.frame) {
        release(context);state={};state.owner=context.cpu;
    }
    state.frame=context.frame_index;
    const auto current=suppressed?Screen::None:screen(context);
    connection(input);
    const bool focus=focused();
    auto held=buttons(input,focus);
    if(test_mode()) {
        neutralize(input);held={};
        // Owned hidden test input; never synthesizes OS/controller events.
        if(current!=Screen::None && !state.prompt.pending()) {
            ++state.test_wait;
            if(state.test_wait>=20) {
                if(test_mode()==1){input.gamepads[0].connected=true;input.gamepads[0].buttons=native_port_gamepad_button_mask(NativePortGamepadButton::B);held=buttons(input,true);}
                else held.back=true; // Same logical edge as focused Escape.
            }
        }
    }
    const auto decision=state.prompt.sample(current,held,focus);
    if(decision==Decision::Opened)std::cerr<<"SONIC_QUIT opened screen="<<int(current)<<" frame="<<context.frame_index<<'\n';
    if(state.prompt.consumes_input())neutralize(input);
}
bool pending() noexcept {return state.prompt.pending();}
bool visible() noexcept {return state.prompt.visible();}
bool draw(NativePortContext& context) {
    if(!pending())return false;
    // The opening edge allowed one last title draw, never a menu action.
    // An idle/demo or other transition in that frame invalidates the request.
    if(screen(context)!=state.prompt.screen() || !context.graphics){state.prompt.cancel();return false;}
    try {
        const auto lang=effective_language(*context.cpu,presentation::settings().text_language);
        const auto image=rasterize(lang);
        NativePortTextureConfig config;config.extent={image.width,image.height};
        NativePortImageView pixels;pixels.extent=config.extent;pixels.format=NativePortTextureFormat::Rgba8Unorm;
        pixels.stride_bytes=image.width*4;pixels.pixels=image.pixels;
        state.texture=context.graphics->create_texture(config,&pixels);
        const auto viewport=context.graphics->layout().game_viewport;
        const float scale=std::min(float(viewport.height)/720.0f,float(viewport.width)/704.0f);
        const float x=float(image.width)*scale/float(viewport.width),y=float(image.height)*scale/float(viewport.height);
        const std::array<std::uint32_t,6> indices{0,1,2,0,2,3};
        const auto quad=[&](float w,float h,std::array<float,4> color,NativePortTextureHandle texture,unsigned order) {
            std::array<NativePortVertex,4> v{};
            v[0].position={-w,h,0.5f};v[0].texture_coordinate={0,0};
            v[1].position={w,h,0.5f};v[1].texture_coordinate={1,0};
            v[2].position={w,-h,0.5f};v[2].texture_coordinate={1,1};
            v[3].position={-w,-h,0.5f};v[3].texture_coordinate={0,1};
            for(auto& vertex:v)vertex.color=color;
            NativePortDrawPacket packet;packet.vertices=v;packet.indices=indices;
            packet.vertex_space=NativePortVertexSpace::ClipHomogeneous;packet.draw_class=NativePortDrawClass::Overlay;
            packet.batch={0x534151554954ull,order,NativePortDrawBatchClass::GameOverlay};
            packet.depth.test_enabled=packet.depth.write_enabled=false;packet.rasterizer.cull=NativePortCullMode::None;
            packet.blend.enabled=true;packet.blend.source_color=NativePortBlendFactor::SourceAlpha;
            packet.blend.destination_color=NativePortBlendFactor::InverseSourceAlpha;
            packet.blend.destination_alpha=NativePortBlendFactor::InverseSourceAlpha;
            packet.texture=texture;packet.texture_stage=texture?NativePortTextureStage::RequiredResolved:NativePortTextureStage::Disabled;
            context.graphics->draw(packet);
        };
        quad(1,1,{0,0,0,0.60f},{},0);quad(x,y,{1,1,1,1},state.texture,1);
        state.prompt.presented();state.test_modal=0;
        std::cerr<<"SONIC_QUIT visible language="<<lang<<" frame="<<context.frame_index<<'\n';
        return true;
    }catch(const std::exception& error) {
        // This optional host UI must not turn resource exhaustion into a title crash.
        std::cerr<<"SONIC_QUIT unavailable="<<error.what()<<'\n';state.prompt.cancel();return false;
    }
}
Decision poll_modal(NativePortContext& context,bool focus) {
    auto input=context.platform->poll_gamepads();connection(input);
    focus=focus && focused();auto held=buttons(input,focus);
    if(test_mode()) {
        ++state.test_modal;
        held={state.test_modal<12,false}; // Deliberately hold opening B/Esc.
        if(state.test_modal==75)held={state.test_cancelled==0,state.test_cancelled!=0};
        if(test_mode()==1) {
            neutralize(input);input.gamepads[0].connected=true;
            input.gamepads[0].buttons=(held.back?native_port_gamepad_button_mask(NativePortGamepadButton::B):0u)|
                (held.accept?native_port_gamepad_button_mask(NativePortGamepadButton::A):0u);
            held=buttons(input,true);
        }
    }
    const auto result=state.prompt.sample(state.prompt.screen(),held,focus);
    if(result==Decision::Cancelled){++state.test_cancelled;state.test_wait=0;}
    if(result==Decision::Cancelled || result==Decision::Confirmed)
        std::cerr<<"SONIC_QUIT "<<(result==Decision::Cancelled?"cancelled":"confirmed")<<" frame="<<context.frame_index<<'\n';
    return result;
}
void release(NativePortContext& context) noexcept {
    if(state.texture && context.graphics)try{context.graphics->destroy_texture(state.texture);}catch(...){}
    state.texture={};
}
}
