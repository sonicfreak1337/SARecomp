#include "sonic_menu.hpp"
#include "sonic_input.hpp"
#include "sonic_presentation.hpp"
#include "katana/runtime/native_port_graphics.hpp"
#include "katana/runtime/native_port_platform.hpp"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace fs=std::filesystem;
using namespace sonic;
using namespace katana::runtime;
using namespace std::chrono_literals;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){
    try{
        require(argc==2,"Fresh isolated test root required");
        const auto root=fs::absolute(argv[1]);require(!fs::exists(root),"Test root exists");
        fs::create_directories(root/"content");
        setenv("KATANA_PORT_BACKGROUND_TEST","1",1);
        setenv("KATANA_USER_DATA_ROOT",(root/"state").c_str(),1);
        setenv("SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS","1",1);
        presentation::Settings settings;settings.setup_complete=true;settings.renderer=rendering::Renderer::Vulkan;
        settings.vsync=2;settings.width=640;settings.height=480;settings.render_percent=100;
        const auto ini=root/"display.ini";setenv("SARECOMP_DISPLAY_CONFIG",ini.c_str(),1);
        presentation::save_settings(ini,settings);presentation::initialize(root/"game");
        NativePortPlatformConfig pc;pc.content_root=root/"content";pc.user_data_root=root/"state";pc.project_id="deck-input-test";
        NativePortPlatformServices platform(pc);
        SDL_VirtualJoystickDesc desc{};SDL_INIT_INTERFACE(&desc);
        desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.naxes=6;desc.nbuttons=15;
        desc.axis_mask=(1u<<6)-1;desc.button_mask=(1u<<15)-1;
        desc.vendor_id=0x28de;desc.product_id=0x11ff;desc.name="Steam Virtual Gamepad";
        auto id=SDL_AttachVirtualJoystick(&desc);require(id!=0,SDL_GetError());
        auto* joystick=SDL_OpenJoystick(id);require(joystick!=nullptr,SDL_GetError());
        NativePortGraphicsConfig gc;gc.output_extent=gc.render_extent={128,128};
        gc.initially_visible=false;gc.synchronize_present=false;gc.maximum_type2_fragment_nodes=131072;
        NativePortDesktopHost host(gc);
        // Only focus is synthetic. All pad values travel through the real SDL
        // virtual device, window/event owner and product platform adapter.
        input::Snapshot focus;focus.focused=true;focus.client_width=focus.client_height=128;input::test_snapshot(&focus);
        auto connected=platform.poll_gamepads();require(connected.gamepads[0].connected,"Steam-format pad was not discovered");
        SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-32768);
        SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-32768);
        std::array<std::byte,4> pixels{std::byte{0},std::byte{0},std::byte{64},std::byte{255}};
        NativePortImageView image;image.extent={1,1};image.format=NativePortTextureFormat::Rgba8Unorm;image.stride_bytes=4;image.pixels=pixels;
        host.graphics().present_image(image,NativePortViewportTarget::Ui,NativePortImageFit::Stretch);
        // Reproduce a static options screen: no new render packets and no
        // test-side SDL_PumpEvents/SDL_UpdateJoysticks to hide a sleeping owner.
        std::this_thread::sleep_for(150ms);
        menu::Model model(settings,1,true);double seconds=0;
        auto sample=[&]{(void)host.poll_lifecycle();return input::sample(input::poll_host(platform),true);};
        auto tick=[&]{seconds+=.02;return model.update(sample(),seconds);};
        for(unsigned i=0;i<4;++i){tick();std::this_thread::sleep_for(20ms);}
        input::set_modal(true);
        auto await=[&](auto predicate,const char* error){
            const auto end=std::chrono::steady_clock::now()+1200ms;
            do{const auto s=sample();if(predicate(s))return s;std::this_thread::sleep_for(10ms);}while(std::chrono::steady_clock::now()<end);
            throw std::runtime_error(error);
        };
        auto button=[&](SDL_GamepadButton button,bool down){
            require(SDL_SetJoystickVirtualButton(joystick,button,down),SDL_GetError());
            const unsigned bit=button==SDL_GAMEPAD_BUTTON_DPAD_DOWN?1:button==SDL_GAMEPAD_BUTTON_SOUTH?10:11;
            await([&](const auto& s){return bool(s.pad&(1u<<bit))==down;},"Static menu did not receive SDL controller events");
            return tick();
        };
        button(SDL_GAMEPAD_BUTTON_DPAD_DOWN,true);require(model.selected()==1,"D-pad did not navigate options");
        button(SDL_GAMEPAD_BUTTON_DPAD_DOWN,false);
        button(SDL_GAMEPAD_BUTTON_SOUTH,true);require(model.rows().front().id=="master_volume","A did not open Audio");
        button(SDL_GAMEPAD_BUTTON_SOUTH,false);
        button(SDL_GAMEPAD_BUTTON_EAST,true);require(model.rows().front().id=="display","B did not return from Audio");
        button(SDL_GAMEPAD_BUTTON_EAST,false);
        // The guest remains frozen while host menus consume physical input.
        SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_START,true);
        auto s=await([](const auto& v){return v.pad&(1u<<4);},"Pause button missing");
        auto native=platform.poll_gamepads();input::transform(native,s,false);
        require(native.gamepads[0].buttons==0,"Modal controller input leaked into gameplay");
        input::set_modal(false);native=platform.poll_gamepads();input::transform(native,s,false);
        require(native.gamepads[0].buttons&(1u<<4),"Pause button lost outside menu");
        SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_START,false);
        SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFTX,32767);
        SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHTY,-32768);
        s=await([](const auto& v){return v.move_x>.99f&&v.look_y>.99f;},"Movement or camera stick missing");
        native=platform.poll_gamepads();input::transform(native,s,false);
        require(native.gamepads[0].left_stick_x>.99f&&native.gamepads[0].right_stick_y>.99f,"Gameplay/camera mapping changed");
        SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFTX,0);
        SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHTY,0);
        SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFTY,32767);
        SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHTX,-32768);
        await([](const auto& v){return v.move_y<-.99f&&v.look_x<-.99f;},"Other stick axes missing");
        constexpr std::array<SDL_GamepadButton,14> buttons{
            SDL_GAMEPAD_BUTTON_DPAD_UP,SDL_GAMEPAD_BUTTON_DPAD_DOWN,SDL_GAMEPAD_BUTTON_DPAD_LEFT,SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
            SDL_GAMEPAD_BUTTON_START,SDL_GAMEPAD_BUTTON_BACK,SDL_GAMEPAD_BUTTON_LEFT_STICK,SDL_GAMEPAD_BUTTON_RIGHT_STICK,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_WEST,SDL_GAMEPAD_BUTTON_NORTH};
        for(unsigned bit=0;bit<buttons.size();++bit){
            SDL_SetJoystickVirtualButton(joystick,buttons[bit],true);
            await([&](const auto& v){return v.pad==(1u<<bit);},"Deck button mapping or release failed");
            SDL_SetJoystickVirtualButton(joystick,buttons[bit],false);
            await([](const auto& v){return v.pad==0;},"Deck button remained held");
        }
        SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,32767);
        SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);
        s=await([](const auto& v){return (v.pad&0xc000)==0xc000;},"Deck triggers missing");
        native=platform.poll_gamepads();input::transform(native,s,false);
        require(native.gamepads[0].left_trigger_raw==255&&native.gamepads[0].right_trigger_raw==255,"Analog triggers lost full range");
        SDL_CloseJoystick(joystick);require(SDL_DetachVirtualJoystick(id),SDL_GetError());
        s=await([](const auto& v){return !v.connected;},"Disconnect not observed while idle");
        require(s.pad==0&&s.move_x==0&&s.look_y==0,"Disconnected state remained held");
        desc.product_id=0x1205;desc.name="Steam Deck";id=SDL_AttachVirtualJoystick(&desc);require(id!=0,SDL_GetError());
        joystick=SDL_OpenJoystick(id);require(joystick!=nullptr,SDL_GetError());
        await([](const auto& v){return v.connected;},"Deck reconnection not discovered");
        SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_SOUTH,true);
        await([](const auto& v){return v.pad&(1u<<10);},"Deck A missing after reconnect");
        SDL_CloseJoystick(joystick);SDL_DetachVirtualJoystick(id);input::test_snapshot(nullptr);
        std::cout<<"SONIC_LINUX_MENU_INPUT_OK static_screen=responsive buttons=14 dpad=1 accept_back=1 pause=1 sticks=2 triggers=2 modal_guest=neutral reconnect=1 event_pump=render_owner\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"SONIC_LINUX_MENU_INPUT_FAIL "<<e.what()<<'\n';return 1;}
}
