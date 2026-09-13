#include "sonic_legacy_video.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/native_port_texture_asset.hpp"
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <bit>
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
std::string digest(std::span<const std::uint8_t> bytes) {
    std::array<unsigned char,32> result{};
    require(bytes.size()<=ULONG_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),result.data(),ULONG(result.size()))>=0,
        "source digest failed");
    constexpr char hex[]="0123456789abcdef";std::string text(64,'0');
    for(std::size_t i=0;i<result.size();++i){text[i*2]=hex[result[i]>>4];text[i*2+1]=hex[result[i]&15];}
    return text;
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
void render_completion_contract(CpuState& cpu) {
    // Execute the byte-bound original notification service. The counter read
    // and interrupt epilogue are outside this hardware-free component.
    constexpr auto returned=0x8CF80000u, fixture=0x8CF80020u;
    for (const bool callback : {false,true}) {
        cpu.pc=0x8C604486u;cpu.pr=returned;cpu.write_sr(sr_md_mask);cpu.write_fpscr(0u);
        cpu.exception_generation=0;cpu.trap_pending=false;
        cpu.r[15]=0x8CF00000u;cpu.r[14]=0xABCDEF01u;cpu.r[4]=0x00BC1234u;
        cpu.memory.write_u32(0x0C6733B8u,callback?fixture:0u);
        cpu.memory.write_u32(0x0C88F710u,1u);cpu.memory.write_u32(0x0C88F718u,0u);
        until(cpu,callback?fixture:returned,64u);
        require(cpu.memory.read_u32(0x0C88F710u)==0u && cpu.memory.read_u32(0x0C88F718u)==1u,
            "render flags not published before callback");
        require(cpu.r[4]==0x00BC1234u,"render counter argument corrupted");
        if(callback) {
            require(cpu.pr==0x8C6044A2u,"render callback return changed");
            cpu.pc=cpu.pr;until(cpu,returned);
        }
        require(cpu.r[15]==0x8CF00000u && cpu.r[14]==0xABCDEF01u,"render service callee-save corrupted");
    }
    std::cout<<"SONIC_ORIGINAL_RENDER_COMPLETION_OK flags_before_callback=1 counter_argument=1 null_and_registered=1\n";
}

