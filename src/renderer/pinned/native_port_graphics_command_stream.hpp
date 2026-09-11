#pragma once

#include "katana/runtime/native_port_frame_queue.hpp"
#include "katana/runtime/native_port_graphics.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <variant>

namespace katana::runtime {

// This is an internal wire contract.  It is deliberately independent of the
// public graphics ABI version: a queue frame is only consumed by a matching
// command-stream reader in the same native port build.
inline constexpr std::uint32_t
    native_port_graphics_command_stream_contract_version = 1u;
inline constexpr std::uint32_t
    native_port_graphics_command_stream_max_mip_levels = 32u;
inline constexpr std::uint32_t
    native_port_graphics_command_stream_max_dimension = 16'384u;
inline constexpr std::uint32_t
    native_port_graphics_command_stream_max_vertices = 1'048'576u;
inline constexpr std::uint32_t
    native_port_graphics_command_stream_max_indices = 3'145'728u;

enum class NativePortGraphicsCommandKind : std::uint32_t {
    Show = 1u,
    PollEvents = 2u,
    CreateTexture = 3u,
    UpdateTexture = 4u,
    DestroyTexture = 5u,
    CreateMesh = 6u,
    DestroyMesh = 7u,
    BeginFrame = 8u,
    Draw = 9u,
    FlushType2 = 10u,
    Present = 11u,
    RepeatPresent = 12u,
    PresentImage = 13u,
    Shutdown = 14u,
};

enum class NativePortGraphicsCommandEncodeError : std::uint8_t {
    None,
    InvalidWriter,
    InvalidInput,
    CapacityExceeded,
    QueueRejected,
};

// Every persisted descriptor is a fixed-width POD.  Offsets are relative to
// the beginning of the command payload (the command's queue payload_offset),
// never to a host address or to another frame.
struct NativePortGraphicsCommandPayloadHeader final {
    std::uint32_t contract_version =
        native_port_graphics_command_stream_contract_version;
    std::uint32_t byte_size = 0u;
};

struct NativePortGraphicsImageDescriptor final {
    std::uint32_t extent_width = 0u;
    std::uint32_t extent_height = 0u;
    std::uint32_t format = 0u;
    std::uint32_t stride_bytes = 0u;
    std::uint32_t bottom_up = 0u;
    std::uint32_t display_aspect_numerator = 0u;
    std::uint32_t display_aspect_denominator = 0u;
    std::uint32_t pixels_offset = 0u;
    std::uint32_t pixels_size = 0u;
};

struct NativePortGraphicsTextureConfigDescriptor final {
    NativePortExtent extent;
    NativePortTextureFormat format = NativePortTextureFormat::Rgba8Unorm;
    std::uint32_t mip_levels = 1u;
    std::uint32_t shader_resource = 1u;
    std::uint32_t dynamic = 0u;
    NativePortTextureProvenance provenance;
};

struct NativePortGraphicsCreateTexturePayload final {
    NativePortGraphicsCommandPayloadHeader header;
    NativePortTextureHandle texture;
    NativePortGraphicsTextureConfigDescriptor config;
    std::uint32_t mip_descriptors_offset = 0u;
    std::uint32_t mip_count = 0u;
};

struct NativePortGraphicsUpdateTexturePayload final {
    NativePortGraphicsCommandPayloadHeader header;
    NativePortTextureHandle texture;
    std::uint32_t mip_descriptors_offset = 0u;
    std::uint32_t mip_count = 0u;
};

struct NativePortGraphicsMeshDescriptor final {
    NativePortMeshHandle mesh;
    std::uint32_t topology = 0u;
    std::uint32_t shading = 0u;
    float small_triangle_area_threshold = 0.0f;
    std::uint32_t vertices_offset = 0u;
    std::uint32_t vertices_count = 0u;
    std::uint32_t indices_offset = 0u;
    std::uint32_t indices_count = 0u;
};

struct NativePortGraphicsCreateMeshPayload final {
    NativePortGraphicsCommandPayloadHeader header;
    NativePortGraphicsMeshDescriptor mesh;
};

struct NativePortGraphicsHandlePayload final {
    NativePortGraphicsCommandPayloadHeader header;
    std::uint64_t handle = 0u;
};

// NativePortDrawPacket is intentionally not persisted because it contains two
// spans.  This is its complete fixed-state projection; the two geometry spans
// are represented by offsets/counts in NativePortGraphicsDrawPayload.
struct NativePortGraphicsDrawStateDescriptor final {
    NativePortMeshHandle mesh;
    NativePortVertexSpace vertex_space =
        NativePortVertexSpace::ObjectHomogeneous;
    NativePortDrawClass draw_class = NativePortDrawClass::Opaque;
    NativePortDrawBatch batch;
    NativePortTranslucencyPolicy translucency =
        NativePortTranslucencyPolicy::NotApplicable;
    NativePortType2AutosortContract type2_autosort;
    NativePortDrawDiagnostics diagnostics;
    NativePortInterpolationMode interpolation =
        NativePortInterpolationMode::PerspectiveCorrect;
    NativePortMatrix4x4 transform;
    NativePortMatrix4x4 normal_transform;
    NativePortTextureHandle texture;
    NativePortTextureStage texture_stage = NativePortTextureStage::Disabled;
    NativePortViewportTarget viewport = NativePortViewportTarget::Game;
    NativePortPrimitiveTopology topology =
        NativePortPrimitiveTopology::TriangleList;
    NativePortBlendState blend;
    NativePortDepthState depth;
    NativePortDepthMapping depth_mapping;
    NativePortRasterizerState rasterizer;
    NativePortSamplerState sampler;
    NativePortMaterialState material;
    NativePortLightingState lighting;
    NativePortFogState fog;
    NativePortColorClampState color_clamp;
    NativePortAlphaTestState alpha_test;
};

struct NativePortGraphicsDrawPayload final {
    NativePortGraphicsCommandPayloadHeader header;
    std::uint32_t vertices_offset = 0u;
    std::uint32_t vertices_count = 0u;
    std::uint32_t indices_offset = 0u;
    std::uint32_t indices_count = 0u;
    NativePortGraphicsDrawStateDescriptor state;
};

struct NativePortGraphicsBeginFramePayload final {
    NativePortGraphicsCommandPayloadHeader header;
    NativePortFrameConfig config;
};

struct NativePortGraphicsPresentImagePayload final {
    NativePortGraphicsCommandPayloadHeader header;
    NativePortGraphicsImageDescriptor image;
    std::uint32_t viewport = 0u;
    std::uint32_t fit = 0u;
};

static_assert(std::is_trivially_copyable_v<
              NativePortGraphicsCommandPayloadHeader>);
static_assert(std::is_standard_layout_v<
              NativePortGraphicsCommandPayloadHeader>);
static_assert(std::is_trivially_copyable_v<NativePortGraphicsImageDescriptor>);
static_assert(std::is_standard_layout_v<NativePortGraphicsImageDescriptor>);
static_assert(std::is_trivially_copyable_v<
              NativePortGraphicsTextureConfigDescriptor>);
static_assert(std::is_standard_layout_v<
              NativePortGraphicsTextureConfigDescriptor>);
static_assert(std::is_trivially_copyable_v<
              NativePortGraphicsCreateTexturePayload>);
static_assert(std::is_standard_layout_v<
              NativePortGraphicsCreateTexturePayload>);
static_assert(std::is_trivially_copyable_v<
              NativePortGraphicsUpdateTexturePayload>);
static_assert(std::is_standard_layout_v<
              NativePortGraphicsUpdateTexturePayload>);
static_assert(std::is_trivially_copyable_v<NativePortGraphicsMeshDescriptor>);
static_assert(std::is_standard_layout_v<NativePortGraphicsMeshDescriptor>);
static_assert(std::is_trivially_copyable_v<
              NativePortGraphicsCreateMeshPayload>);
static_assert(std::is_standard_layout_v<
              NativePortGraphicsCreateMeshPayload>);
static_assert(std::is_trivially_copyable_v<NativePortGraphicsHandlePayload>);
static_assert(std::is_standard_layout_v<NativePortGraphicsHandlePayload>);
static_assert(std::is_trivially_copyable_v<
              NativePortGraphicsDrawStateDescriptor>);
static_assert(std::is_standard_layout_v<
              NativePortGraphicsDrawStateDescriptor>);
static_assert(std::is_trivially_copyable_v<NativePortGraphicsDrawPayload>);
static_assert(std::is_standard_layout_v<NativePortGraphicsDrawPayload>);
static_assert(std::is_trivially_copyable_v<NativePortGraphicsBeginFramePayload>);
static_assert(std::is_standard_layout_v<NativePortGraphicsBeginFramePayload>);
static_assert(std::is_trivially_copyable_v<
              NativePortGraphicsPresentImagePayload>);
static_assert(std::is_standard_layout_v<
              NativePortGraphicsPresentImagePayload>);

struct NativePortGraphicsCommandPayloadRequirement final {
    // Serialized command bytes, excluding any alignment padding before the
    // command in the frame arena.
    std::uint32_t bytes = 0u;
    std::uint32_t alignment = 1u;

