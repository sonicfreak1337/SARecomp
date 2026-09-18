#include "sonic_movement_contact.hpp"
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
namespace family=sonic::movement_contact;
constexpr std::uint32_t returned=0x8CF80000u,A=0x8CE00000u,B=0x8CE01000u,C=0x8CE02000u,Q=0x8CE03000u,
    object=0x8C6BB1BCu,triangles=0x8C6BE1BCu,indices=0x8CE10000u,njs=0x8CE30000u,model=0x8CE40000u;
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
#include "contact-epochs.inc"
void reference_step(CpuState& cpu){
    if(cpu.pc==0x8C10FB7Eu || cpu.pc==0x8C10FBAAu){
        const unsigned reg=cpu.pc==0x8C10FB7Eu?4u:3u;
        require(guest_fetch_u16(cpu,cpu.pc)==(reg==4?0xF43D:0xF33D),"original FTRC identity");
        GuestInstructionAttempt attempt(cpu,cpu.pc,2u);cpu.pc+=2;fpu_truncate_to_fpul(cpu,reg);return;
    }
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



struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{ranges()};
    Fixture* oracle{};
    unsigned steps{},stage{},mutation{},callbacks{};
    Fixture(std::span<const std::uint8_t> image,unsigned mode){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|mode);
        for(unsigned i=0;i<16;++i){cpu.r[i]=0xA5010000u+i;cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i)+.125f);cpu.xf[i]=(i%5==0)?0x3F800000u:0;}
        for(unsigned i=0;i<8;++i)cpu.r_bank[i]=0xD4000000u+i;
        cpu.r[4]=A;cpu.r[5]=B;cpu.r[6]=C;cpu.r[15]=0x8CF00000u;cpu.pc=family::entry;cpu.pr=returned;
        clear(A,0x80000);clear(object,64*192);clear(triangles,64*96);
        put(C+100,Q);half(A+4,1);putf(C+196,10);putf(C+272,2);
        put(0x8C78C568u,A);put(0x8C78C5A8u,C);
        vector(0x8C754B1Cu,0,1,0);put(0x8C1BDF20u,0);vector(0x8C6BAC30u+32,0,0,0);
        put(0x8C88F5D8u,128);put(0x8C88F5DCu,1);put(0x8C88F538u,0x8CE70000u);
        for(unsigned i=0;i<16;++i){put(0x8C67C580u+4*i,i%5==0?0x3F800000u:0);put(0x8CE70000u+4*i,i%5==0?0x3F800000u:0);}
        ram->writable_bytes()[0x752B1C]=0x80;
        put(0x8C19E8A4u,0);put(0x8C19E8A8u,0);put(0x8C19E8B8u,0);put(0x8C759634u,0);put(0x8C02D548u,0);
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{immutable.observe_write(e);},GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        cpu.memory.set_guest_write_batch_observer({&immutable,[](void*,std::span<const GuestWriteEvent>)noexcept{return true;},
            [](void* g,std::span<const GuestWriteEvent> w)noexcept{for(auto& e:w)static_cast<NativePortImmutableWriteGuard*>(g)->observe_write(e);}});
        sonic::scalar_writes::bind(cpu.memory,immutable,cpu.memory.guest_write_observer_generation());
    }
    ~Fixture(){sonic::scalar_writes::unbind(&cpu.memory,&immutable);}
    void clear(std::uint32_t a,unsigned n){std::fill_n(ram->writable_bytes().data()+(a&0xFFFFFFu),n,0);}
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4);}
    void half(std::uint32_t a,std::uint16_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,2);}
    void putf(std::uint32_t a,float v){put(a,std::bit_cast<std::uint32_t>(v));}
    void vector(std::uint32_t a,float x,float y,float z){putf(a,x);putf(a+4,y);putf(a+8,z);}
    void scene(unsigned count,bool reverse=false){
        put(0x8C19E8A4u,count?1:0);put(0x8C757E34u,0x4000);put(0x8C757E38u,njs);put(0x8C757E3Cu,0);
        put(njs+4,model);putf(model+36,32);put(0x8C02D548u,count?object:0);
        put(object+8,njs);put(object+16,count?triangles:0);put(object+20,count?triangles+64*(count-1):0);
        put(object+28,indices);put(object+36,count);putf(object+40,32);
        for(unsigned i=0;i<count;++i){
            const auto tri=triangles+i*64;put(indices+4*i,tri);put(tri,i+1<count?tri+64:0);put(tri+4,i?tri-64:0);
            vector(tri+8,0,0,0);putf(tri+20,32);
            vector(tri+24,-5,0,-5);vector(tri+36,reverse?0:5,0,reverse?5:-5);vector(tri+48,reverse?5:0,0,reverse?-5:5);
        }
        putf(Q,1);vector(Q+4,0,.5f,0);vector(Q+16,0,-.25f,.1f);
    }
    void step(){
        require(++steps<1000000,"reference bound");
        if(cpu.pc==0x8C04F7E0u){cpu.r[0]=stage;cpu.pc=cpu.pr;return;}
        if(cpu.pc==0x8C077720u){cpu.r[0]=0x77720;cpu.pc=cpu.pr;return;}
        reference_step(cpu);
    }
    void until(std::uint32_t pc){while(cpu.pc!=pc && !cpu.trap_pending)step();}
    static bool invoke(void*,CpuState&,std::uint32_t);
    static bool resume(void* p,CpuState& c,std::uint32_t,std::uint32_t end){
        auto& f=*static_cast<Fixture*>(p);f.until(end);return !c.trap_pending;
    }
};
void compare(Fixture& a,Fixture& b,const char* phase){
    if(architecture(a.cpu)!=architecture(b.cpu)){
        for(unsigned i=0;i<16;++i){
            if(a.cpu.r[i]!=b.cpu.r[i])std::cerr<<"R"<<i<<" "<<std::hex<<a.cpu.r[i]<<" "<<b.cpu.r[i]<<'\n';
            if(a.cpu.fr[i]!=b.cpu.fr[i])std::cerr<<"FR"<<i<<" "<<std::hex<<a.cpu.fr[i]<<" "<<b.cpu.fr[i]<<'\n';
            if(a.cpu.xf[i]!=b.cpu.xf[i])std::cerr<<"XF"<<i<<" "<<std::hex<<a.cpu.xf[i]<<" "<<b.cpu.xf[i]<<'\n';
        }
        std::cerr<<phase<<" pc "<<std::hex<<a.cpu.pc<<" "<<b.cpu.pc<<" fpscr "<<a.cpu.read_fpscr()<<" "<<b.cpu.read_fpscr()<<'\n';
        throw std::runtime_error("architecture differs");
    }
    if(std::memcmp(a.ram->bytes().data(),b.ram->bytes().data(),0x1000000)){
        for(unsigned i=0;i<0x1000000;++i)if(a.ram->bytes()[i]!=b.ram->bytes()[i]){std::cerr<<phase<<" RAM "<<std::hex<<i<<'\n';break;}
        throw std::runtime_error("RAM differs");
    }
}
bool Fixture::invoke(void* p,CpuState& c,std::uint32_t target){
    auto& a=*static_cast<Fixture*>(p);require(&c==&a.cpu,"wrong callback CPU");const auto end=c.pr;
    if(a.oracle){a.oracle->until(target);compare(a,*a.oracle,"foreign entry");}
    ++a.callbacks;
    do{a.step();}while(c.pc!=end && !c.trap_pending);
    if(a.oracle){do{a.oracle->step();}while(a.oracle->cpu.pc!=end && !a.oracle->cpu.trap_pending);compare(a,*a.oracle,"foreign return");}
    if(a.mutation && a.callbacks==1){
        for(auto* f:{&a,a.oracle})if(f){
            if(a.mutation==1)f->cpu.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{});
            if(a.mutation==2)f->cpu.write_fpscr(f->cpu.read_fpscr()|fpscr_sz_mask);
            if(a.mutation==3)f->vector(B+4,.1f,.2f,.3f);
        }
    }
    return !c.trap_pending;
}
void setup(Fixture& f,unsigned kind){
    if(kind<24){
        f.scene(kind%6==0?0:kind%6==1?1:kind%6==2?3:16,kind%2);
        if(kind>=6 && kind<12){f.cpu.pc=0x8C028BFEu;f.cpu.r[4]=Q;}
        else if(kind>=12 && kind<18){f.cpu.pc=0x8C029B00u;f.cpu.r[4]=Q;f.cpu.r[5]=njs;}
        else if(kind>=18){f.cpu.pc=family::query_entry;f.cpu.r[5]=Q;f.cpu.r[6]=object;}
        else {f.vector(A+32,0,.5f,0);f.vector(B+4,.2f,-.1f,.15f);}
    }
    if(kind>=24 && kind<30){f.stage=kind&1?0x902:0x903;f.mutation=kind<26?0:kind<28?1:3;}
    if(kind>=30 && kind<34){f.mutation=kind-29;}
    if(kind>=34 && kind<41){
        constexpr std::uint32_t owners[]{0x8C075100u,0x8C075154u,0x8C075356u,0x8C0755C6u,0x8C0757A0u,0x8C075980u,0x8C075ECCu};
        f.cpu.pc=owners[kind-34];f.cpu.r[4]=A;f.cpu.r[5]=B;f.cpu.r[6]=C;f.cpu.r[7]=Q;
    }
}
int main(int argc,char** argv)try{
    require(argc==2,"movement-contact-tests <original-ram>");
    std::ifstream file(argv[1],std::ios::binary);const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000,"RAM size");unsigned cases=0;
    for(unsigned mode:{0u,1u,fpscr_fr_mask,fpscr_fr_mask|1u})for(unsigned kind=0;kind<41;++kind){
        std::cout<<"case="<<kind<<" mode="<<mode<<std::endl;
        Fixture a(image,mode),b(image,mode);setup(a,kind);setup(b,kind);a.oracle=&b;
        const auto result=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,Fixture::resume});
        b.until(a.cpu.pc);compare(a,b,"completion");
        if(result!=family::Outcome::Complete || a.cpu.pc!=returned || a.cpu.trap_pending)std::cerr<<"outcome="<<int(result)<<" pc="<<std::hex<<a.cpu.pc<<" tea="<<a.cpu.tea<<" trap="<<a.cpu.trap_pending<<std::dec<<std::endl;
        if(kind==31)require(result==family::Outcome::Interrupted && a.cpu.trap_pending,"original SZ-change fault");
        else require(result==family::Outcome::Complete && a.cpu.pc==returned && !a.cpu.trap_pending,"complete original return");
        ++cases;
    }
    std::cout<<"SONIC_MOVEMENT_CONTACT_PASS cases="<<cases<<" internal="<<family::counts.internal_calls<<" external="<<family::counts.callbacks<<" resumes="<<family::counts.resumes<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_MOVEMENT_CONTACT_FAIL "<<e.what()<<'\n';return 1;}
