#pragma once
#include "sonic_scalar_write_view.hpp"

namespace sonic::collision_memory {
inline bool enabled() noexcept {
    static const bool value=[] {
        const char* p=std::getenv("SARECOMP_NATIVE_COLLISION_MEMORY");
        return p && std::strcmp(p,"1")==0;
    }();
    return value && !diagnostics::runtime_checks_enabled();
}
inline bool closure_enabled() noexcept {
    static const bool value=[] {
        const char* p=std::getenv("SARECOMP_NATIVE_COLLISION_CLOSURE");
        return p && std::strcmp(p,"1")==0;
    }();
    return value && !diagnostics::runtime_checks_enabled();
}
struct Counts {std::uint64_t intervals=0,reads=0,writes=0,active=0;
    std::uint64_t fused_cross=0,fused_length=0,fused_normalize=0;};
inline thread_local Counts counts{};

// A capability for an ALREADY admitted, closed collision interval. Its caller
// has proved every address, width, alias and write permission, including stack
// and scratch storage. Stores remain immediate and in original order. There
// can be no guest call, callback or mapping/observer change within an interval.
// Only the reviewed callback-free collision_math closed children can share it.
// End it BEFORE every other owner/retained call, then perform the owner's
// existing revalidation before capturing again. Never cache it across owners.
class Access final {
public:
    Access()=default;
    Access(const Access&)=delete;
    ~Access(){reset();}
    void capture(katana::runtime::CpuState& cpu,
        const katana::runtime::NativePortImmutableWriteGuard& immutable,
        const katana::runtime::DirectLinearMemoryGuard& read,bool closed_owner=false) noexcept {
        reset();
        if(!enabled() && !(closed_owner && closure_enabled()))return;
        const scalar_writes::View view(cpu.memory,&immutable,false,false,true);
        const auto write=view.closed_region_snapshot();
        if(!write || write.write_bytes!=read.read_bytes || write.generation!=read.generation ||
           write.physical_base!=0x0C000000u || write.physical_base!=read.physical_base ||
           write.physical_span!=read.physical_span || write.physical_span<0x1000000u ||
           write.backing_mask!=0xFFFFFFu || write.backing_mask!=read.backing_mask)return;
        bytes_=write.write_bytes;
        metrics_=&const_cast<katana::runtime::MemoryPerformanceCounters&>(cpu.memory.performance_counters());
        ++counts.intervals;++counts.active;
    }
    void reset() noexcept {
        if(metrics_){
            metrics_->indexed_region_hits+=reads_+writes_;
            metrics_->unobserved_accesses+=reads_+writes_;
            counts.reads+=reads_;counts.writes+=writes_;--counts.active;
        }
        bytes_=nullptr;metrics_=nullptr;reads_=writes_=0;
    }
    bool direct()const noexcept{return bytes_!=nullptr;}
    template<class T> bool try_read(std::uint32_t address,T& value) noexcept {
        static_assert(std::is_unsigned_v<T> && (sizeof(T)==1 || sizeof(T)==2 || sizeof(T)==4));
        static_assert(std::endian::native==std::endian::little);
        if(!bytes_)return false;
        std::memcpy(&value,bytes_+(address&0xFFFFFFu),sizeof(T));++reads_;return true;
    }
    template<class T> bool try_store(std::uint32_t address,T value) noexcept {
        static_assert(std::is_unsigned_v<T> && (sizeof(T)==1 || sizeof(T)==2 || sizeof(T)==4));
        static_assert(std::endian::native==std::endian::little);
        if(!bytes_)return false;
        std::memcpy(bytes_+(address&0xFFFFFFu),&value,sizeof(T));++writes_;return true;
    }
private:
    std::uint8_t* bytes_=nullptr;
    katana::runtime::MemoryPerformanceCounters* metrics_=nullptr;
    std::uint64_t reads_=0,writes_=0;
};
}
