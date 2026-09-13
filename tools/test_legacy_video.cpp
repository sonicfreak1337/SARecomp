#include "sonic_legacy_video.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/native_port_texture_asset.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <tuple>
using namespace katana::runtime;
namespace {
void require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}
struct Services final:PlatformServices {
    std::string_view name() const noexcept override{return "legacy-video-callers";}
    std::uint32_t abi_version() const noexcept override{return 128u;}
    std::uint32_t guest_cycle_contract() const noexcept override{return 0u;}
    PlatformCapabilities capabilities() const noexcept override{return {};}
    void read_memory(std::uint32_t,std::span<std::uint8_t>) override{throw std::runtime_error("unexpected device read");}
    void write_memory(std::uint32_t,std::span<const std::uint8_t>) override{throw std::runtime_error("unexpected device write");}
    std::uint64_t scheduler_cycle() const noexcept override{return 0u;}
    std::optional<std::uint64_t> next_scheduler_event_cycle() const noexcept override{return {};}
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t,std::size_t) override{return {};}
    std::optional<PlatformInterruptRequest> poll_interrupt() override{return {};}
    PlatformDmaResult start_dma(const PlatformDmaRequest&) override{throw std::runtime_error("unexpected DMA");}
    PlatformFallbackResult controlled_fallback(CpuState&,const PlatformFallbackRequest&) override{throw std::runtime_error("unexpected fallback");}
    bool prefetch(CpuState&,GuestInstructionOrigin,std::uint32_t) override{throw std::runtime_error("unexpected PREF");}
} services;
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);require(bool(file),"retail source missing");
    return {std::istreambuf_iterator<char>(file),{}};
}
void until(CpuState& cpu,std::uint32_t end,unsigned limit=24) {
    for(unsigned n=0;cpu.pc!=end && n<limit;++n) {
        (void)execute_dynamic_sh4_block(cpu,services,1u);
        require(cpu.exception_generation==0u,"retail caller exception");
    }
    require(cpu.pc==end,"unexpected retail caller control flow");
}
auto registers(const CpuState& cpu) {
    return std::tuple(cpu.r,cpu.fr,cpu.xf,cpu.pc,cpu.pr,cpu.sr,cpu.macl,cpu.mach,cpu.t,cpu.read_fpscr());
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"installed content root required");
        const auto content=std::filesystem::path(argv[1]);
        const auto boot=read(content/"boot.bin");
        const auto advertise=decompress_native_port_prs(read(content/"SONICAD/ADVERTISE.PRS"));
        require(boot.size()==6735296u && advertise.size()==935496u,"unexpected retail sources");
        CpuState cpu{.memory=Memory{0u}};
        auto ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
        cpu.memory.map_region("ram",0x0C000000u,ram);
        std::copy(boot.begin(),boot.end(),ram->writable_bytes().begin()+0x10000u);
        std::copy(advertise.begin(),advertise.end(),ram->writable_bytes().begin()+0x900000u);
        constexpr std::uint32_t work=0x8CE00000u,owner=0x8C90E28Eu;
        const auto word=[&](unsigned offset){return cpu.memory.read_u32((work+offset)&0x1fffffffu);};
        NativePortContext context;context.cpu=&cpu;
        unsigned cases=0;
        // Execute the real BSR + delay slot and real post-call UI writes.
        // Apply 50/60, Test 60, and restore either previous mode all share
        // the protected owner. No synthetic caller or replacement guest code.
        for(unsigned path=0;path<3;++path)for(unsigned mode=1;mode<=2;++mode) {
            if(path==1 && mode==1)continue;
            for(unsigned r=0;r<16;++r){cpu.r[r]=0x13579000u+r;cpu.fr[r]=0x3f800000u+r;cpu.xf[r]=r;}
            cpu.r[15]=0x8CF00000u;cpu.r[14]=work;cpu.r[0]=52;
            cpu.r[4]=mode;cpu.r[5]=0;cpu.sr=sr_md_mask;cpu.t=true;
            cpu.exception_generation=0u;cpu.trap_pending=false;cpu.write_fpscr(0u);
            std::fill_n(ram->writable_bytes().begin()+0xE00000u,64u,0x55);
            cpu.memory.write_u8((work+52u)&0x1fffffffu,std::uint8_t(mode));
            cpu.pc=path==0?0x8C90E7F8u:path==1?0x8C90E46Cu:0x8C90E4ACu;
            until(cpu,owner);
            require(cpu.r[4]==mode && cpu.r[5]==(path==0?1u:0u),"retail caller arguments changed");
            require(cpu.pr==(path==0?0x8C90E7FEu:path==1?0x8C90E472u:0x8C90E4B0u),"retail return address changed");
            const auto state=registers(cpu);
            const auto bytes=std::vector<std::uint8_t>(ram->bytes().begin(),ram->bytes().end());
            require(sonic_legacy_video_mode_disabled(context).action==NativePortHookAction::Return,"TV-mode owner not disabled");
            require(state==registers(cpu) && std::equal(bytes.begin(),bytes.end(),ram->bytes().begin()),
                "TV action changed CPU, saves or graphics/texture RAM");
            cpu.pc=cpu.pr;
            until(cpu,path==0?0x8C90E806u:path==1?0x8C90E47Eu:0x8C90E4B4u);
            require(path==0?word(0)==3u:path==1?(word(24)==4u && word(48)==0u && word(44)==0xffffffffu):word(24)==0u,
                "original menu did not continue after disabled TV action");
            ++cases;
        }
        // Decode the original menu/persistence mapping by executing only its
        // retail argument-construction branch. Stop before the original SDK
        // display constructor: this check cannot issue video/device writes.
        const auto tv_address=cpu.memory.read_u32(0x0C90E3D0u);
        const auto display_owner=cpu.memory.read_u32(0x0C90E3D4u);
        require(tv_address==0x8C754B44u,"retail TV-mode destination changed");
        for(unsigned mode=1;mode<=2;++mode)for(unsigned persist=0;persist<=1;++persist) {
            cpu.r[15]=0x8CF00000u;cpu.sr=sr_md_mask;cpu.t=false;
            cpu.exception_generation=0u;cpu.trap_pending=false;
            cpu.memory.write_u32(0x0CF00000u,persist);
            cpu.memory.write_u32(0x0CF00004u,mode);
            cpu.memory.write_u32(tv_address&0x1fffffffu,0x13579BDFu);
            cpu.pc=0x8C90E2C0u;
            until(cpu,display_owner);
            require(cpu.r[4]==(mode==1?58u:56u) && cpu.r[5]==0u && cpu.r[6]==1u,
                "retail display constructor arguments changed");
            const auto stored=cpu.memory.read_u32(tv_address&0x1fffffffu);
            require(stored==(persist?2u-mode:0x13579BDFu),"retail TV-mode mapping changed");
            std::cout<<"SONIC_RETAIL_VIDEO_MAPPING menu="<<mode<<" persist="<<persist
                <<" stored="<<stored<<" display_mode="<<cpu.r[4]
                <<" owner=0x"<<std::hex<<display_owner<<std::dec<<'\n';
        }
        for(unsigned mode=1;mode<=2;++mode) {
            // Separate constructor component: feed the original decoded mode
            // argument into its retail low-bit publication and selector.
            // This does not stub/run the rest of system initialization.
            cpu.r[15]=0x8CF00000u;cpu.r[12]=mode==1?58u:56u;
            cpu.exception_generation=0u;cpu.trap_pending=false;
            cpu.pc=0x8C65263Eu;until(cpu,0x8C652648u);
            require(cpu.memory.read_u32(0x0C8A2CB4u)==(mode==1?2u:0u),"display mode low bits changed");
            cpu.pc=0x8C6526CAu;until(cpu,0x8C6526CEu);
            require(cpu.memory.read_u32(0x0C8A2CB8u)==(mode==1?58u:56u),"full display mode not published");
            cpu.pc=0x8C652756u;
            const auto constructor=mode==1?0x8C658500u:0x8C658744u;
            until(cpu,constructor);
            // The constructors and their original RAM-only argument copy
            // finish before the hardware-facing apply owner is entered.
            until(cpu,0x8C658220u,2048u);
            const auto argument=[&](unsigned offset){return cpu.memory.read_u32((cpu.r[15]+offset)&0x1fffffffu);};
            const auto horizontal=((argument(0)&0x3ffu)<<16u)|(argument(4)&0x3ffu);
            const auto vertical=((argument(0x14)&0x3ffu)<<16u)|(argument(8)&0x3ffu);
            const auto border=((argument(0x0c)&0x3ffu)<<16u)|(argument(0x10)&0x3ffu);
            std::cout<<"SONIC_RETAIL_VIDEO_CONSTRUCTOR menu="<<mode<<" owner=0x"<<std::hex<<constructor
                <<" horizontal=0x"<<horizontal<<" vertical=0x"<<vertical<<" border=0x"<<border<<std::dec<<'\n';
            require(horizontal==(mode==1?0x008D034Bu:0x007E0345u) &&
                vertical==(mode==1?0x0270035Fu:0x020C0359u),"original video constructor tuple changed");
        }
        std::cout<<"SONIC_LEGACY_VIDEO_TESTS_OK retail_calls="<<cases<<" apply=test=restore=ok cpu_ram_preserved=1 menu_continuation=ok clock_arguments=4 constructors=2\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
