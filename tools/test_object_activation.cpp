#include "sonic_object_activation.hpp"
#include "sonic_scalar_write_view.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <tuple>
using namespace katana::runtime;
namespace family=sonic::object_activation;
constexpr std::uint32_t distance_owner=0x8C09105Au,activation_owner=0x8C0912C0u,lifetime_owner=0x8C091928u;
constexpr std::uint32_t returned=0x8C010000u,records=0x8C79A320u,definitions=0x8CC00000u,
    type_root=0x8CC01000u,types=0x8CC02000u,saved=0x8CC03000u,tasks=0x8CC04000u,
    works=0x8CC05000u,reference1=0x8CC06000u,reference2=0x8CC06100u,reference3=0x8CC06200u,
    vector_address=0x8CC07000u;
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
auto architecture(const CpuState& c){return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,
    c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.mach,c.macl,c.fpul,
    c.read_fpscr(),c.fpscr,c.sr,c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,c.last_exception_cause);}
struct Services final:PlatformServices {
    std::string_view name()const noexcept override{return "object-activation-original-bytes";}
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
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{std::vector<NativePortImmutableRange>{
        {0x0C04F698u,8u,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
        {0x0C04F6D8u,4u,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
        {0x0C04F7E0u,14u,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
        {0x0C04F81Cu,8u,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
        {0x0C09105Au,46u,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
        {0x0C0912C0u,0x22Cu,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
        {0x0C091580u,12u,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
        {0x0C091928u,0xAAu,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
        {0x0C091A00u,24u,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)}}};
    std::vector<decltype(architecture(cpu))> entries;
    std::uint16_t stage_level=0x12u,stage_act=0x34u;
    unsigned character=0u,constructors=0,frees=0,mutation=0;
    bool null_constructor=false,null_work=false;
    std::uint32_t interrupt_at=0;
    Fixture(std::span<const std::uint8_t> image,std::uint32_t owner,unsigned mode){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|mode);
        for(unsigned i=0;i<16;++i){cpu.r[i]=0x12340000u+i;cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i)+.125f);cpu.xf[i]=0xA5110000u+i;}
        for(unsigned i=0;i<8;++i)cpu.r_bank[i]=0xFEDC0000u+i;
        cpu.fpul=0xCAB01234u;cpu.mach=0x31415926u;cpu.macl=0x27182818u;
        cpu.r[4]=tasks;cpu.r[15]=0x8CFFFF00u;cpu.pc=owner;cpu.pr=returned;
        put(0x8C78C548u,reference1);put(0x8C18B72Cu,reference2);put(0x8C78C54Cu,reference3);
        half(0x8C19B3A8u,0u);half(0x8C7A4028u,0u);put(0x8C1C0E18u,1u);
        half(0x8C79A31Cu,3u);put(0x8C79A30Cu,type_root);put(type_root+4,types);
        position(reference1+32,0.f,0.f,0.f);position(reference2+32,1.f,2.f,3.f);position(reference3+32,0.f,0.f,0.f);byte(reference3+9,0);
        for(unsigned i=0;i<4;++i){
            const auto rec=records+i*16,def=definitions+i*64,typ=types+i*20,save=saved+i*64,task=tasks+i*128,work=works+i*128;
            byte(rec,0xFEu);byte(rec+1,0xA5u);half(rec+2,0x8000u);put(rec+4,0u);put(rec+8,def);put(rec+12,save);
            half(def,0x8000u+i);half(def+2,0x8123u+i);half(def+4,0x7FFFu-i);half(def+6,0xFEDCu+i);
            position(def+8,0.f,0.f,0.f);for(unsigned j=0;j<3;++j)put(def+20+j*4,0x3F800001u+j+i);
            byte(typ,0x82u+i);byte(typ+1,0xF0u+i);half(typ+2,1u);putf(typ+4,25.f);put(typ+8,0x12345670u+i);put(typ+12,0x8C010200u+i*4);
            for(unsigned j=0;j<9;++j)put(save+j*4,0x3F000001u+i*16+j);
            position(save+12,0.f,0.f,0.f);put(task+28,rec);put(task+32,work);put(task+16,0x8C010300u);position(work+32,0.f,0.f,0.f);
        }
        cpu.fr[4]=std::bit_cast<std::uint32_t>(25.f);
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{immutable.observe_write(e);},GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        cpu.memory.set_guest_write_batch_observer({&immutable,[](void*,std::span<const GuestWriteEvent>)noexcept{return true;},
            [](void* g,std::span<const GuestWriteEvent> w)noexcept{for(const auto& e:w)static_cast<NativePortImmutableWriteGuard*>(g)->observe_write(e);}});
        sonic::scalar_writes::bind(cpu.memory,immutable,cpu.memory.guest_write_observer_generation());
    }
    ~Fixture(){sonic::scalar_writes::unbind(&cpu.memory,&immutable);}
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4);}
    void half(std::uint32_t a,std::uint16_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,2);}
    void byte(std::uint32_t a,std::uint8_t v){ram->writable_bytes()[a&0xFFFFFFu]=v;}
    void putf(std::uint32_t a,float v){put(a,std::bit_cast<std::uint32_t>(v));}
    void position(std::uint32_t a,float x,float y,float z){putf(a,x);putf(a+4,y);putf(a+8,z);}
    void publish_queries(){half(0x8C7492FAu,stage_level);half(0x8C7492FCu,stage_act);half(0x8C749308u,static_cast<std::uint16_t>(character));}
    static bool external(std::uint32_t pc){return pc==0x8C09846Eu || pc==0x8C098A82u;}
    static bool child(void* opaque,CpuState& c,std::uint32_t entry){
        auto& f=*static_cast<Fixture*>(opaque);require(external(entry),"unexpected callback");f.entries.push_back(architecture(c));
        if(f.interrupt_at==entry){c.r[0]=0xBADCA11u;c.fr[2]=0x7FC01234u;return false;}
        std::uint32_t result=0;
        if(entry==0x8C09846Eu){
            const auto index=f.constructors++;
            require(index<4u,"unexpected constructor count");result=f.null_constructor?0u:tasks+index*128u;
            if(result)f.put(result+32,f.null_work?0u:works+index*128u);
            // Mutate data only the subsequent record iteration should read.
            if(index==0 && f.mutation==1){f.half(records+16+2,0u);f.half(types+40+2,2u);f.put(0x8C1C0E18u,0u);}
            if(index==0 && f.mutation==2){f.put(records+16+8,definitions+128);f.half(definitions+128,0x8003u);f.put(type_root+4,types+20);f.half(types+80+2,2u);f.byte(types+80,0x91);f.byte(types+81,0xE2);f.put(types+92,0x8C010204u);}
            if(index==0 && f.mutation==3){f.half(records+2,0x8082u);f.put(records+12,saved+64);f.half(0x8C79A31Cu,0u);f.put(0x8C78C548u,reference3);}
        }
        if(entry==0x8C098A82u){
            ++f.frees;require(c.r[4]>=saved && c.r[4]<saved+256u,"wrong saved-state pointer");
            f.put(c.r[4]+36,0xFEE1DEADu);
            if(f.mutation==4){f.half(records+16+2,0x8003u);f.half(records+2,0x80FAu);f.put(types+8,0xBADC0DEu);}
        }
        // Model ordinary callee-clobbered registers, plus retained architectural scratch.
        for(unsigned i=0;i<8;++i)c.r[i]=0xCA110000u+i+unsigned(f.entries.size())*16u;
        for(unsigned i=0;i<12;++i)c.fr[i]=0x3E000000u+i+unsigned(f.entries.size())*16u;
        c.fpul=0xBEEF1234u+unsigned(f.entries.size());c.mach=0x01234567u;c.macl=0x76543210u;
        c.t=(f.entries.size()&1u)!=0;c.r[0]=result;c.pc=c.pr;return true;
    }
};
void reference(Fixture& f){
    unsigned steps=0;
    while(f.cpu.pc!=returned && ++steps<20000u){
        if(Fixture::external(f.cpu.pc)){if(!Fixture::child(&f,f.cpu,f.cpu.pc))return;}
        else (void)execute_dynamic_sh4_block(f.cpu,services,1u);
        require(!f.cpu.trap_pending,"original owner trapped");
    }
    require(f.cpu.pc==returned,"original owner exceeded instruction bound");
}
void compare(Fixture& a,Fixture& b,const std::string& label){
    if(architecture(a.cpu)!=architecture(b.cpu)){
        std::cerr<<label<<" architecture mismatch (native / original)\n"<<std::hex;
        for(unsigned i=0;i<16;++i){if(a.cpu.r[i]!=b.cpu.r[i])std::cerr<<"R"<<std::dec<<i<<' '<<std::hex<<a.cpu.r[i]<<' '<<b.cpu.r[i]<<'\n';
            if(a.cpu.fr[i]!=b.cpu.fr[i])std::cerr<<"FR"<<std::dec<<i<<' '<<std::hex<<a.cpu.fr[i]<<' '<<b.cpu.fr[i]<<'\n';}
        std::cerr<<"PC "<<a.cpu.pc<<' '<<b.cpu.pc<<" PR "<<a.cpu.pr<<' '<<b.cpu.pr<<" FPSCR "<<a.cpu.read_fpscr()<<' '<<b.cpu.read_fpscr()<<" T "<<a.cpu.t<<' '<<b.cpu.t<<'\n';
        throw std::runtime_error("architectural state differs");
    }
    if(a.entries!=b.entries){
        std::cerr<<label<<" callback entry mismatch; counts "<<a.entries.size()<<' '<<b.entries.size()<<'\n';
        for(unsigned i=0;i<std::min(a.entries.size(),b.entries.size());++i)if(a.entries[i]!=b.entries[i])std::cerr<<"first unequal callback="<<i<<'\n';
        throw std::runtime_error("callback architectural state differs");
    }
    if(!std::ranges::equal(a.ram->bytes(),b.ram->bytes())){
        const auto mismatch=std::mismatch(a.ram->bytes().begin(),a.ram->bytes().end(),b.ram->bytes().begin());
        std::cerr<<label<<" RAM mismatch at "<<std::hex<<(0x8C000000u+std::size_t(mismatch.first-a.ram->bytes().begin()))<<'\n';
        throw std::runtime_error("full RAM differs");
    }
}
template<class Setup> void differential(std::span<const std::uint8_t> image,std::uint32_t owner,unsigned mode,const std::string& label,Setup setup){
    Fixture a(image,owner,mode),b(image,owner,mode);setup(a);setup(b);a.publish_queries();b.publish_queries();
    const auto outcome=family::execute(a.cpu,&a.immutable,{&a,Fixture::child});
    require(outcome==(a.interrupt_at?family::Outcome::Interrupted:family::Outcome::Complete),"unexpected owner outcome");
    reference(b);compare(a,b,label);
}
void activation_case(Fixture& f,unsigned v){
    switch(v){
    case 0:break;
    case 1:f.half(0x8C79A31Cu,0u);break;
    case 2:f.half(0x8C79A31Cu,0xFFFFu);break;
    case 3:f.half(0x8C79A31Cu,0x8000u);break;
    case 4:f.put(0x8C78C548u,0u);break;
    case 5:f.half(0x8C19B3A8u,1u);break;
    case 6:f.stage_level=0xFE2Au;f.stage_act=0x99u;break;
    case 7:f.character=7u;f.half(0x8C7A4028u,0u);break;
    case 8:f.character=7u;f.half(0x8C7A4028u,64u);break;
    case 9:f.put(0x8C78C548u,0u);f.put(0x8C18B72Cu,0u);break;
    case 10:f.half(0x8C19B3A8u,0xFFFFu);break;
    case 11:f.half(records+2,0u);f.half(records+18,0x7FFFu);f.half(records+34,0x8001u);break;
    case 12:for(unsigned i=0;i<3;++i)f.half(records+i*16+2,0xFFFCu);break;
    case 13:for(unsigned i=0;i<3;++i)f.half(records+i*16+2,0x8002u);break;
    case 14:f.null_constructor=true;break;
    case 15:f.null_work=true;break;
    case 16:f.null_work=true;for(unsigned i=0;i<3;++i)f.half(records+i*16+2,0x8002u);break;
    case 17:for(unsigned i=0;i<3;++i)f.half(types+i*20+2,0u);break;
    case 18:for(unsigned i=0;i<3;++i)f.half(types+i*20+2,2u);break;
    case 19:for(unsigned i=0;i<3;++i)f.half(types+i*20+2,3u);break;
    case 20:for(unsigned i=0;i<3;++i)f.half(types+i*20+2,4u);f.put(0x8C1C0E18u,0u);break;
    case 21:for(unsigned i=0;i<3;++i)f.half(types+i*20+2,5u);f.put(0x8C1C0E18u,0u);break;
    case 22:for(unsigned i=0;i<3;++i)f.half(types+i*20+2,4u);break;
    case 23:for(unsigned i=0;i<3;++i)f.half(types+i*20+2,5u);break;
    case 24:for(unsigned i=0;i<3;++i)f.position(definitions+i*64+8,6.f,0.f,0.f);break;
    case 25:for(unsigned i=0;i<3;++i)f.position(definitions+i*64+8,4.f,4.f,0.f);break;
    case 26:for(unsigned i=0;i<3;++i)f.position(definitions+i*64+8,3.f,3.f,3.f);break;
    case 27:for(unsigned i=0;i<3;++i)f.position(definitions+i*64+8,3.f,4.f,0.f);break;
    case 28:for(unsigned i=0;i<3;++i)f.putf(types+i*20+4,0.f);break;
    case 29:for(unsigned i=0;i<3;++i)f.putf(types+i*20+4,-1.f);break;
    case 30:for(unsigned i=0;i<3;++i)f.put(types+i*20+4,0x7FC01234u);break;
    case 31:f.mutation=1;break;
    case 32:f.mutation=2;break;
    case 33:f.mutation=3;for(unsigned i=0;i<3;++i)f.half(records+i*16+2,0x8002u);break;
    case 34:f.mutation=4;for(unsigned i=0;i<3;++i)f.half(records+i*16+2,0x8002u);break;
    case 35:f.half(0x8C79A31Cu,1u);f.cpu.r[15]&=0x1FFFFFFFu;break;
    case 36:f.put(records+8,definitions&0x1FFFFFFFu);f.put(type_root+4,types&0x1FFFFFFFu);break;
    case 37:f.half(records+2,0x8002u);f.position(saved+12,10000.f,0.f,0.f);break;
    case 38:f.half(records+2,0x8002u);f.position(saved+12,3.f,4.f,0.f);break;
    case 39:for(unsigned i=0;i<3;++i)f.half(types+i*20+2,0x8007u);break;
    default:throw std::runtime_error("bad activation case");
    }
}
void lifetime_case(Fixture& f,unsigned v){
    f.position(reference1+32,10000.f,0.f,0.f);f.position(reference2+32,10000.f,0.f,0.f);f.position(reference3+32,10000.f,0.f,0.f);
    if(v<27){
        const std::array globals{0x8C18B72Cu,0x8C78C548u,0x8C78C54Cu};
        const std::array refs{reference2,reference1,reference3};
        for(unsigned i=0;i<3;++i){const auto state=v%3u;v/=3u;if(state==0)f.put(globals[i],0u);if(state==1)f.position(refs[i]+32,0.f,0.f,0.f);}
        return;
    }
    switch(v){
    case 27:f.half(records+2,0x8008u);break;
    case 28:f.put(tasks+28,0u);break;
    case 29:f.cpu.fr[4]=0u;break;
    case 30:f.cpu.fr[4]=0x80000000u;break;
    case 31:f.cpu.fr[4]=std::bit_cast<std::uint32_t>(-1.f);break;
    case 32:f.cpu.fr[4]=0x7FC00001u;break;
    case 33:f.cpu.fr[4]=0x7F800000u;break;
    case 34:f.cpu.fr[4]=1u;break;
    case 35:f.cpu.fr[4]=0x7F800001u;break;
    case 36:f.byte(reference3+9,1u);f.position(reference3+32,0.f,0.f,0.f);break;
    case 37:f.byte(reference3+9,0x80u);break;
    case 38:f.byte(reference3+9,0xFFu);break;
    case 39:f.position(reference2+32,3.f,4.f,0.f);break;
    case 40:f.position(reference2+32,4.f,4.f,0.f);break;
    case 41:f.position(reference1+32,3.f,3.f,3.f);break;
    case 42:f.put(tasks+32,works&0x1FFFFFFFu);f.cpu.r[4]&=0x1FFFFFFFu;break;
    case 43:f.put(0x8C18B72Cu,0u);f.position(reference1+32,3.f,4.f,0.f);break;
    case 44:f.put(0x8C18B72Cu,0u);f.put(0x8C78C548u,0u);f.position(reference3+32,0.f,0.f,0.f);break;
    }
}
int main(int argc,char** argv){try{
    require(argc==2 || argc==3,"usage: test_object_activation RAM [distance|activation|lifetime|rejection|interrupt]");
#ifdef _WIN32
    _putenv_s("SARECOMP_INTERNAL_DIAGNOSTICS","0");
#else
    setenv("SARECOMP_INTERNAL_DIAGNOSTICS","0",1);
#endif
    std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000u,"RAM size");unsigned cases=0;
    const std::string selected=argc==3?argv[2]:"all";
    auto enabled=[&](const char* suite){return selected=="all" || selected==suite;};
    const std::array modes{0u,1u,fpscr_fr_mask,fpscr_fr_mask|1u};
    if(enabled("distance")){
        using Vector=std::array<std::uint32_t,7>;
        auto finite=[](float x,float y,float z,float a,float b,float c,float limit){return Vector{
            std::bit_cast<std::uint32_t>(x),std::bit_cast<std::uint32_t>(y),std::bit_cast<std::uint32_t>(z),
            std::bit_cast<std::uint32_t>(a),std::bit_cast<std::uint32_t>(b),std::bit_cast<std::uint32_t>(c),std::bit_cast<std::uint32_t>(limit)};};
        std::vector<Vector> vectors{
            finite(0,0,0,0,0,0,0),finite(3,4,0,0,0,0,25),finite(6,0,0,0,0,0,25),
            finite(4,4,0,0,0,0,25),finite(3,3,3,0,0,0,25),finite(1,2,3,3,2,1,8),
            finite(-4,4,-4,1,-1,1,75),finite(0,0,0,0,0,0,-1),
            finite(0.1f,0.2f,0.3f,-0.1f,-0.2f,-0.3f,0.56f)};
        for(const auto& v:std::array{
                finite(0,0,0,4,1,0,1e30f),finite(0,0,0,6,1,0,1e30f),
                finite(0,0,0,1,16384,0,1e30f),finite(0,0,0,1,24576,0,1e30f),
                finite(0,0,0,0x1p-126f,0,0,1),finite(0,0,0,1e-20f,0,0,1),
                finite(0,0,0,1e30f,0,0,1),finite(-0.f,0,-0.f,-0.f,-0.f,0.f,0),
                finite(0x1p-28f,0,0,1,0,0,2),finite(0x1p-29f,0,0,1,0,0,2),
                finite(-0x1p-28f,0,0,1,0,0,2),finite(-0x1p-29f,0,0,1,0,0,2),
                // Exact binary32 halfway square-adds and their immediate
                // binary32 neighbors, plus wide-exponent nonnegative sums.
                finite(0,0,0,1,0x1p-12f,0,2),
                finite(0,0,0,1,std::bit_cast<float>(0x39800001u),0,2),
                finite(0,0,0,1,std::bit_cast<float>(0x397FFFFFu),0,2),
                finite(0,0,0,8,0x1p-9f,0,65),
                finite(0,0,0,8,std::bit_cast<float>(0x3B000001u),0,65),
                finite(0,0,0,8,std::bit_cast<float>(0x3AFFFFFFu),0,65),
                finite(0,0,0,1,0x1p-60f,0,2),
                finite(0,0,0,0x1p-60f,1,0,2),
                finite(0,0,0,1,0x1p-12f,0x1p-60f,2)})vectors.push_back(v);
        const std::array exceptional{0x80000000u,1u,0x80000001u,0x007FFFFFu,0x00800000u,0x7F7FFFFFu,
            0xFF7FFFFFu,0x7F800000u,0xFF800000u,0x7FC12345u,0xFFC12345u,0x7F812345u};
        for(auto word:exceptional)for(unsigned axis=0;axis<7;++axis){auto v=finite(1,2,3,1,2,3,25);v[axis]=word;vectors.push_back(v);}
        std::uint32_t random=0x31415926u;
        for(unsigned i=0;i<96;++i){Vector v{};for(auto& word:v){random^=random<<13;random^=random>>17;random^=random<<5;word=(random&0x807FFFFFu)|((110u+((random>>24)%36u))<<23);}vectors.push_back(v);}
        for(unsigned mode:modes)for(unsigned i=0;i<vectors.size();++i){
            differential(image,distance_owner,mode,"distance "+std::to_string(i)+" mode "+std::to_string(mode),[&](Fixture& f){
                const auto& v=vectors[i];f.cpu.r[4]=vector_address;
                if(i&1u)f.cpu.r[4]&=0x1FFFFFFFu;
                for(unsigned axis=0;axis<3;++axis){f.put(vector_address+axis*4,v[axis]);f.cpu.fr[4+axis]=v[3+axis];}
                f.cpu.fr[7]=v[6];
                if(i&1u)f.cpu.write_fpscr(f.cpu.read_fpscr()|0x3F000u|(1u<<(2u+i%5u)));
                if(i%7u==0u)f.cpu.fpscr|=0x80000000u;
            });++cases;
        }
    }
    if(enabled("activation"))for(unsigned mode:modes)for(unsigned v=0;v<44;++v){
        differential(image,activation_owner,mode,"activation "+std::to_string(v)+" mode "+std::to_string(mode),[&](Fixture& f){
            activation_case(f,v<40u?v:0u);
            if(v==40){f.stage_level=0x2Au;f.stage_act=0xFF99u;}
            if(v==41){f.stage_level=0x8000u;f.stage_act=0x7FFFu;}
            if(v==42){f.stage_level=0xFFFFu;f.stage_act=0x8000u;}
            if(v==43)f.character=0xFFFFu;
        });++cases;
    }
    if(enabled("lifetime"))for(unsigned mode:modes)for(unsigned v=0;v<45;++v){
        differential(image,lifetime_owner,mode,"lifetime "+std::to_string(v)+" mode "+std::to_string(mode),[&](Fixture& f){lifetime_case(f,v);});++cases;
    }
    if(enabled("rejection"))for(unsigned owner:{distance_owner,activation_owner,lifetime_owner})for(unsigned v=0;v<18;++v){
        Fixture f(image,owner,0u);f.cpu.r[4]=owner==distance_owner?vector_address:tasks;
        if(v==0)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_pr_mask);
        if(v==1)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);
        if(v==2)f.cpu.write_fpscr(fpscr_dn_mask|2u);
        if(v==3)f.cpu.write_fpscr(0u);
        if(v==4)f.cpu.write_fpscr(fpscr_dn_mask|fpscr_exception_enable_mask);
        if(v==5)f.cpu.write_sr(0u);
        if(v==6)f.cpu.write_sr(sr_md_mask|sr_fd_mask);
        if(v==7)f.cpu.sleeping=true;
        if(v==8)f.cpu.trap_pending=true;
        if(v==9)f.put(distance_owner,0u);
        if(v==10)f.cpu.pc=owner+2u;
        if(v==11)f.cpu.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{},GuestWriteObserverContract::General);
        if(v==12){if(owner==distance_owner)f.cpu.r[4]=vector_address+1u;else f.cpu.r[15]=0x8CFFFF01u;}
        if(v==13){if(owner==distance_owner)f.cpu.r[4]=0x8CFFFFF8u;else f.cpu.r[15]=0x8C000004u;}
        if(v==14){if(owner==distance_owner)f.cpu.r[4]=0xCC000000u;else sonic::scalar_writes::unbind(&f.cpu.memory,&f.immutable);}
        if(v==15)f.put(owner,0u);
        if(v==16){if(owner==activation_owner)f.put(0x8C091588u,0u);else if(owner==lifetime_owner)f.put(0x8C091A10u,0u);else f.cpu.r[4]=0u;}
        const auto before=architecture(f.cpu);const std::vector<std::uint8_t> bytes(f.ram->bytes().begin(),f.ram->bytes().end());
        require(family::execute(f.cpu,v==17?nullptr:&f.immutable,{&f,Fixture::child})==family::Outcome::Declined,"unsafe owner admitted");
        require(before==architecture(f.cpu) && std::ranges::equal(bytes,f.ram->bytes()) && f.entries.empty(),"decline mutated state");++cases;
    }
    if(enabled("rejection"))for(auto target:{0x8C04F7E0u,0x8C04F81Cu,0x8C04F698u,0x8C04F6D8u}){
        Fixture f(image,activation_owner,0u);f.put(target,0u);
        const auto before=architecture(f.cpu);const std::vector<std::uint8_t> bytes(f.ram->bytes().begin(),f.ram->bytes().end());
        require(family::execute(f.cpu,&f.immutable,{&f,Fixture::child})==family::Outcome::Declined,"mutated query admitted");
        require(before==architecture(f.cpu) && std::ranges::equal(bytes,f.ram->bytes()) && f.entries.empty(),"query decline mutated state");++cases;
    }
    if(enabled("interrupt"))for(unsigned mode:modes)for(auto target:{0x8C09846Eu,0x8C098A82u}){
        differential(image,activation_owner,mode,"interrupted "+std::to_string(target),[&](Fixture& f){
            f.interrupt_at=target;for(unsigned i=0;i<3;++i)f.half(records+i*16+2,0x8002u);
        });++cases;
    }
    require(cases>0,"unknown suite");
    if(enabled("distance"))require(family::counts.distance_native && family::counts.distance_fallback,
        "distance oracle must exercise the finite kernel AND exact fallback");
    std::cout<<"SONIC_OBJECT_ACTIVATION_OK cases="<<cases<<" distance="<<family::counts.distance_calls
        <<" activation="<<family::counts.activation_calls<<" lifetime="<<family::counts.lifetime_calls
        <<" finite="<<family::counts.distance_native<<" fallback="<<family::counts.distance_fallback<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
