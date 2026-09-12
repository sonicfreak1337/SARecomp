#include "sonic_camera.hpp"
#include "sonic_camera_orbit.hpp"
#include "sonic_camera_policy.hpp"
#include "sonic_presentation.hpp"
#include "katana/runtime/runtime.hpp"
#include <array>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace sonic::camera {
namespace {
using namespace katana::runtime;
constexpr std::uint32_t game_state=0x8C7492F4u, game_request=0x8C19DD64u;
constexpr std::uint32_t pause_enabled=0x8C19DD66u, pause_blocked=0x8C7491ECu;
constexpr std::uint32_t camera_current=0x8C18B72Cu, camera_control=0x8C111F88u;
constexpr std::uint32_t player_one=0x8C78C548u, special_view=0x8C6B6FACu;
constexpr std::uint32_t stage_major=0x8C7492FAu, stage_minor=0x8C7492FCu;
constexpr std::uint32_t character=0x8C78B39Du;
struct State {
    Orbit orbit;
    const void* owner=nullptr;
    Stick input;
    std::uint64_t input_frame=~std::uint64_t{0}, frame=~std::uint64_t{0}, time=0;
    std::uint64_t first_test_time=0;
    std::uint32_t player=0, camera=0, scene=0, actor=0;
    Vec3 previous_player;
    unsigned log_count=0;
    int last_gate=-1;
};
thread_local State state;
bool env(const char* name) noexcept {
    const auto* value=std::getenv(name);
    return value && std::string_view(value)=="1";
}
bool testing() noexcept {
    static const bool value=env("SARECOMP_CAMERA_TEST") && env("KATANA_PORT_BACKGROUND_TEST");
    return value;
}
bool tracing() noexcept {
    static const bool value=testing() || env("SARECOMP_CAMERA_TRACE");
    return value;
}
bool pointer(std::uint32_t value) noexcept {
    const auto physical=canonical_physical_address(value);
    return (value&3u)==0 && physical>=0x0C010000u && physical<=0x0CFFFF80u;
}
std::uint32_t word(CpuState& cpu,std::uint32_t address) {return cpu.memory.read_u32(canonical_physical_address(address));}
std::uint16_t half(CpuState& cpu,std::uint32_t address) {return cpu.memory.read_u16(canonical_physical_address(address));}
std::uint8_t byte(CpuState& cpu,std::uint32_t address) {return cpu.memory.read_u8(canonical_physical_address(address));}
Vec3 position(CpuState& cpu,std::uint32_t task) {
    return {std::bit_cast<float>(word(cpu,task+0x20u)),std::bit_cast<float>(word(cpu,task+0x24u)),std::bit_cast<float>(word(cpu,task+0x28u))};
}
NativePortHookResult suspend(NativePortContext& context,int reason) noexcept {
    state.orbit.reset();
    state.time=0;
    if (tracing() && state.last_gate!=reason && state.log_count<720) {
        ++state.log_count;
        std::cerr<<"SONIC_CAMERA suspended="<<reason<<" frame="<<context.frame_index<<'\n';
    }
    state.last_gate=reason;
    return {};
}
}
void reset_timeline() noexcept { state={}; }
void suppress_input() noexcept { state.input={}; }
void sample_input(NativePortContext& context,const NativePortInputSnapshot& input,bool suppressed) noexcept {
    if (presentation::settings().camera_style!=Style::Recompiled) return;
    if (state.owner!=context.title_state ||
        (state.input_frame!=~std::uint64_t{0} && context.frame_index<state.input_frame)) {
        state={}; state.owner=context.title_state;
    }
    const auto& pad=input.gamepads[0];
    state.input=suppressed?Stick{}:right_stick(pad.right_stick_x_raw,pad.right_stick_y_raw,pad.connected);
    state.input_frame=context.frame_index;
    if (testing()) {
        const auto now=context.host?context.host->monotonic_time_nanoseconds():0;
        const double seconds=!suppressed && state.first_test_time && now>=state.first_test_time?
            double(now-state.first_test_time)/1e9:0;
        // Bounded hidden-test input at the exact right-stick normalization path.
        // No OS/controller input is injected and no movement/button is replaced.
        std::int16_t x=0,y=0;
        if (seconds>=1 && seconds<5.2) x=32767;
        else if (seconds>=5.2 && seconds<7.2) y=32767;
        else if (seconds>=7.2 && seconds<9.7) y=-32767;
        state.input=right_stick(x,y,true);
    }
}

