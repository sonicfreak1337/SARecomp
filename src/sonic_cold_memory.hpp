#pragma once
#include "katana/runtime/native_port_aot_runtime.hpp"

namespace sonic::cold_memory {
// These helpers preserve the generated fallback's operation order and types.
// Keeping register release and reacquisition out of the fast path changes
// only native code placement.
// Exceptions intentionally bypass reacquisition exactly as before.
template<class Registers>
[[gnu::noinline, gnu::cold]] std::uint32_t read_s8(
    katana::runtime::CpuState& cpu, Registers& registers,
    const katana::runtime::GuestInstructionOrigin& origin, std::uint32_t address) {
    const bool reacquire = registers.owns_registers();
    registers.flush_release();
    const auto value = static_cast<std::uint32_t>(katana::runtime::guest_read_s8_at(cpu, origin, address));
    if (reacquire) registers.reload_acquire();
    return value;
}
template<class Registers>
[[gnu::noinline, gnu::cold]] std::uint32_t read_s16(
    katana::runtime::CpuState& cpu, Registers& registers,
    const katana::runtime::GuestInstructionOrigin& origin, std::uint32_t address) {
    const bool reacquire = registers.owns_registers();
    registers.flush_release();
    const auto value = static_cast<std::uint32_t>(katana::runtime::guest_read_s16_at(cpu, origin, address));
    if (reacquire) registers.reload_acquire();
    return value;
}
template<class Registers>
[[gnu::noinline, gnu::cold]] std::uint32_t read_u32(
    katana::runtime::CpuState& cpu, Registers& registers,
    const katana::runtime::GuestInstructionOrigin& origin, std::uint32_t address) {
    const bool reacquire = registers.owns_registers();
    registers.flush_release();
    const auto value = static_cast<std::uint32_t>(katana::runtime::guest_read_u32_at(cpu, origin, address));
    if (reacquire) registers.reload_acquire();
    return value;
}
template<class Registers, class Value>
[[gnu::noinline, gnu::cold]] void write(
    katana::runtime::CpuState& cpu, Registers& registers,
    const katana::runtime::GuestInstructionOrigin& origin, std::uint32_t address,
    Value value, katana::runtime::CodeWriteSource source) {
    static_assert(sizeof(Value)==1 || sizeof(Value)==2 || sizeof(Value)==4);
    const bool reacquire = registers.owns_registers();
    registers.flush_release();
    if constexpr(sizeof(Value)==1) katana::runtime::guest_write_u8_at(cpu,origin,address,value,source);
    if constexpr(sizeof(Value)==2) katana::runtime::guest_write_u16_at(cpu,origin,address,value,source);
    if constexpr(sizeof(Value)==4) katana::runtime::guest_write_u32_at(cpu,origin,address,value,source);
    if (reacquire) registers.reload_acquire();
}
} // namespace sonic::cold_memory
