#pragma once
#include "katana/runtime/runtime.hpp"
#include <bit>
#include <cstring>

namespace sonic::memory {
// Used only by source-authenticated, straight-line groups of ordinary loads.
// No real CPU/register writes, stores, callbacks or mode changes may intervene
// between construction and commit. Reads go to local temporaries; any rejection
// executes the complete original group without prior observable effects.
class ReadGroup32 final {
public:
    ReadGroup32(katana::runtime::CpuState& cpu,
                const katana::runtime::DirectLinearMemoryGuard& guard,
                bool contains_fmov) noexcept
        : cpu_(cpu), guard_(guard), privileged_(cpu.privileged_mode_inline()),
          mmu_(cpu.mmucr & 1u), valid_(static_cast<bool>(guard) &&
              (!contains_fmov || ((cpu.sr & katana::runtime::sr_fd_mask) == 0u &&
                                  (cpu.fpscr & katana::runtime::fpscr_sz_mask) == 0u))) {}

    bool read(std::uint32_t address, std::uint32_t& value) noexcept {
        if (!valid_) return false;
        const auto segment = address >> 29u;
        std::uint32_t direct;
        if (segment == 4u || segment == 5u) {
            if (!privileged_) return reject();
            direct = address;
        } else {
            if (mmu_ || segment >= 7u || (!privileged_ && address >= 0x80000000u))
                return reject();
            direct = katana::runtime::canonical_physical_address_inline(address) | 0x80000000u;
        }
        if ((direct & 0xC0000003u) != 0x80000000u) return reject();
        const auto physical = direct & 0x1FFFFFFFu;
        if (physical < guard_.physical_base) return reject();
        const auto relative = physical - guard_.physical_base;
        if (relative >= guard_.physical_span || 4u > guard_.physical_span - relative)
            return reject();
        const auto offset = relative & guard_.backing_mask;
        if (4u > static_cast<std::size_t>(guard_.backing_mask) + 1u - offset)
            return reject();
        if constexpr (std::endian::native == std::endian::little) {
            std::memcpy(&value, guard_.read_bytes + offset, sizeof(value));
        } else {
            const auto* bytes = guard_.read_bytes + offset;
            value = std::uint32_t(bytes[0]) | std::uint32_t(bytes[1]) << 8u |
                    std::uint32_t(bytes[2]) << 16u | std::uint32_t(bytes[3]) << 24u;
        }
        ++reads_;
        return true;
    }

    bool commit(std::uint32_t expected_reads) noexcept {
        // Aggregate accounting has stricter admission than direct RAM reads
        // (including write-only observers and MMIO tracking). Refusal must
        // fall back to the whole original group, without publishing temporaries.
        if (!valid_ || reads_ != expected_reads || !guard_) return false;
        valid_ = false; // One commit, even if a caller accidentally retries.
        return cpu_.memory.account_prevalidated_unobserved_accesses(reads_, reads_);
    }

private:
    bool reject() noexcept { valid_ = false; return false; }
    katana::runtime::CpuState& cpu_;
    const katana::runtime::DirectLinearMemoryGuard& guard_;
    bool privileged_;
    bool mmu_;
    bool valid_;
    std::uint32_t reads_ = 0;
};

inline void complete_read_group(katana::runtime::CpuState& cpu,
                                std::uint32_t last_pc,
                                std::uint32_t instructions) noexcept {
    cpu.active_instruction_pc = last_pc;
    const auto offset = last_pc - cpu.active_block_virtual_start;
    cpu.active_instruction_physical_pc = cpu.active_block_size != 0u && offset < cpu.active_block_size
        ? cpu.active_block_physical_start + offset
        : katana::runtime::canonical_physical_address_inline(last_pc);
    cpu.attempted_guest_instructions += instructions;
    cpu.retired_guest_instructions += instructions;
    cpu.pending_guest_cycles += 2u * instructions;
}
} // namespace sonic::memory
