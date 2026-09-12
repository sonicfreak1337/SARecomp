#include "sonic_sony_input.hpp"
#include "sonic_sony_sdl_api.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
using namespace sonic::sony;
struct Pad {
    Uint16 vendor=0x054c, product=0x0ce6;
    bool connected=true, capable=true, closed=false;
    std::string path;
    std::array<Sint16,SDL_GAMEPAD_AXIS_COUNT> axes{};
    std::array<bool,SDL_GAMEPAD_BUTTON_COUNT> buttons{};
};
std::map<SDL_JoystickID,Pad> pads;
std::map<std::string,std::string> hints;
std::vector<std::tuple<Pad*,Uint16,Uint16,Uint32>> effects;
int opened=0, closed=0, updates=0, quits=0;
bool initialized=false, existing_owner=false, fail_effect=false, ready=false;
void check(bool result,const char* message) { if(!result) throw std::runtime_error(message); }
Pad& pad(SDL_Gamepad* value) {
    auto& result=*reinterpret_cast<Pad*>(value);
    check(initialized && !result.closed,"SDL handle used outside lifetime");
    return result;
}
detail::SdlApi fake_api() {
    detail::SdlApi api;
    api.SDL_GetVersion=[] { return SDL_VERSION; };
    api.SDL_SetMainReady=[] {ready=true;};
    api.SDL_SetHintWithPriority=[](const char* key,const char* value,SDL_HintPriority priority) {
        check(!initialized && priority==SDL_HINT_OVERRIDE,"filter changed after Init");
        hints[key]=value; return true;
    };
    api.SDL_WasInit=[](SDL_InitFlags) -> SDL_InitFlags {return existing_owner?SDL_INIT_JOYSTICK:0;};
    api.SDL_InitSubSystem=[](SDL_InitFlags flags) {
        check(ready && flags==SDL_INIT_GAMEPAD,"must not initialize video or audio");
        check(hints["SDL_JOYSTICK_HIDAPI"]=="0" && hints["SDL_JOYSTICK_HIDAPI_PS4"]=="1" &&
            hints["SDL_JOYSTICK_HIDAPI_PS5"]=="1","Sony-only HID filters");
        check(hints["SDL_XINPUT_ENABLED"]=="0" && hints["SDL_JOYSTICK_DIRECTINPUT"]=="0" &&
            hints["SDL_JOYSTICK_RAWINPUT"]=="0" && hints["SDL_JOYSTICK_WGI"]=="0","native backend ownership");
        check(hints["SDL_JOYSTICK_ENHANCED_REPORTS"]=="auto" &&
            hints["SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT"].find("0x054c/0x0ce6")!=std::string::npos,"admission before reports");
        initialized=true; return true;
    };
    api.SDL_QuitSubSystem=[](SDL_InitFlags flags) {check(flags==SDL_INIT_GAMEPAD,"owned subsystem only");++quits;initialized=false;};
    api.SDL_GetGamepads=[](int* count) {
        *count=0;
        auto* ids=static_cast<SDL_JoystickID*>(std::malloc((pads.size()+1)*sizeof(SDL_JoystickID)));
        for(const auto& [id,state]:pads) if(state.connected) ids[(*count)++]=id;
        ids[*count]=0; return ids;
    };
    api.SDL_free=[](void* value) {std::free(value);};
    api.SDL_OpenGamepad=[](SDL_JoystickID id) {++opened;pads.at(id).closed=false;return reinterpret_cast<SDL_Gamepad*>(&pads.at(id));};
    api.SDL_CloseGamepad=[](SDL_Gamepad* value) {pad(value).closed=true;++closed;};
    api.SDL_GetGamepadPath=[](SDL_Gamepad* value) {return pad(value).path.c_str();};
    api.SDL_GetGamepadVendorForID=[](SDL_JoystickID id) {return pads.at(id).vendor;};
    api.SDL_GetGamepadProductForID=[](SDL_JoystickID id) {return pads.at(id).product;};
    api.SDL_GetGamepadAxis=[](SDL_Gamepad* value,SDL_GamepadAxis axis) {return pad(value).axes.at(axis);};
    api.SDL_GetGamepadButton=[](SDL_Gamepad* value,SDL_GamepadButton button) {return pad(value).buttons.at(button);};
    api.SDL_GamepadConnected=[](SDL_Gamepad* value) {return pad(value).connected;};
    api.SDL_GetGamepadProperties=[](SDL_Gamepad* value) -> SDL_PropertiesID {return pad(value).capable?1:2;};
    api.SDL_GetBooleanProperty=[](SDL_PropertiesID props,const char*,bool) {return props==1;};
    api.SDL_UpdateGamepads=[] {check(initialized,"update before Init");++updates;};
    api.SDL_SetGamepadEventsEnabled=[](bool enabled) {check(!enabled,"unused gamepad event queue");};
    api.SDL_SetJoystickEventsEnabled=[](bool enabled) {check(!enabled,"unused joystick event queue");};
    api.SDL_RumbleGamepad=[](SDL_Gamepad* value,Uint16 low,Uint16 high,Uint32 duration) {
        effects.emplace_back(&pad(value),low,high,duration);
        if(fail_effect) {fail_effect=false;return false;}
        return true;
    };
    return api;
}
}
int main() {
    try {
        _putenv_s("KATANA_PORT_BACKGROUND_TEST","1");
        {Backend real;check(!real.initialize(),"hidden run opened real SDL/HID");}
        auto api=fake_api();
        existing_owner=true;
        {Backend other(&api);check(!other.initialize(),"changed another SDL owner's policy");}
        check(hints.empty(),"touched existing owner hints");existing_owner=false;
        pads[1].path="\\\\?\\HID#VID_054C&PID_0CE6#one";
        pads[2].path="\\\\?\\hid#vid_054c&pid_0ce6#two";
        pads[3].vendor=0x045e;pads[3].path="xbox";
        Backend backend(&api);check(backend.initialize(),"fake backend Init");backend.update();
        check(opened==2 && backend.samples().size()==2,"must retain two physical Sonys, exclude Xbox");
        const auto first=backend.samples()[0].identity, second=backend.samples()[1].identity;
        check(first!=second,"same VID/PID collapsed identities");
        auto state=backend.samples()[0].state;
        check(state.connected && state.right_stick_y_raw==0 && state.left_trigger_raw==0 && state.right_trigger_raw==0,"neutral drift");
        pads[1].axes={-32768,32767,16384,-32768,16384,32767};
        pads[1].buttons[SDL_GAMEPAD_BUTTON_SOUTH]=true;pads[1].buttons[SDL_GAMEPAD_BUTTON_START]=true;
        backend.update();state=backend.samples()[0].state;
        check(state.left_stick_x_raw==-32768 && state.left_stick_y_raw==-32767 && state.right_stick_x_raw==16384 && state.right_stick_y_raw==32767,"stick coordinate contract");
        check(state.left_trigger_raw==128 && state.right_trigger_raw==255,"independent analog triggers");
        check(state.buttons==(std::uint32_t(katana::runtime::NativePortGamepadButton::A)|std::uint32_t(katana::runtime::NativePortGamepadButton::Menu)),"physical face/menu layout");
        const int old_endpoint=backend.endpoint(first);
        check(old_endpoint>=4 && backend.rumble(old_endpoint,12345,23456),"Sony endpoint unavailable");
        check(std::get<1>(effects.back())==12345 && std::get<2>(effects.back())==23456 && std::get<3>(effects.back())==65000,"double gain or missing fallback timeout");
        // Enhanced-report transition changes no SDL identity or axis contract.
        pads[1].axes[SDL_GAMEPAD_AXIS_RIGHTY]=1234;backend.update();
        check(backend.samples()[0].identity==first && backend.samples()[0].state.right_stick_y_raw==-1234,"lost input after effect/report transition");
        pads[1].connected=false;
        pads[4].path="\\\\?\\hid#vid_054c&pid_0ce6#ONE";pads[4].product=0x0ba0;
        backend.update();
        check(closed==1 && std::get<1>(effects.back())==0 && std::get<2>(effects.back())==0,"disconnect failed to stop old endpoint");
        const int new_endpoint=backend.endpoint(first);
        check(new_endpoint>old_endpoint && !backend.rumble(old_endpoint,5000,5000),"stale callback reached reconnected device");
        check(backend.samples().size()==2 && backend.samples()[0].identity==second,"unrelated connected device lost identity");
        check(backend.rumble(new_endpoint,20000,20000),"reconnected rumble");
        fail_effect=true;check(!backend.rumble(new_endpoint,0,0),"simulated output failure");
        const auto before_close=effects.size();backend.shutdown();
        check(effects.size()==before_close+1 && std::get<1>(effects.back())==0,"failed stop must be retried before handle close");
        check(closed==opened && quits==1 && !backend.active(),"shutdown leaked handle/subsystem");
        check(!backend.rumble(new_endpoint,100,100),"output after shutdown");
        std::cout<<"SONIC_SONY_INPUT_TESTS PASS transport=fake physical_access=0 analog=1 hotplug=1 lifecycle=1\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
