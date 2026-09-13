// Standalone component comparison, NOT an inverse or gameplay benchmark.
// C++20; include generated header directory and retained SDK/include; link the
// REAL retained katana_runtime (and its normal platform dependencies). Do not
// compile a second fpu.cpp or stub its arithmetic/exception/epoch functions.
// Root owns sonic_inverse_arithmetic_tests / sonic_unit_arithmetic_tests
// (EXCLUDE_FROM_ALL), selecting the 221-site or 423-site source-bound sequence.
// Run with no arguments for correctness, --benchmark for correctness + kernel.
#include "sonic_inverse_arithmetic.hpp"
#include "katana/runtime/exception.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <xmmintrin.h>

using namespace katana::runtime;
using Op = FpuBinaryOperation;
namespace ia = sonic::inverse_arithmetic;
namespace {
#define COUNT_SITE(PC,O,S,D) + 1u
constexpr unsigned selected_operations = 0u SONIC_INVERSE_ARITHMETIC_SEQUENCE(COUNT_SITE);
#undef COUNT_SITE
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct HostState {
    unsigned saved = _mm_getcsr();
    ~HostState() { _mm_setcsr(saved); }
};
struct Fixture {
    CpuState cpu{.memory = Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram = std::make_shared<LinearMemoryDevice>(128u);
    std::vector<std::uint64_t> reset_events;
    Fixture() {
        cpu.memory.map_region("sentinel", 0x0C000000u, ram);
        cpu.manual_reset_sink = {this, [](void* p, CpuState&, const ManualResetRequest& r) noexcept {
            auto& log = static_cast<Fixture*>(p)->reset_events;
            log.push_back(static_cast<std::uint64_t>(r.reason));
            log.push_back(static_cast<std::uint64_t>(r.cause));
            log.push_back(r.event_code); log.push_back(r.fault_address.has_value());
            log.push_back(r.fault_address.value_or(0)); log.push_back(r.instruction_pc);
            log.push_back(r.owner_pc); log.push_back(r.in_delay_slot);
        }};
        reset_events.reserve(1024);
    }
    void seed(std::uint32_t fpscr, std::uint32_t salt) {
        reset_cpu(cpu, {0x8C63907Au, 0x8C001000u, 0x8C010000u, sr_md_mask, fpscr});
        reset_events.clear();
        for (unsigned i=0; i<16; ++i) {
            cpu.r[i] = 0x12340000u + salt + i;
            cpu.fr[i] = 0x3F000000u + ((salt + i*0x12345u) & 0x7FFFFFu);
            cpu.xf[i] = 0xA5A50000u + i;
        }
        for (unsigned i=0; i<cpu.r_bank.size(); ++i) cpu.r_bank[i]=0xBC000000u+i;
        cpu.pr=0x8C067C26u; cpu.gbr=0x8C001000u; cpu.fpul=0x13579BDFu;
        cpu.active_instruction_pc = (salt & 1u) ? 0x8C63907Au : 0u;
        cpu.active_instruction_physical_pc=0x0C63907Au;
        cpu.active_block_virtual_start=0x8C639066u;
        cpu.active_block_physical_start=0x0C639066u; cpu.active_block_size=0x374u;
        cpu.exception_generation=7; cpu.last_exception_generation=3;
        cpu.trap_pending=(salt & 2u)!=0; cpu.sleeping=true;
        cpu.attempted_guest_instructions=11; cpu.retired_guest_instructions=9;
        cpu.total_guest_cycles=123; cpu.pending_guest_cycles=19;
        cpu.last_memory_fault_provenance.valid=true;
        cpu.last_memory_fault_provenance.address=0xACEDu;
        // FD is checked by AOT, not fpu_binary: preserve that distinction.
        cpu.write_sr(sr_md_mask | ((salt & 4u) ? sr_fd_mask : 0u) |
                     ((salt & 8u) ? sr_rb_mask : 0u) |
                     ((salt & 16u) ? sr_bl_mask : 0u) | (salt & 3u));
        std::fill(ram->writable_bytes().begin(), ram->writable_bytes().end(), 0xA5);
    }
};

// Explicit fields, not memcmp(CpuState): avoids padding, shared_ptr and Memory
// object identity. Includes every CPU architectural, exception and accounting
// field; fixture-owned backend identities are checked separately.
std::vector<std::uint64_t> snapshot(const Fixture& f) {
    const auto& c=f.cpu;
    std::vector<std::uint64_t> v; v.reserve(512);
    for(auto x:c.r) v.push_back(x);
    for(auto x:c.r_bank) v.push_back(x);
    for(auto x:c.fr) v.push_back(x);
    for(auto x:c.xf) v.push_back(x);
    for(auto x:c.utlb) {v.push_back(x.pteh);v.push_back(x.ptel);v.push_back(x.ptea);}
#define FIELD(x) v.push_back(static_cast<std::uint64_t>(c.x))
    FIELD(pc);FIELD(pr);FIELD(gbr);FIELD(vbr);FIELD(ssr);FIELD(spc);FIELD(sgr);
    FIELD(dbr);FIELD(tra);FIELD(tea);FIELD(expevt);FIELD(intevt);FIELD(pteh);
    FIELD(ptel);FIELD(ptea);FIELD(ttb);FIELD(mmucr);FIELD(tlb_load_count);
    FIELD(mach);FIELD(macl);FIELD(fpul);FIELD(fpscr);FIELD(sr);
    FIELD(t);FIELD(s);FIELD(q);FIELD(m);FIELD(trap_pending);
    FIELD(exception_generation);FIELD(last_exception_cause);FIELD(exception_in_delay_slot);
    FIELD(last_exception_instruction_pc);FIELD(last_exception_instruction_physical_pc);
    FIELD(last_exception_owner_pc);FIELD(last_exception_generation);
    FIELD(last_memory_fault_provenance.valid);FIELD(last_memory_fault_provenance.instruction_valid);
    FIELD(last_memory_fault_provenance.opcode_valid);FIELD(last_memory_fault_provenance.access_valid);
    FIELD(last_memory_fault_provenance.source_pc);FIELD(last_memory_fault_provenance.runtime_pc);
    FIELD(last_memory_fault_provenance.address);FIELD(last_memory_fault_provenance.opcode);
    FIELD(last_memory_fault_provenance.operation);FIELD(last_memory_fault_provenance.width);
    FIELD(sleeping);FIELD(last_prefetch_address);FIELD(prefetch_count);
    FIELD(attempted_guest_instructions);FIELD(retired_guest_instructions);
    FIELD(total_guest_cycles);FIELD(pending_guest_cycles);FIELD(active_instruction_pc);
    FIELD(active_instruction_physical_pc);FIELD(active_block_virtual_start);
    FIELD(active_block_physical_start);FIELD(active_block_size);FIELD(last_prefetch_was_store_queue);
#undef FIELD
    require(!c.address_space && !c.gdrom_services && !c.g1_bus, "unexpected backend mutation");
    require(c.manual_reset_sink.context==&f && c.manual_reset_sink.callback,
            "reset sink identity changed");
    for(auto x:f.ram->bytes()) v.push_back(x);
    const auto& m=c.memory.performance_counters();
    v.push_back(m.indexed_region_hits);v.push_back(m.reference_region_probes);
    v.push_back(m.unobserved_accesses);v.push_back(m.observed_accesses);
    v.insert(v.end(),f.reset_events.begin(),f.reset_events.end());
    return v;
}

struct Counts {std::uint64_t fast=0, fallback=0, traps=0, cases=0;} counts;
template<Op O, unsigned S, unsigned D>
void compare(Fixture& original, Fixture& candidate) {
    const auto before=snapshot(candidate);
    const auto host_before=_mm_getcsr();
    const bool fast=ia::try_fast<O,S,D>(candidate.cpu);
    require(_mm_getcsr()==host_before,"integer fastpath touched MXCSR");
    if(!fast) require(snapshot(candidate)==before,"rejection mutated state");
    // Probe is not the tested operation: restore its only possible writes.
    candidate.cpu.fpscr=original.cpu.fpscr;
    candidate.cpu.fr[D]=original.cpu.fr[D];
    if(fast) ++counts.fast; else ++counts.fallback;
    const bool trapped=fpu_binary(original.cpu,O,S,D,std::nullopt);
    const auto original_host=_mm_getcsr();
    _mm_setcsr(host_before);
    ia::binary<O,S,D>(candidate.cpu);
    const auto candidate_host=_mm_getcsr();
    if(original_host!=candidate_host || snapshot(original)!=snapshot(candidate)) {
        std::cerr<<"Mismatch operation="<<unsigned(O)<<" src="<<S<<" dst="<<D
                 <<" case="<<counts.cases<<" fast="<<fast<<"\n";
        throw std::runtime_error("retained FPU differential mismatch");
    }
    counts.traps+=trapped; ++counts.cases;
}

template<unsigned S, unsigned D>
void bank_transition_cases(Fixture& a, Fixture& b) {
    // Added constant-register pairs must use the bank selected by an actual
    // FPSCR write, not merely a seeded FR bit. Both banks remain observable.
    const std::array<std::array<std::uint32_t,2>,6> operands{{
        {0x3FC00000u,0x40000000u}, {0xBF800001u,0x3F800003u},
        {0x7F7FFFFFu,0x7F7FFFFFu}, {0x7F800001u,0x3F800000u},
        {0x80000000u,0x40000000u}, {1u,0x00800000u}}};
    unsigned salt=0;
    for (const auto mode : {fpscr_dn_mask, fpscr_dn_mask|1u,
            fpscr_dn_mask|fpscr_enable_overflow_mask, fpscr_dn_mask|fpscr_pr_mask}) {
        for (const auto& pair : operands) {
            a.seed(mode,salt); b.seed(mode,salt++);
            a.cpu.xf[D]=b.cpu.xf[D]=pair[0];a.cpu.xf[S]=b.cpu.xf[S]=pair[1];
            const auto old_fr=a.cpu.fr, old_xf=a.cpu.xf;
            a.cpu.write_fpscr(a.cpu.fpscr^fpscr_fr_mask);
            b.cpu.write_fpscr(b.cpu.fpscr^fpscr_fr_mask);
            require(a.cpu.fr==old_xf && a.cpu.xf==old_fr,"FR write did not exchange banks");
            const auto host_before=_mm_getcsr();
            {
                HostFpuExecutionEpoch retained_epoch(a.cpu);
                compare<Op::Multiply,S,D>(a,b);
            }
            require(_mm_getcsr()==host_before,"bank-case epoch failed to restore host");
            a.cpu.write_fpscr(a.cpu.fpscr^fpscr_fr_mask);
            b.cpu.write_fpscr(b.cpu.fpscr^fpscr_fr_mask);
            require(a.cpu.fr==old_fr && snapshot(a)==snapshot(b),"wrong register bank was modified");
        }
    }
}

void basic_case(Fixture& a,Fixture& b,std::uint32_t n,std::uint32_t m,
                std::uint32_t fpscr,std::uint32_t salt) {
    // Separate seeds so one trapping case never contaminates another.
#define ONE(O,S,D) \
    a.seed(fpscr,salt); b.seed(fpscr,salt); \
    a.cpu.fr[D]=b.cpu.fr[D]=n; a.cpu.fr[S]=b.cpu.fr[S]=m; \
    compare<Op::O,S,D>(a,b)
    ONE(Multiply,5,2);
    ONE(Add,7,6);
    ONE(Subtract,10,6);
#undef ONE
}

void correctness() {
    Fixture a,b;
    HostState restore;
    const std::array words{0u,0x80000000u,1u,0x80000001u,0x007FFFFFu,0x807FFFFFu,
        0x00800000u,0x80800000u,0x00800001u,0x3F000000u,0xBF000000u,
        0x3F7FFFFFu,0x3F800000u,0xBF800000u,0x3F800001u,0x3F800003u,
        0x33800000u,0x33000000u,0x40000000u,0x7EFFFFFFu,0x7F000000u,
        0x7F7FFFFFu,0xFF7FFFFFu,0x7F800000u,0xFF800000u,0x7F800001u,
        0x7FBFFFFFu,0x7FC00000u,0xFFC00001u};
    const std::array enables{0u,fpscr_enable_inexact_mask,fpscr_enable_underflow_mask,
        fpscr_enable_overflow_mask,fpscr_enable_invalid_mask,fpscr_enable_divide_by_zero_mask,
        fpscr_enable_invalid_mask|fpscr_enable_divide_by_zero_mask,fpscr_exception_enable_mask};
    std::uint32_t salt=0;
    for(auto rm:{0u,1u,2u,3u}) for(auto dn:{0u,fpscr_dn_mask})
    for(auto pr:{0u,fpscr_pr_mask}) for(auto enable:enables)
    for(auto n:words) for(auto m:words) {
        // Nondefault host rounding, sticky host flags, DAZ/FTZ and guest stale
        // causes/flags are intentionally present. All host traps stay masked.
        _mm_setcsr(0x1F80u | ((salt&3u)<<13u) | (salt&0x3Fu) |
                   ((salt&4u)?0x8040u:0u));
        const auto ambient=_mm_getcsr();
        const auto fpscr=rm|dn|pr|enable|((salt&1u)?fpscr_flag_mask:0u)|
            ((salt&2u)?fpscr_cause_mask:0u)|((salt&4u)?fpscr_sz_mask:0u)|
            ((salt&8u)?fpscr_fr_mask:0u);
        basic_case(a,b,n,m,fpscr,salt++);
        require(_mm_getcsr()==ambient,"standalone helper did not restore MXCSR");
    }
    // Src==Dest, dependent calls, every enable combination, and matching /
    // nonmatching nested retained epochs. Host status must match both INSIDE
    // the existing epoch and after restoration; no duplicated epoch TLS.
    std::uint32_t rng=0x65432109u;
    for(unsigned i=0;i<2048;++i) {
        rng^=rng<<13; rng^=rng>>17; rng^=rng<<5;
        const auto fpscr=(i&3u)|((i&4u)?fpscr_dn_mask:0u)|
            ((i&8u)?fpscr_pr_mask:0u)|((i%32u)<<7u);
        a.seed(fpscr,i); b.seed(fpscr,i);
        a.cpu.fr[2]=b.cpu.fr[2]=rng;
        a.cpu.fr[4]=b.cpu.fr[4]=words[i%words.size()];
        const auto ambient=_mm_getcsr();
        {
            CpuState epoch_cpu{.memory=Memory{0u}};
            epoch_cpu.fpscr=fpscr^((i&16u)?1u:0u);
            HostFpuExecutionEpoch outer(epoch_cpu);
            const auto outer_host=_mm_getcsr();
            {
                HostFpuExecutionEpoch inner(epoch_cpu);
                compare<Op::Multiply,2,2>(a,b);
                compare<Op::Subtract,2,4>(a,b);
                compare<Op::Add,4,2>(a,b);
                compare<Op::Subtract,2,2>(a,b);
            }
            require(_mm_getcsr()==outer_host,"nested epoch did not restore MXCSR");
        }
        require(_mm_getcsr()==ambient,"outer epoch did not restore MXCSR");
    }
    // Every selected template operand pair also participates in a dependent
    // sequence comparison, with state checked after EACH arithmetic statement.
    // Still not an inverse: intervening original non-arithmetic work is absent.
    for(unsigned i=0;i<32;++i) {
        const auto fpscr=(i&3u)|((i&4u)?fpscr_dn_mask:0u);
        a.seed(fpscr,i); b.seed(fpscr,i);
#define CHECK_SITE(PC,O,S,D) \
        a.cpu.pc=b.cpu.pc=PC; \
        a.cpu.active_instruction_pc=b.cpu.active_instruction_pc=PC; \
        compare<Op::O,S,D>(a,b);
        SONIC_INVERSE_ARITHMETIC_SEQUENCE(CHECK_SITE)
#undef CHECK_SITE
    }
    bank_transition_cases<1,1>(a,b);
    bank_transition_cases<2,2>(a,b);
    bank_transition_cases<7,2>(a,b);
    bank_transition_cases<7,3>(a,b);
    require(counts.fast && counts.fallback && counts.traps,"missing branch coverage");
    std::cout<<"SONIC_INVERSE_ARITHMETIC_TEST_PASS cases="<<counts.cases
             <<" fast="<<counts.fast<<" fallback="<<counts.fallback
             <<" trapped="<<counts.traps<<" selected_operations="<<selected_operations
             <<" bank_transition_cases=96\n";
}

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif
// Intentionally arithmetic-only skeleton, not matrix inversion: the generated
// sequence omits FMOV/FSCHG/stack/control flow. Both variants see identical
// constants, dependencies and runtime data. This cannot establish game FPS.
template<bool Native> NOINLINE void kernel(CpuState& cpu) {
#define STEP(PC,O,S,D) \
    if constexpr(Native) ia::binary<Op::O,S,D>(cpu); \
    else static_cast<void>(fpu_binary(cpu,Op::O,S,D,std::nullopt));
    SONIC_INVERSE_ARITHMETIC_SEQUENCE(STEP)
#undef STEP
}
volatile std::uint64_t observable=0;
struct Measurement {double ms; std::uint64_t fingerprint;};
template<bool Native> Measurement measure(unsigned rounds,std::uint32_t seed) {
    // Duration conversion itself may set host Inexact after the retained epoch
    // ends. Keep benchmark bookkeeping out of the MXCSR restoration assertion.
    HostState ambient_restore;
    CpuState cpu{.memory=Memory{0u}};
    cpu.fpscr=fpscr_dn_mask;
    std::uint64_t hash=1469598103934665603ull;
    const auto begin=std::chrono::steady_clock::now();
    {
        HostFpuExecutionEpoch retained_epoch(cpu);
        for(unsigned i=0;i<rounds;++i) {
            for(auto& word:cpu.fr) {
                seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
                word=0x3F000000u|(seed&0x7FFFFFu);
            }
            cpu.fpscr=fpscr_dn_mask;
            kernel<Native>(cpu);
            for(auto word:cpu.fr) hash=(hash^word)*1099511628211ull;
            hash=(hash^cpu.fpscr)*1099511628211ull;
        }
    }
    const auto end=std::chrono::steady_clock::now();
    observable=hash;
    return {std::chrono::duration<double,std::milli>(end-begin).count(),hash};
}
void benchmark() {
    HostState restore;
    const auto warm_a=measure<false>(100,12345),warm_b=measure<true>(100,12345);
    require(warm_a.fingerprint==warm_b.fingerprint,"warmup fingerprint mismatch");
    for(unsigned pass=0;pass<4;++pass) {
        Measurement a{},b{};
        const auto ambient=_mm_getcsr();
        if(pass&1) {b=measure<true>(5000,0xABCDEFu);a=measure<false>(5000,0xABCDEFu);}
        else {a=measure<false>(5000,0xABCDEFu);b=measure<true>(5000,0xABCDEFu);}
        require(a.fingerprint==b.fingerprint,"kernel fingerprint mismatch");
        require(_mm_getcsr()==ambient,"kernel epoch changed ambient MXCSR");
        std::cout<<"KERNEL_ONLY pass="<<pass<<" operations_per_round="<<selected_operations<<" rounds=5000"
                 <<" retained_ms="<<a.ms<<" inline_ms="<<b.ms
                 <<" fingerprint="<<a.fingerprint<<" (not gameplay evidence)\n";
    }
}
} // namespace
int main(int argc,char** argv) {
    try {
        require(argc==1 || (argc==2 && std::string_view(argv[1])=="--benchmark"),
                "usage: sonic_inverse_arithmetic_tests [--benchmark]");
        correctness();
        if(argc==2) benchmark();
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"SONIC_INVERSE_ARITHMETIC_TEST_FAIL "<<e.what()<<"\n";
        return 1;
    }
}
