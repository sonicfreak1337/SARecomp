#include "sonic_matrix_vectors.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <xmmintrin.h>

using namespace katana::runtime;
namespace cm = sonic::matrix_vectors;
namespace {
constexpr auto returned = 0x8CF80000u;
constexpr std::array entries{cm::leaves[0].entry,cm::leaves[1].entry,cm::leaves[2].entry,cm::leaves[3].entry};
constexpr std::array sizes{cm::leaves[0].size,cm::leaves[1].size,cm::leaves[2].size,cm::leaves[3].size};
constexpr auto code_ranges=[] {
    std::array<NativePortImmutableRange,4> result{};
    for(unsigned i=0;i<4u;++i) result[i]={entries[i]&0x1FFFFFFFu,sizes[i],
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)};
    return result;
}();
void require(bool value,const char* why) { if (!value) throw std::runtime_error(why); }
struct Services final : PlatformServices {
    std::string_view name() const noexcept override { return "matrix-vectors-byte-reference"; }
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
    std::array<unsigned char,32> result{};
    require(bytes.size()<=ULONG_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),result.data(),ULONG(result.size()))>=0,"SHA-256 failed");
    constexpr char hex[]="0123456789abcdef"; std::string text(64,'0');
    for (std::size_t i=0;i<result.size();++i) { text[i*2]=hex[result[i]>>4]; text[i*2+1]=hex[result[i]&15]; }
    return text;
}
// Instruction/cycle/provenance bookkeeping belongs to native-hook integration.
auto architecture(const CpuState& c) {
    return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,
        c.tra,c.tea,c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.mach,c.macl,c.fpul,
        c.read_fpscr(),c.sr,c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,
        c.last_exception_cause,c.sleeping,c.prefetch_count,c.tlb_load_count);
}
// Store order and callback-visible pointers matter, particularly normalize's
// reverse stores, which commit R4 only AFTER each notification.
using Write = std::tuple<std::uint32_t,std::size_t,CodeWriteSource,bool,std::uint32_t,
    std::array<std::uint32_t,5>,std::uint32_t,
    std::array<std::uint32_t,16>,std::array<std::uint32_t,16>>;
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{code_ranges};
    std::vector<Write> events;
    std::string label;
    Fixture(std::span<const std::uint8_t> boot,unsigned leaf,std::uint32_t mode=fpscr_dn_mask,
            bool read_only=false) {
        cpu.memory.map_region("main-ram",0x0C000000u,ram,
            read_only?MemoryRegionAccess::ReadOnly:MemoryRegionAccess::ReadWrite);
        // The SDK only permits a direct alias window over fully writable RAM.
        // A read-only fixture must remain a normal mapped region; admission
        // then declines without touching it. Binding first would throw before
        // this negative test can exercise the leaf at all.
        if (!read_only)
            cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        auto bytes=ram->writable_bytes(); std::fill(bytes.begin(),bytes.end(),0xCDu);
        std::copy(boot.begin(),boot.end(),bytes.begin()+0x10000u);
        cpu.pc=entries[leaf]; cpu.pr=returned; cpu.gbr=0x8CE80000u;
        cpu.write_sr(sr_md_mask); cpu.write_fpscr(mode);
        cpu.t=true; cpu.s=true; cpu.q=true; cpu.m=true;
        cpu.fpul=0xDEADBEEFu; cpu.mach=0x12345678u; cpu.macl=0x87654321u;
        for (unsigned i=0;i<16u;++i) {
            cpu.r[i]=0xABCD1200u+i; cpu.fr[i]=0xCAFE0000u+i; cpu.xf[i]=0x7FA00000u+i;
        }
        for (unsigned i=0;i<8u;++i) cpu.r_bank[i]=0x11220000u+i;
        for (unsigned i=4u;i<=7u;++i) cpu.r[i]=0x8CE00000u+(i-4u)*0x100u;
        cpu.r[15]=0x8CF00000u;
        vector(cpu.r[4],{0x3F800000u,0x40000000u,0x40400000u});
        vector(cpu.r[5],{0xC0800000u,0x40A00000u,0x40C00000u});
        vector(cpu.r[6],{0x40E00000u,0xC1000000u,0x41100000u});
        label="leaf="+std::to_string(leaf)+" fpscr="+std::to_string(mode);
        events.reserve(16u);
    }
    void put(std::uint32_t a,std::uint32_t v) {
        auto b=ram->writable_bytes(); std::memcpy(b.data()+(a&0xFFFFFFu),&v,4u);
    }
    std::uint32_t peek(std::uint32_t a) const {
        std::uint32_t v; std::memcpy(&v,ram->bytes().data()+(a&0xFFFFFFu),4u); return v;
    }
    void vector(std::uint32_t a,std::array<std::uint32_t,3> values) {
        for (unsigned i=0;i<3u;++i) put(a+i*4u,values[i]);
    }
    void observe(bool stable=true) {
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e) noexcept {
            events.emplace_back(e.address,e.size,e.source,e.bytes_changed,peek(e.address),
                std::array{cpu.r[0],cpu.r[4],cpu.r[5],cpu.r[6],cpu.r[7]},cpu.read_fpscr(),cpu.fr,cpu.xf);
            immutable.observe_write(e);
        },stable?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:GuestWriteObserverContract::General);
    }
    void bind_mmu(bool active) {
        cpu.address_space=std::make_shared<RuntimeAddressSpace>();
        cpu.address_space->write_mmucr(cpu.mmucr);
        cpu.address_space->set_mode(active?AddressTranslationMode::Mmu:AddressTranslationMode::NoMmu);
    }
};
void execute_reference(CpuState& cpu,unsigned leaf) {
    // Untouched original executor, fetching the installed SHA-bound SH4 words.
    // No synthetic instruction substitute or FTRC reference repair is involved.
    for (unsigned n=0;cpu.pc!=returned && n<120u;++n) {
        require(cpu.pc>=entries[leaf] && cpu.pc<entries[leaf]+sizes[leaf],"reference left SHA-bound leaf");
        (void)execute_dynamic_sh4_block(cpu,services,1u);
        require(cpu.exception_generation==0u,"reference exception");
    }
    require(cpu.pc==returned,"reference did not return");
}
void compare(Fixture& n,Fixture& r,unsigned leaf) {
    const auto previous_host=_mm_getcsr();
    const auto host=0x1F80u|0x21u|((n.cpu.fpscr&1u)?0x4000u:0xE040u);
    _mm_setcsr(host);
    n.observe(); r.observe();
    if (!cm::try_execute(n.cpu,&n.immutable)) {
        std::cerr<<n.label<<'\n'; throw std::runtime_error("eligible leaf declined");
    }
    require(_mm_getcsr()==host,"native leaf changed host FP state");
    execute_reference(r.cpu,leaf);
    require(_mm_getcsr()==host,"reference changed host FP state");
    _mm_setcsr(previous_host);
    if (architecture(n.cpu)!=architecture(r.cpu)) {
        std::cerr<<n.label<<'\n'<<std::hex;
        for (unsigned i=0;i<16u;++i) {
            if (n.cpu.r[i]!=r.cpu.r[i]) std::cerr<<"r"<<i<<' '<<n.cpu.r[i]<<'/'<<r.cpu.r[i]<<'\n';
            if (n.cpu.fr[i]!=r.cpu.fr[i]) std::cerr<<"fr"<<i<<' '<<n.cpu.fr[i]<<'/'<<r.cpu.fr[i]<<'\n';
            if (n.cpu.xf[i]!=r.cpu.xf[i]) std::cerr<<"xf"<<i<<' '<<n.cpu.xf[i]<<'/'<<r.cpu.xf[i]<<'\n';
        }
        std::cerr<<"fpscr "<<n.cpu.read_fpscr()<<'/'<<r.cpu.read_fpscr()<<std::dec<<'\n';
        throw std::runtime_error("architectural state differs");
    }
    require(std::equal(n.ram->bytes().begin(),n.ram->bytes().end(),r.ram->bytes().begin()),"RAM/CPU-stack differs");
    if (n.events!=r.events) {
        std::cerr<<n.label<<" native stores="<<n.events.size()<<" reference stores="<<r.events.size()<<'\n';
        throw std::runtime_error("ordered stores/value/source/changed/pointers/FPSCR differ");
    }
    require(!n.immutable.write_detected() && !r.immutable.write_detected(),"protected code written");
    if (n.cpu.address_space && r.cpu.address_space)
        require(n.cpu.address_space->snapshot()==r.cpu.address_space->snapshot(),"MMU state differs");
}
void decline(Fixture& f,const NativePortImmutableWriteGuard* guard) {
    const auto before=architecture(f.cpu);
    const auto ram=std::vector<std::uint8_t>(f.ram->bytes().begin(),f.ram->bytes().end());
    const auto events=f.events; const auto metrics=f.cpu.memory.performance_counters();
    const auto translation=f.cpu.address_space?std::optional{f.cpu.address_space->snapshot()}:std::nullopt;
    require(!cm::try_execute(f.cpu,guard),"unsafe leaf admitted");
    require(before==architecture(f.cpu) && events==f.events &&
        std::equal(ram.begin(),ram.end(),f.ram->bytes().begin()),"decline mutated guest");
    const auto after=f.cpu.memory.performance_counters();
    require(metrics.indexed_region_hits==after.indexed_region_hits && metrics.reference_region_probes==after.reference_region_probes &&
        metrics.observed_accesses==after.observed_accesses && metrics.unobserved_accesses==after.unobserved_accesses,"decline changed metrics");
    require(!translation || *translation==f.cpu.address_space->snapshot(),"decline changed MMU");
}
} // namespace

