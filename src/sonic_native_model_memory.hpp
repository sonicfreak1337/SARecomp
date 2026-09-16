#pragma once
#include "sonic_scalar_write_view.hpp"
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
        const sonic::scalar_writes::View view(memory_,&immutable,false,true);
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
}
