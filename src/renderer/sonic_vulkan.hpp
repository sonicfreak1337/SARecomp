#pragma once
#include "katana/runtime/native_port_graphics.hpp"
#include <memory>
#include <vector>

namespace sonic::rendering {
// GPU half of the port-local backend. The pinned command/validation/window
// owner remains shared with D3D11. No Vulkan handles cross the SDK ABI.
class VulkanRenderer final {
public:
    VulkanRenderer(void* window, const katana::runtime::NativePortGraphicsConfig&);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    std::uint64_t create_texture(const katana::runtime::NativePortTextureConfig&);
    void upload_texture(std::uint64_t, const katana::runtime::NativePortImageView&, unsigned level);
    void destroy_texture(std::uint64_t);
    std::uint64_t create_mesh(std::span<const katana::runtime::NativePortVertex>, std::span<const std::uint32_t>);
    void destroy_mesh(std::uint64_t);
    void begin_frame(const katana::runtime::NativePortFrameConfig&);
    void draw(const katana::runtime::NativePortDrawPacket&, std::span<const katana::runtime::NativePortVertex>,
              std::span<const std::uint32_t>, katana::runtime::NativePortPrimitiveTopology,
              std::uint64_t mesh, std::uint64_t texture, katana::runtime::NativePortPixelRect,
              std::span<const std::byte> constants, bool type_two);
    void begin_type_two();
    void resolve_type_two();
    void complete_frame();
    void abort_frame();
    // False means an unavailable swap image, not device loss.
    bool present(katana::runtime::NativePortPixelRect, bool nonblocking,
                 std::span<const std::byte> overlay = {});
    void resize(katana::runtime::NativePortExtent);
    std::vector<std::uint8_t> capture_bgra_bottom_up();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
