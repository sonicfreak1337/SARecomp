#include "sonic_legacy_video.hpp"
#include "katana/runtime/runtime.hpp"
#include <cstdio>

extern "C" katana::runtime::NativePortHookResult sonic_legacy_video_mode_disabled(
    katana::runtime::NativePortContext& context) noexcept {
    // ADVERTISE 8089928E / runtime 8C90E28E is the shared owner of Apply,
    // Test and test restoration. All three callers ignore its return value.
    // Intercept before texture release, KAMUI shutdown or the persisted
    // 8C754B44 TV-mode write. A low-level PVR no-op would leave torn-down
    // native resources and falsely admit unrelated hardware accesses.
    // The manifest binds the exact function bytes and active module identity.
    if(context.cpu) {
        static thread_local unsigned reports=0;
        if(reports++<16u)std::fprintf(stderr,
            "SONIC_LEGACY_VIDEO_MODE disabled=1 mode=%u persist=%u caller=0x%08x frame=%llu\n",
            context.cpu->r[4],context.cpu->r[5],context.cpu->pr,
            static_cast<unsigned long long>(context.frame_index));
    }
    // No register/RAM writes, SDK calls, renderer change or simulation-rate
    // change. The existing menu retains navigation and its test countdown.
    return {katana::runtime::NativePortHookAction::Return,0u,0u};
}
