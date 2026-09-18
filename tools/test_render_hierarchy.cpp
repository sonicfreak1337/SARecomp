#include "sonic_render_hierarchy.hpp"
#include "sonic_scalar_write_view.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <tuple>
using namespace katana::runtime;
namespace family=sonic::render_hierarchy;
constexpr std::uint32_t returned=0x8C010000u,nodes=0x8CE00000u,records=0x8CE10000u,keys=0x8CE20000u,callback=0x8C010080u,alternate=0x8C0100A0u;
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
auto architecture(const CpuState& c){return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,
    c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.mach,c.macl,c.fpul,
    c.read_fpscr(),c.fpscr,c.sr,c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,c.last_exception_cause);}
struct Services final:PlatformServices {
    std::string_view name()const noexcept override{return "world-original-bytes";}
    std::uint32_t abi_version()const noexcept override{return 128u;}
    std::uint32_t guest_cycle_contract()const noexcept override{return 0u;}
    PlatformCapabilities capabilities()const noexcept override{return {};}
    void read_memory(std::uint32_t,std::span<std::uint8_t>)override{throw std::runtime_error("device read");}
    void write_memory(std::uint32_t,std::span<const std::uint8_t>)override{throw std::runtime_error("device write");}
    std::uint64_t scheduler_cycle()const noexcept override{return 0u;}
    std::optional<std::uint64_t> next_scheduler_event_cycle()const noexcept override{return {};}
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t,std::size_t)override{return {};}
    std::optional<PlatformInterruptRequest> poll_interrupt()override{return {};}
    PlatformDmaResult start_dma(const PlatformDmaRequest&)override{throw std::runtime_error("DMA");}
    PlatformFallbackResult controlled_fallback(CpuState&,const PlatformFallbackRequest&)override{throw std::runtime_error("fallback");}
    bool prefetch(CpuState&,GuestInstructionOrigin,std::uint32_t)override{throw std::runtime_error("PREF");}
} services;

// The instruction oracle executes original bytes, with the retained AOT's
// lexical host-FPU boundaries. Separate actual-AOT tests verify this contract.
#include "hierarchy-epochs.inc"
void reference_step(CpuState& cpu){
    for(const auto& scope:original_epochs)if(cpu.pc==scope.begin){
        const auto mode=cpu.read_fpscr();
        std::optional<HostFpuExecutionEpoch> epoch;
        if((mode&(fpscr_exception_enable_mask|fpscr_dn_mask))==fpscr_dn_mask &&
           (mode&fpscr_rounding_mode_mask)<=1u && (!scope.single || !(mode&fpscr_pr_mask)))epoch.emplace(cpu);
        for(auto pc=scope.begin;pc<scope.end;pc+=2u){
            require(cpu.pc==pc,"original epoch stopped being straight-line");
            (void)execute_dynamic_sh4_block(cpu,services,1u);
            if(cpu.trap_pending)return;
        }
        return;
    }
    (void)execute_dynamic_sh4_block(cpu,services,1u);
}

std::vector<NativePortImmutableRange> ranges(){
    std::vector<NativePortImmutableRange> out;
    for(auto s:family::source_spans())out.push_back({s.address&0x1FFFFFFFu,std::uint32_t(s.bytes.size()),native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)});
    return out;
}


