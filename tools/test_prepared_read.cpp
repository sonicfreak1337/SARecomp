#include "sonic_prepared_read.hpp"
#include "katana/runtime/aot_runtime_abi.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/exception.hpp"
#include "katana/runtime/fpu.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>

using namespace katana::runtime;
namespace {
constexpr std::uint32_t source_pc=0x8C02947Cu, sentinel=0xA5A5A5A5u;
using Counts=std::array<std::uint64_t,4>;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
Counts counts(const Memory& m) {
    const auto& c=m.performance_counters();
    return {c.indexed_region_hits,c.reference_region_probes,c.unobserved_accesses,c.observed_accesses};
}
struct Event {
    unsigned kind=0;
    std::array<std::uint64_t,24> data{};
    std::array<char,32> region{};
    bool operator==(const Event&) const=default;
};
struct Log {
    std::array<Event,64> entries{};
    std::size_t size=0;
    bool overflow=false;
    void add(Event e) noexcept { if(size<entries.size()) entries[size++]=e; else overflow=true; }
    bool operator==(const Log&) const=default;
};
struct Services final:PlatformServices {
    Log* log=nullptr;
    std::uint64_t clock=11;
    unsigned polls=0;
    std::function<void()> on_flush;
    std::string_view name() const noexcept override { return "prepared-read-component"; }
    std::uint32_t abi_version() const noexcept override { return platform_services_abi_version; }
    std::uint32_t guest_cycle_contract() const noexcept override { return 0; }
    PlatformCapabilities capabilities() const noexcept override { return {}; }
    void read_memory(std::uint32_t,std::span<std::uint8_t>) override { throw std::runtime_error("unexpected service read"); }
    void write_memory(std::uint32_t,std::span<const std::uint8_t>) override { throw std::runtime_error("unexpected service write"); }
    std::uint64_t scheduler_cycle() const noexcept override { return clock; }
    std::optional<std::uint64_t> next_scheduler_event_cycle() const noexcept override { return {}; }
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t cycles,std::size_t) override {
        log->add({1,{cycles}}); clock+=cycles;
        if(on_flush) { auto action=std::move(on_flush); on_flush={}; action(); }
        PlatformSchedulerResult result{}; result.guest_cycle=clock; return result;
    }
    std::optional<PlatformInterruptRequest> poll_interrupt() override { ++polls; log->add({2,{}}); return {}; }
    PlatformDmaResult start_dma(const PlatformDmaRequest&) override { throw std::runtime_error("unexpected DMA"); }
    PlatformFallbackResult controlled_fallback(CpuState&,const PlatformFallbackRequest&) override { throw std::runtime_error("unexpected platform fallback"); }
    bool prefetch(CpuState&,GuestInstructionOrigin,std::uint32_t) override { throw std::runtime_error("unexpected PREF"); }
};
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x10000u);
    Log log;
    Services services;
    std::uint32_t runtime_pc=source_pc;
    std::optional<MemoryAccessErrorReason> fault;
    std::array<std::uint32_t,7> error{};
    bool preflight=false;
    unsigned fallbacks=0;
    Fixture() {
        services.log=&log;
        cpu.address_space=std::make_shared<RuntimeAddressSpace>();
        cpu.memory.map_region("ram0",0x0C000000u,ram);
        cpu.memory.map_region("ram1",0x0C010000u,ram);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x20000u,*ram);
        cpu.memory.set_lookup_mode(MemoryLookupMode::Indexed);
        cpu.write_sr(sr_md_mask);
        cpu.r.fill(0x13572468u); cpu.r_bank.fill(0x24681357u);
        cpu.r[3]=0x8C000100u; cpu.r[4]=cpu.r_bank[4]=sentinel;
        cpu.fr.fill(0x3F800000u); cpu.xf.fill(0x40000000u);
        cpu.pc=source_pc; cpu.pr=0x8C020000u; cpu.vbr=0x8C040000u;
        cpu.gbr=0x12345678u; cpu.mach=13; cpu.macl=17; cpu.fpul=19;
        cpu.attempted_guest_instructions=5; cpu.retired_guest_instructions=3;
        cpu.pending_guest_cycles=7; cpu.total_guest_cycles=11;
        cpu.exception_generation=2;
        cpu.active_block_virtual_start=source_pc-0x7Cu;
        cpu.active_block_physical_start=0x0C029400u; cpu.active_block_size=0x100u;
        ram->write_u32(0x100u,0x13579BDFu);
        ram->write_u32(0x104u,0x2468ACE0u);
        ram->write_u32(0xFFFCu,0x11223344u);
    }
    void access(unsigned kind,const MemoryAccessEvent& e) noexcept {
        Event v{kind,{static_cast<unsigned>(e.operation),e.address,static_cast<unsigned>(e.width),e.value,
            cpu.attempted_guest_instructions,cpu.retired_guest_instructions}};
        std::copy_n(e.region_name.data(),std::min(e.region_name.size(),v.region.size()),v.region.data());
        log.add(v);
    }
    void watch(MemoryWatchpointAccess kind,std::uint32_t address=0x0C000100u) {
        static_cast<void>(cpu.memory.add_watchpoint(address,4,kind,[this](const auto& e){access(4,e);}));
    }
    void trace() { cpu.memory.set_trace_handler([this](const auto& e){access(3,e);}); }
    void sink() {
        cpu.memory.set_guest_memory_access_sink({this,[](void* p,const GuestMemoryAccessEvent& e) noexcept {
            auto& f=*static_cast<Fixture*>(p);
            f.log.add({5,{static_cast<unsigned>(e.operation),e.instruction.source_pc,e.instruction.runtime_pc,
                e.instruction.valid,e.virtual_address,e.physical_address,static_cast<unsigned>(e.width),e.value,e.size,
                e.attempted_guest_instructions,e.retired_guest_instructions,e.scalar_value_valid,e.linear_backing==f.ram.get(),
                e.linear_offset,e.linear_size,e.linear_contiguous,e.linear_byte_count,
                e.linear_byte_offsets[0],e.linear_byte_offsets[1],e.linear_byte_offsets[3],e.linear_byte_offsets[2],
                static_cast<unsigned>(e.access_origin),static_cast<unsigned>(e.write_source),e.bytes_changed}});
        }});
    }
    void mmio(bool reject=false) {
        auto device=std::make_shared<MmioMemoryDevice>(0x10000u,
            [this,reject](std::uint32_t offset,MemoryAccessWidth width) {
                log.add({6,{offset,static_cast<unsigned>(width),cpu.attempted_guest_instructions,cpu.retired_guest_instructions}});
                if(reject) throw MmioDeviceError("test rejection");
                return 0xDECAFBADu;
            },[](std::uint32_t,std::uint32_t,MemoryAccessWidth){throw std::runtime_error("unexpected MMIO write");});
        cpu.memory.map_region("io",0x10000000u,device); cpu.r[3]=0xB0000000u;
        cpu.memory.set_mmio_access_tracking(true);
        cpu.memory.set_mmio_trace_handler([this](const auto& e){access(7,e);});
        cpu.memory.set_mmio_interrupt_state_sink({this,[](void* p) noexcept {static_cast<Fixture*>(p)->log.add({8,{}});}});
    }
};
auto state(const CpuState& c) {
    return std::tuple(c.r,c.r_bank,c.fr,c.xf,c.pc,c.pr,c.gbr,c.vbr,c.ssr,c.spc,c.sgr,c.dbr,c.tra,c.tea,
        c.expevt,c.intevt,c.pteh,c.ptel,c.ptea,c.ttb,c.mmucr,c.tlb_load_count,c.mach,c.macl,c.fpul,c.read_fpscr(),c.read_sr(),
        c.t,c.s,c.q,c.m,c.trap_pending,c.exception_generation,c.last_exception_cause,c.exception_in_delay_slot,
        c.last_exception_instruction_pc,c.last_exception_instruction_physical_pc,c.last_exception_owner_pc,c.last_exception_generation,
        c.attempted_guest_instructions,c.retired_guest_instructions,c.total_guest_cycles,c.pending_guest_cycles,
        c.active_instruction_pc,c.active_instruction_physical_pc,c.active_block_virtual_start,c.active_block_physical_start,c.active_block_size);
}
auto provenance(const CpuState& c) {
    const auto& p=c.last_memory_fault_provenance;
    return std::tuple(p.valid,p.instruction_valid,p.opcode_valid,p.access_valid,p.source_pc,p.runtime_pc,p.address,p.opcode,p.operation,p.width);
}
using Action=std::function<void(Fixture&)>;