    [[nodiscard]] std::optional<std::uint32_t>
    capacity_from(const std::uint32_t current_payload_size) const noexcept;
};

[[nodiscard]] std::optional<NativePortGraphicsCommandPayloadRequirement>
native_port_graphics_encoded_create_texture_size(
    const NativePortTextureConfig& config,
    std::span<const NativePortImageView> initial_mip_levels) noexcept;
[[nodiscard]] std::optional<NativePortGraphicsCommandPayloadRequirement>
native_port_graphics_encoded_update_texture_size(
    std::span<const NativePortImageView> mip_levels) noexcept;
[[nodiscard]] std::optional<NativePortGraphicsCommandPayloadRequirement>
native_port_graphics_encoded_create_mesh_size(
    const NativePortMeshConfig& config) noexcept;
[[nodiscard]] std::optional<NativePortGraphicsCommandPayloadRequirement>
native_port_graphics_encoded_draw_size(
    const NativePortDrawPacket& packet) noexcept;
[[nodiscard]] std::optional<NativePortGraphicsCommandPayloadRequirement>
native_port_graphics_encoded_present_image_size(
    const NativePortImageView& image) noexcept;

struct NativePortGraphicsEmptyCommandView final {};

struct NativePortGraphicsImageViewRange final {
    std::span<const NativePortGraphicsImageDescriptor> descriptors;
    std::span<const std::byte> payload;

