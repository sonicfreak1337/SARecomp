#pragma once
#include "katana/runtime/runtime.hpp"
#include "sonic_native_cpu_policy.hpp"
#include "sonic_internal_diagnostics.hpp"

namespace katana::runtime { class NativePortImmutableWriteGuard; class NativePortAotServices; }
namespace sonic::object_activation {
inline bool enabled() noexcept {
    static const bool on=native_cpu::enabled("SARECOMP_NATIVE_OBJECT_ACTIVATION");
    return on && !diagnostics::runtime_checks_enabled();
}
struct Calls {
    void* context{};
    bool (*invoke)(void*,katana::runtime::CpuState&,std::uint32_t){};
};
enum class Outcome { Declined, Complete, Interrupted };
struct Statistics {
    std::uint64_t distance_calls{},activation_calls{},lifetime_calls{},records{},created{},retired{},declined{},slow_accesses{};
    std::uint64_t distance_native{},distance_fallback{};
};
inline thread_local Statistics counts;
inline thread_local std::uint32_t return_site{};
Outcome execute(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*,Calls={});
// Implemented at the existing title/AOT bridge; only public function entries
// use this route. Resume entries and disabled/rejected inputs retain Original.
bool try_dispatch(katana::runtime::CpuState&,katana::runtime::NativePortAotServices&);
}