// Oracle copied from the pinned generated unit's translator, guarded reader
// and ordinary 8C02947C MOV.L envelope. Only the candidate branch uses the new
// helper. No guest image, AOT regeneration or interpreter fetches are involved.
void execute(Fixture& f,bool candidate,const Action& after_prepare={}) {
    auto& cpu=f.cpu;
    NativeAotRegisterFile<0x0000FFFDu,0x00000023u> registers(cpu);
    auto guard=cpu.memory.direct_linear_memory_guard(false);
    const auto translate=[&](std::uint32_t address,std::uint32_t& direct) noexcept {
        const auto segment=address>>29u;
        if(segment==4u || segment==5u) {
            if(!cpu.privileged_mode_inline()) return false;
            direct=address;
        } else {
            if((cpu.mmucr&1u)!=0u || segment>=7u || (!cpu.privileged_mode_inline() && address>=0x80000000u)) return false;
            direct=canonical_physical_address_inline(address)|0x80000000u;
        }
        return true;
    };
    const auto original_read=[&](const GuestInstructionOrigin& origin,std::uint32_t address) {
        std::uint32_t value=0,direct=0;
        if(translate(address,direct) && direct_linear_guard_read_u32(guard,direct,value)) return value;
        ++f.fallbacks;
        const bool reacquire=registers.owns_registers();
        registers.flush_release();
        const auto result=guest_read_u32_at(cpu,origin,address);
        if(reacquire) registers.reload_acquire();
        return result;
    };
    const auto before=state(cpu); const auto counters_before=counts(cpu.memory); const auto log_before=f.log;
#if defined(SONIC_TEST_PRELOADED_READ)
    sonic::memory::PreloadedRead32 prepared;
    if(candidate) { prepared=sonic::memory::preload_read32(guard,registers[3],translate); f.preflight=prepared.valid; }
#else
    sonic::memory::PreparedRead32 prepared;
    if(candidate) { prepared=sonic::memory::prepare_read32(guard,registers[3],translate); f.preflight=prepared.valid; }
#endif
    else {
        std::uint32_t direct=0,offset=0;
        f.preflight=translate(registers[3],direct) && direct_linear_guard_offset(guard,direct,4,offset);
    }
    require(state(cpu)==before && f.log==log_before,"preflight changed CPU or emitted an observer event");
#if defined(SONIC_TEST_PRELOADED_READ)
    auto expected_counts=counters_before;
    if(candidate && prepared.valid) {++expected_counts[0];++expected_counts[2];}
    require(counts(cpu.memory)==expected_counts,"preload accounting mismatch");
    require(!after_prepare,"preloaded envelope forbids intervening actions");
#else
    require(counts(cpu.memory)==counters_before,"preflight changed memory counters");
#endif
    if(after_prepare) after_prepare(f); // Synthetic helper boundary stress, not an admitted AOT callback.
    if(!f.preflight) { registers.flush_release(); flush_pending_guest_cycles(cpu,f.services); registers.reload_acquire(); }
    const auto epoch_before=f.preflight?0:cpu.memory.mmio_boundary_epoch();
    ExplicitGuestInstructionAttempt attempt(cpu,f.runtime_pc,2);
    try {
        const GuestInstructionOrigin origin{source_pc,f.runtime_pc,true};
#if defined(SONIC_TEST_PRELOADED_READ)
        if(candidate) registers[4]=sonic::memory::consume_preloaded32(prepared,[&]{return original_read(origin,registers[3]);});
#else
        if(candidate) registers[4]=sonic::memory::read_prepared32(guard,prepared,registers[3],[&]{return original_read(origin,registers[3]);});
#endif
        else registers[4]=original_read(origin,registers[3]);
        attempt.complete();
    } catch(const MemoryAccessError& e) {
        f.fault=e.reason(); const auto o=e.instruction();
        f.error={static_cast<unsigned>(e.reason()),static_cast<unsigned>(e.operation()),e.address(),static_cast<unsigned>(e.width()),o.source_pc,o.runtime_pc,o.valid};
        enter_memory_exception_with_provenance(cpu,e,f.runtime_pc,0x6432u); return;
    }
    if(!f.preflight && cpu.memory.mmio_boundary_epoch()!=epoch_before) {
        registers.flush_release(); cpu.pc=f.runtime_pc+2;
        if(finalize_guest_block(cpu,f.services,1024,f.runtime_pc,0,false,false,false).interrupt.has_value()) return;
        registers.reload_acquire();
    }
}
unsigned cases=0;
bool fpu_family=false;
#if defined(SONIC_TEST_PRELOADED_READ)
// Independent copy of the pinned nonincrementing FMOV instruction envelope.
// The candidate substitutes only admission and the scalar load, like the
// source preparer; pair reads and fault precedence stay in the oracle path.
void execute_fpu(Fixture& f,bool candidate,const Action& after_prepare={}) {
    require(!after_prepare,"FMOV preload forbids intervening synthetic actions");
    auto& cpu=f.cpu;
    NativeAotRegisterFile<0x0000FFFDu,0x00000023u> registers(cpu);
    auto guard=cpu.memory.direct_linear_memory_guard(false);
    const auto translate=[&](std::uint32_t address,std::uint32_t& direct) noexcept {
        const auto segment=address>>29u;
        if(segment==4u || segment==5u) {
            if(!cpu.privileged_mode_inline()) return false;
            direct=address;
        } else {
            if((cpu.mmucr&1u)!=0u || segment>=7u || (!cpu.privileged_mode_inline() && address>=0x80000000u)) return false;
            direct=canonical_physical_address_inline(address)|0x80000000u;
        }
        return true;
    };
    const auto can_read=[&](std::uint32_t address) {
        std::uint32_t direct=0,offset=0;
        return translate(address,direct) && direct_linear_guard_offset(guard,direct,4,offset);
    };
    const auto original_read=[&](const GuestInstructionOrigin& origin,std::uint32_t address) {
        std::uint32_t value=0,direct=0;
        if(translate(address,direct) && direct_linear_guard_read_u32(guard,direct,value)) return value;
        ++f.fallbacks;
        const bool reacquire=registers.owns_registers();
        registers.flush_release();
        const auto result=guest_read_u32_at(cpu,origin,address);
        if(reacquire) registers.reload_acquire();
        return result;
    };
    const auto before=state(cpu);
    const auto counters_before=counts(cpu.memory); const auto log_before=f.log;
    sonic::memory::PreloadedRead32 prepared;
    const bool eligible=(cpu.sr&sr_fd_mask)==0 && (cpu.fpscr&fpscr_sz_mask)==0;
    if(candidate && eligible) {
        prepared=sonic::memory::preload_read32(guard,registers[3],translate);
        f.preflight=prepared.valid;
    } else {
        f.preflight=can_read(registers[3]) && (!(cpu.fpscr&fpscr_sz_mask) || can_read(registers[3]+4u));
    }
    auto expected_counts=counters_before;
    if(prepared.valid) {++expected_counts[0];++expected_counts[2];}
    require(state(cpu)==before && f.log==log_before && counts(cpu.memory)==expected_counts,"FMOV preflight effects differ");
    if(!f.preflight) {registers.flush_release();flush_pending_guest_cycles(cpu,f.services);registers.reload_acquire();}
    const auto epoch_before=f.preflight?0:cpu.memory.mmio_boundary_epoch();
    ExplicitGuestInstructionAttempt attempt(cpu,f.runtime_pc,2);
    if(cpu.sr&sr_fd_mask) {registers.flush_release();raise_fpu_disabled(cpu,f.runtime_pc);return;}
    if((cpu.fpscr&fpscr_pr_mask) && (cpu.fpscr&fpscr_sz_mask)) {registers.flush_release();raise_illegal_instruction(cpu,f.runtime_pc);return;}
    try {
        const GuestInstructionOrigin origin{source_pc,f.runtime_pc,true};
        const std::uint32_t address=registers[3];
        if(cpu.fpscr&fpscr_sz_mask) {
            const auto low=original_read(origin,address);
            const auto high=original_read(origin,address+4u);
            write_fpu_pair_bits(cpu,4u,(std::uint64_t(high)<<32u)|low);
        } else if(candidate) {
            cpu.fr[4]=sonic::memory::consume_preloaded32(prepared,[&]{return original_read(origin,address);});
        } else cpu.fr[4]=original_read(origin,address);
        attempt.complete();
    } catch(const MemoryAccessError& e) {
        registers.flush_release();f.fault=e.reason();const auto o=e.instruction();
        f.error={static_cast<unsigned>(e.reason()),static_cast<unsigned>(e.operation()),e.address(),static_cast<unsigned>(e.width()),o.source_pc,o.runtime_pc,o.valid};
        enter_memory_exception_with_provenance(cpu,e,f.runtime_pc,0xF438u);return;
    }
    if(!f.preflight && cpu.memory.mmio_boundary_epoch()!=epoch_before) {
        registers.flush_release();cpu.pc=f.runtime_pc+2;
        if(finalize_guest_block(cpu,f.services,1024,f.runtime_pc,0,false,false,false).interrupt.has_value())return;
        registers.reload_acquire();
    }
}
#endif
void pair(const char* name,const Action& arrange={},const Action& check={},const Action& after_prepare={}) {
    try {
        Fixture original,prepared;
        if(arrange) {arrange(original);arrange(prepared);}
        original.cpu.memory.reset_performance_counters(); prepared.cpu.memory.reset_performance_counters();
#if defined(SONIC_TEST_PRELOADED_READ)
        const auto runner=fpu_family?execute_fpu:execute;
#else
        const auto runner=execute;
#endif
        runner(original,false,after_prepare); runner(prepared,true,after_prepare);
        require(state(original.cpu)==state(prepared.cpu),"CPU state mismatch");
        require(provenance(original.cpu)==provenance(prepared.cpu),"fault provenance mismatch");
        require(bool(original.cpu.address_space)==bool(prepared.cpu.address_space),"address-space presence mismatch");
        if(original.cpu.address_space)
            require(original.cpu.address_space->snapshot()==prepared.cpu.address_space->snapshot(),"address-space state mismatch");
        for(std::size_t i=0;i<original.cpu.utlb.size();++i) {
            const auto& a=original.cpu.utlb[i]; const auto& b=prepared.cpu.utlb[i];
            require(std::tie(a.pteh,a.ptel,a.ptea)==std::tie(b.pteh,b.ptel,b.ptea),"UTLB mismatch");
        }
        require(counts(original.cpu.memory)==counts(prepared.cpu.memory),"memory accounting mismatch");
        require(original.log==prepared.log && !prepared.log.overflow,"observer order/value mismatch");
        require(original.fault==prepared.fault && original.error==prepared.error,"memory error mismatch");
        require(original.preflight==prepared.preflight && original.fallbacks==prepared.fallbacks,"admission/fallback mismatch");
        require(original.services.clock==prepared.services.clock && original.services.polls==prepared.services.polls,"scheduler mismatch");
        require(original.cpu.memory.mmio_boundary_epoch()==prepared.cpu.memory.mmio_boundary_epoch(),"MMIO boundary mismatch");
        require(original.cpu.memory.mmio_access_epoch()==prepared.cpu.memory.mmio_access_epoch(),"MMIO access epoch mismatch");
        require(std::equal(original.ram->bytes().begin(),original.ram->bytes().end(),prepared.ram->bytes().begin()),"RAM bytes changed differently");
        if(check) {check(original);check(prepared);} ++cases;
    } catch(const std::exception& e) {throw std::runtime_error(std::string(name)+": "+e.what());}
}
void successful(Fixture& f,std::uint32_t value=0x13579BDFu,Counts expected={1,0,1,0}) {
    require(!f.fault && (fpu_family?f.cpu.fr[4]:f.cpu.r[4])==value && f.cpu.r_bank[4]==sentinel,"wrong load/destination");
    require(f.cpu.attempted_guest_instructions==6 && f.cpu.retired_guest_instructions==4,"wrong successful retirement");
    require(f.cpu.exception_generation==2 && counts(f.cpu.memory)==expected,"wrong success exception/counters");
}
void faulted(Fixture& f) {
    require(f.fault.has_value(),"expected a memory fault");
    require(f.cpu.r[4]==sentinel && f.cpu.r_bank[4]==sentinel,"fault changed destination");
    require(!fpu_family || f.cpu.fr[4]==0x3F800000u,"fault changed FP destination");
    require(f.cpu.attempted_guest_instructions==6 && f.cpu.retired_guest_instructions==3,"fault incorrectly retired");
    const auto& p=f.cpu.last_memory_fault_provenance;
    require(f.cpu.exception_generation==3 && p.valid && p.instruction_valid && p.opcode_valid && p.access_valid,"missing fault provenance");
    require(p.source_pc==source_pc && p.runtime_pc==f.runtime_pc && p.opcode==(fpu_family?0xF438u:0x6432u) && p.width==MemoryAccessWidth::Word && p.operation==MemoryAccessOperation::Read,"wrong fault origin/shape");
}
void guard_lifetime() {
    Fixture f; auto& m=f.cpu.memory; auto g=m.direct_linear_memory_guard(false);
    m.reset_performance_counters(); std::uint32_t output=sentinel;
    require(!direct_linear_guard_read_u32(g,0x0C000100u,output) && output==sentinel && counts(m)==Counts{},"raw physical guard must miss cleanly");
    // A changed consume address cannot use a valid token for another word.
    const auto token=sonic::memory::prepare_read32(g,0x8C000100u,
        [](std::uint32_t a,std::uint32_t& d) noexcept {d=a;return true;});
    unsigned fallback_calls=0;
    output=sonic::memory::read_prepared32(g,token,0x8C000104u,[&]{++fallback_calls;return guest_read_u32_at(f.cpu,{source_pc,source_pc,true},0x8C000104u);});
    require(output==0x2468ACE0 && fallback_calls==1 && counts(m)==Counts{1,0,1,0},"changed consume address reused prepared word");
    m.reset_performance_counters(); output=sentinel;
    const auto watcher=m.add_watchpoint(0x0C000100,4,MemoryWatchpointAccess::Write,[](const auto&){});
    require(!m.direct_linear_memory_guard_current(g,false) && !direct_linear_guard_read_u32(g,0x8C000100,output) && counts(m)==Counts{},"observer mutation retained stale guard");
    auto fresh=m.direct_linear_memory_guard(false);
    require(direct_linear_guard_read_u32(fresh,0x8C000100,output) && output==0x13579BDF && counts(m)==Counts{1,0,1,0},"write-only observer blocked fresh read");
    require(m.remove_watchpoint(watcher) && !m.direct_linear_memory_guard_current(fresh,false),"watchpoint removal did not invalidate");
    g=m.direct_linear_memory_guard(false); Memory moved(std::move(m));
    output=sentinel; require(!direct_linear_guard_read_u32(g,0x8C000100,output) && output==sentinel,"moved source guard stayed valid");
    auto moved_guard=moved.direct_linear_memory_guard(false); m=std::move(moved);
    require(!direct_linear_guard_read_u32(moved_guard,0x8C000100,output),"move assignment retained source guard");
    auto destination_guard=m.direct_linear_memory_guard(false); Fixture replacement;
    replacement.ram->write_u32(0x100,0x89ABCDEF); m=std::move(replacement.cpu.memory);
    require(!direct_linear_guard_read_u32(destination_guard,0x8C000100,output),"replacement retained destination guard");
    require(direct_linear_guard_read_u32(m.direct_linear_memory_guard(false),0x8C000100,output) && output==0x89ABCDEF,"replacement fresh read failed");
    ++cases;
}
} // namespace

