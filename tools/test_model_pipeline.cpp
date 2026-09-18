#include "sonic_model_pipeline.hpp"
#include "sonic_palette_lighting.hpp"
#include "sonic_render_context.hpp"
#include "sonic_scalar_write_view.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <tuple>
using namespace katana::runtime;
namespace family=sonic::model_pipeline;
constexpr std::uint32_t model=0x8CC00000u,points=model+0x100u,normals=model+0x500u,
    mesh=model+0x1000u,materials=model+0x1100u,output=model+0x2000u,palette=model+0x3000u,returned=0x8C010000u;
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
auto architecture(const CpuState& c){return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,
    c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.mach,c.macl,c.fpul,
    c.read_fpscr(),c.sr,c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,c.last_exception_cause);}
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
struct Fixture;
void visibility_reference(Fixture&,float);
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{std::vector<NativePortImmutableRange>{
        {0x0C036FFCu,0x29Cu,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)}}};
    unsigned variant,count,interrupt_at=0,decline_closed_at=0;
    bool real_visibility{};float horizontal_extra{};
    std::uint32_t protected_address{},protected_size{};
    std::vector<decltype(architecture(cpu))> entries;
    Fixture(std::span<const std::uint8_t> image,unsigned owner,unsigned n,unsigned v,unsigned mode):variant(v),count(n){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|mode);
        for(unsigned i=0;i<16;++i){cpu.r[i]=0x12340000u+i;cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i)+.125f);cpu.xf[i]=i%5==0?0x3F800000u:0u;}
        cpu.r[4]=model;cpu.gbr=model+0x80u;cpu.r[15]=0x8CFFFF00u;cpu.pc=owner;cpu.pr=returned;
        put(model,points);put(model+4,normals);put(model+8,n);put(model+12,mesh);put(model+16,materials);
        put(mesh,0x80000001u);put(materials+24,0u);put(materials+36,0xA5123456u);
        put(cpu.gbr+52,materials);put(cpu.gbr+44,0x80000u);put(cpu.gbr+60,output);put(cpu.gbr+88,palette);
        put(0x8C88F58Cu,output);put(0x8C754E08u,v==1?1u:0u);
        put(0x8C8FFE1Cu,v&1u);put(0x8C8FFE20u,0xFF00FFFFu);put(0x8C8FFE24u,0x12340000u);
        put(0x8C88FBF4u,model+0x7000u);put(0x8C88FC14u,model+0x8000u);put(0x8C88F56Cu,v&1u?0x4000u:0u);
        for(unsigned i=0;i<9u;++i)put(model+0x7090u+i*4u,0x43210000u+i);
        for(unsigned i=0;i<5u;++i)put(model+0x8000u+i*4u,0x12340000u+i);
        for(unsigned i=0;i<(n+2)*3;++i){put(points+i*4,std::bit_cast<std::uint32_t>(float(int(i%13)-6)*.125f));put(normals+i*4,std::bit_cast<std::uint32_t>(float(int(i%5)-2)*.25f));}
        for(unsigned i=0;i<1024;++i)put(palette+i*4,0xFF112200u+i);
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{immutable.observe_write(e);},GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        cpu.memory.set_guest_write_batch_observer({&immutable,[](void*,std::span<const GuestWriteEvent>)noexcept{return true;},
            [](void* g,std::span<const GuestWriteEvent> w)noexcept{for(const auto& e:w)static_cast<NativePortImmutableWriteGuard*>(g)->observe_write(e);}});
        sonic::scalar_writes::bind(cpu.memory,immutable,cpu.memory.guest_write_observer_generation());
    }
    ~Fixture(){sonic::scalar_writes::unbind(&cpu.memory,&immutable);}
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4);}
    std::uint32_t get(std::uint32_t a){std::uint32_t v;std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4);return v;}
    family::SharedOperation operation(){
        const sonic::scalar_writes::View view(cpu.memory,&immutable,false,false,true);
        return {&cpu,&immutable,cpu.memory.direct_linear_memory_guard(false),view.closed_region_snapshot(),this,
            [](void* p,std::uint32_t a,std::uint32_t n)noexcept{
                const auto& f=*static_cast<Fixture*>(p);const auto b=f.protected_address&0x1FFFFFFFu;
                return !f.protected_size || !(a<std::uint64_t(b)+f.protected_size && b<std::uint64_t(a)+n);
            },false,true};
    }
    static family::ClosedCall closed(void* opaque,CpuState& c,std::uint32_t entry,family::SharedOperation* operation){
        auto& f=*static_cast<Fixture*>(opaque);
        if(f.decline_closed_at==entry)return family::ClosedCall::Declined;
        if(entry==0x8C03718Cu && f.real_visibility){
            f.entries.push_back(architecture(c));return family::visibility(c,*operation,f.horizontal_extra);
        }
        return child(opaque,c,entry)?family::ClosedCall::Complete:family::ClosedCall::Interrupted;
    }
    static bool child(void* opaque,CpuState& c,std::uint32_t entry){
        auto& f=*static_cast<Fixture*>(opaque);f.entries.push_back(architecture(c));
        if(f.interrupt_at==entry){c.r[0]=0xBADCA11u;return false;}
        if(entry==sonic::render_context::capture_entry || entry==sonic::render_context::commit_entry){
            require(sonic::render_context::try_execute(c,&f.immutable),"context fixture declined");return true;
        }
        if(entry==0x8C037350u){require(sonic::palette_lighting::try_execute(c,&f.immutable),"palette fixture declined");return true;}
        if(entry==0x8C03718Cu && f.real_visibility){visibility_reference(f,f.horizontal_extra);return true;}
        if(entry==0x8C03718Cu){c.r[0]=0x1234;c.r[1]=0x5678;c.fr[0]=0x41200000;c.t=f.variant==2;}
        else if(entry==0x8C037294u){
            require(c.r[13]==0u,"clip count not reset");
            if(family::active){require(family::active->points.size()==((f.count+1)&~1u)+1u,"extra transform point lost");
                require(std::memcmp(family::active->points.data(),f.ram->bytes().data()+(points&0xFFFFFFu),family::active->points.size()*12)==0,"captured points differ");}
            // Exercise the real direct-output contract against a retained RAM
            // transaction, including untouched fourth words and extra records.
            auto direct=family::projection_output(c,c.r[4],f.count,output);
            const auto size=((f.count+1u)&~1u)*16u;
            require(direct.empty()==!family::active,"direct-output scope mismatch");
            require(family::projection_output(c,c.r[4]+4u,f.count,output).empty(),"wrong model borrowed output");
            require(family::projection_output(c,c.r[4],f.count+1u,output).empty(),"wrong count borrowed output");
            require(family::projection_output(c,c.r[4],f.count,output+32u).empty(),"wrong address borrowed output");
            std::vector<std::uint8_t> scratch;
            if(direct.empty())scratch.assign(f.ram->bytes().data()+(output&0xFFFFFFu),f.ram->bytes().data()+(output&0xFFFFFFu)+size);
            auto destination=direct.empty()?std::span<std::uint8_t>{scratch}:direct;
            for(unsigned offset=0;offset<size;offset+=16)for(unsigned axis=0;axis<3;++axis){
                const std::uint32_t word=0x3F800000u+offset*16u+axis;
                std::memcpy(destination.data()+offset+axis*4u,&word,4u);
            }
            if(!direct.empty())require(family::publish_projection(c,c.r[4],f.count,output),"output publication declined");
            else {
                const auto pointer=output;
                const std::array writes{
                    LinearMemoryTransactionWrite{(c.gbr+60u)&0x1FFFFFFFu,{reinterpret_cast<const std::uint8_t*>(&pointer),4u}},
                    LinearMemoryTransactionWrite{output&0x1FFFFFFFu,scratch}};
                require(c.memory.commit_linear_transaction_batch(writes,CodeWriteSource::Copy),"reference output transaction failed");
            }
            for(unsigned i=0;i<16;++i)c.fr[i]=std::bit_cast<std::uint32_t>(float(i)+.5f);
            c.r[13]=f.variant==3?f.count:0u;c.r[0]=0x76543210u;c.t=false;
        }else if(entry==0x8C0376D0u){c.r[0]=0;f.put(0x8C89004Cu,0x12345678u);f.put(c.gbr+44,0x80000800u);}
        else throw std::runtime_error("unexpected child");
        c.pc=c.pr;return true;
    }
};

