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
bool submission_enabled() noexcept;
// Borrowed only from a live native hierarchy operation. A real guest call
// revokes this object before it runs; it is never a frame/asset identity cache.
struct SharedOperation {
    katana::runtime::CpuState* cpu{};
    const katana::runtime::NativePortImmutableWriteGuard* immutable{};
    katana::runtime::DirectLinearMemoryGuard read{},write{};
    void* context{};
    bool (*allows_write)(void*,std::uint32_t,std::uint32_t) noexcept{};
    bool sources_proven{},intact{};
    void revoke() noexcept {sources_proven=false;intact=false;}
};
bool source_overlap(std::uint32_t physical,std::uint32_t size) noexcept;
enum class ClosedCall { Declined, Complete, Interrupted };
struct Calls {
    void* context{};
    bool (*invoke)(void*,katana::runtime::CpuState&,std::uint32_t){};
    // Complete promises no guest callback, mapping/observer change or writes
    // outside the complete model footprint admitted by execute().
    ClosedCall (*closed)(void*,katana::runtime::CpuState&,std::uint32_t,SharedOperation*){};
};
// Synchronous child of an admitted model operation. The parent authenticates
// sources and preserves the borrowed mapping until this callback-free leaf returns.
ClosedCall visibility(katana::runtime::CpuState&,SharedOperation&,float horizontal_extra);

enum class Outcome { Declined, Complete, Interrupted };
// Runs the authenticated PAL 03700C/037098/037108 owners. Declined is mutation-free;
// an interrupted child preserves its frontier and must never restart Original.
Outcome execute(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*, Calls,SharedOperation* = nullptr);
// Only the synchronous native transform uses this span. Keep each record's
// fourth word intact; publish_projection completes GBR+60 and write accounting.
std::span<std::uint8_t> projection_output(katana::runtime::CpuState&,
    std::uint32_t model,std::uint32_t count,std::uint32_t output) noexcept;
bool publish_projection(katana::runtime::CpuState&,
    std::uint32_t model,std::uint32_t count,std::uint32_t output) noexcept;
inline void note_normal_reuse() noexcept{++counts.normal_reuses;}
inline void note_draw_reuse() noexcept{++counts.draw_reuses;}
}