struct LoopResult {
    std::vector<unsigned> iterations, waits;
    std::string sequence;
    unsigned timer_calls=0, post_calls=0, extra_post_calls=0, published_press1=0, published_press2=0;
};
// Execute only the original orchestration wrapper. These four callee boundaries
// are deterministic fixtures, NOT replacements for gameplay/physics or timing.
LoopResult run_update_loop(CpuState& cpu,float elapsed=1000.0f,bool alternate=false) {
    constexpr auto returned=0x8CF80000u;
    cpu.pc=alternate?0x8C04E95Au:0x8C04EA4Au;cpu.pr=returned;cpu.write_sr(sr_md_mask);cpu.write_fpscr(0u);
    cpu.exception_generation=0;cpu.trap_pending=false;
    for(unsigned i=0;i<16;++i)cpu.r[i]=0x13579000u+i;
    cpu.r[15]=0x8CF00000u;cpu.fr[15]=0x3F800001u;
    const auto before=cpu.r;const auto before_fr15=cpu.fr[15];
    LoopResult result;
    const auto word=[&](std::uint32_t address){return cpu.memory.read_u32(address&0x1fffffffu);};
    for(unsigned steps=0;cpu.pc!=returned && steps<2048;++steps) {
        const auto iteration=word(0x8C754E08u);
        if(cpu.pc==0x8C04E714u) {
            require(cpu.pr==(alternate?0x8C04E9A0u:0x8C04EA94u),"body caller changed");
            result.iterations.push_back(iteration);result.sequence+='B';cpu.pc=cpu.pr;
        } else if(cpu.pc==0x8C0517F6u) {
            require(cpu.pr==(alternate?0x8C04E9FEu:0x8C04EB38u),"wait caller changed");
            result.waits.push_back(cpu.r[4]);result.sequence+='W';
            // New edge observations arrive at the existing wait boundary.
            cpu.memory.write_u32((word(0x8C754CE8u)+16u)&0x1fffffffu,4u);
            cpu.memory.write_u32((word(0x8C754CECu)+16u)&0x1fffffffu,8u);
            cpu.pc=cpu.pr;
        } else if(cpu.pc==0x8C06C0F2u) {
            require(cpu.pr==(alternate?0x8C04E9DEu:0x8C04EB18u) && iteration==1u,"timer caller changed");
            require(std::bit_cast<float>(cpu.fr[15])==(alternate?1900.0f:1850.0f),"timer threshold changed");
            ++result.timer_calls;result.sequence+='T';
            cpu.fr[0]=std::bit_cast<std::uint32_t>(elapsed);cpu.pc=cpu.pr;
        } else if(cpu.pc==0x8C08A3FAu) {
            require(cpu.pr==(alternate?0x8C04EA2Cu:0x8C04EB66u),"post caller changed");
            ++result.post_calls;result.sequence+='P';
            result.published_press1=word(word(0x8C754CE8u)+16u);
            result.published_press2=word(word(0x8C754CECu)+16u);
            cpu.pc=cpu.pr;
        } else if(cpu.pc==0x8C08A664u) {
            require(alternate && cpu.pr==0x8C04EA32u,"alternate post caller changed");
            ++result.extra_post_calls;result.sequence+='Q';cpu.pc=cpu.pr;
        } else {
            require(cpu.pc>=(alternate?0x8C04E95Au:0x8C04EA4Au) &&
                cpu.pc<(alternate?0x8C04EA4Au:0x8C04EB7Eu),"escaped original loop wrapper");
            (void)execute_dynamic_sh4_block(cpu,services,1u);
        }
        require(cpu.exception_generation==0u && !cpu.trap_pending,"original loop exception");
    }
    require(cpu.pc==returned && cpu.pr==returned && cpu.r[0]==0u,"loop return changed");
    require(std::equal(cpu.r.begin()+8,cpu.r.end(),before.begin()+8) && cpu.fr[15]==before_fr15,
        "loop callee-saved state changed");
    return result;
}
void update_loop_contract(CpuState& cpu) {
    const auto word=[&](std::uint32_t address){return cpu.memory.read_u32(address&0x1fffffffu);};
    const auto phase=[&](){return cpu.memory.read_u8(0x0C19DD6Cu);};
    const auto seed=[&](unsigned tv,unsigned delta,unsigned initial_phase) {
        cpu.memory.write_u32(0x0C754B44u,tv);cpu.memory.write_u32(0x0C754E04u,delta);
        cpu.memory.write_u8(0x0C19DD6Cu,std::uint8_t(initial_phase));
        cpu.memory.write_u32(0x0C754CE8u,0x8CD01000u);cpu.memory.write_u32(0x0C754CECu,0x8CD02000u);
        cpu.memory.write_u32(0x0CD01010u,1u);cpu.memory.write_u32(0x0CD02010u,2u);
    };
    seed(0,2,2);const auto two=run_update_loop(cpu);
    require(two.iterations==std::vector<unsigned>{0,1} && two.waits==std::vector<unsigned>{1}
        && two.sequence=="BWBTP" && two.post_calls==1 && two.timer_calls==1
        && two.published_press1==5 && two.published_press2==10
        && word(0x8C754E08u)==2 && phase()==2,"ordinary delta2 contract differs");
    seed(0,1,2);const auto one_a=run_update_loop(cpu);const auto one_b=run_update_loop(cpu);
    require(one_a.sequence=="BP" && one_b.sequence=="BP" && one_a.waits.empty() && one_b.waits.empty()
        && one_a.published_press1==1 && one_b.published_press1==1
        && word(0x8C754E08u)==1 && phase()==2,"split delta1 contract differs");
    seed(1,2,2);const auto pal_two=run_update_loop(cpu);
    require(pal_two.iterations==std::vector<unsigned>{0,1,2} && pal_two.waits==std::vector<unsigned>{2,1}
        && pal_two.sequence=="BWBTWBP"
        && word(0x8C754E08u)==3 && phase()==3,"PAL delta2 extra-step contract differs");
    seed(1,1,2);const auto pal_one_a=run_update_loop(cpu);const auto pal_one_b=run_update_loop(cpu);
    require(pal_one_a.sequence=="BP" && pal_one_b.sequence=="BP" && phase()==4
        && word(0x8C754E08u)==1,"PAL split delta1 contract differs");
    seed(1,2,0);std::vector<unsigned> cycle;unsigned bodies=0;
    for(unsigned n=0;n<5;++n){const auto r=run_update_loop(cpu);cycle.push_back(unsigned(r.iterations.size()));bodies+=unsigned(r.iterations.size());}
    require(cycle==std::vector<unsigned>{2,2,3,2,3} && bodies==12 && phase()==0,
        "original PAL five-wrapper phase cycle differs");
    seed(0,2,0);const auto slow=run_update_loop(cpu,1850.0f);
    require(slow.iterations==std::vector<unsigned>{0,1,2} && slow.waits==std::vector<unsigned>{1,1}
        && slow.sequence=="BWBTWBP" && word(0x8C754E08u)==3,"timer threshold extra-step differs");
    seed(0,2,0);const auto alternate=run_update_loop(cpu,1850.0f,true);
    require(alternate.sequence=="BWBTPQ" && alternate.iterations==std::vector<unsigned>{0,1}
        && alternate.post_calls==1 && alternate.extra_post_calls==1
        && alternate.published_press1==5 && alternate.published_press2==10,
        "alternate wrapper conflated with primary threshold/post");
    seed(0,2,0);const auto alternate_slow=run_update_loop(cpu,1900.0f,true);
    require(alternate_slow.sequence=="BWBTWBPQ" && alternate_slow.iterations==std::vector<unsigned>{0,1,2}
        && alternate_slow.waits==std::vector<unsigned>{1,1},"alternate 1900 threshold differs");
    for(bool extra:{false,true}) {
        seed(1,2,0);cycle.clear();bodies=0;
        for(unsigned n=0;n<5;++n) {
            const auto r=run_update_loop(cpu,extra?1900.0f:1000.0f,true);
            require(r.post_calls==1 && r.extra_post_calls==1,"alternate post not once per wrapper");
            cycle.push_back(unsigned(r.iterations.size()));bodies+=unsigned(r.iterations.size());
        }
        require(cycle==(extra?std::vector<unsigned>{3,3,4,3,4}:std::vector<unsigned>{2,2,3,2,3})
            && bodies==(extra?17u:12u) && phase()==0,"alternate PAL phase cycle differs");
    }
    std::cout<<"SONIC_RETAIL_UPDATE_LOOP_PASS ordinary_delta2=BWBTP split_delta1=BP,BP "
        <<"pal_phase2_delta2_bodies=3 pal_phase2_split_bodies=2 pal_cycle=2,2,3,2,3 "
        <<"pal_cycle_bodies=12 wrappers=5 timer_threshold=1850 alternate_threshold=1900 "
        <<"alternate_extra_cycle=3,3,4,3,4 alternate_extra_bodies=17 alternate_post=PQ tested=orchestration_only\n";
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"installed content root required");
        const auto content=std::filesystem::path(argv[1]);
        const auto boot=read(content/"boot.bin");
        const auto advertise=decompress_native_port_prs(read(content/"SONICAD/ADVERTISE.PRS"));
        require(boot.size()==6735296u && advertise.size()==935496u,"unexpected retail sources");
        require(digest(boot)=="b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af",
            "retail boot source SHA differs");
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
        update_loop_contract(cpu);
        render_completion_contract(cpu);
        std::cout<<"SONIC_LEGACY_VIDEO_TESTS_OK retail_calls="<<cases<<" apply=test=restore=ok cpu_ram_preserved=1 menu_continuation=ok clock_arguments=4 constructors=2 update_loop=ok\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
