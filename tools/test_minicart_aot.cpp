// Differential test of the two exact original results owners. External title
// services are identical recorded boundaries on both sides; this is not a race
// replay or a substitute implementation of those external services.
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "katana/runtime/native_port.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/sh4/decoder.hpp"
#include "katana/io/input_provenance.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <vector>
using namespace katana::runtime;
namespace {
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
constexpr auto returned=0x8CF80000u;
using Call=std::tuple<std::uint32_t,std::array<std::uint32_t,16>,
    std::array<std::uint32_t,16>,std::array<std::uint32_t,16>,std::uint32_t,std::uint32_t>;
std::vector<Call>* calls=nullptr;
bool original_record_services=false;
void original_call(CpuState&,std::uint32_t);
void native_resume(CpuState&);
void external(CpuState& c,std::uint32_t target){
    if(original_record_services){original_call(c,target);return;}
    if((target&0x1FFFFFFFu)>=0x0C90A81Eu && (target&0x1FFFFFFFu)<0x0C90B000u){
        c.pc=target;native_resume(c);return;
    }
    calls->emplace_back(target,c.r,c.fr,c.xf,c.pr,c.read_fpscr());
    c.r[0]=0x8CE20000u; // valid isolated return buffer, same in both executions
    c.pc=c.pr;
}
struct Host final:NativePortHostServices {
    std::uint64_t monotonic_time_nanoseconds()const noexcept override{return 1;}
    NativePortLifecycleState poll_lifecycle()override{return {};}
    void synchronize_simulation_boundary()override{}
    void begin_frame(std::uint64_t)override{}
    void present_frame(std::uint64_t)override{}
    std::uint64_t presented_frames()const noexcept override{return 0;}
} host;
struct Reference final:PlatformServices {
    std::string_view name()const noexcept override{return "MINICART-original-bytes";}
    std::uint32_t abi_version()const noexcept override{return 128;}
    std::uint32_t guest_cycle_contract()const noexcept override{return 0;}
    PlatformCapabilities capabilities()const noexcept override{return {};}
    void read_memory(std::uint32_t,std::span<std::uint8_t>)override{throw std::runtime_error("reference device read");}
    void write_memory(std::uint32_t,std::span<const std::uint8_t>)override{throw std::runtime_error("reference device write");}
    std::uint64_t scheduler_cycle()const noexcept override{return 0;}
    std::optional<std::uint64_t> next_scheduler_event_cycle()const noexcept override{return {};}
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t,std::size_t)override{return {};}
    std::optional<PlatformInterruptRequest> poll_interrupt()override{return {};}
    PlatformDmaResult start_dma(const PlatformDmaRequest&)override{throw std::runtime_error("reference DMA");}
    PlatformFallbackResult controlled_fallback(CpuState&,const PlatformFallbackRequest&)override{throw std::runtime_error("reference fallback");}
    bool prefetch(CpuState&,GuestInstructionOrigin,std::uint32_t)override{throw std::runtime_error("reference PREF");}
} reference;
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    Fixture(std::span<const std::uint8_t> module,unsigned state,unsigned variant,std::uint32_t base,
            std::span<const std::uint8_t> resident={}){
        cpu.memory.map_region("test-ram",0x0C000000u,ram);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        std::copy(resident.begin(),resident.end(),ram->writable_bytes().begin());
        std::copy(module.begin(),module.end(),ram->writable_bytes().begin()+0x900000u);
        cpu.pc=base+0xA81Eu;cpu.pr=returned;cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask);
        cpu.r[4]=0x8CE00000u;cpu.r[15]=0x8CF00000u;
        put(0x8CE00020u,0x8CE01000u);
        put(0x8CE01000u,state);put(0x8CE01004u,((variant&1)?181u:179u)<<16u);
        put(0x8CE0100Cu,(variant&1)?2u:0u);
        put(0x0CA5E1D0u,(variant>>1)&1u);put(0x0CA5E1D8u,variant&1u);
        put(0x8C754DBCu,std::array{0u,4u,64u,128u}[variant/2]);
        put(0x8C754B3Cu,(variant>>2)&1u);
    }
    void put(std::uint32_t p,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(p&0xFFFFFFu),&v,4);}
};
auto architecture(const CpuState& c){return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,
    c.mach,c.macl,c.fpul,c.read_fpscr(),c.read_sr(),c.exception_generation,c.last_exception_cause);}
