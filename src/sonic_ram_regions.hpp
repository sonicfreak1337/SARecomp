#pragma once
#include "sonic_scalar_write_view.hpp"
#include "katana/runtime/code_address_inline.hpp"
#include "katana/runtime/fpu.hpp"

namespace sonic::ram_regions {
using namespace katana::runtime;

// DN=1 excludes unmaskable denormal exceptions; the ordinary masked scalar
// helpers then affect only FR/FPSCR. GPR/provenance may remain in the prefix.
// End the host epoch before falling back to a memory instruction or scheduler.
inline bool arithmetic_admitted(const CpuState& cpu) noexcept {
    return (cpu.sr & sr_fd_mask) == 0u &&
        (cpu.fpscr & (fpscr_pr_mask | fpscr_sz_mask | fpscr_exception_enable_mask | fpscr_dn_mask)) == fpscr_dn_mask &&
        (cpu.fpscr & fpscr_rounding_mode_mask) <= 1u;
}

// A completed prefix is irrevocable. Each accepted access has the exact RAM
// effect and counters of the original instruction. A miss has no effects and
// resumes THAT original instruction, never the beginning of the region.
class Access final {
public:
    Access(CpuState& cpu, const DirectLinearMemoryGuard& entry,
           const NativePortImmutableWriteGuard* immutable) noexcept
        : entry_(entry), writes_(cpu.memory, immutable, false, true),
          privileged_(cpu.privileged_mode_inline()), mmu_(cpu.mmucr & 1u),
          stores_allowed_(cpu.memory.guest_write_observer_allows_prevalidated_linear_writes()) {}

    template<unsigned Bits, bool Signed = false>
    bool read(std::uint32_t address, std::uint32_t& result) const noexcept {
        static_assert(Bits == 8 || Bits == 16 || Bits == 32);
        std::uint32_t direct = 0;
        if (!translate(address, direct)) return false;
        if constexpr (Bits == 8) {
            std::uint8_t value = 0;
            if (!direct_linear_guard_read_u8(entry_, direct, value)) return false;
            result = Signed && (value & 0x80u) ? 0xFFFFFF00u | value : value;
        } else if constexpr (Bits == 16) {
            std::uint16_t value = 0;
            if (!direct_linear_guard_read_u16(entry_, direct, value)) return false;
            result = Signed && (value & 0x8000u) ? 0xFFFF0000u | value : value;
        } else {
            if (!direct_linear_guard_read_u32(entry_, direct, result)) return false;
        }
        return true;
    }

    template<unsigned Bits>
    bool write(std::uint32_t address, std::uint32_t value) const noexcept {
        static_assert(Bits == 8 || Bits == 16 || Bits == 32);
        std::uint32_t direct = 0, offset = 0;
        // Preserve the original read-guard-based store preflight and thus its
        // scheduler flush on a watched, stale, unaligned or non-RAM access.
        if (!stores_allowed_ || !translate(address, direct) ||
            !direct_linear_guard_offset(entry_, direct, Bits / 8, offset)) return false;
        if constexpr (Bits == 8) return writes_.try_write(direct, static_cast<std::uint8_t>(value));
        if constexpr (Bits == 16) return writes_.try_write(direct, static_cast<std::uint16_t>(value));
        if constexpr (Bits == 32) return writes_.try_write(direct, value);
    }

    template<std::size_t N>
    bool read_group(std::uint32_t address,std::array<std::uint32_t,N>& values) const noexcept {
        std::uint32_t direct=0;
        return translate(address,direct) && direct_linear_guard_read_u32_group(entry_,direct,values);
    }

private:
    bool translate(std::uint32_t address, std::uint32_t& direct) const noexcept {
        const auto segment = address >> 29u;
        if (segment == 4u || segment == 5u) {
            if (!privileged_) return false;
            direct = address;
        } else {
            if (mmu_ || segment >= 7u || (!privileged_ && address >= 0x80000000u)) return false;
            direct = canonical_physical_address_inline(address) | 0x80000000u;
        }
        return true;
    }
    const DirectLinearMemoryGuard& entry_;
    scalar_writes::View writes_;
    bool privileged_, mmu_, stores_allowed_;
};

inline void complete(CpuState& cpu, std::uint32_t instructions,
                     std::uint32_t cycles, std::uint32_t last_attempt_pc) noexcept {
    cpu.attempted_guest_instructions += instructions;
    cpu.retired_guest_instructions += instructions;
    cpu.pending_guest_cycles += cycles;
    // ALU accounting does not change instruction provenance in retained AOT.
    if (last_attempt_pc) {
        const auto pc = relocate_code_address_inline(last_attempt_pc);
        cpu.active_instruction_pc = pc;
        const auto offset = pc - cpu.active_block_virtual_start;
        cpu.active_instruction_physical_pc = cpu.active_block_size && offset < cpu.active_block_size
            ? cpu.active_block_physical_start + offset : canonical_physical_address_inline(pc);
    }
}
} // namespace sonic::ram_regions
