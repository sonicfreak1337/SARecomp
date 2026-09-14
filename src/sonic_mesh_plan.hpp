#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <span>
#include <unordered_map>
#include <vector>
namespace katana::runtime { struct CpuState; }
namespace sonic::geometry {
// Only the descriptor, authored topology and UVs are cached. No transformed
// vertex, normal, color, material, raw guest pointer or GPU resource is retained.
struct MeshPlanRequest {
    std::uint32_t model{},mesh{},point_count{},corner_budget{};
    std::array<std::uint8_t,24> descriptor{};
    bool operator==(const MeshPlanRequest&)const=default;
};
struct MeshPlanReader {
    const void* context{};
    std::span<const std::uint8_t> (*bytes)(const void*,std::uint32_t,std::size_t) noexcept{};
};
struct MeshPlanTriangle {
    std::array<std::uint16_t,3> points{};
    std::array<std::uint32_t,3> corners{};
};
struct MeshPlanPolygon {
    std::uint32_t first_triangle{},triangle_count{},first_corner{},corner_count{};
};
struct MeshPlan {
    MeshPlanRequest request;
    std::uint32_t fpscr_mode{},host_mode{},corner_count{};
    std::vector<std::uint8_t> stream_bytes,uv_bytes;
    std::vector<MeshPlanTriangle> triangles;
    std::vector<MeshPlanPolygon> polygons;
    std::vector<std::array<float,2>> uvs;
    [[nodiscard]] std::size_t allocation_bytes()const noexcept;
};
struct MeshPlanStats {
    std::uint64_t lookups{},hits{},misses{},builds{},changes{},declines{},evictions{};
    std::uint64_t verified_triangles{},verified_uvs{};
};
class MeshPlanCache final {
public:
    MeshPlanCache():MeshPlanCache(16u*1024u*1024u,512u){}
    explicit MeshPlanCache(std::size_t bytes,std::size_t entries=512u)
        :byte_limit_(bytes),entry_limit_(entries){}
    MeshPlanCache(const MeshPlanCache&)=delete;
    MeshPlanCache& operator=(const MeshPlanCache&)=delete;
    MeshPlanCache(MeshPlanCache&&) noexcept;
    MeshPlanCache& operator=(MeshPlanCache&&) noexcept;
    // The returned plan is borrowed only until the next get/clear. The caller
    // must supply a live, unobserved direct reader and run without guest writes
    // or callbacks through topology-to-submit. Otherwise use the retained path.
    [[nodiscard]] const MeshPlan* get(const MeshPlanRequest&,MeshPlanReader,
        const katana::runtime::CpuState&) noexcept;
    void clear() noexcept;
    MeshPlanStats stats;
private:
    struct Entry { MeshPlan plan;std::list<std::uint64_t>::iterator age; };
    std::unordered_map<std::uint64_t,Entry> entries_;
    std::list<std::uint64_t> ages_;
    std::size_t byte_limit_,entry_limit_,bytes_{};
    void erase(std::unordered_map<std::uint64_t,Entry>::iterator) noexcept;
};
}