    [[nodiscard]] std::size_t size() const noexcept {
        return descriptors.size();
    }
    [[nodiscard]] std::optional<NativePortImageView>
    at(const std::size_t index) const noexcept;
    [[nodiscard]] NativePortImageView
    operator[](const std::size_t index) const noexcept {
        const auto result = at(index);
        return result.has_value() ? *result : NativePortImageView{};
    }
};

struct NativePortGraphicsShowView final {};
struct NativePortGraphicsPollEventsView final {};

struct NativePortGraphicsCreateTextureView final {
    NativePortTextureHandle texture;
    NativePortTextureConfig config;
    NativePortGraphicsImageViewRange initial_mip_levels;
};

struct NativePortGraphicsUpdateTextureView final {
    NativePortTextureHandle texture;
    NativePortGraphicsImageViewRange mip_levels;
};

struct NativePortGraphicsDestroyTextureView final {
    NativePortTextureHandle texture;
};

struct NativePortGraphicsCreateMeshView final {
    NativePortMeshHandle mesh;
    std::span<const NativePortVertex> vertices;
    std::span<const std::uint32_t> indices;
    NativePortPrimitiveTopology topology =
        NativePortPrimitiveTopology::TriangleList;
    NativePortShadingMode shading = NativePortShadingMode::Smooth;
    float small_triangle_area_threshold = 0.0f;
};

struct NativePortGraphicsDestroyMeshView final {
    NativePortMeshHandle mesh;
};

struct NativePortGraphicsBeginFrameView final {
    NativePortFrameConfig config;
};

struct NativePortGraphicsDrawView final {
    NativePortDrawPacket packet;
};

struct NativePortGraphicsFlushType2View final {};
struct NativePortGraphicsPresentView final {};
struct NativePortGraphicsRepeatPresentView final {};

struct NativePortGraphicsPresentImageView final {
    NativePortImageView image;
    NativePortViewportTarget viewport = NativePortViewportTarget::Game;
    NativePortImageFit fit = NativePortImageFit::Contain;
};

struct NativePortGraphicsShutdownView final {};

using NativePortGraphicsCommandPayloadView = std::variant<
    NativePortGraphicsShowView,
    NativePortGraphicsPollEventsView,
    NativePortGraphicsCreateTextureView,
    NativePortGraphicsUpdateTextureView,
    NativePortGraphicsDestroyTextureView,
    NativePortGraphicsCreateMeshView,
    NativePortGraphicsDestroyMeshView,
    NativePortGraphicsBeginFrameView,
    NativePortGraphicsDrawView,
    NativePortGraphicsFlushType2View,
    NativePortGraphicsPresentView,
    NativePortGraphicsRepeatPresentView,
    NativePortGraphicsPresentImageView,
    NativePortGraphicsShutdownView>;

struct NativePortGraphicsCommandView final {
    NativePortGraphicsCommandKind kind = NativePortGraphicsCommandKind::Show;
    std::uint32_t ordinal = 0u;
    NativePortGraphicsCommandPayloadView payload;
};

// The writer owns no queue storage.  It is intentionally a thin transaction
// over one write lease: every operation validates and lays out its complete
// command before the first append, then publishes one queue command reference
// after all nested bytes are present.  A failed operation aborts the entire
// lease, so a caller can never publish a partially encoded frame.
class NativePortGraphicsCommandWriter final {
  public:
    explicit NativePortGraphicsCommandWriter(
        NativePortFrameWriteLease& lease) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool failed() const noexcept;
    [[nodiscard]] NativePortGraphicsCommandEncodeError error() const noexcept;
    [[nodiscard]] std::uint32_t encoded_payload_bytes() const noexcept;