void visibility_setup(Fixture& f,float x,float y,float z,float extra,unsigned variant){
    const auto bits=[](float v){return std::bit_cast<std::uint32_t>(v);};
    f.cpu.fr[12]=bits(-extra);f.cpu.fr[13]=bits(640.0f+extra);
    f.put(model+24,bits(x));f.put(model+28,bits(y));f.put(model+32,bits(z));f.put(model+36,bits(2.0f));
    f.put(f.cpu.gbr+8,bits(320));f.put(f.cpu.gbr+12,bits(1));f.put(f.cpu.gbr+16,bits(1));
    f.put(0x8C88F530u,bits(320));f.put(0x8C88F534u,bits(240));
    f.put(0x8C88F540u,bits(0));f.put(0x8C88F544u,bits(0));f.put(0x8C88F548u,bits(640));f.put(0x8C88F54Cu,bits(480));
    f.put(0x8C88F554u,bits(1000));f.put(0x8C88F56Cu,variant?0x34u:0u);
    f.put(0x8C88F5A0u,0x00FFFFFFu);f.put(0x8C88F5A4u,0x80000000u);
    f.put(0x8C8FFE1Cu,variant);f.put(0x8C754E08u,0u);
}
void visibility_reference(Fixture& f,float extra){
    // Only widen the two X FCMP operands. The rest is the actual PAL owner.
    auto bytes=f.ram->writable_bytes();std::uint16_t left=extra?0xF0C5:0xF085,right=extra?0xFBD5:0xFB85;
    std::memcpy(bytes.data()+0x371D2,&left,2);std::memcpy(bytes.data()+0x371E0,&right,2);
    const auto end=f.cpu.pr;unsigned steps=0;
    while(f.cpu.pc!=end && ++steps<512u)(void)execute_dynamic_sh4_block(f.cpu,services,1u);
    require(f.cpu.pc==end && !f.cpu.trap_pending,"real visibility reference failed");
    left=0xF085;right=0xFB85;
    std::memcpy(bytes.data()+0x371D2,&left,2);std::memcpy(bytes.data()+0x371E0,&right,2);
}
unsigned visibility_checks(std::span<const std::uint8_t> image){
    unsigned cases=0;
    const std::array positions{
        std::array{-400.0f,0.0f,100.0f},std::array{-120.0f,0.0f,100.0f},std::array{0.0f,0.0f,100.0f},
        std::array{120.0f,0.0f,100.0f},std::array{400.0f,0.0f,100.0f},std::array{0.0f,-300.0f,100.0f},
        std::array{0.0f,300.0f,100.0f},std::array{0.0f,0.0f,-10.0f},std::array{0.0f,0.0f,1200.0f},
        std::array{0.0f,0.0f,1.0f}};
    for(float extra:{0.0f,106.66667f,253.33333f})for(unsigned mode:{0u,1u,fpscr_fr_mask})for(auto p:positions)for(bool normal_gbr:{false,true}){
        Fixture a(image,0x8C03718Cu,3u,0u,mode),b(image,0x8C03718Cu,3u,0u,mode);
        if(normal_gbr){a.cpu.gbr=b.cpu.gbr=0x8C8FFE00u;}
        visibility_setup(a,p[0],p[1],p[2],extra,cases&1u);visibility_setup(b,p[0],p[1],p[2],extra,cases&1u);
        auto op=a.operation();op.sources_proven=true;
        require(family::visibility(a.cpu,op,extra)==family::ClosedCall::Complete,"visibility declined");
        visibility_reference(b,extra);
        if(architecture(a.cpu)!=architecture(b.cpu))std::cerr<<"visibility case="<<cases<<" fpscr="<<std::hex<<a.cpu.read_fpscr()<<' '<<b.cpu.read_fpscr()<<std::dec<<'\n';
        require(architecture(a.cpu)==architecture(b.cpu),"real visibility CPU differs");
        require(std::ranges::equal(a.ram->bytes(),b.ram->bytes()),"real visibility RAM differs");++cases;
    }
    // Exceptional operands retain SDK semantics inside the closed body.
    for(auto value:{0x7FC00000u,0x7F800000u,0xFF800000u,1u,0x00800000u,0x80000000u})for(unsigned axis:{0u,2u}){
        Fixture a(image,0x8C03718Cu,3u,0u,0u),b(image,0x8C03718Cu,3u,0u,0u);
        visibility_setup(a,0,0,100,0,1);visibility_setup(b,0,0,100,0,1);
        a.put(model+24+axis*4,value);b.put(model+24+axis*4,value);
        auto op=a.operation();op.sources_proven=true;
        require(family::visibility(a.cpu,op,0)==family::ClosedCall::Complete,"exceptional visibility declined");
        visibility_reference(b,0);
        require(architecture(a.cpu)==architecture(b.cpu) && std::ranges::equal(a.ram->bytes(),b.ram->bytes()),"exceptional visibility differs");++cases;
    }
    for(unsigned kind=0;kind<17u;++kind){
        Fixture f(image,0x8C03718Cu,3u,0u,0u);visibility_setup(f,0,0,100,0,1);
        auto op=f.operation();op.sources_proven=true;
        if(kind<6){f.protected_address=f.cpu.gbr+(kind==5?52u:28u+kind*4u);f.protected_size=4;}
        if(kind==6)op.revoke();if(kind==7)op.sources_proven=false;if(kind==8)op.write={};
        if(kind==9)f.put(model+12,0x8CFFFFFEu);
        if(kind==10)f.put(model+16,0xFFFFFFF0u);
        if(kind==11)f.put(model+16,f.cpu.gbr+28u-20u);
        if(kind==12)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);
        if(kind==13)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_exception_enable_mask);
        if(kind==14)f.cpu.gbr=model-28u;
        if(kind==15)f.cpu.gbr=0x8C037198u-28u;
        if(kind==16)op.cpu=nullptr;
        const auto before=architecture(f.cpu);const std::vector<std::uint8_t> bytes(f.ram->bytes().begin(),f.ram->bytes().end());
        require(family::visibility(f.cpu,op,0)==family::ClosedCall::Declined,"unsafe visibility accepted");
        require(before==architecture(f.cpu) && std::ranges::equal(bytes,f.ram->bytes()),"visibility decline mutated guest");++cases;
    }
    // Every newly admitted GBR publication is also fenced at the parent entry.
    for(unsigned offset:{28u,32u,36u,40u,44u,52u})for(unsigned owner:{0x8C03700Cu,0x8C037098u}){
        Fixture f(image,owner,3u,0u,0u);f.protected_address=f.cpu.gbr+offset;f.protected_size=4;
        auto op=f.operation();const auto before=architecture(f.cpu);
        const std::vector<std::uint8_t> bytes(f.ram->bytes().begin(),f.ram->bytes().end());
        require(family::execute(f.cpu,&f.immutable,{&f,Fixture::child,Fixture::closed},&op)==family::Outcome::Declined,"parent missed visibility publication");
        require(before==architecture(f.cpu) && std::ranges::equal(bytes,f.ram->bytes()),"parent rejection mutated guest");++cases;
    }
    for(unsigned owner:{0x8C03700Cu,0x8C037098u})for(float extra:{0.0f,106.66667f})for(float z:{100.0f,-10.0f})for(unsigned mode:{0u,1u})for(bool normal_gbr:{false,true}){
        Fixture a(image,owner,3u,0u,mode),b(image,owner,3u,0u,mode);
        if(normal_gbr)for(auto* f:{&a,&b}){
            std::memcpy(f->ram->writable_bytes().data()+0x8FFE00u,f->ram->bytes().data()+(f->cpu.gbr&0xFFFFFFu),96u);
            f->cpu.gbr=0x8C8FFE00u;
        }
        for(auto* f:{&a,&b}){visibility_setup(*f,0,0,z,extra,1);f->real_visibility=true;f->horizontal_extra=extra;}
        auto op=a.operation();
        require(family::execute(a.cpu,&a.immutable,{&a,Fixture::child,Fixture::closed},&op)==family::Outcome::Complete,"real cull composition declined");
        unsigned steps=0;
        while(b.cpu.pc!=returned && ++steps<2000u){
            if(b.cpu.pc==0x8C03718Cu || b.cpu.pc==0x8C037294u || b.cpu.pc==0x8C037350u || b.cpu.pc==0x8C0376D0u ||
               b.cpu.pc==sonic::render_context::capture_entry || b.cpu.pc==sonic::render_context::commit_entry)Fixture::child(&b,b.cpu,b.cpu.pc);
            else (void)execute_dynamic_sh4_block(b.cpu,services,1u);
        }
        require(op.intact && architecture(a.cpu)==architecture(b.cpu) && a.entries==b.entries,"real cull parent CPU differs");
        require(std::ranges::equal(a.ram->bytes(),b.ram->bytes()),"real cull parent RAM differs");++cases;
    }
    std::cout<<"SONIC_MODEL_VISIBILITY_OK cases="<<cases<<"\n";return cases;
}

