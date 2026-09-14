#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#else
#include <SDL3/SDL.h>
#endif
#include "sonic_input.hpp"
#include "sonic_presentation.hpp"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
using namespace sonic;
using katana::runtime::NativePortInputSnapshot;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
bool same_float(float a,float b){return std::abs(a-b)<.0001f;}
void test_environment(const char* key,const char* value){
#ifdef _WIN32
    check(_putenv_s(key,value)==0,"test environment");
#else
    check(::setenv(key,value,1)==0,"test environment");
#endif
}
int main(int argc,char** argv){
    try{
        check(argc==2,"fresh test directory required");
        const auto root=fs::absolute(argv[1]);check(!fs::exists(root),"test directory exists");fs::create_directories(root);
        test_environment("KATANA_PORT_BACKGROUND_TEST","1");
        const auto ini=root/"sonic-display.ini";test_environment("SARECOMP_DISPLAY_CONFIG",ini.string().c_str());
        presentation::Settings settings;settings.setup_complete=true;settings.keyboard_enabled=1;
        presentation::save_settings(ini,settings);presentation::initialize(root/"game.exe");
        input::Snapshot keyboard;keyboard.focused=true;input::test_snapshot(&keyboard);
        NativePortInputSnapshot physical{};physical.connection_generation=1;
        auto& raw=physical.gamepads[0];raw.connected=true;
        raw.left_trigger_raw=32;raw.left_trigger=32/255.f;
        raw.right_trigger_raw=176;raw.right_trigger=176/255.f;
        const auto map=[&]{
            auto result=physical;const auto sampled=input::sample(physical);
            input::transform(result,sampled,false);return result.gamepads[0];
        };
        auto mapped=map();
        check(mapped.left_trigger_raw==32&&mapped.right_trigger_raw==176&&same_float(mapped.left_trigger,32/255.f),"default analog trigger strength changed");
        settings.bindings[unsigned(input::Action::LeftTrigger)].pad=1u<<15;
        settings.bindings[unsigned(input::Action::RightTrigger)].pad=1u<<14;
        presentation::apply_live(settings);mapped=map();
        check(mapped.left_trigger_raw==176&&mapped.right_trigger_raw==32&&same_float(mapped.right_trigger,32/255.f),"cross-mapped analog triggers lost strength");
        presentation::save_settings(ini,settings);
        check(presentation::read_settings(ini)==settings,"trigger remap did not survive settings serialization");

        // Digital keys/buttons still provide full-strength trigger alternatives.
        keyboard.keys['Q']=true;input::test_snapshot(&keyboard);mapped=map();
        check(mapped.left_trigger_raw==255&&same_float(mapped.left_trigger,1),"keyboard trigger alternative failed");
        keyboard.keys={};input::test_snapshot(&keyboard);
        settings.bindings[unsigned(input::Action::LeftTrigger)].pad=1u<<12;
        settings.bindings[unsigned(input::Action::RightTrigger)].pad=0;
        settings.bindings[unsigned(input::Action::A)].pad=1u<<15;
        raw.buttons=1u<<12;presentation::apply_live(settings);mapped=map();
        check(mapped.left_trigger_raw==255&&mapped.right_trigger_raw==0&&(mapped.buttons&(1u<<10)),"digital trigger or trigger-to-button remap failed");
        raw.buttons=0;raw.right_trigger_raw=32;raw.right_trigger=32/255.f;mapped=map();
        check(!(mapped.buttons&(1u<<10))&&mapped.left_trigger_raw==0,"trigger digital threshold or unused axis leaked");

        // A disconnected packet must not leak the preceding nonzero axes.
        settings.bindings[unsigned(input::Action::LeftTrigger)].pad=1u<<14;
        settings.bindings[unsigned(input::Action::RightTrigger)].pad=1u<<15;
        presentation::apply_live(settings);raw.connected=false;++physical.connection_generation;
        auto sample=input::sample(physical);check(sample.connection_changed&&!sample.connected,"disconnect edge missing");
        auto output=physical;input::transform(output,sample,false);
        check(output.gamepads[0].left_trigger_raw==0&&output.gamepads[0].right_trigger_raw==0,"disconnected trigger state leaked");
        raw.connected=true;raw.buttons=1u<<10;++physical.connection_generation;
        input::note_controller(0,true,true);sample=input::sample(physical);
        check(sample.connection_changed&&sample.glyphs==input::GlyphStyle::PlayStation,"Sony reconnect identity/glyph missing");
        check(input::binding_name(settings.bindings[unsigned(input::Action::LeftTrigger)],sample.glyphs)==L"L2","Sony remap label differs from mapping");
        input::note_controller(0,false,true);++physical.connection_generation;sample=input::sample(physical);
        check(sample.connection_changed&&sample.glyphs==input::GlyphStyle::Xbox,"Xbox reconnect identity/glyph missing");
        check(input::binding_name(settings.bindings[unsigned(input::Action::RightTrigger)],sample.glyphs)==L"RT","Xbox remap label differs from mapping");
        settings.movement_deadzone=25;presentation::apply_live(settings);
        raw.left_stick_x_raw=4096;sample=input::sample(physical);check(same_float(sample.move_x,0),"movement deadzone ignored after reconnect");
        raw.left_stick_x_raw=32767;sample=input::sample(physical);check(same_float(sample.move_x,1),"movement deadzone lost full range");
        check(input::binding_name(input::Binding{0,'Q',0,0},input::GlyphStyle::Keyboard)==input::binding_name(input::Binding{'Q',0,0,0},input::GlyphStyle::Keyboard),"alternate-only key shown as unbound");
        raw={};raw.connected=true;
#ifdef _WIN32
        (void)input::window_message(nullptr,WM_XBUTTONDOWN,std::uintptr_t(XBUTTON1)<<16,0);
#else
        test_environment("SDL_VIDEODRIVER","dummy");check(SDL_InitSubSystem(SDL_INIT_VIDEO),SDL_GetError());
        auto* window=SDL_CreateWindow("Isolated input test",64,64,SDL_WINDOW_HIDDEN);check(window!=nullptr,SDL_GetError());
        input::window_created(window);SDL_Event event{};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_X1;
        input::window_event(window,event);
#endif
        sample=input::sample(physical);check(sample.glyphs==input::GlyphStyle::Keyboard,"mouse side button did not update prompt family");
#ifdef _WIN32
        (void)input::window_message(nullptr,WM_XBUTTONUP,std::uintptr_t(XBUTTON1)<<16,0);
#else
        event.type=SDL_EVENT_MOUSE_BUTTON_UP;input::window_event(window,event);input::window_created(nullptr);
        SDL_DestroyWindow(window);SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif
        input::test_snapshot(nullptr);
        std::cout<<"SONIC_INPUT_MAPPING_OK trigger_swap=analog digital_alternatives=1 disconnect=neutral reconnect_glyphs=sony,xbox deadzone=retained settings=persisted\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"SONIC_INPUT_MAPPING_FAIL "<<error.what()<<'\n';return 1;}
}
