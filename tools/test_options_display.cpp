#include "katana/runtime/native_port.hpp"
#include "katana/runtime/runtime.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <tuple>

extern "C" katana::runtime::NativePortHookResult sonic_options_legacy_display(katana::runtime::NativePortContext&) noexcept;
using namespace katana::runtime;
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
auto registers(const CpuState& cpu){return std::tuple(cpu.r,cpu.fr,cpu.xf,cpu.pc,cpu.pr,cpu.read_sr(),cpu.mach,cpu.macl,cpu.gbr,cpu.fpul,cpu.fpscr,cpu.retired_guest_instructions);}
}
int main(){
    try{
        CpuState cpu{.memory=Memory{0u}};auto ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
        cpu.memory.map_region("options-test-ram",0x0C000000u,ram);
        NativePortContext context;context.cpu=&cpu;
        constexpr std::uint32_t task=0x8CE00000u,work=0x8CE00100u;
        const auto write=[&](std::uint32_t p,std::uint32_t v){cpu.memory.write_u32(canonical_physical_address(p),v);};
        write(0x8C9645D0u,task);write(task+16,0x8C9078E0u);write(task+20,0x8C9079BEu);write(task+44,work);
        cpu.r[4]=task;cpu.r[15]=0x8CF00000;cpu.pc=0x8C9079BE;cpu.pr=0x8C041234;
        for(unsigned i=0;i<16;++i){cpu.fr[i]=0x3F800000+i;cpu.xf[i]=0x80000000+i;}
        for(unsigned phase=0;phase<4;++phase){
            write(work,phase);write(work+20,phase==2?0xFFFFFFFFu:0x55667788u);
            const auto before=registers(cpu);const auto memory=std::vector<std::uint8_t>(ram->bytes().begin(),ram->bytes().end());
            require(sonic_options_legacy_display(context).action==NativePortHookAction::Return,"legacy menu leaked during fade/ready");
            require(before==registers(cpu),"display replacement changed CPU state");
            require(std::equal(memory.begin(),memory.end(),ram->bytes().begin()),"display replacement changed task/transition RAM");
        }
        // Unrelated/stale tasks, Sound Test's display and invalid work keep the
        // original path. No state-2 or current-controller-screen dependency.
        for(unsigned path=0;path<4;++path){
            write(task+16,path==0?0x8C900000u:0x8C9078E0u);write(task+20,path==1?0x8C906000u:0x8C9079BEu);
            write(task+44,path==2?0x8CFFFFF0u:work);cpu.r[4]=path==3?task+0x80u:task;
            require(sonic_options_legacy_display(context).action==NativePortHookAction::ContinueOriginal,"unrelated draw suppressed");
        }
        context.cpu=nullptr;require(sonic_options_legacy_display(context).action==NativePortHookAction::ContinueOriginal,"null context");
        std::cout<<"SONIC_OPTIONS_DISPLAY_TEST_OK entry steady exit cpu_ram_unchanged unrelated_original\n";return 0;
    }catch(const std::exception& e){std::cerr<<"SONIC_OPTIONS_DISPLAY_TEST_FAIL "<<e.what()<<'\n';return 1;}
}
