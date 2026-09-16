#include "sonic_animation_hierarchy.hpp"
#include "sonic_pose_blend.hpp"
#include "sonic_scalar_write_view.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <xmmintrin.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <vector>
using namespace katana::runtime;
namespace family=sonic::animation_hierarchy;
namespace {
constexpr std::uint32_t returned=0x8CF80000u,object=0x8CE00000u,context=0x8CE10000u;
constexpr std::uint32_t rows=0x8CE20000u,keys=0x8CE30000u,output=0x8CE50000u,matrices=0x8CE60000u;
bool reference_epochs=true;
bool product_observer=false;
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
struct Services final:PlatformServices {
    std::string_view name()const noexcept override{return "animation-original-bytes";}
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
    for(const auto s:family::source_spans())ranges.push_back({s.address&0x1FFFFFFFu,std::uint32_t(s.bytes.size()),
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)});
    for(const auto s:sonic::pose_blend::source_spans())ranges.push_back({s.address&0x1FFFFFFFu,std::uint32_t(s.bytes.size()),
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)});
    std::ranges::sort(ranges,{},&NativePortImmutableRange::physical_address);
    std::vector<NativePortImmutableRange> merged;
    for(const auto r:ranges){
        if(!merged.empty() && r.physical_address<=merged.back().physical_address+merged.back().byte_size)
            merged.back().byte_size=std::max(merged.back().byte_size,r.physical_address+r.byte_size-merged.back().physical_address);
        else merged.push_back(r);
    }
    return merged;
}();
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{code_ranges};
    std::vector<Event> events;
    Fixture(std::span<const std::uint8_t> image,unsigned channels,unsigned flags,unsigned mask,unsigned mode,bool tree){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.pc=family::entry;cpu.pr=returned;cpu.gbr=0x8CE80000u;
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|(mode&1u)|((mode&2u)?fpscr_fr_mask:0u));
        cpu.t=cpu.s=cpu.q=cpu.m=true;cpu.fpul=0x11223344u;cpu.mach=0xABCD0123u;cpu.macl=0x10203040u;
        for(unsigned i=0;i<16u;++i){
            cpu.r[i]=0xABCD1000u+i;cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i+1));
            cpu.xf[i]=std::bit_cast<std::uint32_t>(float(int(i)-4)*.125f);
        }
        cpu.r[4]=object;cpu.r[5]=context;cpu.r[15]=0x8CF00000u;
        put(context,rows);put(context+4u,output);put(context+8u,(1u<<channels)-1u);
        put(context+12u,31u);put(context+16u,std::bit_cast<std::uint32_t>(5.25f));
        put(0x8C78B388u,0u);put(0x8C78B38Cu,128u);
        put(0x8C88F5D8u,64u);put(0x8C88F5DCu,1u);put(0x8C88F538u,matrices);
        for(unsigned n=0;n<4u;++n){
            const auto node=object+n*64u;put(node,flags^(n&3u));
            for(unsigned i=0;i<3u;++i){
                put(node+8u+i*4u,std::bit_cast<std::uint32_t>(float(i+n)*1.5f-.25f));
                put(node+20u+i*4u,0xFFFF1234u*(i+1u)*(n+1u));
                put(node+32u+i*4u,std::bit_cast<std::uint32_t>(float(i+n)*.25f+.5f));
                const auto track=keys+n*1024u+i*256u;
                put(rows+n*channels*8u+i*4u,(mask&(1u<<i))?track:0u);
                for(unsigned k=0;k<4u;++k){
                    put(track+k*16u,k*10u);
                    for(unsigned axis=0;axis<3u;++axis)put(track+k*16u+4u+axis*4u,i==1u?
                        0x10001u*(k+1u)*(axis+1u):std::bit_cast<std::uint32_t>(float(k*3u+axis+n)*.25f-.5f));
                }
            }
            put(node+44u,tree?(n<2u?node+64u:0u):0u);
            put(node+48u,tree?(n==2u?object+192u:0u):0u);
        }
        // Root sibling must be returned, not evaluated by this invocation.
        put(object+48u,0x8CFFFFFFu);
    }
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4u);}
    void observe(bool stable=true){
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{
            events.emplace_back(e.address,e.size,e.source,e.bytes_changed);immutable.observe_write(e);
        },stable?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:GuestWriteObserverContract::General);
    }
    void observe_product(){
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{immutable.observe_write(e);},
            GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        cpu.memory.set_guest_write_batch_observer({&immutable,
            [](void*,std::span<const GuestWriteEvent>)noexcept{return true;},
            [](void* guard,std::span<const GuestWriteEvent> writes)noexcept{
                for(const auto& w:writes)static_cast<NativePortImmutableWriteGuard*>(guard)->observe_write(w);
            }});
        sonic::scalar_writes::bind(cpu.memory,immutable,cpu.memory.guest_write_observer_generation());
    }
    ~Fixture(){sonic::scalar_writes::unbind(&cpu.memory,&immutable);}
};
auto architecture(const CpuState& c){
    return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,
        c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.mach,c.macl,c.fpul,
        c.read_fpscr(),c.sr,c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,c.last_exception_cause);
}
void compare(Fixture& n,Fixture& r){
    if(product_observer){n.observe_product();r.observe_product();}else {n.observe();r.observe();}
    const auto mxcsr=_mm_getcsr();
    const bool pose=n.cpu.pc==sonic::pose_blend::entry;
    require(pose?sonic::pose_blend::try_execute(n.cpu,&n.immutable):family::try_execute(n.cpu,&n.immutable),"native declined valid fixture");
    require(_mm_getcsr()==mxcsr,"native MXCSR leaked");
    unsigned steps=0;
    std::optional<HostFpuExecutionEpoch> reference_epoch;
    std::uint32_t epoch_end=0;
    while(r.cpu.pc!=returned && steps++<200000u){
        if(reference_epoch && r.cpu.pc==epoch_end)reference_epoch.reset();
        if(reference_epochs){
            // Reproduce the actual retained SDK AOT boundaries while still
            // independently interpreting every original PAL instruction.
            if(r.cpu.pc==0x8C639C3Eu)epoch_end=0x8C639C58u;
            else if(r.cpu.pc==0x8C639C6Au)epoch_end=0x8C639C82u;
            else if(r.cpu.pc==0x8C639C94u)epoch_end=0x8C639CAAu;
            else if(r.cpu.pc==0x8C639F42u)epoch_end=0x8C639F5Au;
            else if(r.cpu.pc==0x8C639F6Cu)epoch_end=0x8C639F82u;
            else if(r.cpu.pc==0x8C639F94u)epoch_end=0x8C639FAEu;
            else epoch_end=reference_epoch?epoch_end:0;
            if(epoch_end && !reference_epoch)reference_epoch.emplace(r.cpu);
        }
        (void)execute_dynamic_sh4_block(r.cpu,services,1u);require(!r.cpu.trap_pending,"reference exception");
    }
    reference_epoch.reset();
    require(r.cpu.pc==returned,"reference failed to return");require(_mm_getcsr()==mxcsr,"reference MXCSR leaked");
    if(architecture(n.cpu)!=architecture(r.cpu)){
        for(unsigned i=0;i<16u;++i){
            if(n.cpu.r[i]!=r.cpu.r[i])std::cerr<<"R"<<i<<" "<<std::hex<<n.cpu.r[i]<<" vs "<<r.cpu.r[i]<<'\n';
            if(n.cpu.fr[i]!=r.cpu.fr[i])std::cerr<<"FR"<<i<<" "<<std::hex<<n.cpu.fr[i]<<" vs "<<r.cpu.fr[i]<<'\n';
            if(n.cpu.xf[i]!=r.cpu.xf[i])std::cerr<<"XF"<<i<<" "<<std::hex<<n.cpu.xf[i]<<" vs "<<r.cpu.xf[i]<<'\n';
        }
        std::cerr<<"FPUL "<<std::hex<<n.cpu.fpul<<" vs "<<r.cpu.fpul<<" FPSCR "<<n.cpu.read_fpscr()<<" vs "<<r.cpu.read_fpscr()<<'\n';
        throw std::runtime_error("architecture mismatch");
    }
    if(!std::equal(n.ram->bytes().begin(),n.ram->bytes().end(),r.ram->bytes().begin())){
        for(unsigned i=0;i<0x1000000u;i+=4u)if(std::memcmp(n.ram->bytes().data()+i,r.ram->bytes().data()+i,4u)){
            std::cerr<<"RAM offset "<<std::hex<<i<<'\n';break;
        }
        throw std::runtime_error("RAM mismatch");
    }
    if(n.events!=r.events){
        for(unsigned i=0;i<std::min(n.events.size(),r.events.size());++i)if(n.events[i]!=r.events[i]){
            std::cerr<<"write "<<i<<" native "<<std::hex<<std::get<0>(n.events[i])<<" original "<<std::get<0>(r.events[i])<<'\n';break;
        }
        throw std::runtime_error("ordered write events mismatch");
    }
}
using MathState=std::array<std::uint32_t,34>;
std::vector<MathState> math_trace(unsigned mode,bool grouped){
    CpuState cpu{.memory=Memory{0u}};
    cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|mode);
    for(unsigned i=0;i<16u;++i)cpu.xf[i]=std::bit_cast<std::uint32_t>(float(int(i)-4)*.125f);
    std::vector<MathState> trace;
    std::optional<HostFpuExecutionEpoch> epoch;
    if(grouped)epoch.emplace(cpu);
    const auto record=[&]{MathState state{};std::copy(cpu.fr.begin(),cpu.fr.end(),state.begin());
        std::copy(cpu.xf.begin(),cpu.xf.end(),state.begin()+16);state[32]=cpu.read_fpscr();state[33]=cpu.fpul;trace.push_back(state);};
    cpu.fr[4]=0xBE800000u;cpu.fr[5]=0x3FA00000u;cpu.fr[6]=0x40300000u;cpu.fr[7]=0x3F800000u;
    fpu_transform_vector(cpu,4u);record();std::copy_n(cpu.fr.begin()+4,4,cpu.xf.begin()+12);
    for(char axis:{'z','y','x'}){
        cpu.fpul=0xFFFF1234u*(axis=='z'?3u:axis=='y'?2u:1u);cpu.fr[3]=0;
        fpu_sine_cosine(cpu,0u);record();cpu.fr[7]=0;
        if(axis=='x'){cpu.fr[4]=0;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=0;cpu.fr[5]=cpu.fr[2];cpu.fr[6]=cpu.fr[1];fpu_negate(cpu,5u);}
        else if(axis=='y'){cpu.fr[5]=0;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=cpu.fr[1];cpu.fr[1]=0;cpu.fr[6]=cpu.fr[0];cpu.fr[4]=cpu.fr[2];fpu_negate(cpu,2u);}
        else {cpu.fr[6]=0;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=cpu.fr[1];cpu.fr[1]=cpu.fr[2];cpu.fr[2]=0;cpu.fr[5]=cpu.fr[0];cpu.fr[4]=cpu.fr[1];fpu_negate(cpu,4u);}
        fpu_transform_vector(cpu,0u);record();fpu_transform_vector(cpu,4u);record();
        std::copy_n(cpu.fr.begin(),4,cpu.xf.begin()+(axis=='x'?4:0));
        std::copy_n(cpu.fr.begin()+4,4,cpu.xf.begin()+(axis=='z'?4:8));
    }
    return trace;
}
int check_math_epochs(){
    unsigned differences=0;
    for(unsigned mode:{0u,1u}){
        const auto mxcsr=_mm_getcsr();const auto plain=math_trace(mode,false),grouped=math_trace(mode,true);
        require(_mm_getcsr()==mxcsr,"math epoch leaked host control");
        if(mode==0u)for(unsigned step:{8u,9u})for(unsigned which:{0u,1u}){
            std::cout<<"MATH_STATE step="<<step<<" grouped="<<which<<" values="<<std::hex;
            for(const auto value:(which?grouped:plain)[step])std::cout<<value<<',';
            std::cout<<std::dec<<'\n';
        }
        for(unsigned step=0;step<plain.size();++step)for(unsigned slot=0;slot<34u;++slot)if(plain[step][slot]!=grouped[step][slot]){
            std::cout<<"EPOCH_DIFFERENCE mode="<<mode<<" step="<<step<<" slot="<<slot<<" plain="<<std::hex<<plain[step][slot]<<" grouped="<<grouped[step][slot]<<std::dec<<'\n';++differences;
        }
    }
    std::cout<<"EPOCH_DIFFERENCES "<<differences<<'\n';return differences?1:0;
}
}
int main(int argc,char** argv)try{
    if(argc==2 && std::string_view(argv[1])=="--math-epochs")return check_math_epochs();
    require(argc==2 || (argc==3 && (std::string_view(argv[2])=="--plain-reference" || std::string_view(argv[2])=="--product-observer")),
        "animation-hierarchy-tests <PAL-full-RAM> [--plain-reference|--product-observer]");
    reference_epochs=argc==2 || std::string_view(argv[2])!="--plain-reference";
    product_observer=argc==3 && std::string_view(argv[2])=="--product-observer";
    if(product_observer){
#ifdef _WIN32
        _putenv_s("SARECOMP_RAM_REGIONS","1");_putenv_s("SARECOMP_INTERNAL_DIAGNOSTICS","0");
#else
        setenv("SARECOMP_RAM_REGIONS","1",1);setenv("SARECOMP_INTERNAL_DIAGNOSTICS","0",1);
#endif
    }
    std::ifstream file(argv[1],std::ios::binary);const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000u,"PAL RAM size");
    for(const auto s:family::source_spans())require(std::equal(s.bytes.begin(),s.bytes.end(),image.begin()+(s.address&0xFFFFFFu)),"PAL closure bytes");
    for(const auto s:sonic::pose_blend::source_spans())require(std::equal(s.bytes.begin(),s.bytes.end(),image.begin()+(s.address&0xFFFFFFu)),"PAL pose closure bytes");
    unsigned cases=0,declines=0;
    for(unsigned channels:{2u,3u})for(unsigned flags=0;flags<8u;++flags)for(unsigned mask=0;mask<(1u<<channels);++mask){
        Fixture n(image,channels,flags,mask,flags&3u,mask&1u),r(image,channels,flags,mask,flags&3u,mask&1u);
        std::cout<<"case "<<cases<<" channels="<<channels<<" flags="<<flags<<" tracks="<<mask<<'\n';
        compare(n,r);++cases;
    }
    for(unsigned variant=0;variant<12u;++variant){
        Fixture n(image,3u,0u,7u,variant&3u,true),r(image,3u,0u,7u,variant&3u,true);
        for(auto* f:{&n,&r}){
            if(variant<5u)f->put(0x8C78B38Cu,variant);
            if(variant==5u)f->put(0x8C78B388u,0xFFFFFFFFu);
            if(variant==6u)f->put(context+16u,std::bit_cast<std::uint32_t>(30.5f));
            if(variant==7u){f->cpu.r[4]&=0x1FFFFFFFu;f->cpu.r[5]|=0x20000000u;f->cpu.r[15]&=0x1FFFFFFFu;}
            if(variant==8u){f->cpu.xf[3]=0x7F800000u;f->cpu.xf[9]=0x7FC12345u;}
            if(variant==9u)for(unsigned i=0;i<3u;++i)f->put(keys+256u+4u+i*4u,0x7FFF0000u+i);
            if(variant==10u)f->put(context+8u,4u); // bit length, not popcount
            if(variant==11u)for(unsigned i=0;i<3u;++i)f->put(keys+4u+i*4u,1u+i);
        }
        std::cout<<"variant "<<variant<<'\n';compare(n,r);++cases;
    }
    for(unsigned variant=0;variant<10u;++variant){
        Fixture f(image,3u,0u,7u,0u,true);
        if(variant==0u)f.put(context+8u,1u);
        if(variant==1u)f.put(context+4u,object);
        if(variant==2u)f.put(0x8C88F5D8u,2u);
        if(variant==3u)f.put(rows,f.cpu.r[15]-64u);
        if(variant==4u)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);
        if(variant==5u)f.put(context+16u,0x7FC12345u);
        if(variant==6u)f.put(family::entry,0u);
        if(variant==7u)f.put(rows,0x8CFFFFF0u);
        if(variant==8u)--f.cpu.r[15];
        f.observe(variant!=9u);const auto state=architecture(f.cpu);
        const std::vector<std::uint8_t> before(f.ram->bytes().begin(),f.ram->bytes().end());
        require(!family::try_execute(f.cpu,&f.immutable),"unsafe input admitted");
        require(state==architecture(f.cpu) && f.events.empty() && std::equal(before.begin(),before.end(),f.ram->bytes().begin()),"decline mutated guest");++declines;
    }
    const auto pose_setup=[](Fixture& f,unsigned switches,unsigned rotation){
        f.cpu.pc=sonic::pose_blend::entry;
        f.put(0x8C19AC84u,rotation?0x8C639F38u:0x8C639C34u);
        f.put(0x8C88FE7Cu,0x3EC00000u);
        f.put(0x8C88FE80u,((switches&1u)?1u:0u)|((switches&2u)?0xFFFF0000u:0u));
        f.put(0x8C88FE84u,(switches&4u)?0x8000u:0u);
        constexpr std::array<std::uint32_t,6> positions{0x40000000u,0xBF800000u,0x3F000000u,0x41600000u,0x40E00000u,0xBF400000u};
        constexpr std::array<std::uint32_t,6> angles{0x1F002u,0xFFFFABC0u,0u,0xFFFF0001u,0x70000001u,0x10101u};
        constexpr std::array<std::uint32_t,3> scales{0x3F800000u,0x40000000u,0x3E800000u};
        for(unsigned i=0;i<6u;++i){f.put(0x8C88FE8Cu+i*4u,positions[i]);f.put(0x8C88FEECu+i*4u,angles[i]);}
        for(unsigned i=0;i<3u;++i)f.put(0x8C88FEBCu+i*4u,scales[i]);
    };
    unsigned pose_cases=0,pose_declines=0;
    for(unsigned mode=0;mode<4u;++mode)for(unsigned switches=0;switches<8u;++switches)for(unsigned rotation=0;rotation<2u;++rotation){
        Fixture n(image,2u,0u,0u,mode,false),r(image,2u,0u,0u,mode,false);
        pose_setup(n,switches,rotation);pose_setup(r,switches,rotation);
        std::cout<<"pose case "<<pose_cases<<" switches="<<switches<<" rotation="<<rotation<<" mode="<<mode<<'\n';
        compare(n,r);++pose_cases;
    }
    for(unsigned variant=0;variant<12u;++variant){
        Fixture n(image,2u,0u,0u,variant&3u,false),r(image,2u,0u,0u,variant&3u,false);
        for(auto* f:{&n,&r}){
            pose_setup(*f,7u,variant&1u);
            if(variant<6u){constexpr std::array<std::uint32_t,6> weights{0u,0x3F800000u,0x3FA00000u,0xBF000000u,0x7FC12345u,0x7F800000u};f->put(0x8C88FE7Cu,weights[variant]);}
            if(variant==6u)for(unsigned i=0;i<6u;++i)f->put(0x8C88FEECu+i*4u,0u);
            if(variant==7u){f->cpu.xf[3]=0x7FC12345u;f->cpu.xf[9]=0x7F800000u;}
            if(variant==8u)f->cpu.r[15]&=0x1FFFFFFFu;
            if(variant==9u)f->cpu.r[15]|=0x20000000u;
            if(variant==10u)for(unsigned i=0;i<6u;++i)f->put(0x8C88FE8Cu+i*4u,i+1u);
            if(variant==11u)for(unsigned i=0;i<6u;++i)f->put(0x8C88FEECu+i*4u,0x7FFFFFFFu-i);
        }
        std::cout<<"pose variant "<<variant<<'\n';compare(n,r);++pose_cases;
    }
    for(unsigned variant=0;variant<8u;++variant){
        Fixture f(image,2u,0u,0u,0u,false);pose_setup(f,7u,0u);
        if(variant==0u)f.put(0x8C19AC84u,0x8C639C36u);
        if(variant==1u)f.cpu.r[15]=0x8C88FEACu;
        if(variant==2u)f.cpu.r[15]=0x8C19ACA0u;
        if(variant==3u)f.cpu.r[15]=0x8C000008u;
        if(variant==4u)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);
        if(variant==5u)f.put(sonic::pose_blend::entry,0u);
        if(variant==6u)--f.cpu.r[15];
        f.observe(variant!=7u);const auto state=architecture(f.cpu);
        const std::vector<std::uint8_t> before(f.ram->bytes().begin(),f.ram->bytes().end());
        require(!sonic::pose_blend::try_execute(f.cpu,&f.immutable),"unsafe pose admitted");
        require(state==architecture(f.cpu) && f.events.empty() && std::equal(before.begin(),before.end(),f.ram->bytes().begin()),"pose decline mutated guest");++pose_declines;
    }
    std::cout<<"SONIC_ANIMATION_HIERARCHY_TEST_OK cases="<<cases<<" mutation_free_rejections="<<declines<<" reference_epochs="<<reference_epochs<<'\n';
    std::cout<<"SONIC_POSE_BLEND_TEST_OK cases="<<pose_cases<<" mutation_free_rejections="<<pose_declines<<'\n';
#ifndef _WIN32
    if(product_observer)require(family::statistics().direct_write_calls==cases &&
        sonic::pose_blend::statistics().direct_write_calls==pose_cases,"product direct writes not reached");
#endif
    std::cout<<"SONIC_MODEL_DIRECT_WRITES animation="<<family::statistics().direct_write_calls
        <<" pose="<<sonic::pose_blend::statistics().direct_write_calls<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_ANIMATION_HIERARCHY_TEST_FAILED "<<e.what()<<'\n';return 1;}
