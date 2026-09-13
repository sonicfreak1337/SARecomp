#include "sonic_matrix_inverse.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "katana/sh4/decoder.hpp"
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <cfenv>
#include <xmmintrin.h>

using namespace katana::runtime;
namespace {
using namespace sonic::matrix_inverse;
constexpr auto returned=0x8CF80000u, matrix=0x8CE10000u, stack_top=0x8CF00000u;
constexpr std::array code_range{
    NativePortImmutableRange{inverse_entry&0x1FFFFFFFu,inverse_size,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
    NativePortImmutableRange{determinant_entry&0x1FFFFFFFu,determinant_size,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
};
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
struct Services final : PlatformServices {
    std::string_view name() const noexcept override { return "matrix-inverse-byte-reference"; }
    std::uint32_t abi_version() const noexcept override { return 128u; }
    std::uint32_t guest_cycle_contract() const noexcept override { return 0u; }
    PlatformCapabilities capabilities() const noexcept override { return {}; }
    void read_memory(std::uint32_t, std::span<std::uint8_t>) override { throw std::runtime_error("device read"); }
    void write_memory(std::uint32_t, std::span<const std::uint8_t>) override { throw std::runtime_error("device write"); }
    std::uint64_t scheduler_cycle() const noexcept override { return 0u; }
    std::optional<std::uint64_t> next_scheduler_event_cycle() const noexcept override { return {}; }
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t, std::size_t) override { return {}; }
    std::optional<PlatformInterruptRequest> poll_interrupt() override { return {}; }
    PlatformDmaResult start_dma(const PlatformDmaRequest&) override { throw std::runtime_error("DMA"); }
    PlatformFallbackResult controlled_fallback(CpuState&, const PlatformFallbackRequest&) override {
        throw std::runtime_error("executor fallback");
    }
    bool prefetch(CpuState&, GuestInstructionOrigin, std::uint32_t) override { throw std::runtime_error("PREF"); }
} services;

std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary); require(bool(file), "boot.bin missing");
    return {std::istreambuf_iterator<char>(file), {}};
}
std::string digest(std::span<const std::uint8_t> bytes) {
    std::array<unsigned char, 32> result{};
    require(bytes.size() <= ULONG_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
        const_cast<PUCHAR>(bytes.data()), ULONG(bytes.size()), result.data(), ULONG(result.size())) >= 0,
        "SHA-256 failed");
    constexpr char hex[] = "0123456789abcdef"; std::string text(64, '0');
    for (std::size_t i = 0; i < result.size(); ++i) {
        text[i * 2] = hex[result[i] >> 4]; text[i * 2 + 1] = hex[result[i] & 15];
    }
    return text;
}