    [[nodiscard]] bool show() noexcept;
    [[nodiscard]] bool poll_events() noexcept;
    [[nodiscard]] bool create_texture(
        NativePortTextureHandle texture,
        const NativePortTextureConfig& config,
        std::span<const NativePortImageView> initial_mip_levels) noexcept;
    [[nodiscard]] bool create_texture(
        NativePortTextureHandle texture,
        const NativePortTextureConfig& config,
        const NativePortImageView* initial_pixels) noexcept;
    [[nodiscard]] bool update_texture(
        NativePortTextureHandle texture,
        std::span<const NativePortImageView> mip_levels) noexcept;
    [[nodiscard]] bool update_texture(
        NativePortTextureHandle texture,
        const NativePortImageView& pixels) noexcept;
    [[nodiscard]] bool destroy_texture(
        NativePortTextureHandle texture) noexcept;
    [[nodiscard]] bool create_mesh(NativePortMeshHandle mesh,
                                   const NativePortMeshConfig& config) noexcept;
    [[nodiscard]] bool destroy_mesh(NativePortMeshHandle mesh) noexcept;
    [[nodiscard]] bool begin_frame(
        const NativePortFrameConfig& config = {}) noexcept;
    [[nodiscard]] bool draw(const NativePortDrawPacket& packet) noexcept;
    [[nodiscard]] bool flush_type2_translucency() noexcept;
    [[nodiscard]] bool present() noexcept;
    [[nodiscard]] bool repeat_present() noexcept;
    [[nodiscard]] bool present_image(
        const NativePortImageView& image,
        NativePortViewportTarget viewport = NativePortViewportTarget::Game,
        NativePortImageFit fit = NativePortImageFit::Contain) noexcept;
    [[nodiscard]] bool shutdown() noexcept;

    // The writer is also the sole convenient visibility boundary for a
    // command frame.  publish() does not sort, merge, or otherwise inspect
    // commands; the queue's append order is the render order.
    [[nodiscard]] bool publish() noexcept;
    void abort() noexcept;

  private:
    [[nodiscard]] bool reject(
        NativePortGraphicsCommandEncodeError error) noexcept;
    [[nodiscard]] bool append_empty(NativePortGraphicsCommandKind kind) noexcept;

    NativePortFrameWriteLease* lease_ = nullptr;
    NativePortGraphicsCommandEncodeError error_ =
        NativePortGraphicsCommandEncodeError::None;
    std::uint32_t encoded_payload_bytes_ = 0u;
    bool failed_ = false;
};

// The reader performs a complete, fail-closed validation pass in its
// constructor.  next()/at() can therefore never expose a valid prefix of a
// malformed frame.  All spans in returned views point directly into the
// ReadLease arena and become invalid as soon as that lease is completed.
class NativePortGraphicsCommandReader final {
  public:
    explicit NativePortGraphicsCommandReader(
        const NativePortFrameReadLease& lease) noexcept;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t position() const noexcept;
    [[nodiscard]] std::optional<NativePortGraphicsCommandView>
    next() noexcept;
    [[nodiscard]] std::optional<NativePortGraphicsCommandView>
    at(std::size_t ordinal) const noexcept;
    void reset() noexcept;

  private:
    [[nodiscard]] std::optional<NativePortGraphicsCommandView>
    decode(std::size_t ordinal) const noexcept;
    [[nodiscard]] bool validate_all() noexcept;

    const NativePortFrameReadLease* lease_ = nullptr;
    std::span<const NativePortFrameCommand> commands_;
    std::span<const std::byte> payload_;
    std::size_t position_ = 0u;
    bool valid_ = false;
};

// Short aliases make the seam easy to use from title-side integration code
// while retaining the explicit Writer/Reader names for diagnostics and tests.
using NativePortGraphicsCommandEncoder = NativePortGraphicsCommandWriter;
using NativePortGraphicsCommandDecoder = NativePortGraphicsCommandReader;

} // namespace katana::runtime
