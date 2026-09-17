#include "sonic_native_model_memory.hpp"
#include "sonic_palette_lighting.hpp"
#include "sonic_palette_batch.hpp"
#include "sonic_matrix_stack.hpp"
#include "sonic_matrix_vectors.hpp"
#include "sonic_matrix_inverse.hpp"
#include "sonic_motion_sampling.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <tuple>
#include <vector>
using namespace katana::runtime;
namespace mm=sonic::model_memory;
namespace {
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
constexpr std::uint32_t object=0x8CE00000u,work=0x8CE10000u,points=0x8CE20000u;
constexpr std::uint32_t output=0x8CE30000u,palette=0x8CE40000u,matrix=0x8CE50000u;
constexpr std::uint32_t table=0x8CE60000u,counts=0x8CE61000u,keys=0x8CE70000u,returned=0x8CF80000u;
const auto protected_ranges=[] {
    constexpr auto kind=native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable);
    std::vector<NativePortImmutableRange> ranges{{0x0C037350u,0x110u,kind},
        {sonic::matrix_stack::push_entry&0x1FFFFFFFu,sonic::matrix_stack::push_size,kind},
        {sonic::matrix_stack::pop_entry&0x1FFFFFFFu,sonic::matrix_stack::pop_size,kind},
        {sonic::matrix_inverse::inverse_entry&0x1FFFFFFFu,sonic::matrix_inverse::inverse_size,kind},
        {sonic::matrix_inverse::determinant_entry&0x1FFFFFFFu,sonic::matrix_inverse::determinant_size,kind}};
    for(auto s:sonic::matrix_vectors::leaves)ranges.push_back({s.entry&0x1FFFFFFFu,s.size,kind});
    for(auto s:sonic::motion_sampling::source_spans())ranges.push_back({s.address&0x1FFFFFFFu,std::uint32_t(s.bytes.size()),kind});
    std::ranges::sort(ranges,{},&NativePortImmutableRange::physical_address);
    std::vector<NativePortImmutableRange> merged;
    for(auto r:ranges){
        if(!merged.empty() && r.physical_address<=merged.back().physical_address+merged.back().byte_size)
            merged.back().byte_size=std::max(merged.back().byte_size,r.physical_address+r.byte_size-merged.back().physical_address);
        else merged.push_back(r);
    }
    return merged;
}();
auto architecture(const CpuState& c){return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,
    c.ssr,c.spc,c.sgr,c.dbr,c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,
    c.mach,c.macl,c.fpul,c.read_fpscr(),c.sr,c.t,c.s,c.q,c.m,c.trap_pending,
    c.exception_generation,c.last_exception_cause,c.sleeping,c.prefetch_count,c.tlb_load_count);}