// Architectural state only. Executor instruction/cycle/provenance bookkeeping
// is intentionally outside the native hook's architectural leaf contract.
auto architecture(const CpuState& c) {
    return std::tuple(c.r, c.r_bank, c.fr, c.xf, c.pc, c.pr, c.gbr, c.vbr, c.ssr,
        c.spc, c.sgr, c.dbr, c.tra, c.tea, c.expevt, c.intevt, c.pteh, c.ptel,
        c.ptea, c.ttb, c.mmucr, c.mach, c.macl, c.fpul, c.read_fpscr(), c.sr,
        c.t, c.s, c.q, c.m, c.trap_pending, c.exception_generation,
        c.last_exception_cause, c.sleeping, c.prefetch_count, c.tlb_load_count);
}
using Write = std::tuple<std::uint32_t, std::size_t, CodeWriteSource, bool>;
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{code_range};
    std::vector<Write> events;
    std::string label;
    Fixture(std::span<const std::uint8_t> boot,bool determinant,bool xmtrx,unsigned kind,
            unsigned mode,unsigned seed=0u,bool read_only=false) {
        label="det="+std::to_string(determinant)+" xmtrx="+std::to_string(xmtrx)+
            " kind="+std::to_string(kind)+" mode="+std::to_string(mode)+" seed="+std::to_string(seed);
        cpu.memory.map_region("main-ram",0x0C000000u,ram,
            read_only?MemoryRegionAccess::ReadOnly:MemoryRegionAccess::ReadWrite);
        if (!read_only) cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        auto bytes=ram->writable_bytes();std::fill(bytes.begin(),bytes.end(),0xCDu);
        std::copy(boot.begin(),boot.end(),bytes.begin()+0x10000u);
        cpu.pc=determinant?determinant_entry:inverse_entry;cpu.pr=returned;cpu.gbr=0x8CE00000u;
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|mode);
        cpu.t=true;cpu.s=true;cpu.q=true;cpu.m=true;
        cpu.fpul=0xDEADBEEFu;cpu.mach=0x12345678u;cpu.macl=0x87654321u;
        for(unsigned i=0;i<16;++i) {
            cpu.r[i]=0xABCDEF00u+i;cpu.fr[i]=0x3F000000u+i*0x123u;
            cpu.xf[i]=0xBF000000u+i*0x321u;
        }
        for(unsigned i=0;i<8;++i) cpu.r_bank[i]=0x11220000u+i;
        cpu.r[4]=xmtrx?0u:matrix;cpu.r[15]=stack_top;
        std::array<std::uint32_t,16> values{};
        const auto bits=[](float value) {return std::bit_cast<std::uint32_t>(value);};
        for(unsigned i=0;i<16;++i) values[i]=bits(i%5u==0u?1.0f:0.0f);
        switch(kind) {
        case 0:break;
        case 1:values[0]=bits(2.0f);values[5]=bits(3.0f);values[10]=bits(-4.0f);
            values[12]=bits(1.25f);values[13]=bits(-2.5f);values[14]=bits(4.0f);break;
        case 2:values.fill(0u);break;
        case 3:for(unsigned i=0;i<16;++i) values[i]=bits(float(i%4u+1u));break;
        case 4:for(unsigned i=0;i<16;++i) values[i]=bits(i%5u==0u?4.0f:float(int(i%7u)-3)*0.125f);break;
        case 5:for(unsigned i=0;i<16;++i) values[i]=i%5u==0u?0x00800000u:0u;break;
        case 6:for(unsigned i=0;i<16;++i) values[i]=i%5u==0u?0x7F7FFFFFu:bits(0.25f);break;
        case 7:{
            constexpr std::array special{0u,0x80000000u,1u,0x80000001u,0x00800000u,
                0x7F800000u,0xFF800000u,0x7FC00001u,0x7FA00001u,0x7F7FFFFFu};
            for(unsigned i=0;i<16;++i) values[i]=special[(i+seed)%special.size()];break;
        }
        case 8:values[0]=0x3F800000u;values[1]=0x3F800000u;
            values[4]=0x3F800000u;values[5]=0x3F800001u;break;
        case 9:{
            auto state=seed^0x638FF0u;
            for(auto& value:values) {state=state*1664525u+1013904223u;value=state;}
            break;
        }
        default:throw std::runtime_error("unknown matrix case");
        }
        for(unsigned i=0;i<16;++i) put(matrix+i*4u,values[i]);
        if(xmtrx) cpu.xf=values;
    }
    void put(std::uint32_t address,std::uint32_t value) {
        auto bytes=ram->writable_bytes();
        for(unsigned i=0;i<4;++i) bytes[(address&0xFFFFFFu)+i]=static_cast<std::uint8_t>(value>>(i*8u));
    }
    void bind_address_space(bool mmu) {
        cpu.address_space=std::make_shared<RuntimeAddressSpace>();
        cpu.address_space->write_mmucr(cpu.mmucr);
        cpu.address_space->set_mode(mmu?AddressTranslationMode::Mmu:AddressTranslationMode::NoMmu);
    }
    void observe(bool stable=true) {
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e) noexcept {
            events.emplace_back(e.address,e.size,e.source,e.bytes_changed);immutable.observe_write(e);
        },stable?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:GuestWriteObserverContract::General);
    }
};

