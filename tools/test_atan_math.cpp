#include "sonic_atan_math.hpp"
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
#include <utility>
#include <cfenv>
#include <xmmintrin.h>

using namespace katana::runtime;
namespace {
using namespace sonic::atan_math;
constexpr auto returned=0x8CF80000u, output_address=0x8CE10000u, stack_top=0x8CF00000u;
constexpr auto code_range=[] {
    std::array<NativePortImmutableRange,source_spans.size()> result{};
    for(std::size_t i=0;i<result.size();++i) result[i]=NativePortImmutableRange{
        source_spans[i].address&0x1FFFFFFFu,source_spans[i].size,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)};
    // Source spans follow entry/API order; the immutable guard requires
    // ascending, disjoint physical ranges instead.
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b) {
        return a.physical_address<b.physical_address;
    });
    return result;
}();
static_assert([] {
    for(std::size_t i=1;i<code_range.size();++i)
        if(std::uint64_t(code_range[i-1u].physical_address)+code_range[i-1u].byte_size>
            code_range[i].physical_address) return false;
    return true;
}());
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
struct Services final : PlatformServices {
    std::string_view name() const noexcept override { return "atan-original-byte-reference"; }
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
    Fixture(std::span<const std::uint8_t> boot,std::uint32_t entry,std::uint32_t x,
            std::uint32_t y,unsigned mode,std::uint32_t exponent=0u,bool read_only=false) {
        label="entry="+std::to_string(entry)+" x="+std::to_string(x)+
            " y="+std::to_string(y)+" exponent="+std::to_string(exponent)+" mode="+std::to_string(mode);
        cpu.memory.map_region("main-ram",0x0C000000u,ram,
            read_only?MemoryRegionAccess::ReadOnly:MemoryRegionAccess::ReadWrite);
        if (!read_only) cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);
        auto bytes=ram->writable_bytes();std::fill(bytes.begin(),bytes.end(),0xCDu);
        std::copy(boot.begin(),boot.end(),bytes.begin()+0x10000u);
        cpu.pc=entry;cpu.pr=returned;cpu.gbr=0x8CE00000u;
        cpu.write_sr(sr_md_mask);cpu.write_fpscr(fpscr_dn_mask|mode);
        cpu.t=true;cpu.s=true;cpu.q=true;cpu.m=true;
        cpu.fpul=0xDEADBEEFu;cpu.mach=0x12345678u;cpu.macl=0x87654321u;
        for(unsigned i=0;i<16;++i) {
            cpu.r[i]=0xABCDEF00u+i;cpu.fr[i]=0x3F000000u+i*0x123u;
            cpu.xf[i]=0xBF000000u+i*0x321u;
        }
        for(unsigned i=0;i<8;++i) cpu.r_bank[i]=0x11220000u+i;
        cpu.r[4]=entry==scale_entry?exponent:output_address;cpu.r[15]=stack_top;
        cpu.fr[4]=x;cpu.fr[5]=y;
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
            // Stable observer: consume only the event and update observer-owned
            // bookkeeping. Inspect the complete RAM state after execution.
            events.emplace_back(e.address,e.size,e.source,e.bytes_changed);immutable.observe_write(e);
        },stable?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:GuestWriteObserverContract::General);
    }
};

