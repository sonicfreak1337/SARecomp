#pragma once
#include <cstdlib>
#include <cstring>

namespace sonic::native_cpu {
// Qualified whole-operation CPU paths are shared by both timing modes and
// platforms. Callers cache this startup policy, then still perform their own
// complete code, FPU, memory and callback admission on every operation.
// Internal overrides retain the original implementations for diagnosis.
inline bool enabled(const char* option) noexcept {
    const char* group = std::getenv("SARECOMP_NATIVE_CPU_PATHS");
    if (group && std::strcmp(group, "0") == 0) return false;
    const char* value = std::getenv(option);
    return !value || std::strcmp(value, "1") == 0;
}
// Connected gameplay, contact and display operations qualified together.
// The user's September 18 follow-up patch uses this group at normal startup;
// global and individual zero overrides retain the previous original owners.
inline bool gameplay_group_enabled(const char* option) noexcept {
    return enabled("SARECOMP_NATIVE_GAMEPLAY_GROUP") && enabled(option);
}
// Model ownership supplies the output span consumed by SIMD projection.
// Keep the coupled route selectable as one internal group; individual zero
// overrides remain available for the existing comparison tools.
inline bool model_group_enabled(const char* option) noexcept {
    return enabled("SARECOMP_NATIVE_MODEL_GROUP") && enabled(option);
}
}