struct Fixture;
void advance(Fixture&);
void compare(Fixture&,Fixture&,const char*);
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable;
    Fixture* oracle{};
    unsigned steps{},mutation{},callback_index{},stop_after{};
    bool rigid_test{},morph_test{};
    std::vector<std::pair<std::uint32_t,std::uint32_t>> trace;
    Fixture(std::span<const std::uint8_t> image,unsigned mode,bool protect_sources=true)
        :immutable(protect_sources?ranges():std::vector<NativePortImmutableRange>{
            {0x0C000000u,2u,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)}}){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|mode);
        for(unsigned i=0;i<16;++i){cpu.r[i]=0xA5010000u+i;cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i)+.125f);cpu.xf[i]=(i%5==0)?0x3F800000u:0u;}
        for(unsigned i=0;i<8;++i)cpu.r_bank[i]=0xD4000000u+i;
        cpu.fpul=0xA9876543u;cpu.mach=0x12345678u;cpu.macl=0x87654321u;
        cpu.r[4]=nodes;cpu.r[15]=0x8CF00000u;cpu.pc=family::entry;cpu.pr=returned;
        clear(nodes,0x60000);
        put(0x8C88F5D8u,128);put(0x8C88F5DCu,1);put(0x8C88F538u,0x8CE60000u);
        for(unsigned i=0;i<16;++i)put(0x8CE60000u+i*4,(i%5==0)?0x3F800000u:0u);
        for(unsigned i=0;i<8;++i){const auto n=nodes+i*256;put(n+4,0x8CE40000u+i*256);vector(n+8,float(i)+.5f,2.f,-3.f);put(n+20,0x1000u*i);put(n+24,0x4000);put(n+28,0xFE000);vector(n+32,1.f,1.5f,.75f);}
        put(0x8C88FD64u,0x8C0405B2u);put(0x8C88FD6Cu,0x8C04057Au);put(0x8C88FD70u,0x8C040588u);put(0x8C88FD74u,0x8C0405A4u);
        put(0x8C88FD80u,records);putf(0x8C88FD8Cu,5.25f);put(0x8C88FFB4u,0);
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{immutable.observe_write(e);},GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        cpu.memory.set_guest_write_batch_observer({&immutable,[](void*,std::span<const GuestWriteEvent>)noexcept{return true;},
            [](void* g,std::span<const GuestWriteEvent> w)noexcept{for(const auto& e:w)static_cast<NativePortImmutableWriteGuard*>(g)->observe_write(e);}});
        sonic::scalar_writes::bind(cpu.memory,immutable,cpu.memory.guest_write_observer_generation());
    }
    ~Fixture(){sonic::scalar_writes::unbind(&cpu.memory,&immutable);}
    void clear(std::uint32_t a,std::size_t n){std::fill_n(ram->writable_bytes().data()+(a&0xFFFFFFu),n,0);}
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4);}
    std::uint32_t get(std::uint32_t a){std::uint32_t v;std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4);return v;}
    void putf(std::uint32_t a,float v){put(a,std::bit_cast<std::uint32_t>(v));}
    void vector(std::uint32_t a,float x,float y,float z){putf(a,x);putf(a+4,y);putf(a+8,z);}
    static bool external(std::uint32_t pc){return pc==0x8C037098u || pc==0x8C03700Cu || pc==callback || pc==alternate;}
    bool child(std::uint32_t target){
        trace.emplace_back(target,cpu.pr);++callback_index;
        if(stop_after==callback_index)return false;
        if((target==callback || ((rigid_test || morph_test) && target==0x8C037098u)) && callback_index<=2){
            if(mutation==1)put(nodes+4,0x8CE41000u);
            if(mutation==2)put(nodes+44,nodes+768);
            if(mutation==3)put(nodes,0x28u);
            if(mutation==4)put(0x8C88FD70u,alternate);
            if(mutation==5)cpu.r[13]^=0x28u;
            if(mutation==6)cpu.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{});
            if(mutation==7){put(0x8C88FD74u,0x8C0405A4u);cpu.r[13]+=32;}
            if(mutation==8)cpu.write_fpscr(cpu.read_fpscr()|fpscr_sz_mask);
            if(mutation==9)put(0x8C88FDE0u,alternate);
            if(mutation==10)put(0x8C88FE78u,1u);
            if(mutation==11)put(0x8C04085Cu,get(0x8C04085Cu)^1u);
            if(mutation==12)put(0x8C041BB0u,get(0x8C041BB0u)^1u);
            if(mutation==13)put(0x8C63A5DCu,get(0x8C63A5DCu)^1u);
            if(mutation==14)put(nodes,get(nodes)|16u);
            if(mutation==15)put(nodes+48,nodes+768);
            if(mutation==16)cpu.xf[0]=0x40000000u;
            if(mutation==17){put(nodes+0x1000,0x8CE07000u);put(nodes+0x1004,0x8CE07100u);put(0x8C88FD9Cu,records+512u);}
        }
        cpu.fr[1]^=0x00800000u;cpu.r[0]=target;cpu.t=!cpu.t;cpu.pc=cpu.pr;
        return true;
    }
    static bool invoke(void* p,CpuState& c,std::uint32_t target){
        auto& f=*static_cast<Fixture*>(p);require(&c==&f.cpu && external(target),"unexpected hierarchy external");
        if(f.oracle){advance(*f.oracle);compare(f,*f.oracle,"callback entry");}
        const auto ok=f.child(target);
        if(f.oracle){require(f.oracle->child(target)==ok,"callback result");compare(f,*f.oracle,"callback return");}
        return ok;
    }
    static bool resume(void* p,CpuState& c,std::uint32_t,std::uint32_t end){
        auto& f=*static_cast<Fixture*>(p);
        while(c.pc!=end && !c.trap_pending){
            require(++f.steps<500000u,"fallback bound");
            if(external(c.pc)){if(!invoke(p,c,c.pc))return false;}
            else reference_step(c);
        }
        return !c.trap_pending;
    }
};
void advance(Fixture& f){
    while(f.cpu.pc!=returned && !Fixture::external(f.cpu.pc) && !f.cpu.trap_pending){
        require(++f.steps<500000u,"hierarchy reference bound");reference_step(f.cpu);
    }
}
void compare(Fixture& a,Fixture& b,const char* phase){
    if(architecture(a.cpu)!=architecture(b.cpu)){
        std::cerr<<phase<<" mismatch PC "<<std::hex<<a.cpu.pc<<' '<<b.cpu.pc<<" PR "<<a.cpu.pr<<' '<<b.cpu.pr<<'\n';
        for(unsigned i=0;i<16;++i){if(a.cpu.r[i]!=b.cpu.r[i])std::cerr<<"R"<<std::dec<<i<<' '<<std::hex<<a.cpu.r[i]<<' '<<b.cpu.r[i]<<'\n';if(a.cpu.fr[i]!=b.cpu.fr[i])std::cerr<<"FR"<<std::dec<<i<<' '<<std::hex<<a.cpu.fr[i]<<' '<<b.cpu.fr[i]<<'\n';if(a.cpu.xf[i]!=b.cpu.xf[i])std::cerr<<"XF"<<std::dec<<i<<' '<<std::hex<<a.cpu.xf[i]<<' '<<b.cpu.xf[i]<<'\n';}
        std::cerr<<"T "<<a.cpu.t<<' '<<b.cpu.t<<" FPSCR "<<std::hex<<a.cpu.read_fpscr()<<' '<<b.cpu.read_fpscr()<<'\n';throw std::runtime_error("architecture differs");
    }
    if(std::memcmp(a.ram->bytes().data(),b.ram->bytes().data(),0x1000000u)){
        auto d=std::mismatch(a.ram->bytes().begin(),a.ram->bytes().end(),b.ram->bytes().begin());
        std::cerr<<phase<<" RAM mismatch "<<std::hex<<0x8C000000u+std::size_t(d.first-a.ram->bytes().begin())<<'\n';throw std::runtime_error("RAM differs");
    }
}

