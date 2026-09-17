// Complete native owner/admission proof against the installed original bytes.
#include "sonic_collision_candidates.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "test_collision_memory_support.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <vector>
using namespace katana::runtime;
namespace candidate=sonic::collision_candidates;
namespace {
constexpr auto entry=0x8C029B00u,returned=0x8CF80000u;
constexpr auto query=0x8CE00000u,object=0x8CE01000u,indices=0x8CE02000u,triangles=0x8CE03000u;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::string digest(std::span<const std::uint8_t> bytes){
#ifndef _WIN32
    return native_port_content_sha256(bytes);
#else
    std::array<unsigned char,32> result{};
    require(bytes.size()<=ULONG_MAX&&BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),result.data(),ULONG(result.size()))>=0,"SHA failed");
    constexpr char hex[]="0123456789abcdef";std::string text(64,'0');
    for(unsigned i=0;i<result.size();++i){text[2*i]=hex[result[i]>>4];text[2*i+1]=hex[result[i]&15];}return text;
#endif
}
struct Services final:PlatformServices {
    std::string_view name()const noexcept override{return "touch-poly-original-bytes";}
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
using Event=std::tuple<std::uint32_t,std::size_t,CodeWriteSource,bool>;
const auto code_ranges=[] {
    std::vector<NativePortImmutableRange> ranges;
    for(const auto s:candidate::source_spans())ranges.push_back({s.address&0x1FFFFFFFu,
        std::uint32_t(s.bytes.size()),native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)});
    return ranges;
}();
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    std::vector<Event> events;
    NativePortImmutableWriteGuard immutable{code_ranges};
    std::uint64_t calls=0;
    bool interrupt=false;
    Fixture(std::span<const std::uint8_t> image,unsigned count,bool reverse,std::uint32_t mode){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.pc=entry;cpu.pr=returned;cpu.gbr=0x8CE80000u;
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(mode);
        cpu.t=true;cpu.s=true;cpu.q=true;cpu.m=true;
        cpu.fpul=0x11223344u;cpu.mach=0xAABBCCDDu;cpu.macl=0x98765432u;
        for(unsigned i=0;i<16;++i){cpu.r[i]=0xABCD1000u+i;cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i+1));cpu.xf[i]=(i%5==0)?0x3F800000u:0u;}
        cpu.r[4]=query;cpu.r[5]=0x1234u;cpu.r[15]=0x8CF00000u;
        put(query,0x3F800000u);vector(query+4,{0.f,.5f,0.f});vector(query+16,{0.f,-.25f,.1f});
        put(0x8C02D548u,object);put(object,0u);put(object+8,cpu.r[5]);put(object+28,indices);put(object+36,count);
        ram->writable_bytes()[0x752B1Cu]=0x80u;
        put(0x8C88F5D8u,32u);put(0x8C88F5DCu,1u);put(0x8C88F538u,0x8CE90000u);
        for(unsigned i=0;i<16;++i){put(0x8C67C580u+i*4,(i%5==0)?0x3F800000u:0u);put(0x8CE90000u+i*4,0u);}
        for(unsigned i=0;i<count;++i){
            const auto tri=triangles+i*64u;put(indices+i*4u,tri);
            for(unsigned k=0;k<16;++k)put(tri+k*4,0u);
            vector(tri+24,{-5.f,0.f,-5.f});
            vector(tri+36,reverse?std::array<float,3>{0.f,0.f,5.f}:std::array<float,3>{5.f,0.f,-5.f});
            vector(tri+48,reverse?std::array<float,3>{5.f,0.f,-5.f}:std::array<float,3>{0.f,0.f,5.f});
        }
    }
    void put(std::uint32_t a,std::uint32_t value){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&value,4);}
    std::uint32_t peek(std::uint32_t a)const{std::uint32_t value;std::memcpy(&value,ram->bytes().data()+(a&0xFFFFFFu),4);return value;}
    void vector(std::uint32_t a,std::array<float,3> v){for(unsigned i=0;i<3;++i)put(a+4*i,std::bit_cast<std::uint32_t>(v[i]));}
    void observe(bool stable=true){cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{
        events.emplace_back(e.address,e.size,e.source,e.bytes_changed);immutable.observe_write(e);
    },stable?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:GuestWriteObserverContract::General);}
};
void run_original(CpuState& cpu,std::uint32_t stop){
    for(unsigned steps=0;cpu.pc!=stop&&steps<2000000u;++steps){
        require(cpu.pc>=0x8C010000u&&cpu.pc<0x8C680000u,"reference left installed resident code");
        // Existing SDK interpreter defect, isolated to these authenticated
        // libm FTRC instructions (same correction as triangle_contacts tests).
        if(cpu.pc==0x8C10FB7Eu||cpu.pc==0x8C10FBAAu){
            const auto source=cpu.pc==0x8C10FB7Eu?4u:3u;
            require(guest_fetch_u16(cpu,cpu.pc)==(source==4u?0xF43Du:0xF33Du),"FTRC identity changed");
            GuestInstructionAttempt attempt(cpu,cpu.pc,2u);cpu.pc+=2u;
            fpu_truncate_to_fpul(cpu,static_cast<std::uint8_t>(source));
        }else (void)execute_dynamic_sh4_block(cpu,services,1u);
        require(cpu.exception_generation==0u&&!cpu.trap_pending,"reference exception");
    }
    require(cpu.pc==stop,"reference did not return");
}
bool original_bridge(void* opaque,CpuState& cpu,std::uint32_t target){
    collision_test::bridge_boundary();
    auto& fixture=*static_cast<Fixture*>(opaque);
    static constexpr std::array allowed{0x8C10CF48u,0x8C10D038u,0x8C10CF98u,
        0x8C639E08u,0x8C639E9Cu,0x8C10CD1Cu};
    require(std::find(allowed.begin(),allowed.end(),target)!=allowed.end(),"unreviewed/debug callee");
    require(cpu.pc==target,"callee entry differs");
    ++fixture.calls;
    if(fixture.interrupt)return false;
    run_original(cpu,cpu.pr);
    return true;
}
bool native_body(Fixture& fixture){
    return candidate::try_execute(fixture.cpu,&fixture.immutable,{&fixture,original_bridge});
}
auto architecture(const CpuState& c){
    return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,
        c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.mach,c.macl,c.fpul,
        c.read_fpscr(),c.sr,c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,c.last_exception_cause);
}
void compare(Fixture& n,Fixture& r){
    collision_test::Comparison observers(n,r);
    try{require(native_body(n),"body did not return");}
    catch(...){std::cerr<<"native phase, last callee pc="<<std::hex<<n.cpu.pc<<std::dec<<'\n';throw;}
    try{run_original(r.cpu,returned);}
    catch(...){std::cerr<<"original phase, pc="<<std::hex<<r.cpu.pc<<std::dec<<'\n';throw;}
    require(architecture(n.cpu)==architecture(r.cpu),"architecture differs");
    require(std::equal(n.ram->bytes().begin(),n.ram->bytes().end(),r.ram->bytes().begin()),"RAM differs");
    require(observers.product() || n.events==r.events,"ordered guest stores differ");
    observers.verify();
}
void decline(Fixture& f,bool stable=true,bool missing_guard=false){
    f.observe(stable);const auto state=architecture(f.cpu);
    const auto bytes=std::vector<std::uint8_t>(f.ram->bytes().begin(),f.ram->bytes().end());
    require(!candidate::try_execute(f.cpu,missing_guard?nullptr:&f.immutable,{&f,original_bridge}),"unsafe call admitted");
    require(state==architecture(f.cpu) && f.events.empty() && f.calls==0u &&
        std::equal(bytes.begin(),bytes.end(),f.ram->bytes().begin()),"fallback mutated state");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"usage: collision_candidates_tests <authenticated-full-ram>");
    std::ifstream input(argv[1],std::ios::binary);std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(input),{}};
    require(image.size()==0x1000000u&&digest(image)=="b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c","RAM identity differs");
    unsigned cases=0,full_contacts=0,rejections=0;std::uint64_t calls=0;
    for(auto mode:{fpscr_dn_mask,fpscr_dn_mask|1u,fpscr_dn_mask|fpscr_fr_mask})
    for(unsigned count:{0u,1u,2u,16u})for(bool reverse:{false,true}){
        Fixture n(image,count,reverse,mode),r(image,count,reverse,mode);
        std::cout<<"case="<<cases<<" count="<<count<<" reverse="<<reverse<<" mode="<<mode<<'\n';
        compare(n,r);calls+=n.calls;++cases;
        if(n.peek(0x8C73E340u)==16u){require(n.peek(0x8C88F5DCu)==2u,"original full-contact skipped-pop behavior changed");++full_contacts;}
    }
    for(auto radius:{0u,0xBF800000u,0x7FC12345u}){
        Fixture n(image,0,false,fpscr_dn_mask),r(image,0,false,fpscr_dn_mask);n.put(query,radius);r.put(query,radius);compare(n,r);++cases;
    }
    // Actual P0/P2 data aliases, missing selectors, distant/tilted contacts,
    // movement directions and the producer's maximum candidate count.
    for(unsigned variant=0;variant<16u;++variant){
        const auto count=variant==15u?96u:3u;
        Fixture n(image,count,false,fpscr_dn_mask),r(image,count,false,fpscr_dn_mask);
        for(auto* f:{&n,&r}){
            if(variant==0u)f->cpu.r[4]=query&0x1FFFFFFFu;
            else if(variant==1u)f->cpu.r[4]=query|0x20000000u;
            else if(variant==2u)f->put(0x8C02D548u,0u);
            else if(variant==3u)f->cpu.r[5]=0x1235u;
            else {
                f->vector(query+4,{float(int(variant%4u)-2),float(variant)*.15f,0.f});
                f->vector(query+16,{.1f*float(variant),-.2f,-.3f});
                for(unsigned i=0;i<count;++i){
                    const auto a=triangles+i*64u;
                    const float height=float(i%3u)*.2f;
                    f->vector(a+24,{-5.f,height,-5.f});
                    f->vector(a+36,{5.f,height+.7f*float(variant%3u),-5.f});
                    f->vector(a+48,{0.f,height,5.f});
                    f->put(a+60,variant%2u?0x00800000u:0u);
                }
            }
        }
        compare(n,r);calls+=n.calls;++cases;
    }
    for(unsigned kind=0;kind<25;++kind){
        Fixture f(image,1,false,fpscr_dn_mask);
        switch(kind){
        case 0:f.ram->writable_bytes()[0x752B1Cu]=0u;break;
        case 1:f.put(0x8C88F5DCu,0u);break;
        case 2:f.put(0x8C88F5DCu,32u);break;
        case 3:f.put(object+36u,97u);break;
        case 4:f.put(object+36u,0xFFFFFFFFu);break;
        case 5:f.put(object+28u,query+40u);break;
        case 6:f.put(indices,0x8CEFFFF0u);break;
        case 7:f.cpu.r[4]=0x8C73E320u;break;
        case 8:f.cpu.r[4]=entry;break;
        case 9:f.cpu.r[15]=query+128u;break;
        case 10:f.cpu.r[15]=0u;break;
        case 11:f.cpu.r[4]=query+1u;break;
        case 12:f.put(0x8C88F538u,0x8C67C580u);break;
        case 13:f.put(0x8C88F538u,0x8C88F538u);break;
        case 14:f.put(object+8u,0u);f.put(object,object);break;
        case 15:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_pr_mask);break;
        case 16:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);break;
        case 17:f.cpu.write_fpscr(0u);break;
        case 18:f.cpu.write_sr(sr_md_mask|sr_fd_mask);break;
        case 19:f.cpu.write_sr(0u);break;
        case 20:f.cpu.r[4]=query&0x1FFFFFFFu;f.cpu.mmucr=1u;break;
        case 21:f.put(entry,f.peek(entry)^1u);break;
        case 22:f.put(0x8C10CDBCu,f.peek(0x8C10CDBCu)^1u);break;
        default:break;
        }
        decline(f,kind!=23,kind==24);++rejections;
    }
    {Fixture f(image,1,false,fpscr_dn_mask),r(image,1,false,fpscr_dn_mask);
        collision_test::Comparison observers(f,r);f.interrupt=true;
        bool aborted=false;try{(void)native_body(f);}catch(const std::runtime_error&){aborted=true;}
        require(aborted&&f.calls==1u&&(observers.product() || !f.events.empty()),"interrupted owner did not abort after mutation");
        observers.verify();}
    require(full_contacts>0u,"full-contact skipped-pop path was not exercised");
    std::cout<<"SONIC_COLLISION_BODY_PASS cases="<<cases<<" original_callees="<<calls
        <<" full_contact_cases="<<full_contacts<<" safe_rejections="<<rejections<<" interrupted_abort=1\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
