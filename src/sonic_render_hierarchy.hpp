#pragma once
#include "katana/runtime/runtime.hpp"
#include "sonic_internal_diagnostics.hpp"
#include "sonic_native_cpu_policy.hpp"
#include <array>
#include <span>
namespace katana::runtime {class NativePortImmutableWriteGuard;class NativePortAotServices;}
namespace sonic::render_hierarchy {
inline constexpr std::uint32_t entry=0x8C040784u;
inline constexpr std::uint32_t blended_entry=0x8C041A2Eu;
inline constexpr std::uint32_t rigid_entry=0x8C036BC0u;
inline constexpr std::uint32_t morph_entry=0x8C040880u;
inline thread_local unsigned resume_depth{};
inline thread_local std::uint32_t return_site{};
inline bool local_sources_enabled() noexcept {
    static const bool on=[] {const auto* v=std::getenv("SARECOMP_NATIVE_RENDER_LOCAL_PROOFS");
        return !v || std::strcmp(v,"0")!=0;}();
    return on;
}
inline bool enabled() noexcept {
    static const bool on=native_cpu::enabled("SARECOMP_NATIVE_RENDER_HIERARCHY");
    return on && !resume_depth && !diagnostics::runtime_checks_enabled();
}
inline bool root_sources_enabled() noexcept {
    static const bool on=native_cpu::enabled("SARECOMP_NATIVE_RENDER_ROOT_PROOFS");
    return on;
}
inline bool rigid_enabled() noexcept {
    static const bool on=native_cpu::enabled("SARECOMP_NATIVE_RIGID_HIERARCHY");
    return on && enabled();
}
struct SourceSpan {std::uint32_t address;std::span<const std::uint8_t> bytes;};
inline bool morph_enabled() noexcept {
    static const bool on=[] {const auto* v=std::getenv("SARECOMP_NATIVE_MORPH_HIERARCHY");
        return v && std::strcmp(v,"1")==0 && native_cpu::enabled("SARECOMP_NATIVE_MORPH_HIERARCHY");}();
    return on && enabled();
}
std::span<const SourceSpan> source_spans() noexcept;
enum class Outcome {Declined,Complete,ResumeOriginal,Interrupted};
struct Calls {
    void* context{};
    bool (*invoke)(void*,katana::runtime::CpuState&,std::uint32_t){};
    bool (*resume)(void*,katana::runtime::CpuState&,std::uint32_t,std::uint32_t){};
};
struct Statistics {std::uint64_t calls{},declined{},internal_calls{},callbacks{},resumes{},rigid_calls{},morph_calls{};};
inline thread_local Statistics counts;
bool contains(std::uint32_t) noexcept;
Outcome execute(katana::runtime::CpuState&,const katana::runtime::NativePortImmutableWriteGuard*,Calls);
Outcome try_dispatch(katana::runtime::CpuState&,katana::runtime::NativePortAotServices&);
bool retained_source_matches(katana::runtime::CpuState&,const katana::runtime::NativePortImmutableWriteGuard*) noexcept;
bool resume_original(katana::runtime::CpuState&,std::uint32_t);
#ifdef SARECOMP_RENDER_HIERARCHY_TEST_COVERAGE
inline thread_local std::array<bool,0x10000/2> visited{};
#endif
}