void reference_step(CpuState& c){
    const auto word=guest_fetch_u16(c,c.pc);
    const auto instruction=katana::sh4::decode(word);
    if(instruction.kind!=katana::sh4::InstructionKind::Ftrc){
        (void)execute_dynamic_sh4_block(c,reference,1u);return;
    }
    // Same archived interpreter operand defect documented by palette/atan
    // tests: FTRC's source is decoded separately from destination=0. Correct
    // only these six authenticated retail words, never the expected output.
    const auto pc=c.pc&0xFFFFFFu;
    const bool source2=pc==0x90AD78u || pc==0x90AE9Eu;
    require(source2 || pc==0x90ACA8u || pc==0x90ACB8u || pc==0x90AD84u || pc==0x90AE8Eu,
        "unexpected MINICART FTRC address");
    require(word==(source2?0xF23Du:0xF33Du) && instruction.source_register==(source2?2u:3u),
        "unexpected MINICART FTRC operand");
    GuestInstructionAttempt attempt(c,c.pc,2u);c.pc+=2u;
    fpu_truncate_to_fpul(c,instruction.source_register);
}
void original_call(CpuState& c,std::uint32_t target){
    const auto stop=c.pr;c.pc=target;
    for(unsigned i=0;c.pc!=stop && i<100000;++i){
        reference_step(c);require(!c.exception_generation,"original record helper exception");
    }
    require(c.pc==stop,"original record helper did not return");
}
}
namespace katana_port_generated {
thread_local bool native_bringup_dispatch_pending=false;
namespace runtime_dispatch_detail {
thread_local NativePortAotServices* active_services=nullptr;
thread_local BlockAddress active_exit_source;
thread_local BlockEndKind active_exit_kind=BlockEndKind::Fallthrough;
thread_local DynamicDispatchSiteClass active_exit_site_class=DynamicDispatchSiteClass::NotDynamic;
thread_local bool tail_dispatch_completed=false;
bool try_static_return_nop_callback(CpuState&,std::uint32_t)noexcept{return false;}
}
void preflight_native_bringup_indirect_dispatch(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,bool){}
void consume_native_bringup_direct_aot_dispatch(std::uint32_t){}
#define BOUNDARY(name) void name(CpuState& c,std::uint32_t t){external(c,t);}
BOUNDARY(static_call) BOUNDARY(resolved_call) BOUNDARY(guarded_call) BOUNDARY(guarded_jump)
BOUNDARY(runtime_only_call) BOUNDARY(runtime_only_jump) BOUNDARY(unresolved_call) BOUNDARY(unresolved_jump)
#undef BOUNDARY
void exact_guarded_call(CpuState& c,std::uint32_t t,std::uint32_t){external(c,t);}
void exact_guarded_jump(CpuState& c,std::uint32_t t,std::uint32_t){external(c,t);}
BlockExit fn_8298A81E_runtime_entry(CpuState&,BlockExecutionContext&);
BlockExit fn_8298AA60_runtime_entry(CpuState&,BlockExecutionContext&);
BlockExit fn_8298007A_runtime_entry(CpuState&,BlockExecutionContext&);
}
namespace {
void native_resume(CpuState& c){
    const auto stop=c.pr;
    for(unsigned i=0;c.pc!=stop && i<2000;++i){
        const auto pc=c.pc&0x1FFFFFFFu;
        if(pc>=0x0C90007Au && pc<0x0C9000B0u){
            BlockExecutionContext block;
            (void)katana_port_generated::fn_8298007A_runtime_entry(c,block);continue;
        }
        if(pc<0x0C90A81Eu || pc>=0x0C90B000u){external(c,c.pc);continue;}
        BlockExecutionContext block;
        if(pc<0x0C90AA60u)(void)katana_port_generated::fn_8298A81E_runtime_entry(c,block);
        else (void)katana_port_generated::fn_8298AA60_runtime_entry(c,block);
    }
    require(c.pc==stop,"native owner did not return");
}

void record_path_cases(std::span<const std::uint8_t> module,const char* resident_path){
    std::ifstream f(resident_path,std::ios::binary);
    const std::vector<std::uint8_t> resident{std::istreambuf_iterator<char>(f),{}};
    require(resident.size()==0x1000000u && katana::io::sha256_bytes(
        {reinterpret_cast<const char*>(resident.data()),resident.size()})==
        "b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c", "resident record code identity");
    unsigned cases=0;
    original_record_services=true;
    // Execute the actual resident ranking writer and original time conversion
    // and integer division, without service stubs. Only the newly bound owner
    // is native on the candidate side. Saves exist solely in these RAM copies.
    for(const auto base:{0x8C900000u,0xAC900000u})for(unsigned character=0;character<6;++character)
    for(unsigned rank=0;rank<4;++rank){
        Fixture n(module,0,0,base,resident),r(module,0,0,base,resident);
        const unsigned total=std::array{4000u,5500u,6500u,7500u}[rank];
        for(auto* fixture:{&n,&r}){
            fixture->cpu.pc=0x8C0A1C36u;
            fixture->put(0x8C749308u,std::array{0u,2u,3u,5u,6u,7u}[character]);
            fixture->put(0x8C7492FAu,35u); // Twinkle Circuit, act zero
            fixture->put(0x8C161BE4u,0u); // isolated profile zero
            fixture->put(0x0C9180BCu,1234u);fixture->put(0x0C9180C0u,4321u);
            fixture->put(0x0C9180C4u,total);
            for(unsigned slot=0;slot<6;++slot){
                const std::array<std::uint8_t,15> times{0,50,0,1,0,0,1,10,0,85,85,85,85,85,85};
                std::copy(times.begin(),times.end(),fixture->ram->writable_bytes().begin()+0x798A90+15*slot);
            }
        }
        const std::array ranges{NativePortImmutableRange{0x0C90007Au,0x36u,
            native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)}};
        NativePortImmutableWriteGuard guard(ranges);
        NativePortContext context;context.cpu=&n.cpu;context.host=&host;
        NativePortAotServices services(context,+[](std::uint32_t)noexcept{return false;},guard);
        katana_port_generated::runtime_dispatch_detail::active_services=&services;
        unsigned native_lap_calls=0;
        for(auto* fixture:{&n,&r}){
            ScopedCodeAddressMapping mapping({0x82980000u,base,1434052u});
            for(unsigned i=0;fixture->cpu.pc!=returned && i<100000;++i){
                // The resident literal is a physical alias. Native module
                // admission selects the placed P1/P2 code address before
                // calling its owner; use that same boundary on both sides.
                if((fixture->cpu.pc&0x1FFFFFFFu)==0x0C90007Au)
                    fixture->cpu.pc=base+0x7Au;
                if(fixture==&n && (fixture->cpu.pc&0x1FFFFFFFu)==0x0C90007Au){
                    native_resume(fixture->cpu);++native_lap_calls;
                } else reference_step(fixture->cpu);
                require(!fixture->cpu.exception_generation,"record writer exception");
            }
        }
        require(n.cpu.pc==returned && r.cpu.pc==returned,"record writer did not return");
        require(architecture(n.cpu)==architecture(r.cpu) &&
            std::equal(n.ram->bytes().begin(),n.ram->bytes().end(),r.ram->bytes().begin()),
            "native/original record writer mismatch");
        require(native_lap_calls==(rank==0?2u:0u),"new-record lap accessor coverage");
        auto record=n.ram->bytes().subspan(0x798A90+15*character,15);
        if(rank==0){
            const std::array<std::uint8_t,15> expected{0,40,0,0,50,0,1,0,0,0,12,34,0,43,21};
            require(std::equal(record.begin(),record.end(),expected.begin()),"new record and lap times");
        } else {
            require(std::all_of(record.begin()+9,record.end(),[](auto byte){return byte==85;}),
                "slower result changed best lap times");
        }
        ++cases;
    }
    original_record_services=false;
    std::cout<<"SONIC_MINICART_RECORD_PATH_PASS cases="<<cases
        <<" characters=6 ranks=first,second,third,unranked original_services=real cpu=exact ram=exact\n";
}
}
int main(int argc,char** argv)try{
    require(argc==3,"provide authenticated MINICART and resident RAM binaries");
    std::ifstream f(argv[1],std::ios::binary);
    std::vector<std::uint8_t> module{std::istreambuf_iterator<char>(f),{}};
    require(module.size()==1434052u,"MINICART size");
    require(katana::io::sha256_bytes({reinterpret_cast<const char*>(module.data()),module.size()})==
        "2d3ec72d9f62a0ec626155f822a77bac7209999aa89f825b3082a69cf85db2a1","MINICART SHA identity");
    unsigned cases=0;
    for(auto base:{0x8C900000u,0xAC900000u})for(unsigned state=0;state<6;++state)for(unsigned variant=0;variant<8;++variant){
        Fixture n(module,state,variant,base),r(module,state,variant,base);
        std::vector<Call> native_calls,reference_calls;
        const std::array ranges{NativePortImmutableRange{0x0C90A81Eu,0x800u,
            native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)}};
        NativePortImmutableWriteGuard guard(ranges);
        NativePortContext context;context.cpu=&n.cpu;context.host=&host;
        NativePortAotServices services(context,+[](std::uint32_t)noexcept{return false;},guard);
        katana_port_generated::runtime_dispatch_detail::active_services=&services;
        calls=&native_calls;
        {
            ScopedCodeAddressMapping mapping({0x82980000u,base,1434052u});
            native_resume(n.cpu);
        }
        calls=&reference_calls;
        for(unsigned i=0;r.cpu.pc!=returned && i<2000;++i){
            auto pc=r.cpu.pc&0x1FFFFFFFu;
            if(pc>=0x0C90A81Eu && pc<0x0C90B000u)
                reference_step(r.cpu);
            else external(r.cpu,r.cpu.pc);
            require(!r.cpu.exception_generation,"reference exception");
        }
        if(n.cpu.pc!=returned || r.cpu.pc!=returned || architecture(n.cpu)!=architecture(r.cpu) ||
           native_calls!=reference_calls || !std::equal(n.ram->bytes().begin(),n.ram->bytes().end(),r.ram->bytes().begin())){
            std::cerr<<"state="<<state<<" variant="<<variant<<" base="<<std::hex<<base
                <<" native_pc="<<n.cpu.pc<<" reference_pc="<<r.cpu.pc<<" calls="<<std::dec<<native_calls.size()<<'/'<<reference_calls.size()<<'\n';
            for(unsigned i=0;i<16;++i)if(n.cpu.r[i]!=r.cpu.r[i])std::cerr<<"r"<<i<<' '<<std::hex<<n.cpu.r[i]<<'/'<<r.cpu.r[i]<<'\n';
            throw std::runtime_error("MINICART original-byte differential mismatch");
        }
        ++cases;
    }
    std::cout<<"SONIC_MINICART_DIFFERENTIAL_PASS cases="<<cases<<" states=0..5 aliases=P1,P2 cpu=exact ram=exact external_calls=exact\n";
    record_path_cases(module,argv[2]);
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
