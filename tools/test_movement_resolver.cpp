#include "sonic_movement_resolver.hpp"
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
namespace family=sonic::movement;
constexpr std::uint32_t returned=0x8C010000u,A=0x8CE00000u,B=0x8CE01000u,C=0x8CE02000u,Q=0x8CE03000u,
    objects=0x8CE10000u,tasks=0x8CE20000u,works=0x8CE30000u;
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
auto architecture(const CpuState& c){return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,
    c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.mach,c.macl,c.fpul,
    c.read_fpscr(),c.fpscr,c.sr,c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,c.last_exception_cause);}
struct Services final:PlatformServices {
    std::string_view name()const noexcept override{return "movement-original-bytes";}
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
    std::vector<std::pair<std::uint32_t,std::uint32_t>> trace;
    std::vector<std::uint32_t> second_order;
    std::uint32_t last_object{};
    int candidate_count=1;
    unsigned flags=0x4000u,stage=0,hit=1,priority=0,answer=1,alternate=2,mutation=0,stop_after=0;
    unsigned invalidate_after=0,invalidation=0,unsafe=0,angle=0;
    float length_value=0.f,dot_value=1.f;
    bool real_length=false;
    unsigned steps=0;
    Fixture(std::span<const std::uint8_t> image,unsigned mode){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|mode);
        for(unsigned i=0;i<16;++i){cpu.r[i]=0xA5010000u+i;cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i)+.125f);cpu.xf[i]=0xB5110000u+i;}
        for(unsigned i=0;i<8;++i)cpu.r_bank[i]=0xD4000000u+i;
        cpu.fpul=0xA9876543u;cpu.mach=0x12345678u;cpu.macl=0x87654321u;
        cpu.r[4]=A;cpu.r[5]=B;cpu.r[6]=C;cpu.r[15]=0x8CF00000u;cpu.pc=family::entry;cpu.pr=returned;
        for(auto base:{A,B,C,Q})std::fill_n(ram->writable_bytes().data()+(base&0xFFFFFFu),512u,0u);
        put(C+100,Q);half(A+4,1u);putf(C+196,10.f);putf(C+272,2.f);
        vector(0x8C754B1Cu,0,1,0);put(0x8C1BDF20u,0);vector(0x8C6BAC30u+32,0,0,0);
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{immutable.observe_write(e);},GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        cpu.memory.set_guest_write_batch_observer({&immutable,[](void*,std::span<const GuestWriteEvent>)noexcept{return true;},
            [](void* g,std::span<const GuestWriteEvent> w)noexcept{for(const auto& e:w)static_cast<NativePortImmutableWriteGuard*>(g)->observe_write(e);}});
        sonic::scalar_writes::bind(cpu.memory,immutable,cpu.memory.guest_write_observer_generation());
    }
    ~Fixture(){sonic::scalar_writes::unbind(&cpu.memory,&immutable);}
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4);}
    std::uint32_t get(std::uint32_t a){std::uint32_t v;std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4);return v;}
    void half(std::uint32_t a,std::uint16_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,2);}
    void byte(std::uint32_t a,std::uint8_t v){ram->writable_bytes()[a&0xFFFFFFu]=v;}
    void putf(std::uint32_t a,float v){put(a,std::bit_cast<std::uint32_t>(v));}
    void vector(std::uint32_t a,float x,float y,float z){putf(a,x);putf(a+4,y);putf(a+8,z);}
    static bool external(std::uint32_t p){
        switch(p){case 0x8C04F7E0u:case 0x8C077720u:case 0x8C028BFEu:case 0x8C029B00u:case 0x8C049A4Eu:
        case 0x8C055C9Au:case 0x8C055CECu:case 0x8C06CC62u:case 0x8C074214u:case 0x8C074712u:
        case 0x8C074C30u:case 0x8C074E24u:case 0x8C074FAEu:case 0x8C075100u:case 0x8C075154u:
        case 0x8C075356u:case 0x8C0757A0u:case 0x8C075980u:case 0x8C078DF0u:case 0x8C078F40u:
        case 0x8C10CF98u:case 0x8C10D038u:case 0x8C638FD0u:case 0x8C63A69Cu:case 0x8C63A88Cu:
        case 0x8C63FFC0u:case 0x8C640068u:return true;default:return false;}
    }
    bool stub(std::uint32_t target){
        auto& c=cpu;trace.emplace_back(target,c.pr);
        if(stop_after==trace.size()){c.r[0]=0xBADCA11u;return false;}
        if(real_length && target==0x8C63A69Cu){
            const auto ret=c.pr;unsigned n=0;
            do{(void)execute_dynamic_sh4_block(c,services,1u);require(!c.trap_pending,"real length trap");}while(c.pc!=ret && ++n<32u);
            require(c.pc==ret,"real length return");return true;
        }
        switch(target){
        case 0x8C04F7E0u:c.r[0]=stage;break;
        case 0x8C077720u:c.r[0]=0x77720u;break;
        case 0x8C028BFEu:
            half(0x8C754E30u,std::uint16_t(candidate_count));
            for(int i=0;i<candidate_count;++i){put(0x8C754E34u+i*12,flags);put(0x8C754E38u+i*12,objects+i*256);put(0x8C754E3Cu+i*12,mutation==8?tasks+i*64:0);put(tasks+i*64+32,works+i*64);half(works+i*64+4,0x100u);}
            if(unsafe==1)c.r[14]=Q+1;
            if(unsafe==2)c.r[14]=family::entry;
            break;
        case 0x8C029B00u:
            last_object=c.r[5];
            if(c.pr==0x8C0735AAu)second_order.push_back(last_object);
            require(c.r[4]==Q || unsafe==1 || unsafe==2,"TOUCH query");vector(Q+72,0,1,0);half(Q+42,0);
            for(unsigned n=84;n<120;n+=4)put(Q+n,0);c.r[0]=hit;
            if(mutation==1 && c.r[5]==objects)half(0x8C754E30u,1);
            if(mutation==2 && c.r[5]==objects)put(0x8C754E34u+12,0x8000u);
            if(mutation==3)vector(Q+72,1,0,0);
            if(mutation==4){vector(Q+84,1,2,3);vector(Q+108,.25f,0,.5f);}
            break;
        case 0x8C075356u:case 0x8C0757A0u:c.r[0]=(priority && last_object==objects+(priority-1)*256)?1:0;break;
        case 0x8C049A4Eu:c.r[0]=mutation==5?1:0;break;
        case 0x8C055CECu:case 0x8C055C9Au:c.r[0]=mutation==6?0x2000:angle;break;
        case 0x8C638FD0u:c.fr[0]=std::bit_cast<std::uint32_t>(dot_value);break;
        case 0x8C63A69Cu:c.fr[0]=std::bit_cast<std::uint32_t>(length_value);break;
        case 0x8C10CF98u:case 0x8C10D038u:case 0x8C640068u:c.fr[0]=0;break;
        case 0x8C074214u:c.r[0]=1;break;
        case 0x8C075980u:
            c.r[0]=answer;
            if(unsafe==3)c.r[14]=0x8CFFFFB0u;
            break;
        case 0x8C074712u:c.r[0]=alternate;break;
        case 0x8C06CC62u:putf(c.r[6],0);c.r[0]=0;break;
        case 0x8C63FFC0u:{std::array<std::uint32_t,3> v{};for(unsigned i=0;i<3;++i)v[i]=get(c.r[4]+i*4);for(unsigned i=0;i<3;++i)put(c.r[6]+i*4,v[i]);break;}
        default:break;
        }
        c.pc=c.pr;
        if(invalidate_after==trace.size()){
            if(invalidation==0)c.write_fpscr(fpscr_dn_mask|fpscr_pr_mask);
            if(invalidation==1)c.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{});
            if(invalidation==2)byte(family::entry,0);
            if(invalidation==3)c.pc=0x8C010004u;
            if(invalidation==4)c.memory.clear_direct_linear_alias_window();
        }
        return true;
    }
    static bool child(void* opaque,CpuState& cpu,std::uint32_t target){
        auto& a=*static_cast<Fixture*>(opaque);require(&cpu==&a.cpu,"wrong CPU");require(external(target),"unexpected child");
        if(a.oracle){advance(*a.oracle);compare(a,*a.oracle,"callback entry");}
        const bool ok=a.stub(target);
        if(a.oracle){require(a.oracle->stub(target)==ok,"callback outcome");compare(a,*a.oracle,"callback exit");}
        return ok;
    }
};
void advance(Fixture& f){
    while(f.cpu.pc!=returned && !Fixture::external(f.cpu.pc)){
        require(++f.steps<200000u,"original owner instruction bound");
        require(f.cpu.pc>=family::entry && f.cpu.pc<family::end,"original left owner");
        (void)execute_dynamic_sh4_block(f.cpu,services,1u);
        require(!f.cpu.trap_pending,"original owner trapped");
    }
}
void compare(Fixture& a,Fixture& b,const char* phase){
    if(architecture(a.cpu)!=architecture(b.cpu)){
        std::cerr<<phase<<" architecture mismatch PC "<<std::hex<<a.cpu.pc<<' '<<b.cpu.pc<<" PR "<<a.cpu.pr<<' '<<b.cpu.pr<<'\n';
        for(unsigned i=0;i<16;++i){if(a.cpu.r[i]!=b.cpu.r[i])std::cerr<<"R"<<std::dec<<i<<' '<<std::hex<<a.cpu.r[i]<<' '<<b.cpu.r[i]<<'\n';if(a.cpu.fr[i]!=b.cpu.fr[i])std::cerr<<"FR"<<std::dec<<i<<' '<<std::hex<<a.cpu.fr[i]<<' '<<b.cpu.fr[i]<<'\n';}
        std::cerr<<"FPSCR "<<a.cpu.read_fpscr()<<' '<<b.cpu.read_fpscr()<<" T "<<a.cpu.t<<' '<<b.cpu.t<<'\n';throw std::runtime_error("architecture differs");
    }
    if(std::memcmp(a.ram->bytes().data(),b.ram->bytes().data(),0x1000000u)){
        auto d=std::mismatch(a.ram->bytes().begin(),a.ram->bytes().end(),b.ram->bytes().begin());
        std::cerr<<phase<<" RAM mismatch "<<std::hex<<0x8C000000u+std::size_t(d.first-a.ram->bytes().begin())<<'\n';throw std::runtime_error("RAM differs");
    }
}
void setup(Fixture& f,unsigned v){
    if(v<6){const int counts[]{0,-1,1,4,16,17};f.candidate_count=counts[v];}
    if(v==6)f.stage=0x902;
    if(v==7)f.stage=0x903;
    if(v==8)f.hit=0;
    if(v==9)f.flags=0x4040;
    if(v==10)f.flags=0;
    if(v==11)f.flags=0x4001;
    if(v==12)f.flags=0x8000;
    if(v==13)f.flags=0x4008;
    if(v==14)f.answer=0;
    if(v==15)f.answer=2;
    if(v==16 || v==17){f.candidate_count=4;f.priority=2;f.put(C+116,objects+512);f.put(C+108,v==16?tasks:0);}
    if(v>=18 && v<26){f.candidate_count=4;f.mutation=v-17;}
    if(v==26)f.put(C+96,0x4000);
    if(v==27)f.half(C+16,3);
    if(v==28)f.half(A+4,0);
    if(v==29)f.byte(A+9,6);
    if(v==30)f.byte(A,60);
    if(v==31)f.real_length=true;
    if(v==32){f.vector(A+32,1,2,3);f.vector(B+4,.25f,.5f,.75f);}
    if(v==33){f.cpu.pc=family::body;f.cpu.r[15]&=0x1FFFFFFFu;}
    if(v==34)f.angle=0x3000;
    if(v==35){f.angle=0x4000;f.mutation=3;f.dot_value=-.5f;}
    if(v==36){f.length_value=4.f;f.vector(B+4,2,1,.5f);f.real_length=true;}
    if(v==37){f.mutation=4;f.length_value=16.f;f.angle=0x3000;f.vector(B+4,1,2,3);}
    if(v==38){f.mutation=8;f.candidate_count=4;f.priority=2;f.half(C+16,3);}
    if(v==39){f.flags=0x4040;f.alternate=1;}
    if(v==40){f.flags=0x4040;f.alternate=0;}
}
int main(int argc,char** argv){try{
    require(argc==2 || argc==3,"usage: sonic-movement-tests RAM [coverage-file]");
    std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000u,"RAM image size");unsigned cases=0;
    const bool only_frontiers=std::getenv("SARECOMP_TEST_MOVEMENT_FRONTIERS")!=nullptr;
    if(!only_frontiers)for(unsigned mode:{0u,1u,fpscr_fr_mask,fpscr_fr_mask|1u})for(unsigned v=0;v<41;++v){
        std::cout<<"case="<<v<<" mode="<<mode<<'\n';
        Fixture a(image,mode),b(image,mode);setup(a,v);setup(b,v);a.oracle=&b;
        require(family::execute(a.cpu,&a.immutable,{&a,Fixture::child})==family::Outcome::Complete,"native admission/outcome");
        advance(b);require(a.cpu.pc==returned && b.cpu.pc==returned,"complete return");compare(a,b,"return");++cases;
        if(v==16 || v==17){
            const std::vector<std::uint32_t> expected=v==16?
                std::vector<std::uint32_t>{objects+256,objects,objects+768,objects+512}:
                std::vector<std::uint32_t>{objects+256,objects+768,objects,objects+512};
            require(a.second_order==expected,"selected/previous contact order");
        }
    }
    if(!only_frontiers)for(unsigned v=0;v<15;++v){
        Fixture f(image,0);
        if(v==0)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_pr_mask);
        if(v==1)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);
        if(v==2)f.cpu.write_fpscr(fpscr_dn_mask|2u);
        if(v==3)f.cpu.write_fpscr(0);
        if(v==4)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_exception_enable_mask);
        if(v==5)f.cpu.sr|=sr_fd_mask;
        if(v==6)f.cpu.write_sr(0);
        if(v==7)f.cpu.trap_pending=true;
        if(v==8)f.cpu.sleeping=true;
        if(v==9)f.cpu.r[15]=0x8C000001u;
        if(v==10)f.cpu.r[4]=3;
        if(v==11)f.put(C+100,1);
        if(v==12)f.byte(family::entry,0);
        if(v==13)f.byte(0x8C0742C0u,0);
        if(v==14)f.cpu.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{});
        const auto before=architecture(f.cpu);const std::vector<std::uint8_t> bytes(f.ram->bytes().begin(),f.ram->bytes().end());
        require(family::execute(f.cpu,&f.immutable,{&f,Fixture::child})==family::Outcome::Declined,"unsafe admission");
        require(architecture(f.cpu)==before && std::ranges::equal(bytes,f.ram->bytes()),"decline mutated input");++cases;
    }
    if(!only_frontiers)for(unsigned stop=1;stop<=18;++stop){
        Fixture a(image,0),b(image,0);a.stop_after=b.stop_after=stop;a.oracle=&b;
        auto result=family::execute(a.cpu,&a.immutable,{&a,Fixture::child});
        if(a.trace.size()>=stop){require(result==family::Outcome::Interrupted,"interrupted callback restarted");compare(a,b,"interrupt");}
        else {require(result==family::Outcome::Complete,"unexpected finish");advance(b);compare(a,b,"finish");}
        ++cases;
    }
    // A successful child may still invalidate the native context. Publish its
    // actual return state, and never restart an already completed callback.
    for(unsigned kind=0;kind<5;++kind)for(unsigned after:{1u,4u,8u}){
        std::cerr<<"invalidation="<<kind<<" after="<<after<<'\n';
        Fixture a(image,0),b(image,0);a.oracle=&b;
        a.invalidate_after=b.invalidate_after=after;a.invalidation=b.invalidation=kind;
        const auto expected=kind==3?family::Outcome::Interrupted:family::Outcome::ResumeOriginal;
        require(family::execute(a.cpu,&a.immutable,{&a,Fixture::child})==expected,"successful child invalidation");
        require(a.trace.size()==after,"callback repeated after invalidation");compare(a,b,"invalidated return");++cases;
    }
    // Leave unsupported RAM operations to the original owner at its exact
    // pre-instruction frontier, including a JSR with an unsafe delay-slot load.
    for(unsigned kind=0;kind<4;++kind){
        std::cerr<<"unsafe-memory="<<kind<<'\n';
        Fixture a(image,0),b(image,0);a.oracle=&b;
        if(kind==0){a.cpu.r[4]=b.cpu.r[4]=family::entry;a.cpu.pc=b.cpu.pc=family::body;}
        else {a.unsafe=b.unsafe=kind;if(kind==3)a.flags=b.flags=0;}
        require(family::execute(a.cpu,&a.immutable,{&a,Fixture::child})==family::Outcome::ResumeOriginal,"unsafe memory restart");
        while(b.cpu.pc!=a.cpu.pc){
            require(++b.steps<200000u && !Fixture::external(b.cpu.pc),"restart frontier not reached");
            (void)execute_dynamic_sh4_block(b.cpu,services,1u);require(!b.cpu.trap_pending,"restart already performed faulting access");
        }
        compare(a,b,"pre-instruction restart");
        require(!a.immutable.write_detected(),"unsafe native store was performed");
        if(kind==0)require(a.cpu.pc==0x8C0730E2u,"code-write frontier");
        if(kind==3)require(a.cpu.pc==0x8C073A40u && a.cpu.pr==0x8C073994u,"call delay PR rollback");
        ++cases;
    }
    if(argc==3){std::ofstream out(argv[2]);for(unsigned i=0;i<family::visited.size();++i)if(family::visited[i])out<<std::hex<<family::entry+i*2<<'\n';}
    std::cout<<"SONIC_MOVEMENT_TESTS_OK cases="<<cases<<" visited="<<std::count(family::visited.begin(),family::visited.end(),true)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_MOVEMENT_TESTS_FAILED "<<e.what()<<'\n';return 1;}}
