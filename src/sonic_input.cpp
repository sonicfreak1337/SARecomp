#define NOMINMAX
#include <windows.h>
#include "sonic_input.hpp"
#include "sonic_presentation.hpp"
#include "sonic_audio_device.hpp"
#include "sonic_rumble.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <optional>
namespace sonic::input {
namespace {
std::mutex lock;
Snapshot os;
std::optional<Snapshot> injected;
std::atomic<HWND> window{nullptr};
std::atomic<bool> modal{false},camera_active{false},replaying{false};
std::atomic<unsigned> sony_slots{0};
std::atomic<float> look_dx{0},look_dy{0};
std::uint64_t last_connection=0;
bool had_connection=false;
std::atomic<GlyphStyle> last_device{GlyphStyle::Keyboard};
thread_local unsigned host_poll_depth=0;
bool hidden() {static const bool b=[] {auto v=std::getenv("KATANA_PORT_BACKGROUND_TEST");return v && std::string_view(v)=="1";}();return b;}
float normalize(std::int16_t n){return std::clamp(n/32767.0f,-1.0f,1.0f);}
}
float axis_deadzone(float v,unsigned percent) noexcept {
    if(!std::isfinite(v))return 0;
    const float zone=std::clamp(percent/100.0f,0.0f,0.95f);
    return std::copysign(std::max(0.0f,std::abs(std::clamp(v,-1.0f,1.0f))-zone)/(1-zone),v);
}
bool held(const Snapshot& s,Action action,const Bindings& bindings) noexcept {
    const auto& b=bindings[unsigned(action)];
    return (b.key && s.keys[b.key]) || (b.alternate && s.keys[b.alternate]) ||
        (b.mouse && s.mouse[b.mouse]) || (s.connected && (s.pad&b.pad));
}
void window_created(void* handle) {
    const auto h=static_cast<HWND>(handle);window=h;
    RAWINPUTDEVICE device{1,2,0,h};RegisterRawInputDevices(&device,1,sizeof(device));
    std::lock_guard guard(lock);os={};os.focused=GetForegroundWindow()==h;
    RECT r{};GetClientRect(h,&r);os.client_width=std::max(0L,r.right);os.client_height=std::max(0L,r.bottom);
}
void set_modal(bool value) noexcept {modal.store(value);if(value){camera_active=false;look_dx=0;look_dy=0;}if(const auto h=window.load())PostMessageW(h,WM_APP+91,0,0);}
void set_camera_active(bool value) noexcept {if(camera_active.exchange(value)!=value)if(const auto h=window.load())PostMessageW(h,WM_APP+91,0,0);}
bool window_message(void* handle,unsigned message,std::uintptr_t word,std::intptr_t data) noexcept {
    const auto h=static_cast<HWND>(handle);
    try {
        std::lock_guard guard(lock);
        switch(message) {
        case WM_POWERBROADCAST:
            if(word==PBT_APMSUSPEND)rumble::engine().policy(true,0);
            if(word==PBT_APMRESUMEAUTOMATIC || word==PBT_APMRESUMESUSPEND){
                os.keys={};os.mouse={};os.mouse_dx=os.mouse_dy=0;look_dx=0;look_dy=0;
                audio_device::resumed();
            }
            break;
        case WM_DESTROY:if(window.load()==h){rumble::engine().policy(true,0);window=nullptr;os={};camera_active=false;look_dx=0;look_dy=0;if(!hidden())ClipCursor(nullptr);}break;
        case WM_APP+91: {
            if(!hidden() && os.focused && camera_active.load() && !modal.load() && presentation::settings().mouse_camera) {
                RECT r{};GetClientRect(h,&r);MapWindowPoints(h,nullptr,reinterpret_cast<POINT*>(&r),2);ClipCursor(&r);
            }else if(!hidden())ClipCursor(nullptr);
            break;
        }
        case WM_SETFOCUS:os.focused=true;break;
        case WM_KILLFOCUS:rumble::engine().policy(true,0);os={};camera_active=false;look_dx=0;look_dy=0;if(!hidden())ClipCursor(nullptr);break;
        case WM_KEYDOWN:case WM_SYSKEYDOWN:
            if(word<256){os.keys[word]=true;last_device=GlyphStyle::Keyboard;}
            if(message==WM_KEYDOWN && word>=VK_F1 && word<=VK_F24 && !os.keys[VK_CONTROL]) {
                for(const auto& b:presentation::settings().bindings)if(b.key==word||b.alternate==word)return true;
            }
            break;
        case WM_KEYUP:case WM_SYSKEYUP:if(word<256)os.keys[word]=false;break;
        case WM_MOUSEMOVE:os.cursor_x=short(LOWORD(data));os.cursor_y=short(HIWORD(data));break;
        case WM_LBUTTONDOWN:case WM_LBUTTONUP:os.mouse[1]=message==WM_LBUTTONDOWN;last_device=GlyphStyle::Keyboard;break;
        case WM_RBUTTONDOWN:case WM_RBUTTONUP:os.mouse[2]=message==WM_RBUTTONDOWN;last_device=GlyphStyle::Keyboard;break;
        case WM_MBUTTONDOWN:case WM_MBUTTONUP:os.mouse[3]=message==WM_MBUTTONDOWN;last_device=GlyphStyle::Keyboard;break;
        case WM_XBUTTONDOWN:case WM_XBUTTONUP:os.mouse[HIWORD(word)==XBUTTON1?4:5]=message==WM_XBUTTONDOWN;last_device=GlyphStyle::Keyboard;break;
        case WM_MOUSEWHEEL:os.wheel+=short(HIWORD(word))/WHEEL_DELTA;break;
        case WM_INPUT: {
            RAWINPUT raw{};UINT size=sizeof(raw);
            if(GetRawInputData(reinterpret_cast<HRAWINPUT>(data),RID_INPUT,&raw,&size,sizeof(RAWINPUTHEADER))==UINT(-1))break;
            if(raw.header.dwType==RIM_TYPEMOUSE && !(raw.data.mouse.usFlags&MOUSE_MOVE_ABSOLUTE) && os.focused &&
               camera_active.load() && !modal.load() && presentation::settings().mouse_camera) {
                os.mouse_dx+=float(raw.data.mouse.lLastX);os.mouse_dy+=float(raw.data.mouse.lLastY);
                last_device=GlyphStyle::Keyboard;
            }
            break;
        }
        case WM_SETCURSOR:
            if(LOWORD(data)==HTCLIENT && os.focused && camera_active.load() && !modal.load() &&
               presentation::settings().mouse_camera && !hidden()) {SetCursor(nullptr);return true;}
            break;
        }
        if(h && (message==WM_SIZE || message==WM_SETFOCUS || message==WM_MOUSEMOVE)) {
            RECT r{};GetClientRect(h,&r);os.client_width=std::max(0L,r.right);os.client_height=std::max(0L,r.bottom);
        }
    }catch(...){}
    return false;
}
void note_controller(unsigned slot,bool sony,bool connected) noexcept {
    if(slot>=4)return;const auto bit=1u<<slot;
    if(sony && connected)sony_slots.fetch_or(bit);else sony_slots.fetch_and(~bit);
}
void set_replay(bool enabled) noexcept {replaying=enabled;}
bool replay() noexcept {return replaying.load();}
bool window_focused() noexcept {
    try {std::lock_guard guard(lock);return hidden()?(injected&&injected->focused):os.focused;}
    catch(...){return false;}
}
bool host_poll_active() noexcept {return host_poll_depth!=0;}
katana::runtime::NativePortInputSnapshot poll_host(katana::runtime::NativePortPlatformServices& platform) {
    struct Scope {Scope(){++host_poll_depth;}~Scope(){--host_poll_depth;}} scope;
    return platform.poll_gamepads();
}
Snapshot sample(const katana::runtime::NativePortInputSnapshot& pads,bool) {
    Snapshot result;
    {std::lock_guard guard(lock);result=os;os.mouse_dx=os.mouse_dy=0;os.wheel=0;
     if(hidden()){result={};if(injected)result=*injected;}}
    const auto& p=pads.gamepads[0];const auto& config=presentation::settings();
    result.connected=p.connected;result.pad=p.connected?p.buttons:0;
    if(p.connected){if(p.left_trigger_raw>64)result.pad|=1u<<14;if(p.right_trigger_raw>64)result.pad|=1u<<15;}
    result.connection_changed=had_connection!=p.connected || last_connection!=pads.connection_generation;
    last_connection=pads.connection_generation;had_connection=p.connected;
    const auto lx=config.swap_sticks?p.right_stick_x_raw:p.left_stick_x_raw;
    const auto ly=config.swap_sticks?p.right_stick_y_raw:p.left_stick_y_raw;
    const auto rx=config.swap_sticks?p.left_stick_x_raw:p.right_stick_x_raw;
    const auto ry=config.swap_sticks?p.left_stick_y_raw:p.right_stick_y_raw;
    result.move_x=p.connected?axis_deadzone(normalize(lx),config.movement_deadzone):0;
    result.move_y=p.connected?axis_deadzone(normalize(ly),config.movement_deadzone):0;
    result.look_x=p.connected?axis_deadzone(normalize(rx),config.camera_deadzone):0;
    result.look_y=p.connected?axis_deadzone(normalize(ry),config.camera_deadzone):0;
    if(p.connected && (result.pad || std::abs(result.move_x)>0.1f || std::abs(result.move_y)>0.1f ||
        std::abs(result.look_x)>0.1f || std::abs(result.look_y)>0.1f))
        last_device=(sony_slots.load()&1)?GlyphStyle::PlayStation:GlyphStyle::Xbox;
    result.glyphs=config.glyph_style?GlyphStyle(config.glyph_style):last_device.load();
    if(!result.focused && !hidden()){result.keys={};result.mouse={};result.mouse_dx=result.mouse_dy=0;}
    return result;
}
void transform(katana::runtime::NativePortInputSnapshot& pads,const Snapshot& source,bool suppressed) {
    // Replays already own their sampled input; hidden diagnostic profiles must
    // not accidentally consume the user's physical keyboard or cursor.
    const auto& config=presentation::settings();
    if(replay()) {if(suppressed||modal.load())for(auto& pad:pads.gamepads){const bool connected=pad.connected;pad={};pad.connected=connected;}return;}
    auto logical=source;
    if(logical.keys[VK_MENU])logical.keys[VK_RETURN]=false; // Alt+Enter belongs to the window.
    if(!config.keyboard_enabled || !source.focused){logical.keys={};logical.mouse={};}
    const auto old=pads.gamepads[0];auto& p=pads.gamepads[0];
    p={};p.connected=old.connected || (config.keyboard_enabled && source.focused);p.packet_number=old.packet_number;
    if(suppressed || modal.load()){look_dx=0;look_dy=0;return;}
    const auto down=[&](Action a){return held(logical,a,config.bindings);};
    p.left_stick_x=std::clamp(source.move_x+float(down(Action::Right))-float(down(Action::Left)),-1.0f,1.0f);
    p.left_stick_y=std::clamp(source.move_y+float(down(Action::Up))-float(down(Action::Down)),-1.0f,1.0f);
    p.right_stick_x=std::clamp(source.look_x+float(down(Action::LookRight))-float(down(Action::LookLeft)),-1.0f,1.0f);
    p.right_stick_y=std::clamp(source.look_y+float(down(Action::LookUp))-float(down(Action::LookDown)),-1.0f,1.0f);
    p.left_stick_x_raw=std::int16_t(p.left_stick_x*32767);p.left_stick_y_raw=std::int16_t(p.left_stick_y*32767);
    p.right_stick_x_raw=std::int16_t(p.right_stick_x*32767);p.right_stick_y_raw=std::int16_t(p.right_stick_y*32767);
    for(unsigned i=unsigned(Action::A);i<=unsigned(Action::Start);++i)
        if(down(Action(i)))p.buttons|=i==unsigned(Action::Start)?1u<<4:1u<<(10+i-unsigned(Action::A));
    for(unsigned i=0;i<4;++i)if(down(Action(i)))p.buttons|=1u<<i;
    auto digital=logical;digital.pad&=~((1u<<14)|(1u<<15));
    const auto trigger=[&](Action action,std::uint8_t& raw,float& strength){
        if(held(digital,action,config.bindings)){raw=255;strength=1;}
        // Analog sources follow the physical binding, including cross-maps
        // and combinations. A digital alternative still wins at full strength.
        if(!logical.connected||!old.connected)return;
        const auto binding=config.bindings[unsigned(action)].pad;
        if(binding&(1u<<14)){raw=std::max(raw,old.left_trigger_raw);strength=std::max(strength,old.left_trigger);}
        if(binding&(1u<<15)){raw=std::max(raw,old.right_trigger_raw);strength=std::max(strength,old.right_trigger);}
    };
    trigger(Action::LeftTrigger,p.left_trigger_raw,p.left_trigger);
    trigger(Action::RightTrigger,p.right_trigger_raw,p.right_trigger);
    look_dx.store(source.mouse_dx);look_dy.store(source.mouse_dy);
}
std::array<float,2> consume_mouse_look() noexcept {return {look_dx.exchange(0),look_dy.exchange(0)};}
void test_snapshot(const Snapshot* value){if(!hidden())return;std::lock_guard guard(lock);injected=value?std::optional(*value):std::nullopt;}
std::wstring binding_name(const Binding& b,GlyphStyle style) {
    if(style!=GlyphStyle::Keyboard && b.pad) {
        constexpr std::array<std::wstring_view,16> xbox{L"↑",L"↓",L"←",L"→",L"Menu",L"View",L"LS",L"RS",L"LB",L"RB",L"Ⓐ",L"Ⓑ",L"Ⓧ",L"Ⓨ",L"LT",L"RT"};
        constexpr std::array<std::wstring_view,16> sony{L"↑",L"↓",L"←",L"→",L"Options",L"Share",L"L3",L"R3",L"L1",L"R1",L"×",L"○",L"□",L"△",L"L2",L"R2"};
        std::wstring result;for(unsigned i=0;i<16;++i)if(b.pad&(1u<<i)){if(!result.empty())result+=L" / ";result+=(style==GlyphStyle::PlayStation?sony:xbox)[i];}return result;
    }
    const auto key=b.key?b.key:b.alternate;
    if(key){wchar_t name[64]{};const auto scan=MapVirtualKeyW(key,MAPVK_VK_TO_VSC_EX);const auto extended=(scan&0xff00)?1u<<24:0u;GetKeyNameTextW(LONG((scan&255)<<16|extended),name,64);if(*name)return name;}
    if(b.mouse)return L"Mouse "+std::to_wstring(b.mouse);
    return L"—";
}
GlyphStyle resolved_glyph_style() noexcept {
    const auto preference=presentation::settings().glyph_style;
    return preference?GlyphStyle(preference):last_device.load();
}
}
