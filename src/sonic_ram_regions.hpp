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
class CheckedAccess final {
public:
    CheckedAccess(CpuState& cpu, const DirectLinearMemoryGuard& entry,
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

// No callback can occur between construction and destruction. Mapping and
// observer lifetime is proved once; addresses still need permission, alignment
// and bounds checks. Whole 256-byte pages may share a proof, never their values.
// The original entry guard must be current: acquiring a fresh write capability
// cannot turn a stale READ preflight into a successful access.
class Access final {
public:
    Access(CpuState& cpu, const DirectLinearMemoryGuard& entry,
           const NativePortImmutableWriteGuard* immutable) noexcept
        : privileged_(cpu.privileged_mode_inline()), mmu_(cpu.mmucr & 1u) {
        if (!entry || !cpu.memory.direct_linear_memory_guard_current(entry, false)) return;
        read_bytes_ = entry.read_bytes;
        base_ = entry.physical_base;
        span_ = entry.physical_span;
        mask_ = entry.backing_mask;
        counters_ = &const_cast<MemoryPerformanceCounters&>(cpu.memory.performance_counters());
        if (!cpu.memory.guest_write_observer_allows_prevalidated_linear_writes()) return;
        const scalar_writes::View view(cpu.memory, immutable, false, true);
        const auto writable = view.closed_region_snapshot();
        // The entry read proof also governs every store's scheduler preflight.
        if (writable.write_bytes == read_bytes_ && writable.generation == entry.generation &&
            writable.physical_base == base_ && writable.physical_span == span_ &&
            writable.backing_mask == mask_) {
            write_bytes_ = writable.write_bytes;
            immutable_ = immutable;
        }
    }
    Access(const Access&) = delete;
    Access& operator=(const Access&) = delete;
    ~Access() {
        if (accesses_) {
            counters_->indexed_region_hits += accesses_;
            counters_->unobserved_accesses += accesses_;
        }
    }

    template<unsigned Bits, bool Signed = false>
    bool read(std::uint32_t address, std::uint32_t& result) const noexcept {
        static_assert(Bits == 8 || Bits == 16 || Bits == 32);
        static_assert(std::endian::native == std::endian::little);
        std::uint32_t physical = 0;
        if (!read_bytes_ || !translate(address, physical) || (physical & (Bits / 8 - 1))) return false;
        const auto* pointer = read_pointer(physical, Bits / 8);
        if (!pointer) return false;
        if constexpr (Bits == 8) {
            const auto value = *pointer;
            result = Signed && (value & 0x80u) ? 0xFFFFFF00u | value : value;
        } else if constexpr (Bits == 16) {
            std::uint16_t value = 0;
            std::memcpy(&value, pointer, sizeof(value));
            result = Signed && (value & 0x8000u) ? 0xFFFF0000u | value : value;
        } else {
            std::memcpy(&result, pointer, sizeof(result));
        }
        ++accesses_;
        return true;
    }

    template<unsigned Bits>
    bool write(std::uint32_t address, std::uint32_t value) const noexcept {
        static_assert(Bits == 8 || Bits == 16 || Bits == 32);
        std::uint32_t physical = 0;
        if (!write_bytes_ || !translate(address, physical) || (physical & (Bits / 8 - 1))) return false;
        const auto page = physical & ~page_mask;
        const auto within = physical & page_mask;
        std::uint8_t* pointer = nullptr;
        if (page == write_page_) {
            pointer = write_page_pointer_ + within;
        } else {
            std::uint32_t offset = 0;
            if (range(page, page_size, offset) && !immutable_->tracks_address(page, page_size)) {
                write_page_ = page;
                write_page_pointer_ = write_bytes_ + offset;
                pointer = write_page_pointer_ + within;
            } else {
                // A mixed code/data page is not forbidden as a whole. Keep the
                // exact range proof and leave code/RO writes to the original.
                if (!range(physical, Bits / 8, offset) || immutable_->tracks_address(physical, Bits / 8)) return false;
                pointer = write_bytes_ + offset;
            }
        }
        // Aligned scalar widths cannot cross a 256-byte page boundary.
        std::memcpy(pointer, &value, Bits / 8);
        ++accesses_;
        return true;
    }

    template<std::size_t N>
    bool read_group(std::uint32_t address, std::array<std::uint32_t,N>& values) const noexcept {
        static_assert(N > 0 && N <= 16);
        std::uint32_t physical = 0;
        if (!read_bytes_ || !translate(address, physical) || (physical & 3u) ||
            physical > 0x20000000u - 4u*N) return false;
        const auto* pointer = read_pointer(physical, 4u*N);
        if (!pointer) return false;
        std::memcpy(values.data(), pointer, 4u*N);
        accesses_ += N;
        return true;
    }

private:
    bool translate(std::uint32_t address, std::uint32_t& physical) const noexcept {
        const auto segment = address >> 29u;
        if (segment == 4u || segment == 5u) {
            if (!privileged_) return false;
            physical = address & 0x1FFFFFFFu;
        } else {
            if (mmu_ || segment >= 7u || (!privileged_ && address >= 0x80000000u)) return false;
            physical = canonical_physical_address_inline(address);
        }
        return true;
    }
    bool range(std::uint32_t physical, std::uint32_t width, std::uint32_t& offset) const noexcept {
        if (physical < base_) return false;
        const auto relative = physical - base_;
        if (relative >= span_ || width > span_ - relative) return false;
        offset = relative & mask_;
        return width <= static_cast<std::uint64_t>(mask_) + 1u - offset;
    }
    const std::uint8_t* read_pointer(std::uint32_t physical, std::uint32_t width) const noexcept {
        const auto page = physical & ~page_mask;
        const auto within = physical & page_mask;
        const bool inside = width <= page_size - within;
        if (inside && page == read_page_) return read_page_pointer_ + within;
        std::uint32_t offset = 0;
        if (inside && range(page, page_size, offset)) {
            read_page_ = page;
            read_page_pointer_ = read_bytes_ + offset;
            return read_page_pointer_ + within;
        }
        if (!range(physical, width, offset)) return nullptr;
        return read_bytes_ + offset;
    }
    static constexpr std::uint32_t page_size = 256, page_mask = page_size - 1;
    const std::uint8_t* read_bytes_ = nullptr;
    std::uint8_t* write_bytes_ = nullptr;
    const NativePortImmutableWriteGuard* immutable_ = nullptr;
    MemoryPerformanceCounters* counters_ = nullptr;
    std::uint32_t base_ = 0, span_ = 0, mask_ = 0;
    mutable std::uint32_t read_page_ = UINT32_MAX, write_page_ = UINT32_MAX;
    mutable const std::uint8_t* read_page_pointer_ = nullptr;
    mutable std::uint8_t* write_page_pointer_ = nullptr;
    mutable std::uint64_t accesses_ = 0;
    bool privileged_, mmu_;
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