int main() {
    try {
#if defined(SONIC_TEST_PRELOADED_READ)
      for(bool fpu:{false,true}) {
        fpu_family=fpu;
#endif
        for(auto address:{0x0C000100u,0x8C000100u,0xAC000100u,0x0C010100u})
            pair("aliases",[=](auto& f){f.cpu.r[3]=address;},[](auto& f){successful(f);require(f.preflight && f.fallbacks==0 && f.cpu.pending_guest_cycles==9,"direct envelope ordering");});
        for(auto address:{0x0C000100u,0x8C000100u,0xAC000100u}) for(bool privileged:{false,true}) for(bool mmu:{false,true})
            pair((std::string("mode/MMU address=")+std::to_string(address)+" privileged="+std::to_string(privileged)+" mmu="+std::to_string(mmu)).c_str(),[=](auto& f){f.cpu.write_sr(privileged?sr_md_mask:0);f.cpu.r[3]=address;f.cpu.mmucr=mmu?1:0;f.cpu.address_space->set_mode(mmu?AddressTranslationMode::Mmu:AddressTranslationMode::NoMmu);f.cpu.address_space->write_mmucr(f.cpu.mmucr);},
                [=](auto& f){if(address==0x0C000100u ? !mmu : privileged) successful(f);else faulted(f);});
        // MMUCR alone does not install a RuntimeAddressSpace. The pinned SDK's
        // bare-CPU fallback still reads physical RAM in that configuration.
        pair("MMU without address space",[](auto& f){f.cpu.address_space.reset();f.cpu.write_sr(0);f.cpu.r[3]=0x0C000100;f.cpu.mmucr=1;},[](auto& f){successful(f);require(!f.preflight && f.fallbacks==1,"bare MMU fixture did not fall back");});
        pair("last word",[](auto& f){f.cpu.r[3]=0x8C01FFFC;},[](auto& f){successful(f,0x11223344);});
        for(auto address:{0x8C020000u,0x8BFFFFFCu,0x8C000101u,0x8C000102u})
            pair("bounds/alignment",[=](auto& f){f.cpu.r[3]=address;},faulted);
        pair("permissive unaligned",[](auto& f){f.cpu.r[3]=0x8C000101;f.cpu.memory.set_alignment_policy(MemoryAlignmentPolicy::Permissive);},[](auto& f){require(!f.preflight && f.fallbacks==1,"unaligned became direct");if(f.fault)faulted(f);else successful(f,0xE013579B);});
        pair("backing wrap",[](auto& f){f.cpu.memory=Memory{0u};f.ram=std::make_shared<LinearMemoryDevice>(2);f.cpu.memory.map_region("tiny0",0x0C000000,f.ram);f.cpu.memory.map_region("tiny1",0x0C000002,f.ram);f.cpu.memory.bind_direct_linear_alias_window(0x0C000000,4,*f.ram);f.cpu.r[3]=0x8C000000;},[](auto& f){faulted(f);require(!f.preflight && f.fault==MemoryAccessErrorReason::CrossRegion,"backing wrap did not retain scalar refusal");});
        pair("read observer",[](auto& f){f.watch(MemoryWatchpointAccess::Read);},[](auto& f){successful(f,0x13579BDF,{1,0,0,1});require(f.log.size==2 && f.log.entries[1].kind==4,"read observer count");});
        pair("nonoverlapping read observer",[](auto& f){f.watch(MemoryWatchpointAccess::Read,0x0C000200);},[](auto& f){successful(f,0x13579BDF,{1,0,0,1});require(f.log.size==1,"unmatched observer called");});
        pair("write-only observer",[](auto& f){f.watch(MemoryWatchpointAccess::Write);},[](auto& f){successful(f);require(f.preflight && f.log.size==0,"write observer affected read");});
        pair("trace plus watcher order",[](auto& f){f.trace();f.watch(MemoryWatchpointAccess::Read);},[](auto& f){successful(f,0x13579BDF,{1,0,0,1});require(f.log.size==3 && f.log.entries[1].kind==3 && f.log.entries[2].kind==4,"trace/watch order");});
        pair("guest provenance sink",[](auto& f){f.runtime_pc=0x8C12947C;f.cpu.pc=f.runtime_pc;f.cpu.active_block_virtual_start=f.runtime_pc-0x7C;f.sink();},[](auto& f){successful(f);require(f.log.size==2 && f.log.entries[1].kind==5,"guest sink count");const auto& d=f.log.entries[1].data;require(d[1]==source_pc && d[2]==f.runtime_pc && d[4]==0x8C000100 && d[5]==0x0C000100 && d[9]==6 && d[10]==3 && d[12]==1,"sink lost live origin/attempt");});
        pair("relocated fault origin",[](auto& f){f.runtime_pc=0x8C12947C;f.cpu.pc=f.runtime_pc;f.cpu.active_block_virtual_start=f.runtime_pc-0x7C;f.cpu.r[3]=0x8C000101;},faulted);
        pair("reference lookup",[](auto& f){f.cpu.memory.set_lookup_mode(MemoryLookupMode::Reference);},[](auto& f){successful(f,0x13579BDF,{0,1,1,0});});
        pair("guest-write observer",[](auto& f){f.cpu.memory.set_guest_write_observer([&f](const auto&){f.log.add({9,{}});});},[](auto& f){successful(f);require(f.log.size==0,"read emitted write notification");});
#if !defined(SONIC_TEST_PRELOADED_READ)
        // These artificial intervening actions are deliberately outside the
        // preloaded instruction envelope; retain them for the token API.
        pair("stale observer",{},[](auto& f){successful(f,0x13579BDF,{1,0,0,1});require(f.preflight && f.fallbacks==1,"stale guard was consumed");},[](auto& f){f.trace();});
        pair("stale mapping",{},[](auto& f){successful(f);require(f.fallbacks==1,"mapping mutation reused guard");},[](auto& f){f.cpu.memory.map_region("extra",0x11000000,std::make_shared<LinearMemoryDevice>(0x10000));});
        pair("clear window",{},[](auto& f){successful(f);require(f.fallbacks==1,"cleared window reused guard");},[](auto& f){f.cpu.memory.clear_direct_linear_alias_window();});
        pair("late load",{},[](auto& f){successful(f,0xFEDCBA98);},[](auto& f){f.ram->write_u32(0x100,0xFEDCBA98);});
        // The pinned Memory boundary epoch starts at one (zero is reserved).
        pair("noninvalidating settings",{},[](auto& f){successful(f);require(f.fallbacks==0 && f.log.size==0 && f.cpu.memory.mmio_boundary_epoch()==1,"RAM produced MMIO effects");},[](auto& f){f.cpu.memory.set_alignment_policy(MemoryAlignmentPolicy::Permissive);f.cpu.memory.set_mmio_access_tracking(true);f.cpu.memory.set_mmio_trace_handler([&f](const auto& e){f.access(7,e);});f.cpu.memory.set_mmio_interrupt_state_sink({&f,[](void* p) noexcept {static_cast<Fixture*>(p)->log.add({8,{}});}});});
#endif
        pair("miss flush changes source",[](auto& f){f.cpu.r[3]=0x8C020000;f.services.on_flush=[&f]{require(f.cpu.r[3]==0x8C020000 && f.cpu.attempted_guest_instructions==5,"flush saw unflushed registers/early attempt");f.cpu.r[3]=0x8C000104;f.ram->write_u32(0x104,0xABCDEF01);};},[](auto& f){successful(f,0xABCDEF01);require(!f.preflight && f.cpu.r[3]==0x8C000104 && f.log.size==1,"miss used stale source");});
        pair("MMIO read",[](auto& f){f.mmio();},[](auto& f){successful(f,0xDECAFBAD);require(f.fallbacks==1 && f.cpu.memory.mmio_boundary_epoch()==2 && f.services.polls==1,"MMIO boundary/safepoint missing");const auto e=f.cpu.memory.last_mmio_access();require(e && e->value==0xDECAFBAD && e->width==MemoryAccessWidth::Word,"MMIO event missing");});
        pair("MMIO rejected",[](auto& f){f.mmio(true);},[](auto& f){faulted(f);require(f.fault==MemoryAccessErrorReason::DeviceRejected,"wrong device rejection");});
#if defined(SONIC_TEST_PRELOADED_READ)
      }
        for(bool fd:{false,true}) for(bool pr:{false,true}) for(bool sz:{false,true}) for(bool bank:{false,true})
            pair("FPU modes",[=](auto& f){f.cpu.write_sr(sr_md_mask|(fd?sr_fd_mask:0));f.cpu.write_fpscr((pr?fpscr_pr_mask:0)|(sz?fpscr_sz_mask:0)|(bank?fpscr_fr_mask:0));},
                [=](auto& f){
                    if(fd || (pr && sz)) {
                        require(f.cpu.exception_generation==3 && f.cpu.retired_guest_instructions==3 && counts(f.cpu.memory)==Counts{},"FPU exception ordering/accounting changed");
                    } else if(sz) {
                        require(!f.fault && read_fpu_pair_bits(f.cpu,4u)==0x2468ACE013579BDFull && counts(f.cpu.memory)==Counts{2,0,2,0},"paired FMOV changed");
                    } else successful(f);
                });
        pair("pair second word fault",[](auto& f){f.cpu.fpscr=fpscr_sz_mask;f.cpu.r[3]=0x8C01FFFC;},faulted);
        for(unsigned mode=0;mode<5;++mode)
            pair("miss flush changes FP mode",[=](auto& f){
                f.cpu.r[3]=0x8C020000;
                if(mode==1)f.cpu.write_sr(sr_md_mask|sr_fd_mask);
                if(mode==3)f.cpu.fpscr=fpscr_sz_mask;
                f.services.on_flush=[&f,mode]{
                    f.cpu.r[3]=0x8C000100;
                    f.cpu.write_sr(sr_md_mask|(mode==0?sr_fd_mask:0));
                    f.cpu.fpscr=(mode==2?fpscr_sz_mask:mode==4?(fpscr_pr_mask|fpscr_sz_mask):0);
                };
            });
#endif
        guard_lifetime();
        std::cout<<"SONIC_PREPARED_READ_OK cases="<<cases<<" oracle=original-envelope aliases=checked observers=ordered counters=exact faults=provenance\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"SONIC_PREPARED_READ_FAIL "<<e.what()<<'\n';return 1;}
}
