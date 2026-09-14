#include "sonic_motion_sampling.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "katana/sh4/decoder.hpp"
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <xmmintrin.h>
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
namespace family=sonic::motion_sampling;
namespace {
constexpr auto returned=0x8CF80000u,object=0x8CE00000u,table=0x8CE10000u,counts=0x8CE20000u,keys=0x8CE30000u;
void require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
std::string digest(std::span<const std::uint8_t> bytes){
    std::array<unsigned char,32> hash{};
    require(bytes.size()<=ULONG_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),hash.data(),ULONG(hash.size()))>=0,"SHA failed");
    constexpr char hex[]="0123456789abcdef";std::string out(64,'0');
    for(unsigned i=0;i<32;++i){out[2*i]=hex[hash[i]>>4];out[2*i+1]=hex[hash[i]&15];}return out;
}
struct Services final:PlatformServices {
    std::string_view name()const noexcept override{return "motion-original-bytes";}
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
    Fixture(std::span<const std::uint8_t> image,unsigned owner,unsigned count,std::uint32_t mode){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.pc=family::entries[owner];cpu.pr=returned;cpu.gbr=0x8CE80000u;
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(mode);cpu.t=true;cpu.s=true;cpu.q=true;cpu.m=true;
        cpu.fpul=0x11223344u;cpu.mach=0xABCD0123u;cpu.macl=0x10203040u;
        for(unsigned i=0;i<16u;++i){
            cpu.r[i]=0xABCD1000u+i;cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i+1));
            cpu.xf[i]=std::bit_cast<std::uint32_t>(float(int(i)-4)*.125f);
        }
        cpu.r[4]=object;cpu.r[15]=0x8CF00000u;
        put(0x8C88FD84u,table);put(0x8C88FD88u,counts);put(0x8C88FD94u,3u);
        put(0x8C88FD8Cu,std::bit_cast<std::uint32_t>(5.25f));
        put(table+12u,count?keys:0u);put(counts+12u,count);
        for(unsigned i=0;i<3u;++i){
            put(object+8u+i*4u,std::bit_cast<std::uint32_t>(float(i)*1.5f-.25f));
            put(object+20u+i*4u,0xFFFF1234u*(i+1u));
            put(object+32u+i*4u,std::bit_cast<std::uint32_t>(float(i)*.25f+.5f));
        }
        for(unsigned k=0;k<count;++k){
            put(keys+k*16u,k*10u);
            for(unsigned i=0;i<3u;++i)put(keys+k*16u+4u+i*4u,owner<2u?
                std::bit_cast<std::uint32_t>(float(k*3u+i)*.25f-.5f):0x10001u*(k+1u)*(i+1u));
        }
    }
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4u);}
    std::uint32_t peek(std::uint32_t a){std::uint32_t v;std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4u);return v;}
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
    n.observe();r.observe();const auto native_host=_mm_getcsr();
    require(family::try_execute(n.cpu,&n.immutable),"native declined valid fixture");
    require(_mm_getcsr()==native_host,"native leaked host MXCSR");
    const auto reference_host=_mm_getcsr();
    original(r.cpu);
    require(_mm_getcsr()==reference_host,"reference leaked host MXCSR");
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
    require(argc==2,"motion_sampling_tests <original-full-ram>");
    std::ifstream file(argv[1],std::ios::binary);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000u && digest(image)=="b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c","RAM identity");
    unsigned cases=0,rejections=0;
    for(unsigned source=0;source<16u;++source){
        Fixture f(image,0u,0u,fpscr_dn_mask);
        const auto opcode=std::uint16_t(0xF03Du|(source<<8u));
        const auto decoded=katana::sh4::decode(opcode);
        require(decoded.source_register==source && decoded.destination_register==0u,"FTRC decoder contract");
        for(unsigned i=0;i<16u;++i)f.cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i*2u)+.5f);
        f.put(0x8CF81000u,0x00090000u|opcode);f.cpu.pc=0x8CF81000u;
        (void)execute_dynamic_sh4_block(f.cpu,services,1u);
        require(f.cpu.fpul==source*2u,"FTRC oracle source-register contract");
    }
    for(unsigned owner=0;owner<4u;++owner)
    for(auto mode:{fpscr_dn_mask,fpscr_dn_mask|1u,fpscr_dn_mask|fpscr_fr_mask,fpscr_dn_mask|fpscr_fr_mask|1u})
    for(unsigned count:{0u,1u,17u}){
        Fixture n(image,owner,count,mode),r(image,owner,count,mode);
        compare(n,r);require(n.peek(0x8C88FD94u)==4u,"slot progress");++cases;
    }
    for(unsigned owner:{2u,3u})for(unsigned bits=0;bits<8u;++bits){
        Fixture n(image,owner,0u,fpscr_dn_mask),r(image,owner,0u,fpscr_dn_mask);
        for(auto* f:{&n,&r})for(unsigned i=0;i<3u;++i)
            f->put(object+20u+i*4u,(bits&(1u<<i))?(i==0u?0x10000u:0xFFFF4231u):0u);
        compare(n,r);++cases;
    }
    for(unsigned owner=0;owner<4u;++owner)for(unsigned variant=0;variant<6u;++variant){
        Fixture n(image,owner,7u,fpscr_dn_mask),r(image,owner,7u,fpscr_dn_mask);
        for(auto* f:{&n,&r}){
            if(variant==0u){f->cpu.r[4]&=0x1FFFFFFFu;f->cpu.r[15]&=0x1FFFFFFFu;f->put(table+12u,keys&0x1FFFFFFFu);}
            if(variant==1u){f->cpu.r[4]|=0x20000000u;f->cpu.r[15]|=0x20000000u;f->put(table+12u,keys|0x20000000u);}
            if(variant==2u)f->put(0x8C88FD8Cu,0xBF800000u);
            if(variant==3u)f->put(0x8C88FD8Cu,0x7FC12345u);
            if(variant==4u){f->cpu.xf[3]=0x7F800000u;f->cpu.xf[9]=0x7FC12345u;}
            if(variant==5u)for(unsigned i=0;i<7u;++i)f->put(keys+i*16u,(i*11u)%13u);
        }
        compare(n,r);++cases;
    }
    for(unsigned variant=0;variant<16u;++variant){
        Fixture f(image,0u,7u,fpscr_dn_mask);
        if(variant==0u)f.put(counts+12u,0u);
        if(variant==1u)f.put(counts+12u,65537u);
        if(variant==2u)f.put(table+12u,f.cpu.r[15]-40u);
        if(variant==3u)f.put(0x8C88FD84u,0x8C88FD94u-12u);
        if(variant==4u)f.cpu.r[15]=0x8C88FD94u+40u;
        if(variant==5u){f.put(table+12u,0u);f.cpu.r[4]=f.cpu.r[15]-48u;}
        if(variant==6u)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);
        if(variant==7u)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_pr_mask);
        if(variant==8u)f.cpu.write_fpscr(fpscr_dn_mask|0x80u);
        if(variant==9u)f.cpu.write_fpscr(0u);
        if(variant==10u)f.cpu.write_fpscr(fpscr_dn_mask|2u);
        if(variant==12u)f.put(family::entries[0],0u);
        if(variant==13u)f.put(table+12u,0x8CFFFFFCu);
        if(variant==14u)--f.cpu.r[15];
        if(variant==15u)f.put(0x8C88FD94u,65536u);
        decline(f,variant!=11u);++rejections;
    }
    for(unsigned owner:{2u,3u})for(unsigned mode=0;mode<4u;++mode){
        Fixture n(image,owner,0u,fpscr_dn_mask|(mode&1u)|((mode&2u)?fpscr_fr_mask:0u)),
            r(image,owner,0u,fpscr_dn_mask|(mode&1u)|((mode&2u)?fpscr_fr_mask:0u));
        for(auto* f:{&n,&r}){
            for(unsigned i=0;i<3u;++i)f->put(object+20u+i*4u,0u);
            f->cpu.write_fpscr(f->cpu.read_fpscr()|0x0001007Cu);
        }
        struct HostRestore{unsigned value=_mm_getcsr();~HostRestore(){_mm_setcsr(value);}} restore;
        _mm_setcsr(0x1F80u|(mode<<13u)|((mode&1u)?0x8040u:0u)|0x21u);
        compare(n,r);++cases;
    }
    std::cout<<"SONIC_MOTION_SAMPLING_TEST_OK cases="<<cases<<" mutation_free_rejections="<<rejections<<" ftrc_registers=16 host_mxcsr=preserved"<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_MOTION_SAMPLING_TEST_FAILED "<<e.what()<<'\n';return 1;}
