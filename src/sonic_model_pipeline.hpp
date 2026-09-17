#pragma once
#include "katana/runtime/runtime.hpp"
#include "katana/runtime/block_guards.hpp"
#include <array>
#include <span>
#include <vector>

namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::model_pipeline {
using Vector = std::array<float,3>;
struct Capture {
    katana::runtime::CpuState* cpu{};
    katana::runtime::DirectLinearMemoryGuard memory{};
    std::uint32_t model{},points_address{},normals_address{},count{},output_address{},gbr{};
    // One authenticated writable capability for this model's disjoint output.
    // Cleared with active before any retained guest callback.
    std::uint8_t* projected_bytes{};
    std::vector<Vector> points,normals; // includes each leaf's authored extra read
};
// Synchronous title-model scope only. Never retained across an arbitrary guest
// callback, frame submission, scene change, or another model owner.
inline thread_local const Capture* active = nullptr;
struct Statistics { std::uint64_t calls{},declined{},culled{},points{},normal_reuses{},draw_reuses{},direct_outputs{}; };
inline thread_local Statistics counts;
inline const Statistics& statistics() noexcept{return counts;}
bool enabled() noexcept;
struct Calls {
    void* context{};
    bool (*invoke)(void*,katana::runtime::CpuState&,std::uint32_t){};
};
enum class Outcome { Declined, Complete, Interrupted };
// Runs the authenticated PAL 037098/037108 owners. Declined is mutation-free;
// an interrupted child preserves its frontier and must never restart Original.
Outcome execute(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*, Calls);
// Only the synchronous native transform uses this span. Keep each record's
// fourth word intact; publish_projection completes GBR+60 and write accounting.
std::span<std::uint8_t> projection_output(katana::runtime::CpuState&,
    std::uint32_t model,std::uint32_t count,std::uint32_t output) noexcept;
bool publish_projection(katana::runtime::CpuState&,
    std::uint32_t model,std::uint32_t count,std::uint32_t output) noexcept;
inline void note_normal_reuse() noexcept{++counts.normal_reuses;}
inline void note_draw_reuse() noexcept{++counts.draw_reuses;}
}
