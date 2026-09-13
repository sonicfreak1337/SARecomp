#pragma once
#include "katana/runtime/runtime.hpp"
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::matrix_inverse {
inline constexpr std::uint32_t inverse_entry=0x8C638FF0u, inverse_size=0x804u;
inline constexpr auto inverse_source_sha256="ff02ae8352528051e7806b0d08f449d086f052891e499156aaf49b7e76a4a996";
inline constexpr std::uint32_t determinant_entry=0x8C64F32Cu, determinant_size=0x158u;
inline constexpr auto determinant_source_sha256="f237439dce9e4b3ab4b359ce5fce9bb37a82328916e1809955650de95bf5f28c";
// Entire inverse including its fixed determinant call, or determinant alone
// according to CPU.PC. R4==0 selects XMTRX; otherwise inverse updates 64 bytes
// in place. False precedes guest/observer/metric mutation. True preserves bit
// arithmetic, FPSCR, registers, internal PR=8C638FFA, stack residues and ordered
// stores, ending at caller PR. Unsafe stack/matrix/code aliases decline.
// Admission requires PR/SZ/Enables=0, DN=1, legal RM and FD=0, with stable linear
// RAM observers. The native-hook caller owns instruction/cycle accounting.
[[nodiscard]] bool try_execute(katana::runtime::CpuState& cpu,
    const katana::runtime::NativePortImmutableWriteGuard* immutable_guard);
} // namespace sonic::matrix_inverse
