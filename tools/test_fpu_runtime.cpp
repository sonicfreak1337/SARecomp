// Link generated fpu.cpp + fpu-reference.cpp with the real retained SDK.
// Define SONIC_FPU_RUNTIME_TEST_PROBE for the candidate object. Reference
// public symbols and its retained epoch class are renamed; production has
// exactly one normal FPU member/TLS, with no reference/probe code enabled.
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/exception.hpp"
#include "sonic_fpu_reference.hpp"
#include "sonic_fpu_runtime_probe.hpp"
#include <algorithm>
#include <array>
#include <cfenv>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <xmmintrin.h>

using namespace katana::runtime;
using Op = FpuBinaryOperation;
using RefOp = SonicFpuReference_FpuBinaryOperation;
namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
using Host = std::array<unsigned, 3>;
Host host() {
    return {_mm_getcsr(), static_cast<unsigned>(std::fegetround()),
            static_cast<unsigned>(std::fetestexcept(FE_ALL_EXCEPT))};
}
struct RestoreHost {
    std::fenv_t saved{};
    RestoreHost() { require(std::fegetenv(&saved) == 0, "cannot save host FP environment"); }
    ~RestoreHost() { static_cast<void>(std::fesetenv(&saved)); }
};
struct Fixture {
    CpuState cpu{.memory = Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram = std::make_shared<LinearMemoryDevice>(128u);
    std::vector<std::uint64_t> reset_events;
    Fixture() {
        cpu.memory.map_region("sentinel", 0x0C000000u, ram);
        cpu.manual_reset_sink = {this, [](void* opaque, CpuState&, const ManualResetRequest& r) noexcept {
            auto& log = static_cast<Fixture*>(opaque)->reset_events;
            log.push_back(static_cast<std::uint64_t>(r.reason));
            log.push_back(static_cast<std::uint64_t>(r.cause));
            log.push_back(r.event_code); log.push_back(r.fault_address.has_value());
            log.push_back(r.fault_address.value_or(0)); log.push_back(r.instruction_pc);
            log.push_back(r.owner_pc); log.push_back(r.in_delay_slot);
            log.push_back(_mm_getcsr());
        }};
        reset_events.reserve(1024);
    }
    void seed(std::uint32_t fpscr, std::uint32_t salt) {
        reset_cpu(cpu, {0x8C63907Au, 0x8C001000u, 0x8C010000u, sr_md_mask, fpscr});
        reset_events.clear();
        for (unsigned i=0; i<16; ++i) {
            cpu.r[i] = 0x12340000u + salt + i;
            cpu.fr[i] = 0x3F000000u + ((salt+i*0x12345u)&0x7FFFFFu);
            cpu.xf[i] = 0xBF000000u + ((salt+i*0x23456u)&0x7FFFFFu);
        }
        for (unsigned i=0; i<cpu.r_bank.size(); ++i) cpu.r_bank[i]=0xBC000000u+i;
        cpu.pr=0x8C067C26u; cpu.gbr=0x8C001000u; cpu.fpul=0x13579BDFu;
        cpu.active_instruction_pc=(salt&1u)?0x8C63907Au:0u;
        cpu.active_instruction_physical_pc=0x0C63907Au;
        cpu.active_block_virtual_start=0x8C639066u;
        cpu.active_block_physical_start=0x0C639066u; cpu.active_block_size=0x374u;
        cpu.exception_generation=7; cpu.last_exception_generation=3;
        cpu.trap_pending=(salt&0x1000u)!=0; cpu.sleeping=true;
        cpu.attempted_guest_instructions=11; cpu.retired_guest_instructions=9;
        cpu.total_guest_cycles=123; cpu.pending_guest_cycles=19;
        cpu.last_memory_fault_provenance.valid=true;
        cpu.last_memory_fault_provenance.address=0xACEDu;
        // FD/instruction legality belong to the unchanged AOT envelope. The
        // helper must still match the original when called directly with FD.
        cpu.write_sr(sr_md_mask | ((salt&0x2000u)?sr_fd_mask:0u) |
                     ((salt&8u)?sr_rb_mask:0u) | ((salt&16u)?sr_bl_mask:0u) | (salt&3u));
        std::fill(ram->writable_bytes().begin(), ram->writable_bytes().end(), 0xA5);
    }
};

// Architectural state, exception provenance, accounting, sentinel RAM and
// callbacks. Avoid memcmp(CpuState): padding/backend identity is not state.
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
    require(!c.address_space && !c.gdrom_services && !c.g1_bus, "backend unexpectedly installed");
    require(c.manual_reset_sink.context==&f && c.manual_reset_sink.callback, "reset sink changed");
    for(auto x:f.ram->bytes()) v.push_back(x);
    const auto& m=c.memory.performance_counters();
    v.push_back(m.indexed_region_hits);v.push_back(m.reference_region_probes);
    v.push_back(m.unobserved_accesses);v.push_back(m.observed_accesses);
    v.insert(v.end(),f.reset_events.begin(),f.reset_events.end());
    return v;
}

