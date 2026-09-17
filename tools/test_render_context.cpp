#include "sonic_render_context.hpp"
#include "sonic_scalar_write_view.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <tuple>
#include <vector>
using namespace katana::runtime;
namespace rc=sonic::render_context;
namespace {
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
constexpr std::uint32_t returned=0x8CF80000u,packet=0x8C890044u;
struct Services final:PlatformServices {
    std::string_view name()const noexcept override{return "render-context-original-bytes";}
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
const auto ranges=[]{std::vector<NativePortImmutableRange> out;
    for(auto s:rc::source_spans())out.push_back({s.address&0x1FFFFFFFu,std::uint32_t(s.bytes.size()),
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)});return out;}();
auto architecture(const CpuState& c){return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,
    c.ssr,c.spc,c.sgr,c.dbr,c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,
    c.mach,c.macl,c.fpul,c.read_fpscr(),c.sr,c.t,c.s,c.q,c.m,c.trap_pending,
    c.exception_generation,c.last_exception_cause,c.sleeping,c.prefetch_count,c.tlb_load_count);}
using Event=std::tuple<std::uint32_t,std::size_t,CodeWriteSource,bool,std::uint32_t>;
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{ranges};
    std::vector<Event> events;
    Fixture(std::span<const std::uint8_t> image,bool commit,unsigned variant,std::uint32_t flags){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(0x003C0001u);
        for(unsigned i=0;i<16;++i){cpu.r[i]=0xC0410ABDu*(i+variant+1u);cpu.fr[i]=0x7F800001u+i;cpu.xf[i]=i*0xABCDE123u;}
        cpu.pc=commit?rc::commit_entry:rc::capture_entry;cpu.pr=returned;cpu.r[15]=0x8CF00000u;
        cpu.gbr=0x76543210u;cpu.t=cpu.s=cpu.q=cpu.m=true;
        auto renderer=0x8CE00000u,cursor=0x8CE10000u;
        if(variant==1u){renderer|=0x20000000u;cursor|=0x20000000u;}
        if(variant==2u){renderer&=0x1FFFFFFFu;cursor&=0x1FFFFFFFu;}
        if(variant==3u)renderer=packet-0x90u;
        if(variant==4u)renderer=packet-0x8Cu;
        if(variant==5u)cursor=packet+4u;
        if(variant==6u)cursor=renderer+0x98u;
        if(variant==7u){renderer=0x8CFFFF60u;cursor=0x8CFFFFECu;}
        for(unsigned i=0;i<9u;++i)put(packet+i*4u,0xE1234567u*(i+1u));
        for(unsigned i=0;i<4u;++i)put(renderer+0x90u+i*4u,0xFEDCBA98u^(i*0x33221144u));
        for(unsigned i=0;i<5u;++i)put(cursor+i*4u,0x10203040u*(i+1u));
        put(0x8C88FBF4u,renderer);put(0x8C88FC14u,cursor);put(0x8C88F56Cu,flags);
    }
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4u);}
    std::uint32_t peek(std::uint32_t a){std::uint32_t v;std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4u);return v;}
    void observe(bool stable=true){cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{
        events.emplace_back(e.address,e.size,e.source,e.bytes_changed,peek(e.address));immutable.observe_write(e);
    },stable?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:GuestWriteObserverContract::General);}
    void product(){
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{immutable.observe_write(e);},GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        cpu.memory.set_guest_write_batch_observer({&immutable,[](void*,std::span<const GuestWriteEvent>)noexcept{return true;},
            [](void* p,std::span<const GuestWriteEvent> es)noexcept{for(auto e:es)static_cast<NativePortImmutableWriteGuard*>(p)->observe_write(e);}});
        sonic::scalar_writes::bind(cpu.memory,immutable,cpu.memory.guest_write_observer_generation());
    }
    ~Fixture(){sonic::scalar_writes::unbind(&cpu.memory,&immutable);}
};
}
int main(int argc,char** argv)try{
    require(argc==2,"PAL RAM path required");sonic::diagnostics::internal_runtime_enabled=false;
    std::ifstream input(argv[1],std::ios::binary);require(bool(input),"RAM missing");
    std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(input),{}};require(image.size()==0x1000000u,"RAM size");
    for(auto s:rc::source_spans())require(std::equal(s.bytes.begin(),s.bytes.end(),image.begin()+(s.address&0xFFFFFFu)),"original source bytes");
    unsigned cases=0,direct=0;
    for(unsigned owner=0;owner<2u;++owner)for(unsigned variant=0;variant<8u;++variant)
    for(auto flags:std::array{0u,0x4000u,0xABCD8123u,0xFFFFFFFFu})for(unsigned observer=0;observer<3u;++observer){
        Fixture n(image,owner,variant,flags),r(image,owner,variant,flags);
        r.observe();if(!observer)n.product();else if(observer==1u)n.observe();else{n.product();n.observe();}
        auto before=rc::statistics().direct_calls;
        require(rc::try_execute(n.cpu,&n.immutable),"native rejected valid fixture");
        if(rc::statistics().direct_calls!=before)++direct;
        unsigned steps=0;while(r.cpu.pc!=returned && ++steps<100u){(void)execute_dynamic_sh4_block(r.cpu,services,1u);require(!r.cpu.trap_pending,"reference exception");}
        require(r.cpu.pc==returned,"reference return");
        if(architecture(n.cpu)!=architecture(r.cpu)){
            for(unsigned i=0;i<16u;++i)if(n.cpu.r[i]!=r.cpu.r[i])std::cerr<<"R"<<i<<" "<<std::hex<<n.cpu.r[i]<<" vs "<<r.cpu.r[i]<<'\n';
            throw std::runtime_error("CPU mismatch");
        }
        require(std::ranges::equal(n.ram->bytes(),r.ram->bytes()),"RAM mismatch");
        if(observer)require(n.events==r.events,"ordered write mismatch");
        require(!n.immutable.write_detected(),"source write");++cases;
    }
    unsigned rejected=0;
    for(unsigned variant=0;variant<10u;++variant){
        Fixture n(image,true,0,0);n.observe();
        if(variant==0u)n.put(0x8C88FBF4u,0u);
        if(variant==1u)n.put(0x8C88FC14u,0x8CE10001u);
        if(variant==2u)n.put(0x8C88FC14u,0x8C88FBF4u);
        if(variant==3u)n.put(0x8C88FBF4u,rc::capture_entry-0x90u);
        if(variant==4u)n.put(rc::capture_entry,0u);
        if(variant==5u)n.put(0x8C605DF4u,0u);
        if(variant==6u)n.cpu.write_sr(0u);
        if(variant==7u)n.cpu.trap_pending=true;
        if(variant==8u)n.observe(false);
        if(variant==9u){n.put(0x8C88FC14u,0x0CE10000u);n.cpu.mmucr=1u;}
        const auto before=architecture(n.cpu);const std::vector<std::uint8_t> data(n.ram->bytes().begin(),n.ram->bytes().end());
        require(!rc::try_execute(n.cpu,&n.immutable),"unsafe fixture admitted");
        require(before==architecture(n.cpu) && std::ranges::equal(data,n.ram->bytes()) && n.events.empty(),"mutating fallback");++rejected;
    }
    require(direct==cases/3u,"direct capability not reached");
    std::cout<<"RENDER_CONTEXT_TEST_OK cases="<<cases<<" rejected="<<rejected<<" direct="<<direct<<" cpu=exact ram=exact ordered_aliases=exact\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"RENDER_CONTEXT_TEST_FAIL "<<e.what()<<'\n';return 1;}
