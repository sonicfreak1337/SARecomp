#pragma once
#include "katana/runtime/runtime.hpp"
#include <array>
#include "sonic_internal_diagnostics.hpp"
#include "sonic_native_cpu_policy.hpp"
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::atan_math {
struct SourceSpan { std::uint32_t address, size; const char* sha256; };
inline constexpr std::uint32_t atan_entry=0x8C10EEC4u, atan_size=0x1E0u;
inline constexpr auto atan_source_sha256="1361220e5d950f6c9548df0303e16156c0aceb2c3f19753d7329dc28070d6496";
inline constexpr std::uint32_t quotient_entry=0x8C10FAF8u, quotient_size=0x104u;
inline constexpr auto quotient_source_sha256="8edb1eea052f1622840e3f6fa67dd7aa2efccfcb8e8e030d3935ea5b5a826fb4";
inline constexpr std::uint32_t polynomial_entry=0x8C10FAD4u, polynomial_size=0x24u;
inline constexpr auto polynomial_source_sha256="4c9efceb0a2491382e2251fb758565cb4073f1292ea079692f68e79e22246b82";
inline constexpr std::uint32_t scale_entry=0x8C10E6F8u, scale_size=0xC0u;
inline constexpr auto scale_source_sha256="316c8b53e094bc27f5d85d3be392105d732e2aae3609409e41b862ce1dddb4ca";
inline constexpr std::uint32_t constants_entry=0x8C16012Cu, constants_size=0x50u;
inline constexpr auto constants_source_sha256="d9857d1b702c629cf4614576e64fbe0f1c785a39ef4d0e75a7f5a8ee709a7cd6";
inline constexpr std::uint32_t coefficients_entry=0x8C1602C4u, coefficients_size=0x30u;
inline constexpr auto coefficients_source_sha256="fb62207f78af9dbab8c317128195fe75082eea7acd27b8fef1fbae57b3d410c1";
inline constexpr std::array source_spans{
    SourceSpan{atan_entry,atan_size,atan_source_sha256},
    SourceSpan{quotient_entry,quotient_size,quotient_source_sha256},
    SourceSpan{polynomial_entry,polynomial_size,polynomial_source_sha256},
    SourceSpan{scale_entry,scale_size,scale_source_sha256},
    SourceSpan{constants_entry,constants_size,constants_source_sha256},
    SourceSpan{coefficients_entry,coefficients_size,coefficients_source_sha256},
};
inline constexpr std::array inverse_source_spans{
    SourceSpan{0x8C10CF48u,0x50u,"67190b24fdf1f5c43e1a3a7ea5c1fbd06087f1fbd0b721cba490e73102bf777a"},
    SourceSpan{0x8C10CF98u,0x50u,"96055337e822b6e0fee2a34e5d6c37e0c3e495cb49cc2bf75581e0f11ade1111"},
    SourceSpan{0x8C10CFE8u,0x50u,"78db9abf037229a0aebf28c5d183f0fd7adab9c5d0ab948341257ceb170805b6"},
    SourceSpan{0x8C10D038u,0x50u,"78db9abf037229a0aebf28c5d183f0fd7adab9c5d0ab948341257ceb170805b6"},
    SourceSpan{0x8C10E4D0u,0x1Cu,"c64279e7db910660f546db7619305d6fa96170593bf67c472d1672b3c4dcbe88"},
    SourceSpan{0x8C10E4ECu,0xF4u,"cca074bef0fb28c0432706617278427494850ca927e2dbaeb6a53da156b58913"},
    SourceSpan{0x8C10E5E0u,0x58u,"42f5dbc9da5124c303617d367cb4f87cf27d7d2761e1a4b3b325744e1e92df2b"},
};
inline thread_local std::uint32_t inverse_return_site{};
inline thread_local std::uint64_t inverse_calls{},inverse_declined{};
inline thread_local std::uint64_t closed_memory_calls{},closed_memory_stores{};
inline bool closed_memory_enabled() noexcept {
    static const bool on=[] {const auto* v=std::getenv("SARECOMP_NATIVE_INVERSE_MEMORY");
        return v && std::strcmp(v,"1")==0 && native_cpu::enabled("SARECOMP_NATIVE_INVERSE_MEMORY");}();
    return on && !diagnostics::runtime_checks_enabled();
}
inline bool inverse_enabled() noexcept {
    static const bool on=native_cpu::enabled("SARECOMP_NATIVE_INVERSE_TRIG");
    return on && !diagnostics::runtime_checks_enabled();
}
bool is_inverse_entry(std::uint32_t) noexcept;
// Full original atan and its three fixed, call-free helpers; CPU.PC selects any
// of the four entries or the seven inverse-trig parents. FR4 is the float input; quotient also consumes FR5 and
// writes its integer quotient at R4; scale consumes signed R4 as the exponent.
// True preserves all terminal registers/FPSCR/PR and ordered stack/output stores,
// returning to caller PR. Maximum stack: atan 72, quotient 32, scale 8, polynomial 0.
// False is mutation-free and occurs only before entry. PR/SZ/Enables=0, DN=1,
// RM=0/1, FD=0 and stable unobserved linear RAM are required. The hook caller
// owns guest instruction/cycle accounting. Code AND data words are authenticated.
// The seven parents share this admission and the four existing children.
// Their exact original basic-block FPU epochs are preserved; no owner-wide
// epoch may span the polynomial FMAC loop or a child call/delay instruction.
// Parent stack depths are 100/96/92/92/92/88/84 bytes. Wrapper errno at
// 8C7AC048 cannot alias their stack; unsafe cases decline before any mutation.
[[nodiscard]] bool try_execute(katana::runtime::CpuState& cpu,
    const katana::runtime::NativePortImmutableWriteGuard* immutable_guard);
} // namespace sonic::atan_math
