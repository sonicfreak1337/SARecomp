#pragma once
#include "katana/runtime/runtime.hpp"
#include "sonic_internal_diagnostics.hpp"
#include "sonic_native_cpu_policy.hpp"
#include <array>
#include <span>
namespace katana::runtime {class NativePortImmutableWriteGuard;class NativePortAotServices;}
namespace sonic::movement {
inline constexpr std::uint32_t entry=0x8C073018u,body=0x8C0730A0u,end=0x8C074214u;
inline bool enabled() noexcept {
    static const bool on=[] {const auto* v=std::getenv("SARECOMP_NATIVE_MOVEMENT");
        return v && std::strcmp(v,"1")==0 && native_cpu::enabled("SARECOMP_NATIVE_MOVEMENT");}();
    return on && !diagnostics::runtime_checks_enabled();
}
struct Calls {void* context{};bool (*invoke)(void*,katana::runtime::CpuState&,std::uint32_t){};};
enum class Outcome {Declined,Complete,ResumeOriginal,Interrupted};
struct SourceSpan {std::uint32_t address;std::span<const std::uint8_t> bytes;};
std::span<const SourceSpan> source_spans() noexcept;
bool retained_source_matches(katana::runtime::CpuState&,const katana::runtime::NativePortImmutableWriteGuard*) noexcept;
struct Statistics {std::uint64_t calls{},declined{},callbacks{},slow_accesses{};};
inline thread_local Statistics counts;
inline thread_local std::uint32_t return_site{};
#ifdef SARECOMP_MOVEMENT_TEST_COVERAGE
inline thread_local std::array<bool,(end-entry)/2> visited{};
#endif
Outcome execute(katana::runtime::CpuState&,const katana::runtime::NativePortImmutableWriteGuard*,Calls);
Outcome try_dispatch(katana::runtime::CpuState&,katana::runtime::NativePortAotServices&);
}
