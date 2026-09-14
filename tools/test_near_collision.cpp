#include "sonic_near_collision.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
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
namespace family=sonic::near_collision;
namespace {
constexpr auto returned=0x8CF80000u,query=0x8CE00000u,object=0x8C6BB1BCu,nodes=0x8C6BE1BCu;
constexpr auto first=0x8C757E34u,second=0x8C758434u,njs=0x8CE30000u,model=0x8CE40000u;
void require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
std::string digest(std::span<const std::uint8_t> bytes){
    std::array<unsigned char,32> hash{};
    require(bytes.size()<=ULONG_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),hash.data(),ULONG(hash.size()))>=0,"SHA failed");
    constexpr char hex[]="0123456789abcdef";std::string out(64,'0');
    for(unsigned i=0;i<32;++i){out[2*i]=hex[hash[i]>>4];out[2*i+1]=hex[hash[i]&15];}return out;
}
struct Services final:PlatformServices {
    std::string_view name()const noexcept override{return "near-original-bytes";}
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
const auto code_ranges=[]{
    std::vector<NativePortImmutableRange> ranges;
    for(const auto s:family::source_spans())
        ranges.push_back({s.address&0x1FFFFFFFu,std::uint32_t(s.bytes.size()),
            native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)});
    return ranges;
}();
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{code_ranges};
    std::vector<Event> events;
    Fixture(std::span<const std::uint8_t> image,unsigned node_count,std::uint32_t mode){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.pc=family::entry;cpu.pr=returned;cpu.gbr=0x8CE80000u;
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(mode);cpu.t=true;cpu.s=true;cpu.q=true;cpu.m=true;
        cpu.fpul=0x11223344u;cpu.mach=0xABCD0123u;cpu.macl=0x10203040u;
        for(unsigned i=0;i<16;++i){
            cpu.r[i]=0xABCD1000u+i;cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i+1));
            cpu.xf[i]=(i%5==0)?0x3F800000u:0u;
        }
        cpu.r[4]=query;cpu.r[15]=0x8CF00000u;
        vector(query,{1.f,0.f,0.f});put(query+12u,0u);vector(query+16u,{0.f,0.f,0.f});
        ram->writable_bytes()[0x752B1Cu]=0x80u;
        put(0x8C88F5D8u,32u);put(0x8C88F5DCu,1u);put(0x8C88F538u,0x8CE90000u);
        for(unsigned i=0;i<16;++i){put(0x8C67C580u+i*4,(i%5==0)?0x3F800000u:0u);put(0x8CE90000u+i*4,0u);}
        put(0x8C19E8A4u,node_count?1u:0u);put(0x8C19E8A8u,0u);
        put(0x8C19E8B8u,0u);put(0x8C759634u,0u);
        put(first,0u);put(first+4u,njs);put(first+8u,123u);
        for(unsigned i=0;i<32;i+=4)put(njs+i,0u);
        put(njs+4u,model);vector(njs+8u,{0.f,0.f,0.f});
        for(unsigned i=0;i<40;i+=4)put(model+i,0u);
        put(model+36u,0x40000000u);
        put(0x8C02D548u,node_count?object:0u);
        for(unsigned i=0;i<64;i+=4)put(object+i,0u);
        put(object+8u,njs);put(object+16u,node_count?nodes:0u);
        put(object+20u,node_count?nodes+(node_count-1u)*64u:0u);
        put(object+28u,0x11111111u);put(object+36u,0x22222222u);put(object+40u,0x40000000u);
        for(unsigned i=0;i<node_count;++i){
            const auto node=nodes+i*64u;
            for(unsigned k=0;k<64;k+=4)put(node+k,0u);
            put(node,i+1u<node_count?node+64u:0u);put(node+4u,i?node-64u:0u);
            vector(node+8u,{.1f,.2f,(float(i)-float(node_count/2u))*.25f});
            put(node+20u,0x40000000u);
        }
    }
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4u);}
    std::uint32_t peek(std::uint32_t a)const{std::uint32_t v;std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4u);return v;}
    void vector(std::uint32_t a,std::array<float,3u> v){for(unsigned i=0;i<3u;++i)put(a+4u*i,std::bit_cast<std::uint32_t>(v[i]));}
    void observe(bool stable=true){
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{
            events.emplace_back(e.address,e.size,e.source,e.bytes_changed);immutable.observe_write(e);
        },stable?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:GuestWriteObserverContract::General);
    }
};
auto architecture(const CpuState& c){
    return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,
        c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.mach,c.macl,c.fpul,
        c.read_fpscr(),c.sr,c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,c.last_exception_cause);
}
void original(CpuState& c){
    for(unsigned steps=0;c.pc!=returned && steps<2000000u;++steps){
        require(c.pc>=0x8C010000u && c.pc<0x8C680000u,"reference left installed code");
        (void)execute_dynamic_sh4_block(c,services,1u);
        require(!c.trap_pending && !c.exception_generation,"reference exception");
    }
    require(c.pc==returned,"reference did not return");
}
void compare(Fixture& n,Fixture& r){
    n.observe();r.observe();require(family::try_execute(n.cpu,&n.immutable),"native declined valid fixture");
    original(r.cpu);
    if(architecture(n.cpu)!=architecture(r.cpu)){
        for(unsigned i=0;i<16;++i){
            if(n.cpu.r[i]!=r.cpu.r[i])std::cerr<<"R"<<i<<" "<<std::hex<<n.cpu.r[i]<<" vs "<<r.cpu.r[i]<<"\n";
            if(n.cpu.fr[i]!=r.cpu.fr[i])std::cerr<<"FR"<<i<<" "<<std::hex<<n.cpu.fr[i]<<" vs "<<r.cpu.fr[i]<<"\n";
            if(n.cpu.xf[i]!=r.cpu.xf[i])std::cerr<<"XF"<<i<<" "<<std::hex<<n.cpu.xf[i]<<" vs "<<r.cpu.xf[i]<<"\n";
        }
        std::cerr<<"FPSCR "<<std::hex<<n.cpu.read_fpscr()<<" vs "<<r.cpu.read_fpscr()<<"\n";
        throw std::runtime_error("architecture differs");
    }
    require(std::equal(n.ram->bytes().begin(),n.ram->bytes().end(),r.ram->bytes().begin()),"RAM differs");
    require(n.events==r.events,"ordered stores differ");
}
void decline(Fixture& f,bool stable=true){
    f.observe(stable);const auto state=architecture(f.cpu);
    const auto before=std::vector<std::uint8_t>(f.ram->bytes().begin(),f.ram->bytes().end());
    require(!family::try_execute(f.cpu,&f.immutable),"unsafe fixture admitted");
    require(state==architecture(f.cpu) && f.events.empty() &&
        std::equal(before.begin(),before.end(),f.ram->bytes().begin()),"decline mutated guest");
}
}
int main(int argc,char** argv)try{
    require(argc==2,"near_collision_tests <original-full-ram>");
    std::ifstream file(argv[1],std::ios::binary);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000u && digest(image)=="b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c","RAM identity");
    unsigned cases=0,rejections=0;
    for(auto mode:{fpscr_dn_mask,fpscr_dn_mask|1u,fpscr_dn_mask|fpscr_fr_mask})
    for(unsigned count:{0u,1u,12u,96u,120u}){
        Fixture n(image,count,mode),r(image,count,mode);
        if(count>=96)for(auto* f:{&n,&r})
            for(unsigned i=0;i<count;++i)f->vector(nodes+i*64u+8u,{.1f,.2f,0.f});
        std::cout<<"case "<<cases<<" nodes "<<count<<" mode "<<mode<<"\n";
        compare(n,r);++cases;
        if(count>=96)require(n.peek(0x8C73E1BCu)==96u,"original 96 candidate exit");
    }
    for(unsigned variant=0;variant<29;++variant){
        Fixture n(image,variant==25?120u:12u,fpscr_dn_mask),r(image,variant==25?120u:12u,fpscr_dn_mask);
        for(auto* f:{&n,&r}){
            if(variant<8){
                f->put(first,0x10000000u);
                f->put(njs+20u,(variant&1)?0xFFFF4321u:0u);
                f->put(njs+24u,(variant&2)?0x0000ABCDu:0u);
                f->put(njs+28u,(variant&4)?0x12344000u:0u);
                f->vector(model+24u,{1.f,-.25f,2.f});
            }else if(variant<12){
                f->put(0x8C19E8A4u,0u);f->put(0x8C19E8A8u,1u);
                f->put(0x8C19E8B8u,1u);f->put(0x8C759634u,1u);
                for(unsigned k=0;k<36;k+=4)f->put(second+k,0u);
                f->vector(second,{.5f,.25f,1.f});f->put(second+12u,0x40A00000u);
                f->put(second+24u,njs);f->put(second+32u,variant==11?0u:0x00400003u);
                if(variant==9)f->put(second+12u,0xBF800000u);
                if(variant==10)f->put(second,0x7FC12345u);
            }else if(variant<16){
                for(unsigned i=0;i<12;++i)f->vector(nodes+i*64u+8u,{.1f,.2f,variant==12?-10.f:(variant==13?10.f:(variant==14?-3.f:0.f))});
                if(variant==15)f->put(query+12u,0x7FC12345u);
            }else if(variant==16){
                f->cpu.r[4]=query&0x1FFFFFFFu;f->cpu.r[15]&=0x1FFFFFFFu;
            }else if(variant==17){
                f->cpu.r[4]=query+0x20000000u;f->cpu.r[15]+=0x20000000u;
            }else if(variant==18){
                f->put(object+8u,0xFFFFFFFFu);
            }else if(variant==19){
                f->put(0x8C19E8A4u,1024u);
                for(unsigned i=0;i<1024;++i){f->put(first+i*12u,0u);f->put(first+i*12u+4u,njs);f->put(first+i*12u+8u,i);}
            }else if(variant==20)f->put(object+40u,0x7FC12345u);
            else if(variant==21)f->put(query,0xBF800000u);
            else if(variant==22)f->put(model+36u,0x7F800000u);
            else if(variant==23){
                f->put(0x8C19E8A4u,0u);f->put(0x8C19E8A8u,1024u);f->put(0x8C19E8B8u,1u);
                for(unsigned i=0;i<1024;++i){
                    const auto p=second+i*36u;
                    for(unsigned k=0;k<36u;k+=4u)f->put(p+k,0u);
                    f->put(p+12u,0x40A00000u);f->put(p+24u,njs);f->put(p+32u,0x00400003u);
                }
                f->put(0x8C759634u,1u);
            }else if(variant==24 || variant==25){
                const auto split=variant==24?6u:96u,total=variant==24?12u:120u;
                f->put(object,object+64u);f->put(object+20u,nodes+(split-1u)*64u);
                f->put(nodes+(split-1u)*64u,0u);f->put(nodes+split*64u+4u,0u);
                for(unsigned k=0;k<64u;k+=4u)f->put(object+64u+k,0u);
                f->put(object+64u+8u,njs);f->put(object+64u+16u,nodes+split*64u);
                f->put(object+64u+20u,nodes+(total-1u)*64u);
                f->put(object+64u+28u,0x12345678u);f->put(object+64u+36u,0x76543210u);
                f->put(object+64u+40u,0x40000000u);
                for(unsigned i=0;i<total;++i)f->vector(nodes+i*64u+8u,{.1f,.2f,0.f});
            }else if(variant==26){
                for(unsigned i=0;i<12u;++i)f->vector(nodes+i*64u+8u,{.1f,.2f,float((i*7u)%13u)-6.f});
            }else if(variant==27){
                f->put(first,0x10000000u);f->put(njs+20u,0xFFF04321u);
                f->put(njs+24u,0xCDEF6789u);f->put(njs+28u,0xABCDE000u);
                f->cpu.write_fpscr(fpscr_dn_mask|fpscr_fr_mask|1u);
                for(unsigned i=0;i<16u;++i)f->put(0x8C67C580u+i*4u,std::bit_cast<std::uint32_t>(float(i)*.125f-1.f));
            }else if(variant==28){
                f->put(nodes+12u,0x7F800000u);f->put(nodes+64u+8u,0x7FC12345u);
            }
        }
        std::cout<<"variant "<<variant<<"\n";compare(n,r);++cases;
        if(variant==19)require((n.peek(0x8C754E30u)&0xFFFFu)==1024u,"original eligibility saturation");
        if(variant==23)require((n.peek(0x8C754E30u)&0xFFFFu)==1024u,"second-list saturation");
        if(variant==25)require(n.peek(object+64u+28u)==0x12345678u &&
            n.peek(object+64u+36u)==0x76543210u,"96th hit reset a later object");
    }
    for(unsigned variant=0;variant<14;++variant){
        Fixture f(image,3u,fpscr_dn_mask);
        if(variant==0)f.put(nodes+4u,nodes);
        if(variant==1)f.put(nodes,nodes);
        if(variant==2)f.put(nodes+8u*0u,0x8C000000u);
        if(variant==3)f.put(object+20u,nodes);
        if(variant==4)f.put(object+16u,0u);
        if(variant==5)f.cpu.r[4]=object;
        if(variant==6)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);
        if(variant==7)f.cpu.write_fpscr(fpscr_dn_mask|2u);
        if(variant==8)f.cpu.write_fpscr(fpscr_dn_mask|0x80u);
        if(variant==9)f.ram->writable_bytes()[0x752B1Cu]=0u;
        if(variant==10)f.put(first+4u,0u);
        if(variant==11)f.put(0x8C88F538u,object);
        if(variant==12)f.put(family::entry,f.peek(family::entry)^1u);
        decline(f,variant!=13);++rejections;
    }
    std::cout<<"SONIC_NEAR_COLLISION_TEST_OK cases="<<cases<<" mutation_free_rejections="<<rejections<<"\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_NEAR_COLLISION_TEST_FAIL "<<e.what()<<"\n";return 1;}
