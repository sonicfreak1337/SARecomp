#pragma once

#include "katana/runtime/native_port_graphics.hpp"

#include <array>
#include <cmath>

namespace katana::runtime::detail {

// Flycast's full-texture edge contraction, restricted to transient UI quads.
// Validate the entire primitive before writing the caller-owned scratch array.
[[nodiscard]] inline bool prepare_ui_texture_edges(
    const NativePortDrawPacket& packet,
    const NativePortExtent texture_extent,
    const std::uint32_t ui_render_height,
    std::array<NativePortVertex, 4u>& destination) noexcept {
    if (ui_render_height <= 480u || texture_extent.width == 0u ||
        texture_extent.height == 0u || !packet.texture || packet.mesh ||
        packet.texture_stage != NativePortTextureStage::RequiredResolved ||
        !packet.indices.empty() || packet.vertices.size() != 4u ||
        packet.topology != NativePortPrimitiveTopology::TriangleStrip ||
        packet.viewport != NativePortViewportTarget::Ui ||
        packet.batch.semantic == NativePortDrawBatchClass::Scene3D ||
        packet.vertex_space != NativePortVertexSpace::PvrScreenReciprocal ||
        packet.depth_mapping.mode != NativePortDepthCoordinateMode::ReciprocalPositive ||
        packet.material.texture_coordinates != NativePortTextureCoordinateSource::Vertex ||
        packet.material.bump_mapping)
        return false;
    const auto endpoint = [](const float value) noexcept {
        return value == 0.0f || (value > 0.995f && value <= 1.0f);
    };
    const float depth = packet.vertices.front().depth_coordinate;
    if (!std::isfinite(depth)) return false;
    for (const auto& vertex : packet.vertices) {
        if (vertex.depth_coordinate != depth ||
            !endpoint(vertex.texture_coordinate[0]) ||
            !endpoint(vertex.texture_coordinate[1]))
            return false;
    }
    const float width = static_cast<float>(texture_extent.width);
    const float height = static_cast<float>(texture_extent.height);
    for (std::size_t index = 0u; index < destination.size(); ++index) {
        destination[index] = packet.vertices[index];
        auto& uv = destination[index].texture_coordinate;
        // NJS 255/256 endpoints satisfy the same >.995 condition as Flycast.
        uv[0] = (0.5f + (uv[0] > 0.995f ? width - 1.0f : 0.0f)) / width;
        uv[1] = (0.5f + (uv[1] > 0.995f ? height - 1.0f : 0.0f)) / height;
    }
    return true;
}

} // namespace katana::runtime::detail
