#pragma once
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
} // namespace