struct Counts {
    std::array<std::uint64_t,4> fast{}, fallback{};
    std::uint64_t cases=0, traps=0;
} counts;
struct Outcome {
    bool trapped=false;
    std::array<Host,4> environments{};
    bool operator==(const Outcome&) const = default;
};

template<bool Reference>
Outcome execute(Fixture& f, Op operation, std::uint8_t source,
                std::uint8_t destination, unsigned form, unsigned epoch_mode) {
    using Epoch = std::conditional_t<Reference, SonicFpuReference_HostFpuExecutionEpoch,
                                   HostFpuExecutionEpoch>;
    Outcome out;
    const auto ambient=host();
    const auto call = [&] {
        if constexpr (!Reference) {
            const auto before=snapshot(f);
            const auto old_host=host();
            const auto old_fpscr=f.cpu.fpscr, old_dest=f.cpu.fr[destination&15u];
            const bool fast=sonic_fpu_runtime_try_fast_for_test(f.cpu,operation,source,destination);
            require(host()==old_host,"integer fast probe changed host FP environment");
            if ((old_fpscr&(fpscr_pr_mask|fpscr_exception_enable_mask)) ||
                    (f.cpu.sr&sr_fd_mask) || source>=16u || destination>=16u || f.cpu.trap_pending)
                require(!fast,"conservative fast-admission guard failed");
            if (!fast) require(snapshot(f)==before,"fast rejection mutated state");
            // Restore the probe's only permissible writes before testing the
            // actual ABI entry. All other CPU/callback/memory changes fail.
            f.cpu.fpscr=old_fpscr; f.cpu.fr[destination&15u]=old_dest;
            require(snapshot(f)==before,"fast probe changed more than FPSCR/destination");
            (fast?counts.fast:counts.fallback)[static_cast<unsigned>(operation)]++;
        }
        const std::optional<std::uint32_t> delay_owner =
            form==2u?std::optional<std::uint32_t>{0x8C639078u}:std::nullopt;
        if constexpr (Reference) {
            const auto op=static_cast<RefOp>(operation);
            if (form==0u) SonicFpuReference_fpu_binary(f.cpu,op,source,destination);
            else out.trapped=SonicFpuReference_fpu_binary(f.cpu,op,source,destination,delay_owner);
        } else {
            if (form==0u) fpu_binary(f.cpu,operation,source,destination);
            else out.trapped=fpu_binary(f.cpu,operation,source,destination,delay_owner);
        }
    };
    if (epoch_mode==0u) {
        out.environments[0]=host();call();out.environments[1]=host();
        out.environments[2]=host();
    } else {
        CpuState epoch_cpu{.memory=Memory{0u}};
        epoch_cpu.fpscr=f.cpu.fpscr^((epoch_mode==2u)?1u:0u);
        {
            Epoch outer(epoch_cpu);
            const auto outer_host=host();
            if (epoch_mode==3u) {
                // Changed host flags before a nested epoch must be restored
                // exactly on its exit; each implementation uses its own TLS.
                _mm_setcsr(_mm_getcsr()|0x20u);
                const auto nested_ambient=host();
                {
                    Epoch inner(epoch_cpu);
                    out.environments[0]=host();call();out.environments[1]=host();
                }
                require(host()==nested_ambient,"nested epoch did not restore host");
            } else {
                out.environments[0]=outer_host;call();out.environments[1]=host();
            }
            out.environments[2]=host();
        }
    }
    out.environments[3]=host();
    require(host()==ambient,"standalone helper/outer epoch did not restore ambient host");
    return out;
}