NativePortHookResult publish(NativePortContext& context) noexcept {
    // Original is a true passthrough, including zero guest reads/writes.
    if (presentation::settings().camera_style!=Style::Recompiled) return {};
    if (!context.cpu || !context.host) return {};
    auto& cpu=*context.cpu;
    // Only the state-2 camera task's normal publication. Other callers are
    // not evidence of gameplay and must keep their scripted view untouched.
    if (cpu.pr!=0x8C01991Cu) return suspend(context,1);
    try {
        const auto mode=half(cpu,game_state);
        if (mode!=15u) return suspend(context,100+mode); // 16 is retail pause.
        if (half(cpu,game_request)!=0u) return suspend(context,2);
        if (half(cpu,pause_enabled)!=1u || word(cpu,pause_blocked)!=0u) return suspend(context,3);
        if (word(cpu,special_view)!=0u) return suspend(context,4);
        const auto control=word(cpu,camera_control),cam=cpu.r[4],player=word(cpu,player_one);
        if (!pointer(control)||!pointer(cam)||!pointer(player)||cam==player||
            word(cpu,camera_current)!=cam||byte(cpu,cam)!=2u) return suspend(context,5);
        const auto level=byte(cpu,control+8u);
        // 0 = normal, 1 = spatial gameplay camera areas (01995E/01A7D6).
        // Explicit/event camera registrars select 2, 4 or 5. They retain
        // complete ownership, including scripted sequences inside stages.
        if (level>1u) return suspend(context,200+level);
        const auto type=byte(cpu,control+6u); // +7 is the independent output format.
        const auto callback=word(cpu,control+12u);
        if (!manual_camera_type(type,callback)) return suspend(context,1000+type);
        const auto player_position=position(cpu,player),original_eye=position(cpu,cam);
        const auto actor=byte(cpu,character);
        const auto scene=std::uint32_t(half(cpu,stage_major))<<16u | half(cpu,stage_minor);
        if (!finite(player_position)||!finite(original_eye)) return suspend(context,6);
        const auto target=player_position+Vec3{0,actor==6u || actor==7u?11.0f:7.0f,0};
        const auto now=context.host->monotonic_time_nanoseconds();
        const bool reset=!state.orbit.active()||state.owner!=context.title_state||
            state.player!=player||state.camera!=cam||state.scene!=scene||state.actor!=actor||
            context.frame_index<state.frame||now<state.time||length(player_position-state.previous_player)>200;
        if (reset && !state.orbit.enter(original_eye,target)) return suspend(context,7);
        // No second integration if a display path republishes the same frame.
        const bool new_frame=reset||context.frame_index!=state.frame;
        const float dt=reset||!new_frame||!state.time?0.0f:
            std::min(0.25f,float(double(now-state.time)/1e9));
        const auto input=state.input_frame==context.frame_index?state.input:Stick{};
        const auto pose=state.orbit.update(target,input,dt);
        if (!finite(pose.eye)) return suspend(context,8);
        // One contiguous write: pitch, yaw, original roll, XYZ. All other
        // camera/task data and every CPU register/FPU flag remain unchanged.
        const std::array values{std::bit_cast<std::uint32_t>(angle(pose.pitch)),
            std::bit_cast<std::uint32_t>(angle(pose.yaw)),word(cpu,cam+0x1Cu),
            std::bit_cast<std::uint32_t>(pose.eye.x),std::bit_cast<std::uint32_t>(pose.eye.y),std::bit_cast<std::uint32_t>(pose.eye.z)};
        const auto bytes=std::as_bytes(std::span(values));
        cpu.memory.write_bytes(canonical_physical_address(cam+0x14u),
            std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()),bytes.size()),CodeWriteSource::Copy);
        state.owner=context.title_state; state.frame=context.frame_index;
        // Duplicate publishers must not advance the integration clock: that
        // would discard most of the interval before the next actual frame.
        if (new_frame) state.time=now;
        state.player=player;state.camera=cam;state.scene=scene;state.actor=actor;state.previous_player=player_position;
        if (testing() && !state.first_test_time) state.first_test_time=now;
        if (tracing() && new_frame && state.log_count<720) {
            ++state.log_count;
            std::cerr<<"SONIC_CAMERA active=1 frame="<<context.frame_index<<" ns="<<now
                <<" reset="<<reset<<" state="<<mode<<" level="<<unsigned(level)<<" type="<<unsigned(type)<<" scene="<<scene
                <<" dt="<<dt<<" yaw="<<pose.yaw<<" pitch="<<pose.pitch<<" radius="<<state.orbit.radius()
                <<" stick="<<input.x<<','<<input.y<<" eye="<<pose.eye.x<<','<<pose.eye.y<<','<<pose.eye.z
                <<" target="<<target.x<<','<<target.y<<','<<target.z<<'\n';
        }
        state.last_gate=0;
        return {}; // Original 01A100 now builds/publishes the consistent view.
    } catch (...) {
        return suspend(context,9); // Experimental camera never terminates a story.
    }
}
}
extern "C" katana::runtime::NativePortHookResult sonic_recompiled_camera_publish(
    katana::runtime::NativePortContext& context) noexcept {
    return sonic::camera::publish(context);
}
