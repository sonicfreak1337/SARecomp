#pragma once

#include "katana/runtime/runtime.hpp"

#include <array>
#include <cstdint>

namespace sonic_native {

inline constexpr std::array<std::uint32_t, 16u> postpal_r{
    0x00000000u, 0x8C8A3300u, 0xACFDFE20u, 0x00004000u,
    0x8C890044u, 0x8C88FC14u, 0x8C8A3300u, 0xACF5FDC0u,
    0x8C099124u, 0x0000000Au, 0x8C7608A8u, 0x0C900000u,
    0x00000000u, 0x8C7608B0u, 0x00000000u, 0x8C00F3A4u};
inline constexpr std::array<std::uint32_t, 8u> postpal_r_bank{
    0x700000F0u, 0xFFFFFF0Fu, 0xFF00001Cu, 0x00000000u,
    0xAC00FC00u, 0x00000000u, 0x00000400u, 0x00000400u};
inline constexpr std::array<std::uint32_t, 16u> postpal_fr{
    0x43E00000u, 0x43800000u, 0x44300000u, 0x44000000u,
    0x3F800000u, 0x00000000u, 0x3DCCCCCCu, 0x3F800000u,
    0x80000000u, 0x80000000u, 0x00000000u, 0x3F800000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u};
inline constexpr std::array<std::uint32_t, 16u> postpal_xf{
    0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0xBF800000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0xBF800000u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000000u, 0x3F800000u};

inline void apply_postpal_cpu_state(katana::runtime::CpuState& cpu) noexcept {
    cpu.write_sr(0x60000001u);
    cpu.write_fpscr(0x00040001u);
    cpu.r = postpal_r;
    cpu.r_bank = postpal_r_bank;
    cpu.fr = postpal_fr;
    cpu.xf = postpal_xf;
    cpu.pc = 0x8C053CA2u;
    cpu.pr = 0x8C053CA2u;
    cpu.gbr = 0x8C8FFE00u;
    cpu.vbr = 0x8C00F400u;
    cpu.ssr = 0x60000000u;
    cpu.spc = 0x8C652480u;
    cpu.sgr = 0x8C00B9A4u;
    cpu.dbr = 0x8C000010u;
    cpu.tra = 0u;
    cpu.tea = 0u;
    cpu.expevt = 0x20u;
    cpu.intevt = 0x320u;
    cpu.pteh = 0u;
    cpu.ptel = 0u;
    cpu.ptea = 0u;
    cpu.ttb = 0u;
    cpu.mmucr = 0u;
    cpu.mach = 0u;
    cpu.macl = 0x16Cu;
    cpu.fpul = 0x200u;
    cpu.trap_pending = false;
    cpu.sleeping = false;
    cpu.pending_guest_cycles = 0u;
    cpu.active_instruction_pc = 0u;
    cpu.active_instruction_physical_pc = 0u;
    cpu.active_block_virtual_start = 0u;
    cpu.active_block_physical_start = 0u;
    cpu.active_block_size = 0u;
}

} // namespace sonic_native
