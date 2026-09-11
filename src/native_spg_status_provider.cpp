#include "katana/runtime/native_port.hpp"
#include "katana/runtime/runtime.hpp"

namespace {

constexpr std::uint32_t sonic_native_spg_status_error_context = 0x53415620u;
constexpr std::uint32_t sonic_native_spg_status_error_sync = 0x53415621u;

} // namespace

extern "C" katana::runtime::NativePortHookResult
sonic_native_spg_status_boundary_wait(
    katana::runtime::NativePortContext& context) noexcept {
    if (context.cpu == nullptr || context.host == nullptr)
        return {katana::runtime::NativePortHookAction::Abort, 0u,
                sonic_native_spg_status_error_context};

    try {
        // The identity-bound latent SDK leaf waits until SPG_STATUS bit 0x2000
        // is observed set and then until the same bit clears. That is exactly
        // one complete Dreamcast vertical-blank pulse. The native product has
        // no scanline/MMIO device, so consume one host simulation boundary and
        // reproduce the leaf's complete architectural result. Export-time
        // module/function/literal identities prove the displaced code; latent
        // AOT bytes need not be materialized as ordinary guest RAM at runtime.
        context.host->synchronize_simulation_boundary();
        context.cpu->r[0] = 0u;
        context.cpu->r[1] = 0x2000u;
        context.cpu->t = true;
        return {katana::runtime::NativePortHookAction::Return, 0u, 0u};
    } catch (...) {
        return {katana::runtime::NativePortHookAction::Abort, 0u,
                sonic_native_spg_status_error_sync};
    }
}

extern "C" katana::runtime::NativePortHookResult
sonic_native_spg_status_boundary_wait_840d03e0(
    katana::runtime::NativePortContext& context) noexcept {
    return sonic_native_spg_status_boundary_wait(context);
}
