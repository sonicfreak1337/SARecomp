#include "sonic_collision_world.hpp"
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
namespace family=sonic::collision_world;
constexpr std::uint32_t returned=0x8C010000u,A=0x8CE00000u,B=0x8CE01000u,C=0x8CE02000u,Q=0x8CE03000u,
    objects=0x8C6BB1BCu,tasks=0x8CE70000u,works=0x8CE80000u;
constexpr std::uint32_t polygons=0x8C6BE1BCu,njs=0x8CE30000u,models=0x8CE40000u,vertices=0x8CE50000u,meshes=0x8CE60000u;
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
struct Record {std::uint32_t flags,key,task;};
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{ranges()};
    Fixture* oracle{};
    std::vector<Record> records;
    std::vector<std::pair<std::uint32_t,std::uint32_t>> trace;
    unsigned steps{},mutation{},callback_index{},stop_after{};
    Fixture(std::span<const std::uint8_t> image,unsigned mode){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|mode);
        for(unsigned i=0;i<16;++i){cpu.r[i]=0xA5010000u+i;cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i)+.125f);cpu.xf[i]=(i%5==0)?0x3F800000u:0u;}
        for(unsigned i=0;i<8;++i)cpu.r_bank[i]=0xD4000000u+i;
        cpu.fpul=0xA9876543u;cpu.mach=0x12345678u;cpu.macl=0x87654321u;
        cpu.r[15]=0x8CF00000u;cpu.pc=family::entry;cpu.pr=returned;
        clear(objects,192*64+8192*64);clear(njs,0x60000);
        byte(0x8C752B1Cu,0x80);put(0x8C195F84u,1);put(0x8C73ED04u,0);
        put(0x8C88F5D8u,64);put(0x8C88F5DCu,1);put(0x8C88F538u,0x8CE90000u);
        for(unsigned i=0;i<16;++i){put(0x8C67C580u+i*4,(i%5==0)?0x3F800000u:0u);put(0x8CE90000u+i*4,(i%5==0)?0x3F800000u:0u);}
        put(0x8C18B72Cu,Q);vector(Q+32,0,0,0);put(0x8C78C548u,0);put(0x8C78C54Cu,0);
        half(0x8C759644u,0);put(0x8C759634u,0);putf(0x8C19E8ACu,40.f);
        setup_pools(0,128);
        for(unsigned i=0;i<32;++i)model(i,0);
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{immutable.observe_write(e);},GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        cpu.memory.set_guest_write_batch_observer({&immutable,[](void*,std::span<const GuestWriteEvent>)noexcept{return true;},
            [](void* g,std::span<const GuestWriteEvent> w)noexcept{for(const auto& e:w)static_cast<NativePortImmutableWriteGuard*>(g)->observe_write(e);}});
        sonic::scalar_writes::bind(cpu.memory,immutable,cpu.memory.guest_write_observer_generation());
    }
    ~Fixture(){sonic::scalar_writes::unbind(&cpu.memory,&immutable);}
    void clear(std::uint32_t a,std::size_t n){std::fill_n(ram->writable_bytes().data()+(a&0xFFFFFFu),n,0);}
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4);}
    std::uint32_t get(std::uint32_t a){std::uint32_t v;std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4);return v;}
    void half(std::uint32_t a,std::uint16_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,2);}
    void byte(std::uint32_t a,std::uint8_t v){ram->writable_bytes()[a&0xFFFFFFu]=v;}
    void putf(std::uint32_t a,float v){put(a,std::bit_cast<std::uint32_t>(v));}
    void vector(std::uint32_t a,float x,float y,float z){putf(a,x);putf(a+4,y);putf(a+8,z);}
    void setup_pools(unsigned active,unsigned pool){
        put(0x8C02D548u,active?objects:0);
        for(unsigned i=0;i<active;++i){const auto p=objects+i*64;put(p,i+1<active?p+64:0);put(p+4,i?p-64:0u-0x8C02D548u);put(p+8,njs+i*256);}
        put(0x8C02D544u,objects+active*64);
        for(unsigned i=active;i<192;++i)put(objects+i*64,i+1<192?objects+(i+1)*64:0);
        put(0x8C02D540u,polygons);
        for(unsigned i=0;i<pool;++i)put(polygons+i*64,i+1<pool?polygons+(i+1)*64:0);
    }
    void model(unsigned i,unsigned kind){
        const auto n=njs+i*256,m=models+i*256,v=vertices+i*256,mesh=meshes+i*64;
        put(n+4,m);vector(n+32,1,1,1);put(m,v);put(m+12,mesh);half(m+20,1);putf(m+36,10);
        vector(v,0,0,0);vector(v+12,2,0,0);vector(v+24,0,0,2);vector(v+36,2,0,2);
        half(mesh,std::uint16_t(kind));half(mesh+2,1);put(mesh+4,mesh+16);
        const unsigned count=kind==0?3:4;
        if(kind&0x8000){half(mesh+16,std::uint16_t(4|(kind==0xC000?0x8000:0)));for(unsigned j=0;j<4;++j)half(mesh+18+j*2,std::uint16_t(j));}
        else for(unsigned j=0;j<count;++j)half(mesh+16+j*2,std::uint16_t(j));
    }
    static bool external(std::uint32_t p){
        switch(p){case 0x8C052E30u:case 0x8C638E0Cu:case 0x8C639BB0u:case 0x8C639AD8u:
        case 0x8C63A744u:case 0x8C639E08u:case 0x8C639E9Cu:case 0x8C63A10Cu:case 0x8C63A52Cu:
        case 0x8C63A820u:case 0x8C63A904u:case 0x8C640862u:case 0x8C6409C0u:case 0x8C6406A6u:case 0x8C10C99Cu:return true;default:return false;}
    }
    bool child(std::uint32_t target){
        trace.emplace_back(target,cpu.pr);++callback_index;
        if(stop_after==callback_index)return false;
        if(target==0x8C052E30u){
            half(0x8C754E30u,std::uint16_t(records.size()));
            for(unsigned i=0;i<records.size();++i){const auto p=0x8C754E34u+i*12;put(p,records[i].flags);put(p+4,records[i].key);put(p+8,records[i].task);}
            cpu.pc=cpu.pr;
        }else if(target==0x8C640862u || target==0x8C6409C0u || target==0x8C6406A6u || target==0x8C10C99Cu){cpu.pc=cpu.pr;}
        else{
            const auto end=cpu.pr;
            do{require(++steps<3000000u,"SDK instruction bound");(void)execute_dynamic_sh4_block(cpu,services,1u);require(!cpu.trap_pending,"SDK trap");}while(cpu.pc!=end);
        }
        if(mutation && callback_index==1){
            if(mutation==1)cpu.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{});
            if(mutation==2)put(njs+4,models+1);
        }
        return true;
    }
    static bool invoke(void* p,CpuState& c,std::uint32_t target){
        auto& f=*static_cast<Fixture*>(p);require(&c==&f.cpu && external(target),"unexpected external");
        if(f.oracle){advance(*f.oracle);compare(f,*f.oracle,"callback entry");}
        const auto ok=f.child(target);
        if(f.oracle){require(f.oracle->child(target)==ok,"callback result");compare(f,*f.oracle,"callback return");}
        return ok;
    }
};
void advance(Fixture& f){
    while(f.cpu.pc!=returned && !Fixture::external(f.cpu.pc)){
        require(++f.steps<3000000u,"world instruction bound");
        (void)execute_dynamic_sh4_block(f.cpu,services,1u);require(!f.cpu.trap_pending,"original trap");
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
    if(variant==0){f.put(0x8C195F84u,0);return;}
    unsigned count=variant<5?variant:4;
    for(unsigned i=0;i<count;++i)f.records.push_back({0,njs+i*256,0});
    if(variant>=5 && variant<=8){f.setup_pools(4,128);}
    if(variant==6)f.records.erase(f.records.begin());
    if(variant==7)f.records[1].flags=0x08000000u;
    if(variant==8)f.put(0x8C73ED04u,njs);
    if(variant==9)f.records[1]=f.records[0];
    if(variant==10){f.setup_pools(4,128);f.records[0].flags=0x08000000u;f.records.insert(f.records.begin()+1,{0,njs,0});}
    if(variant==11){f.setup_pools(4,128);f.records.insert(f.records.begin()+1,{0x08000000u,njs,0});}
    if(variant>=12 && variant<=14)for(unsigned i=0;i<count;++i)f.model(i,variant==12?0x4000:(variant==13?0x8000:0xC000));
    if(variant==15){f.put(njs+44,njs+256);f.put(njs+256+48,njs+512);}
    if(variant==16){f.setup_pools(0,2);f.put(njs+44,njs+256);}
    if(variant==17){for(unsigned i=0;i<count;++i){f.put(njs+i*256+20,0x1000);f.put(njs+i*256+24,0x2000);f.put(njs+i*256+28,0x3000);f.vector(njs+i*256+32,1.5f,.5f,2);}}
    if(variant==18){f.cpu.r[15]|=0x20000000u;f.records[0].key|=0x20000000u;}
    if(variant==19){f.setup_pools(4,128);f.records.clear();}
    if(variant==20){
        f.setup_pools(192,128);f.put(0x8C02D544u,0);f.records.clear();
        for(unsigned i=0;i<192;++i){f.records.push_back({0,njs+(i%32)*256,0});f.put(objects+i*64+8,njs+(i%32)*256);}
    }
    if(variant==21){f.put(0x8C195F84u,0);f.half(0x8C759644u,4);for(unsigned i=0;i<4;++i){const auto p=0x8C759648u+i*12;f.put(p,0);f.put(p+4,njs+i*256);f.put(p+8,tasks+i*64);f.put(tasks+i*64+32,works+i*64);f.half(works+i*64+4,0x100);}}
    if(variant==22){f.setup_pools(4,128);f.put(objects+8,0);f.records[0].key=njs;}
    if(variant==23){f.setup_pools(4,128);f.records[0].key|=0x20000000u;}
    if(variant==24){f.setup_pools(4,128);f.put(objects+16,polygons+64*200);f.put(objects+20,polygons+64*200);f.records.erase(f.records.begin());}
    if(variant==25){f.records.clear();f.byte(0x8C752B1Cu,0);}
}
int main(int argc,char** argv)try{
    require(argc==2,"world-tests <original-ram>");std::ifstream file(argv[1],std::ios::binary);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};require(image.size()==0x1000000u,"original RAM size");
    unsigned cases=0;
    for(unsigned variant=0;variant<26;++variant)for(auto mode:{0u,1u,fpscr_fr_mask}){
        Fixture n(image,mode),r(image,mode);setup(n,variant);setup(r,variant);n.oracle=&r;
        std::cout<<"world case "<<std::dec<<variant<<" mode "<<mode<<'\n';
        const auto outcome=family::execute(n.cpu,&n.immutable,{&n,Fixture::invoke,nullptr});
        require(outcome==family::Outcome::Complete,"native did not complete");advance(r);compare(n,r,"return");
        require(n.trace==r.trace,"external call order");++cases;
    }
    unsigned coverage=0;for(bool v:family::visited)coverage+=v;
    std::cout<<"SONIC_COLLISION_WORLD_OK cases="<<cases<<" instructions_visited="<<coverage
        <<" membership="<<family::counts.membership_hits<<" eligibility="<<family::counts.eligibility_hits<<" internal="<<family::counts.internal_calls<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