std::uint64_t corrected_reference_ftrc=0u;
void execute_reference(CpuState& cpu) {
    for(unsigned n=0;cpu.pc!=returned && n<5000u;++n) {
        bool inside=false;
        for(unsigned i=0;i<4u;++i) inside=inside ||
            (cpu.pc>=source_spans[i].address && cpu.pc<source_spans[i].address+source_spans[i].size);
        require(inside,"reference left original atan family");
        const auto opcode=guest_fetch_u16(cpu,cpu.pc);
        const auto instruction=katana::sh4::decode(opcode);
        if(instruction.kind==katana::sh4::InstructionKind::Ftrc) {
            // Retained interpreter bug: FTRC reads destination=0 rather than
            // decoded source. Correct only these two authenticated words, using
            // the original SDK helper and original operand. Retained AOT unit
            // v8C10EC94... lines 50415/51056 call source 4/3 respectively.
            // No candidate/reference native hook or arithmetic substitution.
            require(!cpu.fpu_disabled() && !cpu.fpu_double_precision() &&
                (cpu.read_fpscr()&fpscr_rounding_mode_mask)<=1u,"unsupported reference FTRC mode");
            require((cpu.pc==0x8C10FB7Eu && opcode==0xF43Du) ||
                    (cpu.pc==0x8C10FBAAu && opcode==0xF33Du),"unexpected FTRC instruction");
            require(instruction.destination_register==0u &&
                instruction.source_register==(cpu.pc==0x8C10FB7Eu?4u:3u),"FTRC decode changed");
            GuestInstructionAttempt attempt(cpu,cpu.pc,2u);
            cpu.pc+=2u;fpu_truncate_to_fpul(cpu,instruction.source_register);
            ++corrected_reference_ftrc;
        } else {
            // Executes all original branches, delayed instructions and JSRs
            // transitively. In particular the original three callees run here.
            (void)execute_dynamic_sh4_block(cpu,services,1u);
        }
        require(cpu.exception_generation==0u,"reference FPU/memory exception");
    }
    require(cpu.pc==returned,"reference did not return");
}
void compare(Fixture& native, Fixture& reference) {
    native.observe(); reference.observe();
    const auto host_before=_mm_getcsr();
    require(sonic::atan_math::try_execute(native.cpu, &native.immutable), "eligible leaf declined");
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
    require(!sonic::atan_math::try_execute(fixture.cpu, guard), "unsafe admission accepted");
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

struct RestoreHost {
    std::fenv_t saved{};
    unsigned mxcsr=_mm_getcsr();
    RestoreHost() {require(std::fegetenv(&saved)==0,"cannot snapshot host FP");}
    ~RestoreHost() {std::fesetenv(&saved);_mm_setcsr(mxcsr);}
};
int main(int argc,char** argv) {
    try {
        RestoreHost restore;
        require(argc==2,"usage: test_atan_math <installed-content-root>");
        const auto boot=read(std::filesystem::path(argv[1])/"boot.bin");
        require(boot.size()==6735296u && digest(boot)==
            "b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af","retail boot SHA differs");
        for(const auto& span:source_spans)
            require(digest(std::span<const std::uint8_t>(boot).subspan(span.address-0x8C010000u,span.size))==
                span.sha256,"atan code/data SHA differs");
        unsigned cases=0u;
        const auto run=[&](std::uint32_t entry,std::uint32_t x,std::uint32_t y,unsigned mode,std::uint32_t exponent=0u) {
            _mm_setcsr(0x1F80u|((cases&3u)<<13u)|(cases&0x3Fu)|((cases&4u)?0x8040u:0u));
            Fixture native(boot,entry,x,y,mode,exponent),reference(boot,entry,x,y,mode,exponent);
            compare(native,reference);++cases;
        };
        // All quotient-selection bins and their immediate floating neighbours,
        // ±1 transforms, exponent cut-off and special-value classification.
        std::vector<std::uint32_t> atan_inputs{0u,0x80000000u,1u,0x80000001u,
            0x007FFFFFu,0x00800000u,0x7F800000u,0xFF800000u,0x7F800001u,0x7FC00001u,
            0xFF800001u,0x7F7FFFFFu,0x4EFFFFFFu,0x4F000000u,0x4F7FFFFFu,0x4F800000u,0x4F800001u};
        for(unsigned i=1;i<=8u;++i) {
            const auto center=std::bit_cast<std::uint32_t>(float(i)*0.125f);
            for(auto bits:{center-1u,center,center+1u}) for(auto sign:{0u,0x80000000u})
                atan_inputs.push_back(bits|sign);
        }
        for(unsigned mode:{0u,1u,fpscr_fr_mask|fpscr_cause_mask|fpscr_flag_mask,
                fpscr_fr_mask|fpscr_cause_mask|fpscr_flag_mask|1u}) {
            for(auto x:atan_inputs) run(atan_entry,x,0x3E000000u,mode);
            constexpr std::array<std::pair<std::uint32_t,std::uint32_t>,18> pairs{{
                {0u,0x3E000000u},{0x80000000u,0x3E000000u},{0x3F400000u,0x3E000000u},
                {0xBF400000u,0xBE000000u},{0x3F800000u,0x3E000000u},
                {0x4EFFFFFFu,0x3F800000u},{0x4F000000u,0x3F800000u},
                {0x4F7FFFFFu,0x3F800000u},{0x4F800000u,0x3F800000u},
                {0x7F7FFFFFu,0x00800000u},{1u,0x3F800000u},{0x00800000u,0x7F7FFFFFu},
                {0x3F800000u,0u},{0x3F800000u,0x7F800000u},{0x7F800000u,0x3F800000u},
                {0x7F800001u,0x3F800000u},{0x3F800000u,0x7FC00001u},{0x3F800000u,1u}}};
            for(auto [x,y]:pairs) run(quotient_entry,x,y,mode);
            for(auto x:{0u,0x80000000u,1u,0x00800000u,0x3DFFFFFFu,0x3E000000u,
                    0x3F800000u,0xBF800000u,0x7F7FFFFFu,0x7F800000u,0x7F800001u,0x7FC00001u})
                run(polynomial_entry,x,0x3E000000u,mode);
            constexpr std::array<std::pair<std::uint32_t,std::uint32_t>,17> scaling{{
                {0x3F800000u,0u},{0x3F800000u,1u},{0x3F800000u,0xFFFFFFFFu},
                {0xBF800000u,0xFFFFFFFEu},{0x00800000u,0xFFFFFFFFu},
                {0x7F7FFFFFu,1u},{1u,32u},{0x80000001u,32u},{0u,32u},{0x80000000u,32u},
                {0x7F800000u,0u},{0xFF800000u,0u},{0x7F800001u,0u},
                {0x3F800000u,0x7FFFFFFFu},{0x3F800000u,0x80000000u},
                {0x3F800000u,0xFFFFFF81u},{0x3F800000u,128u}}};
            for(auto [x,exponent]:scaling) run(scale_entry,x,0u,mode,exponent);
        }
        // A few deterministic raw bit patterns cross whole-function branches.
        auto random=0x10EEC4u;
        for(unsigned i=0;i<12u;++i) {
            random=random*1664525u+1013904223u;
            run(atan_entry,random,0x3E000000u,(i&1u)|fpscr_cause_mask|fpscr_flag_mask);
        }
        for(auto entry:{atan_entry,quotient_entry,polynomial_entry,scale_entry}) {
            // Nested opposite rounding epoch must survive native and original.
            CpuState outer{.memory=Memory{0u}};outer.fpscr=fpscr_dn_mask|1u;
            const auto ambient=_mm_getcsr();
            {HostFpuExecutionEpoch epoch(outer);
                _mm_setcsr(_mm_getcsr()|0x20u);
                Fixture native(boot,entry,0x3F200000u,0x3E000000u,0u,1u),
                    reference(boot,entry,0x3F200000u,0x3E000000u,0u,1u);
                compare(native,reference);++cases;
            }
            require(_mm_getcsr()==ambient,"enclosing epoch was not restored");
        }
        for(auto entry:{atan_entry,quotient_entry,scale_entry}) for(unsigned alias=0;alias<3u;++alias) {
            Fixture native(boot,entry,0x3F200000u,0x3E000000u,1u,1u),
                reference(boot,entry,0x3F200000u,0x3E000000u,1u,1u);
            for(auto* f:{&native,&reference}) {
                const auto translate=[&](auto a) {return alias==0u?a&0x1FFFFFFFu:a|0x20000000u;};
                f->cpu.r[15]=translate(f->cpu.r[15]);
                if(entry==quotient_entry) f->cpu.r[4]=translate(f->cpu.r[4]);
                if(alias==2u) {f->cpu.mmucr=1u;f->bind_address_space(true);}
                else f->bind_address_space(false);
            }
            compare(native,reference);++cases;
        }
        // Exact lower stack and upper output boundaries; polynomial needs no
        // stack range at all. Compare ordered store events and final RAM.
        for(auto entry:{atan_entry,quotient_entry,polynomial_entry,scale_entry}) {
            Fixture native(boot,entry,0x3F200000u,0x3E000000u,0u,1u),
                reference(boot,entry,0x3F200000u,0x3E000000u,0u,1u);
            for(auto* f:{&native,&reference}) {
                f->cpu.r[15]=entry==atan_entry?0x8C000048u:entry==quotient_entry?0x8C000020u:
                    entry==scale_entry?0x8C000008u:0u;
                if(entry==quotient_entry) f->cpu.r[4]=0x8CFFFFFCu;
            }
            compare(native,reference);++cases;
        }
        // Source/data authentication and every unsafe writable alias decline
        // BEFORE mutations, including observer events and memory metrics.
        for(unsigned scenario=0;scenario<34u;++scenario) {
            Fixture f(boot,quotient_entry,0x3F200000u,0x3E000000u,0u,0u,scenario==19u);
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
            case 10:case 11:case 12:case 13:case 14:case 15:
                f.ram->writable_bytes()[source_spans[scenario-10u].address&0xFFFFFFu]^=1u;break;
            case 16:f.cpu.r[4]+=2u;break;
            case 17:f.cpu.r[4]=0x8CFFFFFFu;break;
            case 18:f.observe(false);break;
            case 19:break;
            case 20:f.cpu.r[4]&=0x1FFFFFFFu;f.cpu.mmucr=1u;break;
            case 21:f.cpu.r[4]&=0x1FFFFFFFu;f.bind_address_space(true);break;
            case 22:f.cpu.pc=atan_entry+2u;break;
            case 23:f.cpu.r[15]=0x8C00001Cu;break;
            case 24:f.cpu.r[15]+=2u;break;
            case 25:f.cpu.r[4]=stack_top-4u;break;
            case 26:f.cpu.r[4]=atan_entry;break;
            case 27:f.cpu.r[4]=constants_entry;break;
            case 28:f.cpu.r[15]=coefficients_entry+32u;break;
            case 29:f.cpu.r[15]=scale_entry+32u;break;
            case 30:f.cpu.r[15]&=0x1FFFFFFFu;f.cpu.mmucr=1u;break;
            case 31:f.cpu.write_fpscr(fpscr_dn_mask|3u);break;
            case 32:f.cpu.pc=atan_entry;f.cpu.r[15]=0x8C000044u;break;
            case 33:f.cpu.pc=scale_entry;f.cpu.r[15]=0x8C000004u;break;
            }
            if(scenario!=18u) f.observe();
            decline(f,scenario==9u?nullptr:&f.immutable);++cases;
        }
        {
            Fixture f(boot,quotient_entry,0x3F200000u,0x3E000000u,0u);f.observe();
            const std::array protected_output{NativePortImmutableRange{output_address&0x1FFFFFFFu,4u,
                native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)}};
            NativePortImmutableWriteGuard guard{protected_output};decline(f,&guard);++cases;
        }
        std::cout<<"ATAN_MATH_TEST_OK cases="<<cases
            <<" original_family bytes_constants_sha registers_fpscr_fr ram_stack ordered_sources host_fp declines"
            <<" reference_ftrc_source_corrections="<<corrected_reference_ftrc<<'\n';
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"ATAN_MATH_TEST_FAIL "<<error.what()<<'\n';return 1;
    }
}