void setup(Fixture& f,unsigned variant){
    if(variant==0)return;
    f.put(nodes+44,nodes+256);f.put(nodes+256+48,nodes+512);
    if(variant==1)return;
    if(variant<=4){f.put(nodes,(variant==2?0x20u:variant==3?0x40u:0x48u));return;}
    if(variant>=5 && variant<=9){constexpr std::uint32_t entries[]{0x8C0405B2u,0x8C0405D2u,0x8C040612u,0x8C0406A0u,0x8C0406E0u};f.put(0x8C88FD64u,entries[variant-5]);return;}
    if(variant>=10 && variant<=17){
        f.put(0x8C88FD64u,0x8C0406A0u);f.put(0x8C88FD6Cu,0x8C0400A0u);f.put(0x8C88FD70u,variant&1u?0x8C0401A8u:0x8C040150u);f.put(0x8C88FD74u,0x8C0400F8u);
        const unsigned count=variant<12?0:variant<14?1:variant<16?2:4;
        for(unsigned node=0;node<4;++node)for(unsigned channel=0;channel<3;++channel){
            const auto r=records+node*24,k=keys+node*1024+channel*256;f.put(r+channel*4,count?k:0);f.put(r+12+channel*4,count);
            for(unsigned i=0;i<count;++i){f.put(k+i*16,i*8);for(unsigned axis=0;axis<3;++axis){if(channel==1)f.put(k+i*16+4+axis*4,0xFFFF1234u*(i+axis+1));else f.putf(k+i*16+4+axis*4,float(i+axis+1)*.25f);}}
        }
        return;
    }
    if(variant>=18 && variant<=24){f.put(0x8C88FFB4u,callback);f.mutation=variant-17;if(variant==21 || variant==24)f.put(0x8C88FD6Cu,callback);return;}
    if(variant==25){f.cpu.r[4]|=0x20000000u;f.cpu.r[15]|=0x20000000u;return;}
    if(variant==26){f.put(0x8C88F5D8u,1);return;}
    if(variant==27){f.put(0x8C88FFB4u,callback);f.stop_after=2;return;}
    if(variant==28){f.cpu.r[15]+=2;return;}
    if(variant==29){f.put(nodes+44,nodes+1);return;}
}
void setup_blended(Fixture& f,unsigned variant){
    f.cpu.pc=family::blended_entry;
    f.put(0x8C88FDC0u,0x8C041516u);f.put(0x8C88FDC4u,0x8C041516u);
    f.put(0x8C88FD98u,0);f.put(0x8C19AC84u,variant&1u?0x8C639F38u:0x8C639C34u);
    f.putf(0x8C88FE7Cu,.375f);
    for(unsigned channel=0;channel<2;++channel){
        f.put(0x8C88FDD8u+4*channel,0x8C0414A6u);
        f.put(0x8C88FDE0u+4*channel,0x8C0414F0u);
        f.put(0x8C88FDE8u+4*channel,0x8C0414CCu);
        const auto context=0x8C88FDF8u+channel*28;
        f.put(context,records+channel*4096);f.putf(context+12,5.25f+float(channel));
    }
    if(!variant)return;
    f.put(nodes+44,nodes+256);f.put(nodes+256+48,nodes+512);
    if(variant==1)return;
    if(variant>=2 && variant<=4){f.put(nodes,variant==2?0x20:variant==3?0x40:0x48);return;}
    if(variant>=5 && variant<=9){
        constexpr std::uint32_t entries[]{0x8C041516u,0x8C04154Eu,0x8C041600u,0x8C041666u,0x8C0416CCu};
        f.put(0x8C88FDC0u,entries[variant-5]);f.put(0x8C88FDC4u,entries[variant-5]);return;
    }
    if(variant>=10 && variant<=17){
        f.put(0x8C88FDC0u,0x8C041666u);f.put(0x8C88FDC4u,0x8C041666u);
        const auto count=variant<12?0u:variant<14?1u:variant<16?2u:4u;
        for(unsigned channel=0;channel<2;++channel){
            f.put(0x8C88FDD8u+4*channel,0x8C040DF0u);
            f.put(0x8C88FDE0u+4*channel,0x8C040EF8u);
            f.put(0x8C88FDE8u+4*channel,0x8C040E74u);
            if(variant==17)f.putf(0x8C88FE04u+channel*28,30.5f);
            for(unsigned node=0;node<4;++node)for(unsigned axis=0;axis<3;++axis){
                const auto row=records+channel*4096+node*24;
                const auto key=keys+channel*8192+node*1024+axis*256;
                f.put(row+axis*4,count?key:0);f.put(row+12+axis*4,count);
                for(unsigned i=0;i<count;++i){
                    f.put(key+i*16,i*8);
                    for(unsigned part=0;part<3;++part){
                        if(axis==1)f.put(key+i*16+4+part*4,0xFFFF1234u*(i+part+1+channel));
                        else f.putf(key+i*16+4+part*4,float(i+part+1+channel)*.25f);
                    }
                }
            }
        }
        return;
    }
    if(variant>=18 && variant<=24){f.put(0x8C88FFB4u,callback);f.mutation=variant-17;return;}
    if(variant==25){f.cpu.r[4]|=0x20000000u;f.cpu.r[15]|=0x20000000u;return;}
    if(variant==26){f.put(0x8C88F5D8u,1);return;}
    if(variant==27){f.put(0x8C88FFB4u,callback);f.stop_after=2;return;}
    if(variant==28){f.cpu.r[15]+=2;return;}
    if(variant==29){f.put(nodes+44,nodes+1);return;}
    if(variant==30 || variant==31){f.put(0x8C88FDE8u,callback);f.mutation=variant==30?6:8;return;}
    if(variant==32){f.put(0x8C19AC84u,callback);return;}
    if(variant==33){f.put(0x8C88FDD8u,callback);f.mutation=9;return;}
    if(variant==34){f.put(0x8C88FDD8u,callback);f.mutation=10;return;}
    if(variant==35){
        f.put(nodes,0x60);f.put(0x8C88FD98u,0x8CE50000u);
        for(unsigned i=0;i<16;++i)f.put(0x8CE50000u+i*4,i%5==0?0x3F800000u:0u);
    }
}
void setup_rigid(Fixture& f,unsigned variant){
    f.rigid_test=true;f.cpu.pc=family::rigid_entry;
    for(unsigned i=0;i<8;++i)f.put(nodes+i*256,16u);
    f.put(nodes,variant<64?variant:0u);f.put(nodes+44,nodes+256);f.put(nodes+48,nodes+512);
    if(variant<64)return;
    if(variant<=69){constexpr unsigned changes[]{14,15,2,16,6,8};f.mutation=changes[variant-64];return;}
    if(variant==70){f.cpu.r[4]|=0x20000000u;f.cpu.r[15]|=0x20000000u;return;}
    if(variant==71){f.cpu.r[15]+=2;return;}
    if(variant==72){f.cpu.r[4]=nodes+1;return;}
    if(variant==73){f.put(nodes+44,0);return;}
    if(variant==74){f.put(nodes+44,nodes+1);return;}
    if(variant==75){f.put(0x8C88F5D8u,1);return;}
    if(variant==76){f.put(nodes,23);f.put(nodes+48,0);f.put(0x8C88F538u,0x8CE60002u);return;}
    if(variant==77){f.put(0x8C88F538u,0x8CE60002u);return;}
    if(variant==78){f.put(nodes+4,0);return;}
    if(variant==79){f.stop_after=1;return;}
}
void setup_morph(Fixture& f,unsigned variant){
    f.morph_test=true;f.cpu.pc=family::morph_entry;
    const auto channel=variant<32?variant%4:3u;
    constexpr auto model=nodes+0x1000u,pk=nodes+0x3000u,nk=nodes+0x3100u;
    constexpr auto p0=nodes+0x4000u,p1=nodes+0x4100u,n0=nodes+0x4200u,n1=nodes+0x4300u,scratch=nodes+0x6000u;
    f.put(nodes,0x40u);f.put(nodes+4,model);f.vector(nodes+8,0,0,0);
    f.put(nodes+20,0);f.put(nodes+24,0);f.put(nodes+28,0);f.vector(nodes+32,1,1,1);
    f.put(nodes+44,0);f.put(nodes+48,0);
    f.put(model,p0);f.put(model+4,n0);f.put(model+8,2);
    f.put(pk,0);f.put(pk+4,p0);f.put(pk+8,4);f.put(pk+12,p1);
    f.put(nk,0);f.put(nk+4,n0);f.put(nk+8,4);f.put(nk+12,n1);
    f.vector(p0,0,0,0);f.vector(p0+12,4,0,0);f.vector(p1,4,8,12);f.vector(p1+12,8,8,12);
    f.vector(n0,1,0,0);f.vector(n0+12,0,1,0);f.vector(n1,0,1,0);f.vector(n1+12,0,0,1);
    f.put(0x8C88FF6Cu,scratch);f.putf(0x8C88FDA8u,1.f);f.put(0x8C88FD9Cu,records);
    f.put(0x8C88FDB0u,0);f.put(0x8C88FFB4u,0);f.put(0x8C749044u,channel);
    f.put(0x8C88FD68u,channel==0?0x8C040720u:channel==3?0x8C040760u:0x8C04073Cu);
    for(unsigned row=0;row<4;++row){
        const auto r=records+row*(channel==3?16u:8u);
        if(channel==3){f.put(r,pk);f.put(r+4,nk);f.put(r+8,2);f.put(r+12,2);}
        else{f.put(r,channel==2?nk:pk);f.put(r+4,2);}
    }
    const auto kind=variant/4;
    if(variant<32){
        if(kind==1)f.putf(0x8C88FDA8u,4.5f);
        if(kind==2){f.put(records+(channel==3?8u:4u),1);if(channel==3)f.put(records+12,1);}
        if(kind==3){f.put(records,0);if(channel==3)f.put(records+4,0);}
        if(kind==4){f.putf(0x8C88FDA8u,2.f);f.vector(n1,-1,0,0);f.vector(n1+12,0,-1,0);}
        if(kind==5)f.mutation=17;
        if(kind==6)f.mutation=6;
        if(kind==7)f.stop_after=1;
        return;
    }
    if(variant==32)f.put(0x8C88FF6Cu,p0);
    if(variant==33)f.put(0x8C88FF6Cu,scratch+2);
    if(variant==34)f.put(pk+4,p0+2);
    if(variant==35){f.cpu.r[4]|=0x20000000u;f.cpu.r[15]|=0x20000000u;}
    if(variant==36)f.mutation=8;
    if(variant==37)f.put(nodes,0xC0u);
    if(variant==38){
        f.put(0x8C88FFB4u,callback);f.mutation=2;
        f.put(nodes+768,0x40u);f.put(nodes+768+4,model);
    }
    if(variant==39){
        for(unsigned i=1;i<3;++i){f.put(nodes+i*256,0x40u);f.put(nodes+i*256+4,model);}
        f.put(nodes+48,nodes+256);f.put(nodes+256+48,nodes+512);
    }
    if(variant==40){f.put(pk+8,0);f.put(nk+8,0);}
    if(variant==41)f.put(0x8C88FF6Cu,p0+4);
    if(variant==42)f.put(model+8,1);
    if(variant==43)f.cpu.r[15]+=2;
    if(variant==44)f.put(records,0); // positions absent, normals still present
    if(variant==45)f.put(records+4,0); // normals absent, positions still present
    if(variant==46){f.put(pk+4,0x8CFFFFF4u);f.vector(0x8CFFFFF4u,0,0,0);}
    if(variant==47)f.put(0x8C88FF6Cu,0x8CFFFFF4u);
}
int main(int argc,char** argv)try{
    require(argc==2 || (argc==3 && (std::string(argv[2])=="--rigid-only" || std::string(argv[2])=="--morph-only" || std::string(argv[2])=="--submission-only")),"render-hierarchy-tests <original-ram> [--rigid-only|--morph-only|--submission-only]");std::ifstream file(argv[1],std::ios::binary);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};require(image.size()==0x1000000u,"RAM size");unsigned cases=0;
    if(argc==3 && std::string(argv[2])=="--submission-only"){
#ifdef _WIN32
        _putenv_s("SARECOMP_NATIVE_MODEL_SUBMISSION","1");_putenv_s("SARECOMP_INTERNAL_DIAGNOSTICS","0");
#else
        setenv("SARECOMP_NATIVE_MODEL_SUBMISSION","1",1);setenv("SARECOMP_INTERNAL_DIAGNOSTICS","0",1);
#endif
        for(unsigned mode:{0u,1u,fpscr_fr_mask})for(unsigned kind=0;kind<6;++kind){
            Fixture a(image,mode),b(image,mode);
            for(auto* f:{&a,&b}){if(kind==2)setup_rigid(*f,0);else if(kind==3)setup_blended(*f,1);else setup(*f,1);}
            a.oracle=&b;
            struct Probe {Fixture* f;unsigned kind,attempts{};} probe{&a,kind};
            const auto invoke=+[](void* p,CpuState& c,std::uint32_t pc){return Fixture::invoke(static_cast<Probe*>(p)->f,c,pc);};
            const auto resume=+[](void* p,CpuState& c,std::uint32_t owner,std::uint32_t end){return Fixture::resume(static_cast<Probe*>(p)->f,c,owner,end);};
            const auto model=+[](void* p,CpuState& c,sonic::model_pipeline::SharedOperation& operation){
                using Result=sonic::model_pipeline::Outcome;auto& probe=*static_cast<Probe*>(p);++probe.attempts;
                require(operation.intact && operation.cpu==&c && operation.read && operation.write,"missing shared model capability");
                require(!operation.allows_write(operation.context,0x0C040784u,4u) &&
                    !operation.allows_write(operation.context,0x0C037098u,4u) &&
                    operation.allows_write(operation.context,0x0CE40000u,4u),"shared model source fence");
                if(probe.kind==4)return Result::Declined;
                operation.sources_proven=true;
                if(probe.kind==5)operation.revoke();
                return Fixture::invoke(probe.f,c,c.pc)?Result::Complete:Result::Interrupted;
            };
            const auto result=family::execute(a.cpu,&a.immutable,{&probe,invoke,resume,model});
            advance(b);compare(a,b,"shared hierarchy final");
            if(result!=family::Outcome::Complete || a.cpu.pc!=returned || !probe.attempts || a.trace!=b.trace)
                std::cerr<<"shared case="<<kind<<" mode="<<mode<<" attempts="<<probe.attempts<<" outcome="<<int(result)<<" pc="<<std::hex<<a.cpu.pc<<std::dec<<'\n';
            require(result==family::Outcome::Complete && a.cpu.pc==returned && probe.attempts && a.trace==b.trace,"shared hierarchy completion");++cases;
        }
        {
            Fixture f(image,0,false);f.cpu.pc=0x8C639BB0u;f.cpu.r[4]=0;f.put(0x8C88F538u,0x8C037098u);
            const auto before=f.get(0x8C037098u);
            const auto resume=+[](void*,CpuState& c,std::uint32_t owner,std::uint32_t){
                require(owner==0x8C639BB0u && c.pc==0x8C639BD2u,"model-source restart frontier");return false;};
            require(family::execute(f.cpu,&f.immutable,{&f,Fixture::invoke,resume})==family::Outcome::Interrupted &&
                f.get(0x8C037098u)==before,"hierarchy overwrote borrowed model source");++cases;
        }
        require(family::counts.model_calls && family::counts.model_revocations,"shared/foreign paths not exercised");
        std::cout<<"SONIC_RENDER_SUBMISSION_PASS cases="<<cases<<'\n';return 0;
    }
    if(argc==2){
    for(unsigned mode:{0u,1u,fpscr_fr_mask})for(unsigned variant=0;variant<30;++variant){
        Fixture a(image,mode),b(image,mode);setup(a,variant);setup(b,variant);a.oracle=&b;
        const auto result=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,Fixture::resume});
        while(b.cpu.pc!=a.cpu.pc && !b.cpu.trap_pending){advance(b);if(Fixture::external(b.cpu.pc))require(b.child(b.cpu.pc),"reference stopped unexpectedly");}
        compare(a,b,"hierarchy final");require(a.trace==b.trace,"callback sequence differs");
        if(variant<27)require(result==family::Outcome::Complete && a.cpu.pc==returned,"hierarchy incomplete");
        else require(result==family::Outcome::Interrupted,"expected original failure/stop");
        std::cout<<"hierarchy case="<<variant<<" mode="<<mode<<" outcome="<<int(result)<<'\n';++cases;
    }
    for(unsigned mode:{0u,1u,fpscr_fr_mask})for(unsigned variant=0;variant<36;++variant){
        Fixture a(image,mode),b(image,mode);setup_blended(a,variant);setup_blended(b,variant);a.oracle=&b;
        const auto result=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,Fixture::resume});
        while(b.cpu.pc!=a.cpu.pc && !b.cpu.trap_pending){advance(b);if(Fixture::external(b.cpu.pc))require(b.child(b.cpu.pc),"blended original child");}
        compare(a,b,"blended hierarchy final");require(a.trace==b.trace,"blended callback sequence differs");
        if(variant>=27 && variant<=29)require(result==family::Outcome::Interrupted,"blended expected failure");
        else require(result==family::Outcome::Complete && a.cpu.pc==returned,"blended incomplete");
        std::cout<<"blended case="<<variant<<" mode="<<mode<<" outcome="<<int(result)<<'\n';++cases;
    }
    for(unsigned mode:{0u,1u,fpscr_fr_mask})for(auto owner:{0x8C639BB0u,0x8C639AD8u,0x8C639C34u,0x8C639F38u,0x8C63A5DCu,0x8C63A7B8u})for(unsigned kind=0;kind<6;++kind){
        Fixture a(image,mode),b(image,mode);
        for(auto* f:{&a,&b}){
            f->cpu.pc=owner;f->cpu.r[4]=kind?0x8CE30000u:0;f->cpu.r[5]=0x1234;f->cpu.r[6]=0xFFFF4567;f->cpu.r[7]=0x789A;
            if(kind==2)f->cpu.r[4]=f->cpu.r[15]-16;
            if(kind==3)f->cpu.r[4]|=0x20000000u;
            if(kind==4)f->cpu.r[4]+=2;
            if(kind==5)f->cpu.r[5]=f->cpu.r[6]=f->cpu.r[7]=0;
            if(kind!=4 && kind)for(unsigned i=0;i<16;++i)f->putf(f->cpu.r[4]+i*4,float(int(i)-4)*.25f);
            if(owner==0x8C639AD8u)f->cpu.r[4]=kind==0?1:kind==1?0:kind==2?2:kind==3?0xFFFFFFFFu:1;
        }
        const auto outcome=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,Fixture::resume});
        advance(b);compare(a,b,"SDK full owner");
        require(a.cpu.trap_pending?outcome==family::Outcome::Interrupted:outcome==family::Outcome::Complete,"SDK completion");
        ++cases;
    }
    for(unsigned kind=0;kind<3;++kind){
        Fixture f(image,0);
        if(kind==1){setup_blended(f,0);f.put(0x8C88FDD8u,callback);}
        else f.put(0x8C88FD6Cu,callback);
        f.mutation=11+kind;
        // A foreign call changes source bytes without advancing Memory's map
        // generation. Parent and future-child proofs must not survive it.
        struct Probe {Fixture* fixture;std::uint32_t expected;unsigned resumes{};} probe{
            &f,kind==0?family::entry:kind==1?family::blended_entry:0x8C63A5DCu};
        const auto invoke=+[](void* p,CpuState& c,std::uint32_t target){return Fixture::invoke(static_cast<Probe*>(p)->fixture,c,target);};
        const auto resume=+[](void* p,CpuState&,std::uint32_t owner,std::uint32_t){
            auto& probe=*static_cast<Probe*>(p);++probe.resumes;
            require(!family::local_sources_enabled() || owner==probe.expected,"source proof survived a nested callback");return false;};
        const auto result=family::execute(f.cpu,&f.immutable,{&probe,invoke,resume});
        require(result==family::Outcome::Interrupted && probe.resumes==1,"changed owner entered natively");
        ++cases;
    }
    {
        Fixture f(image,0,false);
        f.cpu.pc=0x8C639BB0u;f.cpu.r[4]=0;f.put(0x8C88F538u,0x8C04085Cu);
        const auto literal=f.get(0x8C04085Cu);
        const auto resume=+[](void*,CpuState& c,std::uint32_t owner,std::uint32_t){
            require(owner==0x8C639BB0u && c.pc==0x8C639BD2u,"source-overlap restart frontier");return false;};
        const auto result=family::execute(f.cpu,&f.immutable,{&f,Fixture::invoke,resume});
        require(result==family::Outcome::Interrupted && f.get(0x8C04085Cu)==literal &&
            f.get(0x8C88F5DCu)==2u,"untracked source overwrite or lost earlier store");
        ++cases;
    }
    }
    if(argc==2 || std::string(argv[2])=="--rigid-only")for(unsigned mode:{0u,1u})for(unsigned variant=mode?64u:0u;variant<80;++variant){
        Fixture a(image,mode),b(image,mode);setup_rigid(a,variant);setup_rigid(b,variant);a.oracle=&b;
        const auto result=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,Fixture::resume});
        while(b.cpu.pc!=a.cpu.pc && !b.cpu.trap_pending){advance(b);if(Fixture::external(b.cpu.pc))require(b.child(b.cpu.pc),"rigid original child");}
        compare(a,b,"rigid hierarchy final");require(a.trace==b.trace,"rigid callback sequence differs");
        if(a.cpu.trap_pending || variant==79)require(result==family::Outcome::Interrupted,"rigid expected interruption");
        else require(result==family::Outcome::Complete && a.cpu.pc==returned,"rigid incomplete");
        std::cout<<"rigid case="<<variant<<" mode="<<mode<<" outcome="<<int(result)<<'\n';++cases;
    }
    if(argc==2 || std::string(argv[2])=="--morph-only")for(unsigned mode:{0u,1u})for(unsigned variant=0;variant<48;++variant){
        Fixture a(image,mode),b(image,mode);setup_morph(a,variant);setup_morph(b,variant);a.oracle=&b;
        const auto result=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,Fixture::resume});
        while(b.cpu.pc!=a.cpu.pc && !b.cpu.trap_pending){advance(b);if(Fixture::external(b.cpu.pc))require(b.child(b.cpu.pc),"morph original child");}
        compare(a,b,"morph hierarchy final");require(a.trace==b.trace,"morph callback sequence differs");
        if(a.cpu.trap_pending || (variant>=28 && variant<32))require(result==family::Outcome::Interrupted,"morph expected interruption");
        else require(result==family::Outcome::Complete && a.cpu.pc==returned,"morph incomplete");
        if(variant==3){
            require(a.get(nodes+0x1000)==nodes+0x4000 && a.get(nodes+0x1004)==nodes+0x4200,"morph model pointers not restored");
            require(a.get(nodes+0x6000)==0x3F800000u && a.get(nodes+0x6004)==0x40000000u && a.get(nodes+0x6008)==0x40400000u,"morph known position");
        }
        if(variant==23)require(a.get(nodes+0x1000)==nodes+0x4000 && a.get(nodes+0x1004)==nodes+0x4200 && a.get(0x8C88FD9Cu)==records+16u,"morph restoration after mutable draw");
        std::cout<<"morph case="<<variant<<" mode="<<mode<<" outcome="<<int(result)<<'\n';++cases;
    }
    std::cout<<"SONIC_RENDER_HIERARCHY_PASS cases="<<cases<<" visited="<<std::count(family::visited.begin(),family::visited.end(),true)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_RENDER_HIERARCHY_FAIL "<<e.what()<<'\n';return 1;}
