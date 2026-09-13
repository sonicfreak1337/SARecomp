#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace sonic::geometry {
// Transient, authored-corner reuse within one polygon only. A model point can
// have different UVs on either side of a seam, so point identity is not a key.
struct CornerIndices {
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> corners;
    std::uint64_t meshes = 0, logical_corners = 0, stored_vertices = 0,
                  verified_reuses = 0;
    static constexpr auto missing = std::numeric_limits<std::uint32_t>::max();

    void begin_mesh() { indices.clear(); }
    void begin_polygon(std::size_t count) { corners.assign(count, missing); }
    bool fits(std::size_t count, std::size_t limit) const noexcept {
        return indices.size() <= limit && count <= limit - indices.size();
    }

    template<class Vertices, class Append>
    bool append_shared(Vertices& vertices, const std::array<std::size_t, 3>& keys,
                       std::size_t limit, Append&& append) {
        if (!fits(3, limit)) return false;
        std::array<std::uint32_t, 3> triangle;
        for (std::size_t i = 0; i < 3; ++i) {
            if (keys[i] >= corners.size()) return false;
            auto& index = corners[keys[i]];
            if (index == missing) {
                const auto next = vertices.size();
                if (next >= limit || !append(i)) return false;
                index = static_cast<std::uint32_t>(next);
            }
            triangle[i] = index;
        }
        indices.insert(indices.end(), triangle.begin(), triangle.end());
        return true;
    }

    bool append_expanded(std::size_t first, std::size_t count, std::size_t limit) {
        if (!fits(count, limit) || first > limit || count > limit - first) return false;
        for (std::size_t i = 0; i < count; ++i)
            indices.push_back(static_cast<std::uint32_t>(first + i));
        return true;
    }
};
}
