#pragma once
#include "sonic_scalar_write_view.hpp"
#include <optional>
#include <stdexcept>

namespace sonic::model_memory {
// Only use after the WHOLE native owner's read/write/code alias admission.
// No callbacks, mappings, observer changes or guest calls may occur before
// destruction. Every write address must belong to those admitted ranges.
// The registered product observer has no effect outside immutable ranges;
// arbitrary observers retain every ordered write through the public API.
class Writes {
public:
    Writes(katana::runtime::CpuState& cpu,
           const katana::runtime::NativePortImmutableWriteGuard& immutable,
           const katana::runtime::DirectLinearMemoryGuard& read)
        : memory_(cpu.memory) {
        const sonic::scalar_writes::View view(memory_,&immutable,false,false,true);
        const auto writable=view.closed_region_snapshot();
        if(writable && writable.write_bytes==read.read_bytes && writable.generation==read.generation &&
           writable.physical_base==read.physical_base && writable.physical_span==read.physical_span &&
           writable.backing_mask==read.backing_mask){
            bytes_=writable.write_bytes;
            counters_=&const_cast<katana::runtime::MemoryPerformanceCounters&>(memory_.performance_counters());
        }
    }
    Writes(const Writes&)=delete;
    ~Writes(){
        if(counters_){counters_->indexed_region_hits+=count_;counters_->unobserved_accesses+=count_;}
    }
    bool direct()const noexcept{return bytes_!=nullptr;}
    std::uint64_t word_count()const noexcept{return count_;}
    void store(std::uint32_t address,std::uint32_t value,katana::runtime::CodeWriteSource source){
        if(bytes_){std::memcpy(bytes_+(address&0xFFFFFFu),&value,4u);++count_;return;}
        if(!memory_.try_write_direct_linear_u32(address&0x1FFFFFFFu,value,source))
            throw std::runtime_error("native model: admitted write failed; Original restart forbidden");
    }
private:
    katana::runtime::Memory& memory_;
    std::uint8_t* bytes_=nullptr;
    katana::runtime::MemoryPerformanceCounters* counters_=nullptr;
    std::uint64_t count_=0;
};

// Port-wide experiment for already-native, closed leaves. It does not grant
// access to arbitrary guest stores: each caller first admits its complete
// read/write/code footprint, then retains the existing product capability.
inline bool closed_leaves_enabled() noexcept {
    static const bool value=native_cpu::enabled("SARECOMP_NATIVE_CLOSED_MEMORY");
    return value && !sonic::diagnostics::runtime_checks_enabled();
}
struct ClosedLeafCounts {std::uint64_t calls=0,words=0;};
inline thread_local ClosedLeafCounts closed_leaf_counts{};
class ClosedLeafWrites final {
public:
    ClosedLeafWrites(katana::runtime::CpuState& cpu,
        const katana::runtime::NativePortImmutableWriteGuard& immutable,
        const katana::runtime::DirectLinearMemoryGuard& read) {
        if(closed_leaves_enabled())writes_.emplace(cpu,immutable,read);
        if(direct())++closed_leaf_counts.calls;
    }
    ~ClosedLeafWrites(){if(direct())closed_leaf_counts.words+=writes_->word_count();}
    bool direct()const noexcept{return writes_ && writes_->direct();}
    bool try_store(std::uint32_t address,std::uint32_t value,katana::runtime::CodeWriteSource source){
        if(!direct())return false;
        writes_->store(address,value,source);return true;
    }
private:
    std::optional<Writes> writes_;
};
}
