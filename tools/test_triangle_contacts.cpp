#include "sonic_triangle_contacts.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "katana/sh4/decoder.hpp"
#include "test_collision_memory_support.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <vector>
using namespace katana::runtime;
namespace tc=sonic::triangle_contacts;
namespace {
constexpr auto returned=0x8CF80000u;
void require(bool value,const char* why) { if (!value) throw std::runtime_error(why); }
const auto code_ranges=[] {
    std::vector<NativePortImmutableRange> r;
    auto spans=tc::source_spans;
    std::sort(spans.begin(),spans.end(),[](const auto& a,const auto& b){return a.address<b.address;});
    for(const auto s:spans)r.push_back({s.address&0x1FFFFFFFu,s.size,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)});
    return r;
}();
struct Services final : PlatformServices {
    std::string_view name() const noexcept override { return "triangle-contacts-byte-reference"; }
    std::uint32_t abi_version() const noexcept override { return 128u; }
    std::uint32_t guest_cycle_contract() const noexcept override { return 0u; }
    PlatformCapabilities capabilities() const noexcept override { return {}; }
    void read_memory(std::uint32_t,std::span<std::uint8_t>) override { throw std::runtime_error("device read"); }
    void write_memory(std::uint32_t,std::span<const std::uint8_t>) override { throw std::runtime_error("device write"); }
    std::uint64_t scheduler_cycle() const noexcept override { return 0u; }
    std::optional<std::uint64_t> next_scheduler_event_cycle() const noexcept override { return {}; }
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t,std::size_t) override { return {}; }
    std::optional<PlatformInterruptRequest> poll_interrupt() override { return {}; }
    PlatformDmaResult start_dma(const PlatformDmaRequest&) override { throw std::runtime_error("DMA"); }
    PlatformFallbackResult controlled_fallback(CpuState&,const PlatformFallbackRequest&) override {
        throw std::runtime_error("executor fallback");
    }
    bool prefetch(CpuState&,GuestInstructionOrigin,std::uint32_t) override { throw std::runtime_error("PREF"); }
} services;
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary); require(bool(f),"boot.bin missing");
    return {std::istreambuf_iterator<char>(f),{}};
}
std::string digest(std::span<const std::uint8_t> bytes) {
#ifndef _WIN32
    return native_port_content_sha256(bytes);
#else
    std::array<unsigned char,32> result{};
    require(bytes.size()<=ULONG_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),result.data(),ULONG(result.size()))>=0,"SHA-256 failed");
    constexpr char hex[]="0123456789abcdef"; std::string text(64,'0');
    for (std::size_t i=0;i<result.size();++i) { text[i*2]=hex[result[i]>>4]; text[i*2+1]=hex[result[i]&15]; }
    return text;