void seed(Fixture& f,unsigned leaf,bool ram_matrix,unsigned sample,bool normalize) {
    constexpr std::array words{0u,0x80000000u,0x3F800000u,0xBF800000u,0x40000000u,
        0x3F800001u,0x00800000u,0x007FFFFFu,1u,0x7F7FFFFFu,0x7F800000u,0xFF800000u,
        0x7FC00001u,0x7FA00001u};
    for(unsigned i=0;i<16u;++i) {
        const auto v=sample==0u ? (i%5u==0u?0x3F800000u:0u):words[(sample+i)%words.size()];
        f.cpu.xf[i]=v;f.put(f.cpu.r[4]+i*4u,v);
    }
    f.vector(f.cpu.r[5],sample==0u?std::array{0x3FC00000u,0xC0100000u,0x40E80000u}:
        std::array{words[sample%words.size()],words[(sample+2u)%words.size()],words[(sample+5u)%words.size()]});
    f.put(0x8C88FFB0u,normalize?1u:0u);
    if(leaf!=2u && !ram_matrix) f.cpu.r[4]=0u;
    f.label+=" ram="+std::to_string(ram_matrix)+" sample="+std::to_string(sample)+" norm="+std::to_string(normalize);
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"usage: test_matrix_vectors <installed-content-root>");
        const auto boot=read(std::filesystem::path(argv[1])/"boot.bin");
        require(boot.size()==6735296u && digest(boot)=="b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af","boot SHA differs");
        for(unsigned i=0;i<4u;++i)
            require(digest(std::span<const std::uint8_t>(boot).subspan(entries[i]-0x8C010000u,sizes[i]))==cm::leaves[i].sha256,"leaf SHA differs");
        unsigned cases=0u;
        constexpr std::array modes{fpscr_dn_mask,fpscr_dn_mask|1u,
            fpscr_dn_mask|fpscr_fr_mask,fpscr_dn_mask|fpscr_fr_mask|1u|fpscr_flag_mask|fpscr_cause_mask};
        for(unsigned leaf=0u;leaf<4u;++leaf) {
            for(auto mode:modes) for(bool ram_matrix:{false,true}) {
                if(leaf==2u && !ram_matrix) continue;
                for(unsigned sample=0;sample<15u;++sample) for(bool normalize:{false,true}) {
                    if(leaf!=1u && normalize) continue;
                    Fixture n(boot,leaf,mode),r(boot,leaf,mode);
                    seed(n,leaf,ram_matrix,sample,normalize);seed(r,leaf,ram_matrix,sample,normalize);
                    compare(n,r,leaf);++cases;
                }
                for(unsigned alias=0;alias<5u;++alias) {
                    Fixture n(boot,leaf,mode),r(boot,leaf,mode);
                    for(auto* f:{&n,&r}) {
                        seed(*f,leaf,ram_matrix,0u,true);
                        if(alias<3u) {
                            constexpr std::array segments{0u,0x80000000u,0xA0000000u};
                            for(unsigned reg=4u;reg<=6u;++reg) if(f->cpu.r[reg])
                                f->cpu.r[reg]=(f->cpu.r[reg]&0x1FFFFFFFu)|segments[(alias+reg)%3u];
                        } else if(leaf<2u) {
                            f->cpu.r[6]=alias==3u?f->cpu.r[5]+4u:(ram_matrix?f->cpu.r[4]+4u:f->cpu.r[5]);
                        } else if(leaf==3u && ram_matrix) f->cpu.r[5]=f->cpu.r[4]+48u;
                        f->label+=" alias="+std::to_string(alias);
                    }
                    compare(n,r,leaf);++cases;
                }
            }
            for(unsigned scenario=0u;scenario<15u;++scenario) {
                Fixture f(boot,leaf);seed(f,leaf,true,0u,false);f.observe();
                const auto output=leaf==2u?4u:leaf==3u?5u:6u;
                switch(scenario) {
                case 0:f.cpu.sr|=sr_fd_mask;break;
                case 1:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);break;
                case 2:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_pr_mask);break;
                case 3:f.observe(false);break;
                case 4:f.put(entries[leaf],0u);break;
                case 5:f.cpu.r[output]=0x0CFFFFFCu;break;
                case 6:f.cpu.r[output]=0x0D000000u;break;
                case 7:f.cpu.r[output]+=2u;break;
                case 8:f.cpu.memory.clear_direct_linear_alias_window();break;
                case 9:f.cpu.sleeping=true;break;
                case 10:f.cpu.trap_pending=true;break;
                case 11:f.cpu.pc+=2u;break;
                case 12:f.cpu.write_sr(0u);break;
                case 13:f.cpu.r[output]=entries[leaf];break;
                case 14:f.cpu.mmucr=1u;f.bind_mmu(true);f.cpu.r[output]&=0x1FFFFFFFu;break;
                }
                decline(f,&f.immutable);++cases;
            }
            {Fixture f(boot,leaf);seed(f,leaf,true,0u,false);decline(f,nullptr);++cases;}
            if(leaf<2u) for(auto mode:{0u,fpscr_dn_mask|2u,fpscr_dn_mask|fpscr_exception_enable_mask}) {
                Fixture f(boot,leaf,mode);seed(f,leaf,true,0u,false);decline(f,&f.immutable);++cases;
            }
        }
        std::cout<<"SONIC_MATRIX_VECTORS_TEST_PASS cases="<<cases<<" original_words=1 state=exact ordered_stores=exact aliases=1\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"SONIC_MATRIX_VECTORS_TEST_FAIL "<<e.what()<<'\n';return 1;}
}
