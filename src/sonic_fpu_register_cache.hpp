#pragma once
#include "katana/runtime/fpu.hpp"
#include <cstdlib>

namespace sonic::fpu_register_cache {
// Internal experiment switch. No arithmetic implementation changes here.
inline bool enabled() noexcept {
    static const bool value=[] {
        const auto* setting=std::getenv("SARECOMP_FPU_REGISTER_CACHE");
        return setting && setting[0]=='1' && setting[1]=='\0';
    }();
    return value;
}

inline bool admitted(const katana::runtime::CpuState& cpu) noexcept {
    using namespace katana::runtime;
    // DN also excludes the otherwise unmaskable denormal-input exception.
    // Only the authenticated FR/FPSCR-only helpers may use this contract.
    return (cpu.sr & sr_fd_mask)==0u &&
        (cpu.fpscr & (fpscr_pr_mask|fpscr_exception_enable_mask|fpscr_dn_mask))==fpscr_dn_mask &&
        (cpu.fpscr & fpscr_rounding_mode_mask)<=1u;
}
} // namespace sonic::fpu_register_cache
