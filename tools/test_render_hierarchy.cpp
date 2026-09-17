#include "sonic_render_hierarchy.hpp"
#include "sonic_scalar_write_view.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
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
    NativePortImmutableWriteGuard immutable{ranges()};
    Fixture* oracle{};
    unsigned steps{},mutation{},callback_index{},stop_after{};
    std::vector<std::pair<std::uint32_t,std::uint32_t>> trace;
    Fixture(std::span<const std::uint8_t> image,unsigned mode){
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
    static bool external(std::uint32_t pc){return pc==0x8C037098u || pc==callback || pc==alternate;}
    bool child(std::uint32_t target){
        trace.emplace_back(target,cpu.pr);++callback_index;
        if(stop_after==callback_index)return false;
        if(target==callback && callback_index<=2){
            if(mutation==1)put(nodes+4,0x8CE41000u);
            if(mutation==2)put(nodes+44,nodes+768);
            if(mutation==3)put(nodes,0x28u);
            if(mutation==4)put(0x8C88FD70u,alternate);
            if(mutation==5)cpu.r[13]^=0x28u;
            if(mutation==6)cpu.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{});
            if(mutation==7){put(0x8C88FD74u,0x8C0405A4u);cpu.r[13]+=32;}
            if(mutation==8)cpu.write_fpscr(cpu.read_fpscr()|fpscr_sz_mask);
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
            else (void)execute_dynamic_sh4_block(c,services,1u);
        }
        return !c.trap_pending;
    }
};
void advance(Fixture& f){
    while(f.cpu.pc!=returned && !Fixture::external(f.cpu.pc) && !f.cpu.trap_pending){
        require(++f.steps<500000u,"hierarchy reference bound");(void)execute_dynamic_sh4_block(f.cpu,services,1u);
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
int main(int argc,char** argv)try{
    require(argc==2,"render-hierarchy-tests <original-ram>");std::ifstream file(argv[1],std::ios::binary);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};require(image.size()==0x1000000u,"RAM size");unsigned cases=0;
    for(unsigned mode:{0u,1u,fpscr_fr_mask})for(unsigned variant=0;variant<30;++variant){
        Fixture a(image,mode),b(image,mode);setup(a,variant);setup(b,variant);a.oracle=&b;
        const auto result=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,Fixture::resume});
        while(b.cpu.pc!=a.cpu.pc && !b.cpu.trap_pending){advance(b);if(Fixture::external(b.cpu.pc))require(b.child(b.cpu.pc),"reference stopped unexpectedly");}
        compare(a,b,"hierarchy final");require(a.trace==b.trace,"callback sequence differs");
        if(variant<27)require(result==family::Outcome::Complete && a.cpu.pc==returned,"hierarchy incomplete");
        else require(result==family::Outcome::Interrupted,"expected original failure/stop");
        std::cout<<"hierarchy case="<<variant<<" mode="<<mode<<" outcome="<<int(result)<<'\n';++cases;
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
    std::cout<<"SONIC_RENDER_HIERARCHY_PASS cases="<<cases<<" visited="<<std::count(family::visited.begin(),family::visited.end(),true)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_RENDER_HIERARCHY_FAIL "<<e.what()<<'\n';return 1;}