using Event=std::tuple<std::uint32_t,std::size_t,CodeWriteSource,bool,std::uint32_t>;
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{protected_ranges};
    std::vector<Event> events;
    Fixture(std::span<const std::uint8_t> image,unsigned family,unsigned variant,unsigned mode){
        std::copy(image.begin(),image.end(),ram->writable_bytes().begin());
        cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|(mode&1u)|((mode&2u)?fpscr_fr_mask:0u));
        cpu.pr=returned;cpu.gbr=work;cpu.r[15]=0x8CF00000u;
        cpu.r[4]=object;cpu.r[5]=points;cpu.r[6]=output;cpu.r[7]=0x12345678u;
        cpu.t=cpu.s=cpu.q=cpu.m=true;cpu.fpul=0x98765432u;
        for(unsigned i=0;i<16u;++i){cpu.xf[i]=std::bit_cast<std::uint32_t>(i%5u==0u?1.0f:0.0f);
            cpu.fr[i]=std::bit_cast<std::uint32_t>(float(i)*.125f-.75f);put(matrix+i*4u,cpu.xf[i]);}
        put(matrix,0x40000000u);put(matrix+20u,0x40400000u);put(matrix+40u,0xC0800000u);
        for(unsigned i=0;i<120u;++i)put(points+4u*i,std::bit_cast<std::uint32_t>(float(int(i%13u)-6)*.125f));
        if(family==0u){
            cpu.fpscr|=fpscr_flag_inexact_mask;
            cpu.pc=0x8C037350u;put(object+4u,points);put(object+8u,std::array{2u,3u,4u,5u,16u,17u,32u}[variant%7u]);
            put(work+44u,variant%3u==2u?0x80000u:0u);put(work+52u,object+64u);
            put(object+68u,variant%3u==1u?1u:0u);put(work+60u,output);put(work+88u,palette);
            put(0x8C038F18u,std::bit_cast<std::uint32_t>(127.5f));
            put(0x8C038F24u,0x3F800000u);put(0x8C038F28u,0xBF000000u);put(0x8C038F2Cu,0x3E800000u);
            for(unsigned i=0;i<1024u;++i)put(palette+i*4u,0xC0010000u^(i*0x9E3779B9u));
        }else if(family==1u){
            // Include raw reserved state: the retained FSCHG pair masks it,
            // while full-stack/empty-pop early returns must preserve it.
            cpu.fpscr|=fpscr_flag_mask|fpscr_cause_mask|fpscr_exception_enable_mask;
            if(mode&1u)cpu.fpscr|=~fpscr_writable_mask;
            constexpr std::array raw{0u,0x80000000u,1u,0x80000001u,0x7F800000u,
                0xFF800000u,0x7FC00001u,0x7FA00001u};
            for(unsigned i=0;i<16u;++i){
                cpu.xf[i]=raw[(i+mode)%raw.size()]^(i<<8u);
                put(points+i*4u,raw[(i+variant)%raw.size()]^(i<<9u));
                put(matrix+i*4u,raw[(i+variant+mode)%raw.size()]^(i<<10u));
                put(matrix-64u+i*4u,raw[(i+3u)%raw.size()]^(i<<11u));
            }
            cpu.pc=variant<3u?sonic::matrix_stack::push_entry:sonic::matrix_stack::pop_entry;
            put(0x8C88F5D8u,8u);put(0x8C88F5DCu,variant==2u?8u:3u);put(0x8C88F538u,matrix);
            cpu.r[4]=variant<3u?(variant==0u?0u:points):(variant==3u?1u:3u);
        }else if(family==2u){
            auto leaf=variant%4u;cpu.pc=sonic::matrix_vectors::leaves[leaf].entry;
            cpu.r[4]=leaf==2u?output:(variant<4u?0u:matrix);cpu.r[5]=leaf==3u?output:points;
            put(0x8C88FFB0u,variant&1u);
            if(variant>=8u && leaf<2u)cpu.r[6]=matrix+16u; // preserve read-after-write alias behavior
        }else if(family==3u){
            cpu.pc=variant<4u?sonic::matrix_inverse::inverse_entry:sonic::matrix_inverse::determinant_entry;
            cpu.r[4]=variant&1u?matrix:0u;
            if(variant%4u>=2u){for(unsigned i=0;i<16u;++i)put(matrix+i*4u,0u);cpu.xf.fill(0u);}
        }else{
            const auto owner=variant%4u,n=variant<4u?0u:variant<8u?1u:4u;
            cpu.pc=sonic::motion_sampling::entries[owner];
            put(0x8C88FD84u,table);put(0x8C88FD88u,counts);put(0x8C88FD94u,3u);
            put(0x8C88FD8Cu,std::bit_cast<std::uint32_t>(5.25f));put(table+12u,n?keys:0u);put(counts+12u,n);
            for(unsigned i=0;i<3u;++i){put(object+8u+i*4u,std::bit_cast<std::uint32_t>(float(i)*1.5f-.25f));
                put(object+20u+i*4u,0xFFFF1234u*(i+1u));put(object+32u+i*4u,std::bit_cast<std::uint32_t>(float(i)*.25f+.5f));}
            for(unsigned k=0;k<n;++k){put(keys+k*16u,k*10u);for(unsigned i=0;i<3u;++i)
                put(keys+k*16u+4u+i*4u,owner<2u?std::bit_cast<std::uint32_t>(float(k*3u+i)*.25f-.5f):0x10001u*(k+1u)*(i+1u));}
        }
    }
    void put(std::uint32_t a,std::uint32_t v){std::memcpy(ram->writable_bytes().data()+(a&0xFFFFFFu),&v,4u);}
    std::uint32_t peek(std::uint32_t a){std::uint32_t v;std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4u);return v;}
    void observed(){cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{
        events.emplace_back(e.address,e.size,e.source,e.bytes_changed,peek(e.address));immutable.observe_write(e);
    },GuestWriteObserverContract::StableForPrevalidatedLinearWrites);}
    void product(){
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{immutable.observe_write(e);},
            GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        cpu.memory.set_guest_write_batch_observer({&immutable,
            [](void*,std::span<const GuestWriteEvent>)noexcept{return true;},
            [](void* p,std::span<const GuestWriteEvent> events)noexcept{for(auto e:events)static_cast<NativePortImmutableWriteGuard*>(p)->observe_write(e);}});
        sonic::scalar_writes::bind(cpu.memory,immutable,cpu.memory.guest_write_observer_generation());
    }
    ~Fixture(){sonic::scalar_writes::unbind(&cpu.memory,&immutable);}
    bool run(unsigned f){switch(f){
        case 0:return sonic::palette_lighting::try_execute(cpu,&immutable);
        case 1:return sonic::matrix_stack::try_execute(cpu,&immutable);
        case 2:return sonic::matrix_vectors::try_execute(cpu,&immutable);
        case 3:return sonic::matrix_inverse::try_execute(cpu,&immutable);
        default:return sonic::motion_sampling::try_execute(cpu,&immutable);}}
};
}
int main(int argc,char** argv)try{
    require(argc==3,"PAL RAM and direct/fallback required");
    sonic::diagnostics::internal_runtime_enabled=false;
    std::ifstream input(argv[1],std::ios::binary);require(bool(input),"RAM missing");
    std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(input),{}};require(image.size()==0x1000000u,"RAM size");
    const bool expect_direct=std::string_view(argv[2])=="direct";
    require(mm::closed_leaves_enabled(),"set SARECOMP_NATIVE_CLOSED_MEMORY=1");
    unsigned cases=0;std::uint64_t words=0;
    for(unsigned f=0;f<5u;++f)for(unsigned mode=0;mode<4u;++mode)
    for(unsigned variant=0;variant<std::array{12u,5u,12u,8u,12u}[f];++variant){
        for(unsigned observer=0;observer<3u;++observer){
            Fixture fast(image,f,variant,mode),reference(image,f,variant,mode);
            reference.observed();
            if(observer==0u)fast.product();else if(observer==1u)fast.observed();
            else{fast.product();fast.observed();} // revoke the registered observer generation
            const auto before=mm::closed_leaf_counts;
            const auto before_metrics=fast.cpu.memory.performance_counters();
            require(fast.run(f),"native fixture declined");
            const auto after=mm::closed_leaf_counts;
            const auto after_metrics=fast.cpu.memory.performance_counters();
            require(reference.run(f),"retained fixture declined");
            require(architecture(fast.cpu)==architecture(reference.cpu),"CPU differs");
            require(std::ranges::equal(fast.ram->bytes(),reference.ram->bytes()),"RAM differs");
            require(!fast.immutable.write_detected() && !reference.immutable.write_detected(),"immutable write");
            if(observer)require(fast.events==reference.events,"ordered fallback writes differ");
            require((after.calls>before.calls)==(observer==0u && expect_direct),"wrong native admission");
            if(observer==0u && expect_direct)require(after.words-before.words==reference.events.size(),"write count differs");
            if(f==1u && observer==0u && expect_direct){
                // Whole matrix copies still account every read and overwritten
                // MOVCA write of the original closed leaf.
                const auto expected_reads=variant==0u?3u:variant==1u?19u:
                    variant==2u?2u:variant==3u?18u:1u;
                const auto expected_accesses=expected_reads+reference.events.size();
                require(after_metrics.indexed_region_hits-before_metrics.indexed_region_hits==expected_accesses &&
                    after_metrics.unobserved_accesses-before_metrics.unobserved_accesses==expected_accesses,
                    "matrix bulk memory accounting differs");
            }
            words+=after.words-before.words;++cases;
        }
    }
    if(sonic::palette_batch::enabled() && expect_direct)
        require(sonic::palette_batch::counts.closed_loops==48,"palette closed loop coverage");
    if(sonic::matrix_stack::bulk_enabled() && expect_direct){
        const auto& c=sonic::matrix_stack::bulk_counts;
        require(c.pushes==8u && c.pops==4u && c.saved==12u && c.loaded==8u,"matrix bulk coverage");
    }
    std::cout<<"NATIVE_CLOSED_MEMORY_OK cases="<<cases<<" direct_words="<<words
        <<" palette_closed_loops="<<sonic::palette_batch::counts.closed_loops
        <<" matrix_bulk_pushes="<<sonic::matrix_stack::bulk_counts.pushes
        <<" matrix_bulk_pops="<<sonic::matrix_stack::bulk_counts.pops
        <<" full_ram=exact cpu=exact observer_revoke=ok\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"NATIVE_CLOSED_MEMORY_FAIL "<<e.what()<<'\n';return 1;}