int main(int argc,char** argv){try{
    require(argc==2 || (argc==3 && (std::string(argv[2])=="--shared-only" || std::string(argv[2])=="--visibility-only")),"usage: test_model_pipeline RAM [--shared-only|--visibility-only]");
#ifdef _WIN32
    _putenv_s("SARECOMP_INTERNAL_DIAGNOSTICS","0");
#else
    setenv("SARECOMP_INTERNAL_DIAGNOSTICS","0",1);
#endif
    std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000u,"RAM size");unsigned cases=0;
    if(argc==3 && std::string(argv[2])=="--visibility-only"){visibility_checks(image);return 0;}
    if(argc==2){
    for(unsigned owner:{0x8C037098u,0x8C037108u})for(unsigned n:{2u,3u,16u,17u})for(unsigned v=0;v<8;++v)for(unsigned mode:{0u,1u}){
        Fixture a(image,owner,n,v,mode),b(image,owner,n,v,mode);
        if(v>=4){a.cpu.r[4]&=0x1FFFFFFFu;b.cpu.r[4]&=0x1FFFFFFFu;}
        if(v>=6){a.cpu.write_fpscr(a.cpu.read_fpscr()|fpscr_fr_mask);b.cpu.write_fpscr(b.cpu.read_fpscr()|fpscr_fr_mask);}
        require(family::execute(a.cpu,&a.immutable,{&a,Fixture::child})==family::Outcome::Complete,"owner declined");
        unsigned steps=0;
        while(b.cpu.pc!=returned && ++steps<2000){
            if(b.cpu.pc==0x8C03718Cu || b.cpu.pc==0x8C037294u || b.cpu.pc==0x8C037350u || b.cpu.pc==0x8C0376D0u)Fixture::child(&b,b.cpu,b.cpu.pc);
            else (void)execute_dynamic_sh4_block(b.cpu,services,1u);
            require(!b.cpu.trap_pending,"original wrapper trapped");
        }
        if(architecture(a.cpu)!=architecture(b.cpu))for(unsigned i=0;i<16;++i)if(a.cpu.r[i]!=b.cpu.r[i])std::cerr<<"R"<<i<<' '<<std::hex<<a.cpu.r[i]<<' '<<b.cpu.r[i]<<'\n';
        require(architecture(a.cpu)==architecture(b.cpu),"architectural state differs");
        require(a.entries==b.entries,"child entry state differs");
        require(std::ranges::equal(a.ram->bytes(),b.ram->bytes()),"RAM differs");require(!family::active,"scope leaked");++cases;
    }
    for(unsigned variant=0;variant<13;++variant){
        Fixture f(image,0x8C037108u,3u,0u,0u);
        if(variant==0)f.put(model,0x8C8FFE5Cu);
        if(variant==1)f.put(model+4,output);
        if(variant==2)f.put(0x8C88F58Cu,0x8C8FFE60u);
        if(variant==3)f.put(model+8,0u);
        if(variant==4)f.put(0x8C037108u,0u);
        if(variant==5)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);
        if(variant==6)sonic::scalar_writes::unbind(&f.cpu.memory,&f.immutable);
        if(variant==7)f.put(model,0x8CFFFFF0u);
        if(variant==8)f.cpu.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{},GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        if(variant==9)f.cpu.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{},GuestWriteObserverContract::General);
        if(variant==10)f.put(0x8C037350u,0u);
        if(variant==11)f.put(0x8C037294u,0u);
        if(variant==12)f.put(0x8C0376D0u,0u);
        const auto before=architecture(f.cpu);const std::vector<std::uint8_t> bytes(f.ram->bytes().begin(),f.ram->bytes().end());
        require(family::execute(f.cpu,&f.immutable,{&f,Fixture::child})==family::Outcome::Declined,"unsafe owner admitted");
        require(before==architecture(f.cpu) && std::ranges::equal(bytes,f.ram->bytes()),"decline mutated state");++cases;
    }
    for(unsigned target:{0x8C03718Cu,0x8C037294u,0x8C037350u,0x8C0376D0u}){
        Fixture f(image,0x8C037098u,3u,0u,0u);f.interrupt_at=target;
        require(family::execute(f.cpu,&f.immutable,{&f,Fixture::child})==family::Outcome::Interrupted,"interrupted child restarted");
        require(f.cpu.pc==target && f.cpu.r[0]==0xBADCA11u && !family::active,"failed frontier was changed");++cases;
    }
    }
    // Complete fused submissions, retaining every CPU/RAM effect of the
    // original wrappers and existing native child implementations.
    for(unsigned owner:{0x8C03700Cu,0x8C037098u,0x8C037108u})for(unsigned mode:{0u,1u,fpscr_fr_mask})for(unsigned v=0;v<5;++v){
        Fixture a(image,owner,v&1?3u:16u,v,mode),b(image,owner,v&1?3u:16u,v,mode);
        auto operation=a.operation();
        require(family::execute(a.cpu,&a.immutable,{&a,Fixture::child,Fixture::closed},&operation)==family::Outcome::Complete,"shared owner declined");
        unsigned steps=0;
        while(b.cpu.pc!=returned && ++steps<2000){
            if(b.cpu.pc==0x8C03718Cu || b.cpu.pc==0x8C037294u || b.cpu.pc==0x8C037350u || b.cpu.pc==0x8C0376D0u ||
               b.cpu.pc==sonic::render_context::capture_entry || b.cpu.pc==sonic::render_context::commit_entry)Fixture::child(&b,b.cpu,b.cpu.pc);
            else (void)execute_dynamic_sh4_block(b.cpu,services,1u);
            require(!b.cpu.trap_pending,"shared original trapped");
        }
        require(operation.intact && operation.sources_proven,"closed operation revoked");
        require(architecture(a.cpu)==architecture(b.cpu) && a.entries==b.entries,"shared CPU or callback entry differs");
        require(std::ranges::equal(a.ram->bytes(),b.ram->bytes()) && !family::active,"shared RAM differs or capture leaked");++cases;
    }
    for(unsigned target:{0x8C03718Cu,0x8C037294u,0x8C037350u,0x8C0376D0u,
                         sonic::render_context::capture_entry,sonic::render_context::commit_entry})for(bool stop:{false,true}){
        const auto owner=target==sonic::render_context::capture_entry || target==sonic::render_context::commit_entry?0x8C03700Cu:0x8C037098u;
        Fixture a(image,owner,3u,0u,0u),b(image,owner,3u,0u,0u);
        a.decline_closed_at=stop?0u:target;a.interrupt_at=b.interrupt_at=stop?target:0u;
        auto operation=a.operation();
        const auto actual=family::execute(a.cpu,&a.immutable,{&a,Fixture::child,Fixture::closed},&operation);
        const auto expected=family::execute(b.cpu,&b.immutable,{&b,Fixture::child});
        require(actual==expected && actual==(stop?family::Outcome::Interrupted:family::Outcome::Complete),"shared fallback frontier");
        require(!operation.intact && !operation.sources_proven,"foreign/failed child retained parent proof");
        require(architecture(a.cpu)==architecture(b.cpu) && std::ranges::equal(a.ram->bytes(),b.ram->bytes()),"shared fallback state differs");
        require(!family::active,"fallback capture leaked");++cases;
    }
    for(unsigned kind=0;kind<10;++kind){
        Fixture f(image,0x8C037108u,3u,0u,0u);auto operation=f.operation();
        if(kind<7){
            const std::array<std::pair<std::uint32_t,std::uint32_t>,7> writes{{
                {0x8C8FFE5Cu,48},{output,64},{0x8C03D760u,12},{f.cpu.gbr+44,4},
                {f.cpu.gbr+52,4},{f.cpu.gbr+60,8},{0x8C89004Cu,4}}};
            f.protected_address=writes[kind].first;f.protected_size=writes[kind].second;
        }
        if(kind==7)operation.revoke();
        if(kind==8)operation.cpu=nullptr;
        if(kind==9)operation.write={};
        const auto cpu=architecture(f.cpu);const std::vector<std::uint8_t> ram(f.ram->bytes().begin(),f.ram->bytes().end());
        require(family::execute(f.cpu,&f.immutable,{&f,Fixture::child,Fixture::closed},&operation)==family::Outcome::Declined,"unadmitted shared writes");
        require(cpu==architecture(f.cpu) && std::ranges::equal(ram,f.ram->bytes()),"shared rejection mutated guest");++cases;
    }
    for(unsigned kind=0;kind<6;++kind){
        Fixture f(image,0x8C03700Cu,3u,0u,0u);auto operation=f.operation();
        if(kind==0){f.protected_address=model+0x7090u;f.protected_size=16;}
        if(kind==1){f.protected_address=model+0x8000u;f.protected_size=20;}
        if(kind==2)f.put(0x8C88FBF4u,0xFFFFFFFCu);
        if(kind==3)f.put(0x8C88FC14u,0x8CFFFFF0u);
        if(kind==4)f.put(0x8C88FC14u,points);
        if(kind==5)f.put(0x8C03700Cu,0u);
        const auto before=architecture(f.cpu);const std::vector<std::uint8_t> bytes(f.ram->bytes().begin(),f.ram->bytes().end());
        require(family::execute(f.cpu,&f.immutable,{&f,Fixture::child,Fixture::closed},&operation)==family::Outcome::Declined,"unsafe context submission");
        require(before==architecture(f.cpu) && std::ranges::equal(bytes,f.ram->bytes()),"context rejection mutated guest");++cases;
    }
    for(const auto address:{0x0C036FFCu,0x0C03700Cu,0x0C037098u,0x0C037294u,0x0C037350u,0x0C0376D0u,0x0C605CECu,0x0C605D4Au})
        require(family::source_overlap(address,1u) && family::source_overlap(address-1u,2u),"model source fence gap");
    require(!family::source_overlap(0x0CE00000u,4u),"model source fence overbroad");
    require(family::statistics().direct_outputs>0u,"direct output was never exercised");
    std::cout<<"SONIC_MODEL_PIPELINE_OK cases="<<cases<<" normal_reuses="<<family::statistics().normal_reuses
        <<" direct_outputs="<<family::statistics().direct_outputs<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