void compare(Fixture& a,Fixture& b,Op operation,unsigned source,unsigned destination,
             unsigned form,unsigned epoch_mode) {
    const auto ambient=host();
    const auto generation=a.cpu.exception_generation;
    const auto original=execute<true>(a,operation,static_cast<std::uint8_t>(source),
                                      static_cast<std::uint8_t>(destination),form,epoch_mode);
    require(host()==ambient,"reference changed ambient FP state");
    const auto candidate=execute<false>(b,operation,static_cast<std::uint8_t>(source),
                                        static_cast<std::uint8_t>(destination),form,epoch_mode);
    if (original!=candidate || snapshot(a)!=snapshot(b)) {
        std::cerr<<"Mismatch case="<<counts.cases<<" op="<<unsigned(operation)
                 <<" src="<<source<<" dst="<<destination<<" form="<<form
                 <<" epoch="<<epoch_mode<<'\n';
        throw std::runtime_error("runtime FPU differential mismatch");
    }
    counts.traps+=a.cpu.exception_generation!=generation;
    ++counts.cases;
}

void one_case(Fixture& a,Fixture& b,Op operation,std::uint32_t n,std::uint32_t m,
              std::uint32_t mode,unsigned salt,unsigned source=5,unsigned destination=2) {
    const std::array rounds{FE_TONEAREST,FE_TOWARDZERO,FE_DOWNWARD,FE_UPWARD};
    require(std::fesetround(rounds[salt&3u])==0,"cannot seed host rounding");
    _mm_setcsr(0x1F80u|((salt&3u)<<13u)|(salt&0x3Fu)|((salt&4u)?0x8040u:0u));
    const auto fpscr=mode|((salt&1u)?fpscr_flag_mask:0u)|
        ((salt&2u)?fpscr_cause_mask:0u)|((salt&4u)?fpscr_sz_mask:0u)|
        ((salt&0x80u)?fpscr_fr_mask:0u);
    for (unsigned form=0;form<3;++form) {
        a.seed(fpscr,salt);b.seed(fpscr,salt);
        for (auto* f:{&a,&b}) {
            auto& bank=(salt&0x40u)?f->cpu.xf:f->cpu.fr;
            bank[destination&15u]=n;bank[source&15u]=m;
            if (salt&0x40u) f->cpu.write_fpscr(f->cpu.fpscr^fpscr_fr_mask);
        }
        compare(a,b,operation,source,destination,form,(salt>>4u)&3u);
    }
}

