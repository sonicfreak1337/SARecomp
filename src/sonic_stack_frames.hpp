#pragma once
#include "sonic_scalar_write_view.hpp"
#include <array>
#include <limits>

namespace sonic::stack_frames {
using namespace katana::runtime;

inline bool translate(const CpuState& cpu,std::uint32_t address,std::uint32_t& direct) noexcept {
    const auto segment=address>>29u;
    if(segment==4u || segment==5u) {
        if(!cpu.privileged_mode_inline())return false;
        direct=address;
    } else {
        if((cpu.mmucr&1u)!=0 || segment>=7u ||
           (!cpu.privileged_mode_inline() && address>=0x80000000u))return false;
        direct=canonical_physical_address_inline(address)|0x80000000u;
    }
    return true;
}

inline void complete(CpuState& cpu,std::uint32_t last_pc,std::size_t count) noexcept {
    cpu.attempted_guest_instructions += count;
    cpu.retired_guest_instructions += count;
    cpu.pending_guest_cycles += count*2u;
    cpu.active_instruction_pc=last_pc;
    const auto offset=last_pc-cpu.active_block_virtual_start;
    cpu.active_instruction_physical_pc=cpu.active_block_size && offset<cpu.active_block_size
        ? cpu.active_block_physical_start+offset : canonical_physical_address_inline(last_pc);
}

template<unsigned Index,class Registers>
inline std::uint32_t& reg(Registers& registers) noexcept {
    static_assert(Index<15 || Index==16);
    if constexpr(Index==16)return registers.pr();
    else return registers[Index];
}

// This is a complete callback-free group, not deferred stores. The unchanged
// instruction path handles rejection, including any partial progress/fault.
template<unsigned... Indices,class Registers>
[[nodiscard]] inline bool push(CpuState& cpu,Registers& registers,
    const DirectLinearMemoryGuard& original_read_guard,
    const NativePortImmutableWriteGuard* immutable,std::uint32_t last_pc) noexcept {
    constexpr auto n=sizeof...(Indices);
    static_assert(n>=2 && n<=16);
    if(!scalar_writes::stack_frames_enabled() || !registers.owns_registers() || cpu.trap_pending)
        return false;
    const auto sp=registers[15];
    if(sp<n*4u)return false;
    std::uint32_t direct=0;
    if(!translate(cpu,sp-n*4u,direct))return false;
    // Original push preflight uses the function-entry READ guard. A freshly
    // captured write view alone would bypass its cycle flush for read-only
    // watchpoints or a stale entry guard. Keep that scheduler boundary intact.
    std::uint32_t first=0,last=0;
    if(!cpu.memory.guest_write_observer_allows_prevalidated_linear_writes() ||
       !direct_linear_guard_offset(original_read_guard,direct,4,first) ||
       !direct_linear_guard_offset(original_read_guard,direct+(n-1)*4u,4,last) ||
       last!=first+(n-1)*4u)return false;
    const scalar_writes::View view(cpu.memory,immutable,true);
    const std::array<std::uint32_t,n> order{reg<Indices>(registers)...};
    std::array<std::uint32_t,n> values{};
    for(std::size_t i=0;i<n;++i)values[i]=order[n-1-i];
    if(!view.try_write_words(direct,values))return false;
    registers[15]=sp-n*4u;
    complete(cpu,last_pc,n);
    return true;
}

template<unsigned... Indices,class Registers>
[[nodiscard]] inline bool pop(CpuState& cpu,Registers& registers,
    const DirectLinearMemoryGuard& guard,std::uint32_t last_pc) noexcept {
    constexpr auto n=sizeof...(Indices);
    static_assert(n>=2 && n<=16);
    if(!scalar_writes::stack_frames_enabled() || !registers.owns_registers() || cpu.trap_pending)
        return false;
    const auto sp=registers[15];
    if(sp>std::numeric_limits<std::uint32_t>::max()-n*4u)return false;
    std::uint32_t direct=0;
    if(!translate(cpu,sp,direct))return false;
    std::array<std::uint32_t,n> values{};
    if(!direct_linear_guard_read_u32_group(guard,direct,values))return false;
    std::size_t i=0;
    ((reg<Indices>(registers)=values[i++]),...);
    registers[15]=sp+n*4u;
    complete(cpu,last_pc,n);
    return true;
}
} // namespace sonic::stack_frames
