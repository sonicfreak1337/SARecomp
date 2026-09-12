#define NOMINMAX
#include <windows.h>
#include "sonic_quit_prompt.hpp"
#include "sonic_language.hpp"
#include "katana/runtime/runtime.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace sonic::quit_prompt;
using namespace katana::runtime;
namespace {
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void bitmap(const std::filesystem::path& path,Image image) {
    for(std::size_t i=0;i<image.pixels.size();i+=4)std::swap(image.pixels[i],image.pixels[i+2]);
    BITMAPFILEHEADER file{};file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);
    file.bfSize=DWORD(file.bfOffBits+image.pixels.size());
    BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=LONG(image.width);info.biHeight=-LONG(image.height);
    info.biPlanes=1;info.biBitCount=32;
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&file),sizeof(file));
    out.write(reinterpret_cast<const char*>(&info),sizeof(info));out.write(reinterpret_cast<const char*>(image.pixels.data()),image.pixels.size());
    require(bool(out),"image write");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"provide an isolated output directory");
        const auto directory=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(directory);
        Prompt p;
        require(p.sample(Screen::PressStart,{true,false})==Decision::None,"held B while entering title opens prompt");
        p.sample(Screen::PressStart,{});
        require(p.sample(Screen::PressStart,{true,false})==Decision::Opened && p.consumes_input(),"opening edge not consumed");
        p.presented();
        for(int i=0;i<60;++i)require(p.sample(Screen::PressStart,{true,false})==Decision::None && p.visible(),"held opening B cancels");
        p.sample(Screen::PressStart,{});
        require(p.sample(Screen::PressStart,{false,true})==Decision::Confirmed && p.consumes_input(),"A confirmation");
        require(p.sample(Screen::PressStart,{false,true})==Decision::None && p.consumes_input(),"held closing A leaks");
        p.sample(Screen::PressStart,{});require(!p.consumes_input(),"release never returns control");
        p.sample(Screen::PressStart,{true,false});p.presented();p.sample(Screen::PressStart,{});
        require(p.sample(Screen::PressStart,{true,true})==Decision::Cancelled,"cancel must win simultaneous confirm/cancel");
        p.sample(Screen::MainMenu,{});p.sample(Screen::MainMenu,{true,false});p.presented();
        p.sample(Screen::MainMenu,{},false);
        require(p.sample(Screen::MainMenu,{false,true})==Decision::None && p.visible(),"focus return with held A confirms");
        p.sample(Screen::MainMenu,{});
        p.disarm();require(p.sample(Screen::MainMenu,{false,true})==Decision::None,"reconnected held A confirms");
        p.sample(Screen::MainMenu,{});
        require(p.sample(Screen::MainMenu,{true,false})==Decision::Cancelled,"B/Escape cancellation");
        p.sample(Screen::MainMenu,{});p.sample(Screen::MainMenu,{true,false});
        p.sample(Screen::None,{});require(!p.pending() && !p.visible() && !p.consumes_input(),"transition keeps modal armed");
        p.sample(Screen::PressStart,{});require(p.sample(Screen::PressStart,{false,true})==Decision::None,"A at title tries quitting");

        auto remapped=sonic::input::default_bindings;
        remapped[unsigned(sonic::input::Action::Confirm)]={'Z',0,1,1u<<13};
        remapped[unsigned(sonic::input::Action::Cancel)]={'C',0,2,1u<<12};
        Controls controls;sonic::input::Snapshot physical;physical.focused=true;physical.connected=true;
        physical.pad=1u<<10;require(!controls.sample(physical,remapped,true).accept,"old A accepted after remapping");
        physical.pad=1u<<13;require(controls.sample(physical,remapped,true).accept,"physical Y did not confirm");
        physical.pad=1u<<12;require(controls.sample(physical,remapped,true).back,"physical X did not cancel");
        physical.pad=0;physical.keys['Z']=true;require(controls.sample(physical,remapped,true).accept,"remapped keyboard confirm");
        physical.keys={};physical.keys[VK_MENU]=physical.keys[VK_RETURN]=true;
        require(!controls.sample(physical,remapped,true).accept,"Alt+Enter confirmed quit");
        physical.keys={};physical.keys[VK_ESCAPE]=true;require(controls.sample(physical,remapped,true).back,"Escape fallback missing");physical.keys={};
        const ButtonRect confirm_box{20,30,100,80},cancel_box{120,30,200,80};
        physical.cursor_x=40;physical.cursor_y=40;physical.mouse[1]=true;
        require(!controls.sample(physical,remapped,true,confirm_box,cancel_box).accept,"mouse-down confirmed quit");
        physical.mouse[1]=false;physical.cursor_x=300;
        require(!controls.sample(physical,remapped,true,confirm_box,cancel_box).accept,"drag-out confirmed quit");
        physical.cursor_x=40;physical.mouse[1]=true;controls.sample(physical,remapped,true,confirm_box,cancel_box);
        physical.mouse[1]=false;require(controls.sample(physical,remapped,true,confirm_box,cancel_box).accept,"release-inside failed");
        physical.cursor_x=140;physical.mouse[1]=true;controls.sample(physical,remapped,true,confirm_box,cancel_box);
        physical.focused=false;controls.sample(physical,remapped,true,confirm_box,cancel_box);
        physical.focused=true;physical.mouse[1]=false;
        require(!controls.sample(physical,remapped,true,confirm_box,cancel_box).back,"focus loss retained pending click");
        physical.pad=1u<<13;physical.connection_changed=true;
        require(!controls.sample(physical,remapped,true).accept,"reconnection pulse confirmed quit");
        for(unsigned button=1;button<=5;++button){
            auto mapping=sonic::input::default_bindings;mapping[unsigned(sonic::input::Action::Cancel)].mouse=button;
            Controls mouse;sonic::input::Snapshot clicks;clicks.focused=true;clicks.cursor_x=40;clicks.cursor_y=40;
            mouse.sample(clicks,mapping,true,confirm_box,cancel_box);
            Prompt dialog;dialog.sample(Screen::PressStart,{});dialog.sample(Screen::PressStart,{true,false});dialog.presented();dialog.sample(Screen::PressStart,{});
            clicks.mouse[button]=true;const auto pressed=mouse.sample(clicks,mapping,true,confirm_box,cancel_box);
            require(!pressed.back&&!pressed.accept,"remapped mouse cancelled/confirmed quit at press");
            clicks.mouse[button]=false;const auto released=mouse.sample(clicks,mapping,true,confirm_box,cancel_box);
            require(released.back&&dialog.sample(Screen::PressStart,released)==Decision::Cancelled,"remapped mouse cancel could confirm quit");
        }

        CpuState cpu{.memory=Memory{0u}};
        require(read_screen(cpu,true)==Screen::None,"unmapped memory is not a title screen");
        auto ram=std::make_shared<LinearMemoryDevice>(0x1000000u);cpu.memory.map_region("ram",0x0C000000u,ram);
        const auto put=[&](std::uint32_t p,std::uint32_t v){cpu.memory.write_u32(canonical_physical_address(p),v);};
        constexpr std::uint32_t c=0x8CF10000u,w=0x8CF10100u,t=0x8CF10200u,tw=0x8CF10300u;
        put(0x8C7608B0u,11);put(0x8C960AF4u,1);put(0x8C960AE8u,c);put(c+0x2C,w);
        put(w+4,6);put(w+12,6);put(0x8C9645D8u,t);put(0x8C9645DCu,t);put(t+0x10,0x8C909380u);
        put(t+0x2C,tw);put(tw,2);put(tw+24,2);put(tw+36,180);
        require(read_screen(cpu,false)==Screen::None && read_screen(cpu,true)==Screen::PressStart,"bound ready title predicate");
        put(tw+36,0);require(read_screen(cpu,true)==Screen::PressStart,"quit must not inherit the original Start delay");
        put(tw,1);require(read_screen(cpu,true)==Screen::None,"title entrance animation");put(tw,2);
        put(tw+24,3);require(read_screen(cpu,true)==Screen::None,"Start transition");put(tw+24,2);
        for(auto address:{w+20,w+28,w+30}) {
            cpu.memory.write_u8(canonical_physical_address(address),1);
            require(read_screen(cpu,true)==Screen::None,"controller transition guard");
            cpu.memory.write_u8(canonical_physical_address(address),0);
        }
        for(auto id:{1u,2u,4u,8u,9u}){put(w+4,id);put(w+12,id);require(read_screen(cpu,true)==Screen::None,"submenu must retain original B");}
        put(w+4,7);put(w+12,7);put(t+0x10,0x8C909A50u);put(tw+28,1);
        require(read_screen(cpu,true)==Screen::MainMenu,"root menu predicate");
        put(tw+28,2);require(read_screen(cpu,true)==Screen::None,"nested root dialog");put(tw+28,1);
        put(w+12,6);require(read_screen(cpu,true)==Screen::None,"screen not ready");put(w+12,7);
        put(t+0x10,0x8C909380u);require(read_screen(cpu,true)==Screen::None,"wrong task owner");put(t+0x10,0x8C909A50u);
        put(t+0x2C,0xDEADBEEFu);require(read_screen(cpu,true)==Screen::None,"invalid work pointer");put(t+0x2C,tw);
        put(0x8C7608B0u,15);require(read_screen(cpu,true)==Screen::None,"gameplay must not open prompt");
        for(int language=0;language<5;++language) {
            put(sonic::language::text_global,std::uint32_t(language));
            require(effective_language(cpu,-1)==language && effective_language(cpu,(language+1)%5)==(language+1)%5,"language preference/game fallback");
            require(!labels(language).question.empty() && !labels(language).cancel.empty(),"missing translation");
            auto image=rasterize(language);require(image.pixels.size()==640*256*4,"raster size");
            bitmap(directory/("language-"+std::to_string(language)+".bmp"),std::move(image));
        }
        put(sonic::language::text_global,99);require(effective_language(cpu,-1)==1,"invalid game language fallback");
        bitmap(directory/"remapped-playstation.bmp",rasterize(4,remapped,sonic::input::GlyphStyle::PlayStation));
        std::cout<<"SONIC_QUIT_TESTS_OK title_states input_edges focus transitions languages=5 physical_remapping mouse_release alt_enter\n";return 0;
    }catch(const std::exception& error){std::cerr<<"SONIC_QUIT_TESTS_FAILED "<<error.what()<<'\n';return 1;}
}