void execute_reference(CpuState& cpu) {
    for(unsigned n=0;cpu.pc!=returned && n<5000u;++n) {
        require((cpu.pc>=inverse_entry && cpu.pc<inverse_entry+inverse_size) ||
            (cpu.pc>=determinant_entry && cpu.pc<determinant_entry+determinant_size),
            "reference left byte-bound family");
        // The original executor performs JSR and executes the original callee.
        // Neither a reference native hook nor any instruction repair is used.
        (void)execute_dynamic_sh4_block(cpu,services,1u);
        require(cpu.exception_generation==0u,"reference FPU/memory exception");
    }
    require(cpu.pc==returned,"reference did not return");
}
void compare(Fixture& native, Fixture& reference) {
    native.observe(); reference.observe();
    const auto host_before=_mm_getcsr();
    require(sonic::matrix_inverse::try_execute(native.cpu, &native.immutable), "eligible leaf declined");
    require(_mm_getcsr()==host_before,"native changed ambient MXCSR");
    execute_reference(reference.cpu);
    require(_mm_getcsr()==host_before,"reference changed ambient MXCSR");
    if (architecture(native.cpu) != architecture(reference.cpu)) {
        std::cerr << native.label << '\n' << std::hex;
        for (unsigned i = 0; i < 16; ++i) {
            if (native.cpu.r[i] != reference.cpu.r[i]) std::cerr << "r" << i << " native=" << native.cpu.r[i] << " reference=" << reference.cpu.r[i] << '\n';
            if (native.cpu.fr[i] != reference.cpu.fr[i]) std::cerr << "fr" << i << " native=" << native.cpu.fr[i] << " reference=" << reference.cpu.fr[i] << '\n';
            if (native.cpu.xf[i] != reference.cpu.xf[i]) std::cerr << "xf" << i << " native=" << native.cpu.xf[i] << " reference=" << reference.cpu.xf[i] << '\n';
        }
        std::cerr << "T=" << native.cpu.t << '/' << reference.cpu.t << " FPSCR=" << native.cpu.read_fpscr() << '/' << reference.cpu.read_fpscr()
            << " FPUL=" << native.cpu.fpul << '/' << reference.cpu.fpul << std::dec << '\n';
        throw std::runtime_error("architectural register/status mismatch");
    }
    require(std::equal(native.ram->bytes().begin(), native.ram->bytes().end(), reference.ram->bytes().begin()),
        "RAM/stack mismatch");
    if (native.events != reference.events) {
        std::cerr<<native.label<<" native_events="<<native.events.size()
                 <<" reference_events="<<reference.events.size()<<'\n';
        const auto shared=std::min(native.events.size(),reference.events.size());
        std::size_t first=0;
        while(first<shared && native.events[first]==reference.events[first]) ++first;
        const auto show=[](const char* name,const std::vector<Write>& events,std::size_t index) {
            std::cerr<<name<<"["<<index<<"]=";
            if(index>=events.size()) {std::cerr<<"<missing>\n";return;}
            const auto& [address,size,source,changed]=events[index];
            std::cerr<<"address=0x"<<std::hex<<address<<std::dec<<" size="<<size
                     <<" source="<<static_cast<unsigned>(source)<<" changed="<<changed<<'\n';
        };
        for(std::size_t i=first?first-1u:0u;i<std::min(first+3u,std::max(native.events.size(),reference.events.size()));++i) {
            show("native",native.events,i);show("reference",reference.events,i);
        }
        throw std::runtime_error("store order/source/changed flags differ");
    }
    require(!native.immutable.write_detected(), "native wrote protected code");
}
void decline(Fixture& fixture, const NativePortImmutableWriteGuard* guard) {
    const auto before = architecture(fixture.cpu);
    const auto memory = std::vector<std::uint8_t>(fixture.ram->bytes().begin(), fixture.ram->bytes().end());
    const auto events = fixture.events;
    const auto metrics = fixture.cpu.memory.performance_counters();
    const auto translation = fixture.cpu.address_space ?
        std::optional{fixture.cpu.address_space->snapshot()} : std::nullopt;
    require(!sonic::matrix_inverse::try_execute(fixture.cpu, guard), "unsafe admission accepted");
    require(before == architecture(fixture.cpu) && events == fixture.events &&
        std::equal(memory.begin(), memory.end(), fixture.ram->bytes().begin()), "decline mutated guest state");
    const auto after = fixture.cpu.memory.performance_counters();
    require(metrics.indexed_region_hits == after.indexed_region_hits &&
        metrics.reference_region_probes == after.reference_region_probes &&
        metrics.observed_accesses == after.observed_accesses && metrics.unobserved_accesses == after.unobserved_accesses,
        "decline changed memory metrics");
    require(!translation || *translation == fixture.cpu.address_space->snapshot(),
        "decline changed translation state");
}
} // namespace

