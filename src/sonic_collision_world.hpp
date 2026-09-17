#pragma once
#include "katana/runtime/runtime.hpp"
#include "sonic_internal_diagnostics.hpp"
#include "sonic_native_cpu_policy.hpp"
#include <array>
#include <span>
namespace katana::runtime {class NativePortImmutableWriteGuard;class NativePortAotServices;}
namespace sonic::collision_world {
inline constexpr std::uint32_t entry=0x8C028EC2u;
inline thread_local unsigned resume_depth{};
inline thread_local std::uint32_t return_site{};
inline bool enabled() noexcept {
    static const bool on=native_cpu::enabled("SARECOMP_NATIVE_COLLISION_WORLD");
    return on && !resume_depth && !diagnostics::runtime_checks_enabled();
}
struct SourceSpan {std::uint32_t address;std::span<const std::uint8_t> bytes;};
std::span<const SourceSpan> source_spans() noexcept;
enum class Outcome {Declined,Complete,ResumeOriginal,Interrupted};
// resume executes the retained CURRENT owner's continuation, never its entry
// again and never the global sparse dispatch table at an unregistered PC.
struct Calls {
    void* context{};
    bool (*invoke)(void*,katana::runtime::CpuState&,std::uint32_t){};
    bool (*resume)(void*,katana::runtime::CpuState&,std::uint32_t){};
};
struct Statistics {std::uint64_t calls{},declined{},internal_calls{},callbacks{},resumes{},membership_hits{},eligibility_hits{};};
inline thread_local Statistics counts;
bool contains(std::uint32_t) noexcept;
Outcome execute(katana::runtime::CpuState&,const katana::runtime::NativePortImmutableWriteGuard*,Calls,bool indexed=true);
Outcome try_dispatch(katana::runtime::CpuState&,katana::runtime::NativePortAotServices&);
bool retained_source_matches(katana::runtime::CpuState&,const katana::runtime::NativePortImmutableWriteGuard*) noexcept;
bool resume_geometry(katana::runtime::CpuState&,std::uint32_t);
bool resume_pools(katana::runtime::CpuState&,std::uint32_t);
bool resume_eligibility(katana::runtime::CpuState&,std::uint32_t);
#ifdef SARECOMP_COLLISION_WORLD_TEST_COVERAGE
inline thread_local std::array<bool,0x53000/2> visited{};
#endif
}