void correctness() {
    Fixture a,b;RestoreHost restore;
    const std::array words{0u,0x80000000u,1u,0x80000001u,0x007FFFFFu,0x807FFFFFu,
        0x00800000u,0x80800000u,0x00800001u,0x3F000000u,0xBF000000u,
        0x3F7FFFFFu,0x3F800000u,0xBF800000u,0x3F800001u,0x3F800003u,
        0x33800000u,0x33000000u,0x40000000u,0x7EFFFFFFu,0x7F000000u,
        0x7F7FFFFFu,0xFF7FFFFFu,0x7F800000u,0xFF800000u,0x7F800001u,
        0x7FBFFFFFu,0x7FC00000u,0xFFC00001u};
    const std::array ops{Op::Add,Op::Subtract,Op::Multiply,Op::Divide};
    unsigned salt=0;
    // The same bit-pattern corpus now covers quotient rounding/boundaries as
    // well as existing product/sum cases, through both actual ABI overloads.
    for (auto op:ops) for(auto rm:{0u,1u,2u,3u}) for(auto dn:{0u,fpscr_dn_mask})
        for(auto n:words) for(auto m:words)
            one_case(a,b,op,n,m,rm|dn,salt++);
    // Every Enable combination plus PR: conservative O/U/I traps must remain
    // observable even on exact results, including a supplied delay owner.
    for(unsigned enable=0;enable<32;++enable) for(auto pr:{0u,fpscr_pr_mask})
        for(auto op:ops) for(unsigned i=0;i<words.size();++i)
            one_case(a,b,op,words[i],words[(i*7u+12u)%words.size()],
                     (enable<<7u)|pr|(i&3u)|((i&4u)?fpscr_dn_mask:0u),salt++);
    // All 16x16 register pairs, including source==destination. This checks
    // odd/even register identity and actual FR bank changes without templates.
    for(auto op:ops) for(unsigned s=0;s<16;++s) for(unsigned d=0;d<16;++d)
        one_case(a,b,op,0x3F800001u,0xBF000003u,fpscr_dn_mask,(salt++&0xFFFu),s,d);
    // Out-of-range encodings must reject the new fastpath and preserve the
    // retained helper's masking/PR behavior; they must not invent a new trap.
    for(auto op:ops) for(auto s:{16u,31u,255u}) for(auto d:{0u,17u,255u})
        for(auto pr:{0u,fpscr_pr_mask})
            one_case(a,b,op,0x3FC00000u,0x40000000u,pr|fpscr_dn_mask,salt++,s,d);
    std::uint32_t rng=0xA17C934Du;
    const auto random=[&] {rng^=rng<<13; rng^=rng>>17; rng^=rng<<5;return rng;};
    for(unsigned i=0;i<8192;++i) {
        const auto n=random(),m=random();
        const bool normal=(i&1u)!=0u;
        one_case(a,b,ops[i&3u],normal?((n&0x807FFFFFu)|0x3F000000u):n,
                 normal?((m&0x807FFFFFu)|0x40000000u):m,
                 (i&3u)|((i&4u)?fpscr_dn_mask:0u),salt++,i&15u,(i>>4u)&15u);
    }
    // Dependent arithmetic preserves status accumulation and exception state.
    for(unsigned i=0;i<64;++i) {
        a.seed(fpscr_dn_mask|(i&3u),i);b.seed(fpscr_dn_mask|(i&3u),i);
        for(unsigned step=0;step<16;++step)
            compare(a,b,ops[step&3u],step&15u,(step+1u)&15u,step%3u,i&3u);
    }
    for(unsigned op=0;op<4;++op)
        require(counts.fast[op] && counts.fallback[op],"missing per-operation fast/fallback coverage");
    require(counts.traps,"missing real exception coverage");
    std::cout<<"SONIC_FPU_RUNTIME_TEST_PASS cases="<<counts.cases<<" traps="<<counts.traps;
    for(unsigned op=0;op<4;++op)
        std::cout<<" op"<<op<<"_fast="<<counts.fast[op]<<" op"<<op<<"_fallback="<<counts.fallback[op];
    std::cout<<" reference=retained-renamed both_overloads=1 host_state=exact\n";
}
} // namespace

int main() {
    try {correctness();return 0;}
    catch(const std::exception& error) {
        std::cerr<<"SONIC_FPU_RUNTIME_TEST_FAIL "<<error.what()<<'\n';return 1;
    }
}