int main(int argc,char** argv) {
    try {
        require(argc==2,"usage: test_matrix_inverse <installed-content-root>");
        const auto boot=read(std::filesystem::path(argv[1])/"boot.bin");
        require(boot.size()==6735296u && digest(boot)==
            "b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af","retail boot SHA differs");
        require(digest(std::span<const std::uint8_t>(boot).subspan(inverse_entry-0x8C010000u,inverse_size))==
            inverse_source_sha256,"inverse byte SHA differs");
        require(digest(std::span<const std::uint8_t>(boot).subspan(determinant_entry-0x8C010000u,determinant_size))==
            determinant_source_sha256,"determinant byte SHA differs");
        const auto original_mxcsr=_mm_getcsr();
        unsigned cases=0u;
        for(bool determinant:{false,true}) for(bool xmtrx:{false,true})
            for(unsigned mode:{0u,1u,fpscr_fr_mask,fpscr_fr_mask|1u}) for(unsigned kind=0;kind<10u;++kind) {
                _mm_setcsr(0x1F80u|((cases&3u)<<13u)|(cases&0x3Fu)|((cases&4u)?0x8040u:0u));
                Fixture native(boot,determinant,xmtrx,kind,mode),reference(boot,determinant,xmtrx,kind,mode);
                compare(native,reference);++cases;
            }
        for(unsigned seed=1u;seed<=8u;++seed) for(bool determinant:{false,true}) for(bool xmtrx:{false,true}) {
            const auto mode=(seed&1u)|fpscr_fr_mask|fpscr_cause_mask|fpscr_flag_mask;
            Fixture native(boot,determinant,xmtrx,9u,mode,seed),reference(boot,determinant,xmtrx,9u,mode,seed);
            compare(native,reference);++cases;
        }
        for(bool determinant:{false,true}) for(unsigned alias=0;alias<3u;++alias) {
            Fixture native(boot,determinant,false,4u,1u),reference(boot,determinant,false,4u,1u);
            for(auto* f:{&native,&reference}) {
                const auto translate=[&](auto address) {return alias==0u?address&0x1FFFFFFFu:address|0x20000000u;};
                f->cpu.r[4]=translate(f->cpu.r[4]);f->cpu.r[15]=translate(f->cpu.r[15]);
                if(alias==2u) {f->cpu.mmucr=1u;f->bind_address_space(true);}
                else f->bind_address_space(false);
            }
            compare(native,reference);++cases;
        }
        // Exact RAM boundaries for the 64-byte matrix and 68-byte stack.
        for(bool determinant:{false,true}) {
            Fixture native(boot,determinant,false,4u,0u),reference(boot,determinant,false,4u,0u);
            for(auto* f:{&native,&reference}) {
                auto bytes=f->ram->writable_bytes();
                std::copy_n(bytes.begin()+(matrix&0xFFFFFFu),64u,bytes.end()-64u);
                f->cpu.r[4]=0x8CFFFFC0u;f->cpu.r[15]=0x8C000044u;
            }
            compare(native,reference);++cases;
        }
        for(unsigned scenario=0;scenario<24u;++scenario) {
            Fixture f(boot,false,false,4u,0u,0u,scenario==19u);
            switch(scenario) {
            case 0:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_pr_mask);break;
            case 1:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_sz_mask);break;
            case 2:f.cpu.write_fpscr(fpscr_dn_mask|fpscr_exception_enable_mask);break;
            case 3:f.cpu.sr|=sr_fd_mask;break;
            case 4:f.cpu.write_fpscr(0u);break;
            case 5:f.cpu.write_fpscr(fpscr_dn_mask|2u);break;
            case 6:f.cpu.write_sr(0u);break;
            case 7:f.cpu.trap_pending=true;break;
            case 8:f.cpu.sleeping=true;break;
            case 9:break;
            case 10:f.ram->writable_bytes()[(inverse_entry&0xFFFFFFu)+1u]^=1u;break;
            case 11:f.ram->writable_bytes()[(determinant_entry&0xFFFFFFu)+1u]^=1u;break;
            case 12:f.cpu.r[4]+=2u;break;
            case 13:f.cpu.r[4]=0x8CFFFFE0u;break;
            case 14:f.cpu.r[15]+=2u;break;
            case 15:f.cpu.r[4]=stack_top-64u;break;
            case 16:f.cpu.r[4]=inverse_entry;break;
            case 17:f.cpu.r[15]=determinant_entry+68u;break;
            case 18:f.observe(false);break;
            case 19:break;
            case 20:f.cpu.r[4]&=0x1FFFFFFFu;f.cpu.mmucr=1u;break;
            case 21:f.cpu.r[4]&=0x1FFFFFFFu;f.bind_address_space(true);break;
            case 22:f.cpu.pc=inverse_entry+2u;break;
            case 23:f.cpu.r[15]=0x8C000040u;break;
            }
            if(scenario!=18u) f.observe();
            decline(f,scenario==9u?nullptr:&f.immutable);++cases;
        }
        {
            Fixture f(boot,false,false,4u,0u);f.observe();
            const std::array protected_matrix{
                NativePortImmutableRange{matrix&0x1FFFFFFFu,64u,
                    native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)}};
            NativePortImmutableWriteGuard guard{protected_matrix};
            decline(f,&guard);++cases;
        }
        _mm_setcsr(original_mxcsr);
        std::cout<<"MATRIX_INVERSE_TEST_OK cases="<<cases
            <<" original_call bytes_sha registers_fpscr_fr_sz ram_stack ordered_sources host_fp declines\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"MATRIX_INVERSE_TEST_FAIL "<<error.what()<<'\n';return 1;
    }
}