#endif
}
// Instruction/cycle/provenance bookkeeping belongs to native-hook integration.
auto architecture(const CpuState& c) {
    return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,
        c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.mach,c.macl,c.fpul,
        c.read_fpscr(),c.sr,c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,
        c.last_exception_cause,c.sleeping,c.prefetch_count,c.tlb_load_count);
}
using Event=std::tuple<std::uint32_t,std::size_t,CodeWriteSource,bool>;
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{code_ranges};
    std::vector<Event> events;
    std::uint64_t bridge_calls=0u;
    std::string label;
    static constexpr auto query=0x8CE00000u,object=0x8CE01000u,nodes=0x8CE02000u;
    Fixture(std::span<const std::uint8_t> boot,std::uint32_t fpscr=fpscr_dn_mask,bool readonly=false) {
        cpu.memory.map_region("main-ram",0x0C000000u,ram,
            readonly?MemoryRegionAccess::ReadOnly:MemoryRegionAccess::ReadWrite);
        if (!readonly) cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        auto bytes=ram->writable_bytes(); std::fill(bytes.begin(),bytes.end(),0xCDu);
        std::copy(boot.begin(),boot.end(),bytes.begin()+0x10000u);
        cpu.pc=tc::entry; cpu.pr=returned; cpu.gbr=0x8CE80000u;
        cpu.write_sr(sr_md_mask); cpu.write_fpscr(fpscr);
        cpu.t=true;cpu.s=true;cpu.q=true;cpu.m=true;
        cpu.fpul=0xDEADBEEFu;cpu.mach=0x12345678u;cpu.macl=0x87654321u;
        for(unsigned i=0;i<16u;++i){cpu.r[i]=0xABCD1200u+i;cpu.fr[i]=0xCAFE0000u+i;cpu.xf[i]=0x7FA00000u+i;}
        for(unsigned i=0;i<8u;++i)cpu.r_bank[i]=0x11220000u+i;
        cpu.r[4]=query;cpu.r[5]=0x1234u;cpu.r[15]=0x8CF00000u;
        put(0x8C02D548u,object);put(object,0u);put(object+8u,cpu.r[5]);
        put(object+12u,0x8CE09000u);put(object+16u,nodes);
        vector(query,{0.f,0.f,0.f});box(6u,false);
        events.reserve(4000u);
    }
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4u);}
    std::uint32_t peek(std::uint32_t a)const{std::uint32_t v;std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4u);return v;}
    void vector(std::uint32_t a,std::array<float,3> v){for(unsigned i=0;i<3u;++i)put(a+4u*i,std::bit_cast<std::uint32_t>(v[i]));}
    void box(unsigned count,bool reverse,float offset=0.f) {
        put(object+16u,count?nodes:0u);
        for(unsigned i=0;i<count;++i){
            const auto a=nodes+0x40u*i;const unsigned axis=(i/2u)%3u;
            const float distance=(i%2u?-1.f:1.f)*(10.f+float(i/6u)*2.f);
            std::array<float,3> v0{offset,offset,offset},v1=v0,v2=v0;
            v0[axis]=v1[axis]=v2[axis]=distance+offset;
            v0[(axis+1u)%3u]-=50.f;v0[(axis+2u)%3u]-=50.f;
            v1[(axis+1u)%3u]+=50.f;v1[(axis+2u)%3u]-=50.f;
            v2[(axis+2u)%3u]+=50.f;
            if(reverse)std::swap(v1,v2);
            vector(a+24u,v0);vector(a+36u,v1);vector(a+48u,v2);
            put(a,i+1u<count?a+0x40u:0u);
        }
    }
    void observe(bool stable=true){
        // This observer obeys the SDK's stable contract: ONLY event fields,
        // no CPU/RAM/metrics inspection and no guest or mapping mutation.
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{
            events.emplace_back(e.address,e.size,e.source,e.bytes_changed);immutable.observe_write(e);
        },stable?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:GuestWriteObserverContract::General);
    }
};
std::uint64_t reference_ftrc_repairs=0u;
bool bound_pc(std::uint32_t pc){
    // The last identity is a data table, never an executable reference body.
    for(std::size_t i=0;i+1u<tc::source_spans.size();++i){const auto s=tc::source_spans[i];if(pc>=s.address&&pc<s.address+s.size)return true;}
    return false;
}
void step(CpuState& cpu){
    require(bound_pc(cpu.pc),"reference left authenticated closure");
    const auto opcode=guest_fetch_u16(cpu,cpu.pc);
    // Existing SDK interpreter FTRC incorrectly takes destination_register=0.
    // Repair only these two exact original words in this TEST executor. The
    // untouched AOT unit-v8C10EC94-8C10FCEC-2baf008af52674a0.cpp confirms
    // FB7E/F43D -> source4 at line50415, FBAA/F33D -> source3 at line51056.
    // Owner FTRCs are F03D and already use FR0 correctly. No baseline edits.
    if(cpu.pc==0x8C10FB7Eu||cpu.pc==0x8C10FBAAu){
        const auto source=cpu.pc==0x8C10FB7Eu?4u:3u;
        require(opcode==(source==4u?0xF43Du:0xF33Du),"retained FTRC word changed");
        const auto decoded=katana::sh4::decode(opcode);
        require(decoded.source_register==source&&decoded.destination_register==0u,"FTRC decode differs");
        GuestInstructionAttempt attempt(cpu,cpu.pc,2u);cpu.pc+=2u;
        fpu_truncate_to_fpul(cpu,static_cast<std::uint8_t>(source));++reference_ftrc_repairs;
    }else (void)execute_dynamic_sh4_block(cpu,services,1u);
    require(cpu.exception_generation==0u&&!cpu.trap_pending,"reference exception");
}
void run_until(CpuState& cpu,std::uint32_t stop){
    for(unsigned i=0;cpu.pc!=stop&&i<500000u;++i)step(cpu);
    require(cpu.pc==stop,"reference did not return");
}
bool original_bridge(void* context,CpuState& cpu,std::uint32_t target){
    collision_test::bridge_boundary();
    auto& f=*static_cast<Fixture*>(context);++f.bridge_calls;
    require((target==0x8C10D038u||target==0x8C10CF98u)&&cpu.pc==target,"bad bridge target");
    const auto ret=cpu.pr;
    require(ret==0x8C029714u||ret==0x8C029726u||ret==0x8C0298F4u||ret==0x8C029906u||
        ret==0x8C029A84u||ret==0x8C029A96u,"PR was not the original continuation");
    run_until(cpu,ret);return true;
}
void compare(Fixture& n,Fixture& r){
    collision_test::Comparison observers(n,r);
    require(tc::try_execute(n.cpu,&n.immutable,{&n,original_bridge}),"eligible owner declined");
    run_until(r.cpu,returned);
    if(architecture(n.cpu)!=architecture(r.cpu)){
        std::cerr<<n.label<<std::hex<<'\n';
        for(unsigned i=0;i<16u;++i){
            if(n.cpu.r[i]!=r.cpu.r[i])std::cerr<<"r"<<i<<' '<<n.cpu.r[i]<<'/'<<r.cpu.r[i]<<'\n';
            if(n.cpu.fr[i]!=r.cpu.fr[i])std::cerr<<"fr"<<i<<' '<<n.cpu.fr[i]<<'/'<<r.cpu.fr[i]<<'\n';
        }
        std::cerr<<"PC "<<n.cpu.pc<<'/'<<r.cpu.pc<<" PR "<<n.cpu.pr<<'/'<<r.cpu.pr<<
            " FPSCR "<<n.cpu.read_fpscr()<<'/'<<r.cpu.read_fpscr()<<" FPUL "<<n.cpu.fpul<<'/'<<r.cpu.fpul<<std::dec<<'\n';
        throw std::runtime_error("full architectural state differs");
    }
    if(!std::equal(n.ram->bytes().begin(),n.ram->bytes().end(),r.ram->bytes().begin())){
        for(std::size_t i=0;i<n.ram->bytes().size();++i)if(n.ram->bytes()[i]!=r.ram->bytes()[i]){
            std::cerr<<n.label<<" first RAM difference physical="<<std::hex<<(0x0C000000u+i)<<std::dec<<'\n';break;}
        throw std::runtime_error("complete RAM/stack differs");
    }
    if(!observers.product() && n.events!=r.events){
        std::cerr<<n.label<<" stores "<<n.events.size()<<'/'<<r.events.size()<<'\n';
        for(std::size_t i=0;i<std::min(n.events.size(),r.events.size());++i)if(n.events[i]!=r.events[i]){
            std::cerr<<"first event difference "<<i<<" addresses "<<std::hex<<std::get<0>(n.events[i])<<'/'<<std::get<0>(r.events[i])<<std::dec<<'\n';break;}
        throw std::runtime_error("ordered address/size/source/changed events differ");
    }
    observers.verify();
    require(!n.immutable.write_detected()&&!r.immutable.write_detected(),"immutable write");
}
void decline(Fixture& f,const NativePortImmutableWriteGuard* guard,bool missing_bridge=false){
    f.observe();const auto a=architecture(f.cpu);const auto events=f.events;
    const auto bytes=std::vector<std::uint8_t>(f.ram->bytes().begin(),f.ram->bytes().end());
    const auto counters=f.cpu.memory.performance_counters();
    require(!tc::try_execute(f.cpu,guard,{&f,missing_bridge?nullptr:original_bridge}),"unsafe owner admitted");
    require(a==architecture(f.cpu)&&events==f.events&&f.bridge_calls==0u&&
        std::equal(bytes.begin(),bytes.end(),f.ram->bytes().begin()),"fallback mutated guest");
    const auto after=f.cpu.memory.performance_counters();
    require(counters.indexed_region_hits==after.indexed_region_hits&&counters.reference_region_probes==after.reference_region_probes&&
        counters.observed_accesses==after.observed_accesses&&counters.unobserved_accesses==after.unobserved_accesses,"fallback changed metrics");
}
} // namespace
int main(int argc,char** argv){
    try{
        require(argc==2,"usage: test_triangle_contacts <installed-content-root>");
        const auto boot=read(std::filesystem::path(argv[1])/"boot.bin");
        require(boot.size()==6735296u&&digest(boot)=="b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af","boot identity differs");
        for(const auto span:tc::source_spans)require(digest(std::span<const std::uint8_t>(boot).subspan(span.address-0x8C010000u,span.size))==span.sha256,"closure span SHA differs");
        unsigned cases=0u;std::uint64_t calls=0u;
        const auto positive=[&](auto configure,std::string label,std::uint32_t mode=fpscr_dn_mask){
            Fixture n(boot,mode),r(boot,mode);configure(n);configure(r);n.label=label;
            compare(n,r);calls+=n.bridge_calls;++cases;
        };
        constexpr std::array modes{fpscr_dn_mask,fpscr_dn_mask|1u,fpscr_dn_mask|fpscr_fr_mask,
            fpscr_dn_mask|fpscr_fr_mask|1u|fpscr_flag_mask|fpscr_cause_mask};
        for(const auto mode:modes)for(unsigned geometry=0;geometry<12u;++geometry){
            positive([&](Fixture& f){
                const float offset=float(geometry%3u)-1.f;
                f.box(geometry<6u?6u:12u,geometry%2u!=0u,offset);
                f.vector(Fixture::query,{offset,offset,offset});
                if(geometry==2u)f.vector(Fixture::nodes+48u,{0.f,0.f,0.f});
                if(geometry==3u)f.vector(Fixture::nodes+24u,{1.e10f,1.e10f,1.e10f});
                if(geometry==4u)f.vector(Fixture::nodes+24u,{0.f,0.f,0.f});
            },"geometry="+std::to_string(geometry)+" fpscr="+std::to_string(mode),mode);
        }
        for(const auto mode:modes)positive([](Fixture& f){
            f.box(1u,false);
            // Oblique normal forces nontrivial atan/asin reduction, including
            // the real FAF8/FAD4 path rather than only axis/zero special cases.
            f.vector(Fixture::nodes+24u,{-30.f,-30.f,75.f});
            f.vector(Fixture::nodes+36u,{-30.f,75.f,-30.f});
            f.vector(Fixture::nodes+48u,{75.f,-30.f,-30.f});
        },"oblique angle closure",mode);
        positive([](Fixture& f){f.put(0x8C02D548u,0u);},"empty object list");
        positive([](Fixture& f){f.cpu.r[5]=99u;},"object not found");
        positive([](Fixture& f){f.box(0u,false);},"empty triangle list");
        positive([](Fixture& f){f.put(Fixture::object+8u,99u);f.put(Fixture::object,Fixture::object+0x100u);
            f.put(Fixture::object+0x100u,0u);f.put(Fixture::object+0x108u,f.cpu.r[5]);
            f.put(Fixture::object+0x10Cu,77u);f.put(Fixture::object+0x110u,Fixture::nodes);},"second object");
        positive([](Fixture& f){f.cpu.r[4]=Fixture::object+8u;f.box(0u,false);},"read/read query/object alias");
        for(unsigned mix=0;mix<8u;++mix)positive([&](Fixture& f){
            if(mix&1u)f.cpu.r[4]&=0x1FFFFFFFu;
            if(mix&2u){f.put(0x8C02D548u,Fixture::object&0x1FFFFFFFu);f.put(Fixture::object+16u,Fixture::nodes&0x1FFFFFFFu);
                for(unsigned i=0;i<5u;++i)f.put(Fixture::nodes+64u*i,(Fixture::nodes+64u*(i+1u))&0x1FFFFFFFu);}
            if(mix&4u)f.cpu.r[15]=(f.cpu.r[15]&0x1FFFFFFFu)|0xA0000000u;
        },"P0/P1/P2 mix="+std::to_string(mix));
        positive([](Fixture& f){f.cpu.mmucr=1u;f.cpu.address_space=std::make_shared<RuntimeAddressSpace>();
            f.cpu.address_space->write_mmucr(1u);f.cpu.address_space->set_mode(AddressTranslationMode::Mmu);},"P1 MMU active");
        require(calls>0u&&reference_ftrc_repairs>0u,"real angle closure never exercised");
        for(unsigned bad=0;bad<18u;++bad){
            Fixture f(boot,fpscr_dn_mask,bad==17u);
            switch(bad){
            case 0:f.cpu.r[4]=0x8C027360u-12u;break;
            case 1:f.cpu.r[4]=f.cpu.r[15]-0x100u;break;
            case 2:f.cpu.r[4]=Fixture::nodes+12u;break;
            case 3:f.cpu.r[15]=0x8C7AC050u;break;
            case 4:f.put(Fixture::nodes,Fixture::nodes);break;
            case 5:f.put(Fixture::object+8u,0u);f.put(Fixture::object,Fixture::object);break;
            case 6:f.put(Fixture::nodes,0x8CFFFFFFu);break;
            case 7:f.cpu.r[4]=0x8CFFFFF0u;break;
            case 8:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);break;
            case 9:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_pr_mask);break;
            case 10:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_exception_enable_mask);break;
            case 11:f.cpu.write_sr(sr_md_mask|sr_fd_mask);break;
            case 12:f.cpu.r[4]&=0x1FFFFFFFu;f.cpu.mmucr=1u;break;
            case 13:f.put(tc::entry,0u);break;
            case 14:f.put(0x8C10D038u,0u);break;
            case 15:f.put(0x8C1602DCu,0u);break;
            default:break;
            }
            decline(f,bad==16u?nullptr:&f.immutable);++cases;
        }
        {Fixture f(boot);decline(f,&f.immutable,true);++cases;}
        // Fatal interruption after visible stores must never be a false fallback.
        for(unsigned fault=0;fault<3u;++fault){
            Fixture f(boot),r(boot);collision_test::Comparison observers(f,r);bool threw=false;
            struct Failure { Fixture* fixture;unsigned fault; } failure{&f,fault};
            const tc::RetainedCallBridge b{&failure,[](void* p,CpuState& c,std::uint32_t t){
                const auto& x=*static_cast<Failure*>(p);
                if(x.fault==0u)return false;
                (void)original_bridge(x.fixture,c,t);
                if(x.fault==1u)c.pc+=2u;
                else c.memory.clear_direct_linear_alias_window();
                return true;
            }};
            try{(void)tc::try_execute(f.cpu,&f.immutable,b);}catch(const std::runtime_error&){threw=true;}
            require(threw && (observers.product() || !f.events.empty()),"post-mutation bridge failure was not fatal");
            observers.verify();++cases;
        }
        std::cout<<"triangle-contacts: PASS "<<cases<<" cases, original-angle-calls="<<calls<<
            " reference_ftrc_source_corrections="<<reference_ftrc_repairs<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
