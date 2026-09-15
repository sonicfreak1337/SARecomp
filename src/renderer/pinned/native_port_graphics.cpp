#include "katana/runtime/native_port_graphics.hpp"
#include "katana/runtime/native_port_platform.hpp"
#include "katana/runtime/native_port_telemetry.hpp"
#include "native_port_graphics_command_stream.hpp"
#include "native_port_ui_texture_edges.hpp"
#include "../renderer_selection.hpp"
#include "../sonic_vulkan.hpp"
#include "../../sonic_startup.hpp"
#include "../../sonic_input.hpp"
#include "../../sonic_presentation.hpp"
#include "../../sonic_render_completion.hpp"
#include "../../sonic_internal_diagnostics.hpp"
#include "../sonic_motion.hpp"

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include "../window_fullscreen.hpp"

#include <commdlg.h>
#include <timeapi.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <array>
#else
#include <condition_variable>
#include <unistd.h>
#include <SDL3/SDL.h>
#include <codecvt>
#include <locale>
#include "../../linux/deck_detection.hpp"
#endif

namespace katana::runtime {
namespace {

constexpr std::uint32_t maximum_graphics_dimension = 16'384u;
constexpr std::size_t maximum_graphics_title_bytes = 1'024u;
constexpr std::uint32_t maximum_native_frame_rate_hz = 1'000u;
constexpr std::uint64_t nanoseconds_per_second = 1'000'000'000u;
constexpr std::uint32_t minimum_dynamic_vertex_buffer_bytes = 1u << 20u;
constexpr std::uint32_t minimum_dynamic_index_buffer_bytes = 1u << 18u;
// Match the bounded per-pixel OIT depth used by the validated Flycast oracle
// profile.  Sixteen is insufficient for Sonic Adventure's Character Select
// model, whose overlapping translucent strips legitimately exceed that depth.
constexpr std::uint64_t graphics_digest_seed = 0xCBF29CE484222325ull;
constexpr std::size_t maximum_development_state_path_bytes = 32'768u;

// The producer-owned desktop host and consumer-owned Win32 window exchange
// only fixed atomics here. Menu clicks never enter the render command queue,
// and the consumer never reads mutable host pacing state directly.
struct NativePortRuntimeOptionsBridge final {
    std::atomic<std::uint32_t> simulation_rate_hz{60u};
    std::atomic<std::uint32_t> presentation_rate_hz{60u};
    std::atomic<std::uint32_t> maximum_presentation_rate_hz{144u};
    std::atomic<std::uint32_t> requested_presentation_rate_hz{60u};
    std::atomic<std::uint64_t> simulation_frames{0u};
    std::atomic<bool> frame_pacing_enabled{true};
    std::atomic<bool> independent_presentation_enabled{false};
    std::atomic<bool> performance_overlay_enabled{false};
    // One consumer-to-producer SPSC mailbox. The render/window owner writes
    // the payload before release-publishing `state_request_publication`; the
    // simulation owner copies it before release-publishing `consumed`.
    std::atomic<std::uint64_t> state_request_publication{0u};
    std::atomic<std::uint64_t> state_request_consumed{0u};
    NativePortDevelopmentStateOperation state_request_operation =
        NativePortDevelopmentStateOperation::Save;
    std::uint32_t state_request_path_bytes = 0u;
    std::array<char, maximum_development_state_path_bytes>
        state_request_path{};
};

enum class NativePortBackendPresentOutcome : std::uint8_t {
    Presented,
    Occluded,
    Deferred,
};

constexpr std::array<std::uint32_t, 4u>
    runtime_presentation_rate_choices{60u, 90u, 120u, 144u};

#ifdef _WIN32
constexpr UINT runtime_menu_output_fps = 0x7100u;
constexpr UINT runtime_menu_simulation_fps = 0x7101u;
constexpr UINT runtime_menu_performance_overlay = 0x7102u;
constexpr UINT runtime_menu_save_state = 0x7103u;
constexpr UINT runtime_menu_load_state = 0x7104u;
constexpr UINT runtime_menu_quick_save_state = 0x7105u;
constexpr UINT runtime_menu_quick_load_state = 0x7106u;
constexpr UINT runtime_menu_keyboard_controls = 0x7107u;
constexpr UINT runtime_menu_rate_first = 0x7110u;
constexpr UINT runtime_menu_rate_last =
    runtime_menu_rate_first +
    static_cast<UINT>(runtime_presentation_rate_choices.size()) - 1u;

constexpr std::uint32_t performance_overlay_width = 128u;
constexpr std::uint32_t performance_overlay_height = 32u;

struct alignas(16) PerformanceOverlayConstants final {
    std::array<std::array<std::uint32_t, 4u>,
               performance_overlay_height>
        rows{};
};

static_assert(sizeof(PerformanceOverlayConstants) % 16u == 0u);

[[nodiscard]] constexpr std::array<std::uint8_t, 7u>
performance_overlay_glyph(const char value) noexcept {
    switch (value) {
    case '0': return {14u, 17u, 19u, 21u, 25u, 17u, 14u};
    case '1': return {4u, 12u, 4u, 4u, 4u, 4u, 14u};
    case '2': return {14u, 17u, 1u, 2u, 4u, 8u, 31u};
    case '3': return {30u, 1u, 1u, 14u, 1u, 1u, 30u};
    case '4': return {2u, 6u, 10u, 18u, 31u, 2u, 2u};
    case '5': return {31u, 16u, 16u, 30u, 1u, 1u, 30u};
    case '6': return {14u, 16u, 16u, 30u, 17u, 17u, 14u};
    case '7': return {31u, 1u, 2u, 4u, 8u, 8u, 8u};
    case '8': return {14u, 17u, 17u, 14u, 17u, 17u, 14u};
    case '9': return {14u, 17u, 17u, 15u, 1u, 1u, 14u};
    case 'F': return {31u, 16u, 16u, 30u, 16u, 16u, 16u};
    case 'P': return {30u, 17u, 17u, 30u, 16u, 16u, 16u};
    case 'S': return {15u, 16u, 16u, 14u, 1u, 1u, 30u};
    case 'I': return {31u, 4u, 4u, 4u, 4u, 4u, 31u};
    case 'M': return {17u, 27u, 21u, 21u, 17u, 17u, 17u};
    case '.':
    case ',': return {0u, 0u, 0u, 0u, 0u, 4u, 4u};
    default: return {};
    }
}

void set_performance_overlay_pixel(
    PerformanceOverlayConstants& constants,
    const std::uint32_t x,
    const std::uint32_t y) noexcept {
    if (x >= performance_overlay_width ||
        y >= performance_overlay_height)
        return;
    constants.rows[y][x >> 5u] |= std::uint32_t{1u} << (x & 31u);
}

void rasterize_performance_overlay_text(
    PerformanceOverlayConstants& constants,
    const std::string_view text,
    const std::uint32_t origin_y) noexcept {
    constexpr std::uint32_t scale = 2u;
    constexpr std::uint32_t glyph_width = 5u;
    constexpr std::uint32_t glyph_height = 7u;
    constexpr std::uint32_t glyph_advance = 6u * scale;
    std::uint32_t origin_x = 4u;
    for (const auto character : text) {
        const auto glyph = performance_overlay_glyph(character);
        for (std::uint32_t row = 0u; row < glyph_height; ++row) {
            for (std::uint32_t column = 0u; column < glyph_width; ++column) {
                if ((glyph[row] &
                     (std::uint8_t{1u} << (glyph_width - 1u - column))) == 0u)
                    continue;
                for (std::uint32_t dy = 0u; dy < scale; ++dy)
                    for (std::uint32_t dx = 0u; dx < scale; ++dx)
                        set_performance_overlay_pixel(
                            constants,
                            origin_x + column * scale + dx,
                            origin_y + row * scale + dy);
            }
        }
        origin_x += glyph_advance;
        if (origin_x >= performance_overlay_width) break;
    }
}
#endif

[[nodiscard]] constexpr bool runtime_presentation_rate_choice(
    const std::uint32_t rate_hz) noexcept {
    return std::find(runtime_presentation_rate_choices.begin(),
                     runtime_presentation_rate_choices.end(),
                     rate_hz) != runtime_presentation_rate_choices.end();
}

void saturating_atomic_increment(
    std::atomic<std::uint64_t>& value) noexcept {
    auto current = value.load(std::memory_order_relaxed);
    while (current != std::numeric_limits<std::uint64_t>::max() &&
           !value.compare_exchange_weak(current,
                                        current + 1u,
                                        std::memory_order_release,
                                        std::memory_order_relaxed)) {
    }
}

[[nodiscard]] constexpr std::uint64_t mix_graphics_digest(
    std::uint64_t digest,
    const std::uint64_t value) noexcept {
    digest ^= value + 0x9E3779B97F4A7C15ull + (digest << 6u) +
        (digest >> 2u);
    return std::rotl(digest, 27) * 0x94D049BB133111EBull;
}

[[nodiscard]] std::uint64_t graphics_diagnostic_process_id() noexcept {
#ifdef _WIN32
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

[[nodiscard]] bool background_test_mode_requested() noexcept {
    static const bool requested = [] {
        const auto* const value = std::getenv("KATANA_PORT_BACKGROUND_TEST");
        return value != nullptr && std::string_view(value) == "1";
    }();
    return requested;
}

[[nodiscard]] bool present_failure_test_requested() noexcept {
    const auto* const value =
        std::getenv("KATANA_PORT_TEST_PRESENT_FAILURE");
    return background_test_mode_requested() && value != nullptr &&
           std::string_view(value) == "1";
}

[[nodiscard]] bool present_busy_test_requested() noexcept {
    const auto* const value =
        std::getenv("KATANA_PORT_TEST_PRESENT_BUSY");
    return background_test_mode_requested() && value != nullptr &&
           std::string_view(value) == "1";
}

[[nodiscard]] std::string copy_validated_graphics_title(
    const std::string_view title) {
    if (title.empty() || title.size() > maximum_graphics_title_bytes)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidConfig, 0u, "title");
    return std::string(title);
}

[[nodiscard]] std::string copy_development_state_directory(
    const std::string_view directory) {
    if (directory.size() >= maximum_development_state_path_bytes)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidConfig,
            0u,
            "development-state-directory");
    return std::string(directory);
}

[[nodiscard]] bool valid_extent(const NativePortExtent extent) noexcept {
    return extent.width != 0u && extent.height != 0u &&
           extent.width <= maximum_graphics_dimension &&
           extent.height <= maximum_graphics_dimension;
}

[[nodiscard]] bool valid_aspect(const NativePortAspectRatio aspect) noexcept {
    return aspect.numerator != 0u && aspect.denominator != 0u &&
           aspect.numerator <= 65'535u && aspect.denominator <= 65'535u;
}

[[nodiscard]] bool valid_viewport_policy(
    const NativePortViewportPolicy policy) noexcept {
    return policy == NativePortViewportPolicy::FullRender ||
           policy == NativePortViewportPolicy::FitAspect;
}

[[nodiscard]] bool valid_camera_policy(
    const NativePortCameraAspectPolicy policy) noexcept {
    return policy == NativePortCameraAspectPolicy::GameViewport ||
           policy == NativePortCameraAspectPolicy::OutputSurface ||
           policy == NativePortCameraAspectPolicy::Explicit;
}

[[nodiscard]] bool valid_texture_format(
    const NativePortTextureFormat format) noexcept {
    return format == NativePortTextureFormat::Rgba8Unorm ||
           format == NativePortTextureFormat::Bgra8Unorm;
}

[[nodiscard]] bool valid_viewport_target(
    const NativePortViewportTarget target) noexcept {
    return target == NativePortViewportTarget::Game ||
           target == NativePortViewportTarget::Ui;
}

[[nodiscard]] bool valid_image_fit(const NativePortImageFit fit) noexcept {
    return fit == NativePortImageFit::Contain ||
           fit == NativePortImageFit::Cover ||
           fit == NativePortImageFit::Stretch;
}

[[nodiscard]] bool valid_topology(
    const NativePortPrimitiveTopology topology) noexcept {
    return topology == NativePortPrimitiveTopology::PointList ||
           topology == NativePortPrimitiveTopology::LineList ||
           topology == NativePortPrimitiveTopology::LineStrip ||
           topology == NativePortPrimitiveTopology::TriangleList ||
           topology == NativePortPrimitiveTopology::TriangleStrip ||
           false;
}

template <std::size_t Size>
[[nodiscard]] bool finite_array(
    const std::array<float, Size>& values) noexcept {
    return std::all_of(values.begin(), values.end(), [](const float value) {
        return std::isfinite(value);
    });
}

[[nodiscard]] bool valid_blend_factor(
    const NativePortBlendFactor factor) noexcept {
    return factor == NativePortBlendFactor::Zero ||
           factor == NativePortBlendFactor::One ||
           factor == NativePortBlendFactor::SourceColor ||
           factor == NativePortBlendFactor::InverseSourceColor ||
           factor == NativePortBlendFactor::DestinationColor ||
           factor == NativePortBlendFactor::InverseDestinationColor ||
           factor == NativePortBlendFactor::SourceAlpha ||
           factor == NativePortBlendFactor::InverseSourceAlpha ||
           factor == NativePortBlendFactor::DestinationAlpha ||
           factor == NativePortBlendFactor::InverseDestinationAlpha;
}

[[nodiscard]] bool valid_blend_operation(
    const NativePortBlendOperation operation) noexcept {
    return operation == NativePortBlendOperation::Add ||
           operation == NativePortBlendOperation::Subtract ||
           operation == NativePortBlendOperation::ReverseSubtract ||
           operation == NativePortBlendOperation::Minimum ||
           operation == NativePortBlendOperation::Maximum;
}

[[nodiscard]] bool valid_blend_source(
    const NativePortBlendSource source) noexcept {
    return source == NativePortBlendSource::Fragment ||
           source == NativePortBlendSource::SecondaryAccumulation;
}

[[nodiscard]] bool valid_blend_destination(
    const NativePortBlendDestination destination) noexcept {
    return destination == NativePortBlendDestination::Framebuffer ||
           destination == NativePortBlendDestination::SecondaryAccumulation;
}

[[nodiscard]] bool valid_blend(
    const NativePortBlendState& blend) noexcept {
    return valid_blend_factor(blend.source_color) &&
           valid_blend_factor(blend.destination_color) &&
           valid_blend_operation(blend.color_operation) &&
           valid_blend_factor(blend.source_alpha) &&
           valid_blend_factor(blend.destination_alpha) &&
           valid_blend_operation(blend.alpha_operation) &&
           valid_blend_source(blend.source_buffer) &&
           valid_blend_destination(blend.destination_buffer) &&
           (blend.color_write_mask & 0xF0u) == 0u;
}

[[nodiscard]] constexpr std::uint32_t pack_type2_blend_factors(
    const NativePortBlendState& blend) noexcept {
    return static_cast<std::uint32_t>(blend.source_color) |
           (static_cast<std::uint32_t>(blend.destination_color) << 4u) |
           (static_cast<std::uint32_t>(blend.source_alpha) << 8u) |
           (static_cast<std::uint32_t>(blend.destination_alpha) << 12u);
}

[[nodiscard]] constexpr std::uint32_t pack_type2_blend_buffers(
    const NativePortBlendState& blend) noexcept {
    return (blend.source_buffer == NativePortBlendSource::SecondaryAccumulation
                ? 1u
                : 0u) |
           (blend.destination_buffer ==
                    NativePortBlendDestination::SecondaryAccumulation
                ? 2u
                : 0u);
}

[[nodiscard]] bool valid_compare(
    const NativePortCompareOperation compare) noexcept {
    return compare == NativePortCompareOperation::Never ||
           compare == NativePortCompareOperation::Less ||
           compare == NativePortCompareOperation::Equal ||
           compare == NativePortCompareOperation::LessEqual ||
           compare == NativePortCompareOperation::Greater ||
           compare == NativePortCompareOperation::NotEqual ||
           compare == NativePortCompareOperation::GreaterEqual ||
           compare == NativePortCompareOperation::Always;
}

[[nodiscard]] bool valid_depth(
    const NativePortDepthState& depth) noexcept {
    return valid_compare(depth.compare) &&
           (!depth.write_enabled || depth.test_enabled);
}

[[nodiscard]] bool valid_vertex_space(
    const NativePortVertexSpace space) noexcept {
    return space == NativePortVertexSpace::ObjectHomogeneous ||
           space == NativePortVertexSpace::PvrScreenReciprocal ||
           space == NativePortVertexSpace::ClipHomogeneous;
}

[[nodiscard]] bool valid_draw_class(
    const NativePortDrawClass draw_class) noexcept {
    return draw_class == NativePortDrawClass::Opaque ||
           draw_class == NativePortDrawClass::PunchThrough ||
           draw_class == NativePortDrawClass::Translucent ||
           draw_class == NativePortDrawClass::Overlay;
}

[[nodiscard]] bool valid_draw_batch_class(
    const NativePortDrawBatchClass batch) noexcept {
    return batch == NativePortDrawBatchClass::Scene3D ||
           batch == NativePortDrawBatchClass::GameOverlay ||
           batch == NativePortDrawBatchClass::UiOverlay ||
           batch == NativePortDrawBatchClass::FontOverlay;
}

[[nodiscard]] bool valid_translucency_policy(
    const NativePortTranslucencyPolicy policy) noexcept {
    return policy == NativePortTranslucencyPolicy::NotApplicable ||
           policy == NativePortTranslucencyPolicy::AuthoredUnsorted ||
           policy == NativePortTranslucencyPolicy::StableDepthSorted ||
           policy == NativePortTranslucencyPolicy::Type2AutoSorted;
}

[[nodiscard]] bool valid_type2_autosort_contract(
    const NativePortType2AutosortContract& contract) noexcept {
    return contract.contract_version ==
               native_port_type2_autosort_contract_version &&
           contract.presort == 0u;
}

[[nodiscard]] bool valid_draw_logical_use(
    const NativePortDrawLogicalUse use) noexcept {
    return use == NativePortDrawLogicalUse::Unspecified ||
           use == NativePortDrawLogicalUse::SceneMaterial ||
           use == NativePortDrawLogicalUse::Interface ||
           use == NativePortDrawLogicalUse::Font ||
           use == NativePortDrawLogicalUse::Presentation;
}

[[nodiscard]] bool valid_draw_origin_kind(
    const NativePortDrawOriginKind origin) noexcept {
    return origin == NativePortDrawOriginKind::Unspecified ||
           origin == NativePortDrawOriginKind::Immediate ||
           origin == NativePortDrawOriginKind::ModelMesh ||
           origin == NativePortDrawOriginKind::Sprite ||
           origin == NativePortDrawOriginKind::Font ||
           origin == NativePortDrawOriginKind::Movie ||
           origin == NativePortDrawOriginKind::Presentation;
}

[[nodiscard]] bool valid_draw_intent(
    const NativePortDrawIntent intent) noexcept {
    return intent == NativePortDrawIntent::Unspecified ||
           intent == NativePortDrawIntent::SceneObject ||
           intent == NativePortDrawIntent::Shadow ||
           intent == NativePortDrawIntent::Interface ||
           intent == NativePortDrawIntent::Font ||
           intent == NativePortDrawIntent::Movie ||
           intent == NativePortDrawIntent::Presentation;
}

[[nodiscard]] bool valid_texture_resolver_kind(
    const NativePortTextureResolverKind resolver) noexcept {
    return resolver == NativePortTextureResolverKind::Unspecified ||
           resolver == NativePortTextureResolverKind::NativeDescriptor ||
           resolver == NativePortTextureResolverKind::DynamicSurface ||
           resolver == NativePortTextureResolverKind::CachedArchive ||
           resolver == NativePortTextureResolverKind::ExactArchiveNames ||
           resolver == NativePortTextureResolverKind::ExactArchiveLayouts ||
           resolver == NativePortTextureResolverKind::PartialArchive ||
           resolver == NativePortTextureResolverKind::Checkpoint ||
           resolver == NativePortTextureResolverKind::RegisteredTexture ||
           resolver == NativePortTextureResolverKind::IdentityBoundOverride;
}

[[nodiscard]] bool valid_texture_writer_kind(
    const NativePortTextureBindingWriterKind writer) noexcept {
    return writer == NativePortTextureBindingWriterKind::Unspecified ||
           writer == NativePortTextureBindingWriterKind::TextureListBind ||
           writer ==
               NativePortTextureBindingWriterKind::TextureNumberSelect ||
           writer ==
               NativePortTextureBindingWriterKind::RegisteredTextureSelect ||
           writer ==
               NativePortTextureBindingWriterKind::IdentityBoundOverride;
}

[[nodiscard]] bool valid_draw_diagnostics(
    const NativePortDrawDiagnostics& diagnostics) noexcept {
    if (!diagnostics.enabled) return true;
    const auto& binding = diagnostics.texture_binding;
    if (!valid_draw_logical_use(diagnostics.logical_use) ||
        !valid_draw_origin_kind(diagnostics.origin) ||
        !valid_draw_intent(diagnostics.intent) ||
        !valid_texture_resolver_kind(binding.resolver) ||
        !valid_texture_writer_kind(binding.last_writer) ||
        diagnostics.material_identity_bound !=
            (diagnostics.material_identity != 0u) ||
        diagnostics.origin_identity_bound !=
            (diagnostics.origin_identity != 0u) ||
        diagnostics.model_identity_bound !=
            (diagnostics.model_identity != 0u) ||
        (!diagnostics.texture_list_index_bound &&
         diagnostics.texture_list_index != 0u) ||
        (!diagnostics.mesh_index_bound && diagnostics.mesh_index != 0u) ||
        (!diagnostics.primitive_index_bound &&
         diagnostics.primitive_index != 0u) ||
        binding.last_writer_bound !=
            (binding.last_writer_identity != 0u &&
             binding.last_writer_sequence != 0u &&
             binding.last_writer !=
                 NativePortTextureBindingWriterKind::Unspecified) ||
        binding.expected_asset_bound !=
            (binding.expected_asset_identity != 0u) ||
        binding.resolved_asset_bound !=
            (binding.resolved_asset_identity != 0u))
        return false;
    if (!binding.texture_list_bound &&
        (binding.texture_list_identity != 0u ||
         binding.texture_list_count != 0u))
        return false;
    if (binding.texture_list_bound &&
        (binding.texture_list_identity == 0u ||
         binding.texture_list_count == 0u))
        return false;
    if (binding.texture_list_epoch_bound !=
        (binding.texture_list_bound && binding.texture_list_epoch != 0u))
        return false;
    if (binding.texture_list_bound &&
        diagnostics.texture_list_index_bound &&
        diagnostics.texture_list_index >= binding.texture_list_count)
        return false;
    return true;
}

[[nodiscard]] bool valid_interpolation(
    const NativePortInterpolationMode interpolation) noexcept {
    return interpolation ==
               NativePortInterpolationMode::PerspectiveCorrect ||
           interpolation ==
               NativePortInterpolationMode::PvrScreenGouraud;
}

[[nodiscard]] bool valid_depth_mapping(
    const NativePortDepthMapping& mapping) noexcept {
    if (mapping.mode != NativePortDepthCoordinateMode::ClipSpace &&
        mapping.mode != NativePortDepthCoordinateMode::ReciprocalPositive &&
        mapping.mode !=
            NativePortDepthCoordinateMode::ReciprocalPositiveHomogeneousClip)
        return false;
    return std::isfinite(mapping.reciprocal_scale) &&
           std::isfinite(mapping.logarithm_divisor) &&
           mapping.reciprocal_scale > 0.0f &&
           mapping.logarithm_divisor > 0.0f;
}

[[nodiscard]] bool reciprocal_depth_mode(
    const NativePortDepthCoordinateMode mode) noexcept {
    return mode == NativePortDepthCoordinateMode::ReciprocalPositive ||
           mode ==
               NativePortDepthCoordinateMode::ReciprocalPositiveHomogeneousClip;
}

[[nodiscard]] bool valid_depth_buffer_convention(
    const NativePortDepthBufferConvention convention) noexcept {
    return convention == NativePortDepthBufferConvention::Forward ||
           convention ==
               NativePortDepthBufferConvention::ReciprocalPositive;
}

[[nodiscard]] bool compatible_depth_contract(
    const NativePortDepthBufferConvention convention,
    const NativePortDepthState& depth,
    const NativePortDepthMapping& mapping) noexcept {
    if (!depth.test_enabled) return true;
    const bool reciprocal = reciprocal_depth_mode(mapping.mode);
    if (reciprocal !=
        (convention ==
         NativePortDepthBufferConvention::ReciprocalPositive))
        return false;
    switch (depth.compare) {
    case NativePortCompareOperation::Less:
    case NativePortCompareOperation::LessEqual:
        return !reciprocal;
    case NativePortCompareOperation::Greater:
    case NativePortCompareOperation::GreaterEqual:
        return reciprocal;
    case NativePortCompareOperation::Never:
    case NativePortCompareOperation::Equal:
    case NativePortCompareOperation::NotEqual:
    case NativePortCompareOperation::Always:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_cull(const NativePortCullMode cull) noexcept {
    return cull == NativePortCullMode::None ||
           cull == NativePortCullMode::Front ||
           cull == NativePortCullMode::Back;
}

[[nodiscard]] bool valid_fill(const NativePortFillMode fill) noexcept {
    return fill == NativePortFillMode::Solid ||
           fill == NativePortFillMode::Wireframe;
}

[[nodiscard]] bool valid_shading(const NativePortShadingMode shading) noexcept {
    return shading == NativePortShadingMode::Smooth ||
           shading == NativePortShadingMode::FlatLastVertex;
}

[[nodiscard]] bool valid_triangle_area_space(
    const NativePortTriangleAreaSpace space) noexcept {
    return space == NativePortTriangleAreaSpace::Submitted ||
           space ==
               NativePortTriangleAreaSpace::LogicalViewportAfterTransform;
}

[[nodiscard]] bool valid_rasterizer(
    const NativePortRasterizerState& rasterizer) noexcept {
    const auto& clip = rasterizer.logical_clip;
    const bool valid_clip_mode =
        clip.mode == NativePortLogicalClipMode::Disabled ||
        clip.mode == NativePortLogicalClipMode::Inside ||
        clip.mode == NativePortLogicalClipMode::Outside;
    const bool valid_clip =
        valid_clip_mode && finite_array(clip.bounds) &&
        (clip.mode == NativePortLogicalClipMode::Disabled
             ? clip.logical_extent == NativePortExtent{} &&
                   clip.bounds == std::array<float, 4u>{}
             : valid_extent(clip.logical_extent) &&
                   clip.bounds[0] >= 0.0f && clip.bounds[1] >= 0.0f &&
                   clip.bounds[2] <=
                       static_cast<float>(clip.logical_extent.width) &&
                   clip.bounds[3] <=
                       static_cast<float>(clip.logical_extent.height) &&
                   clip.bounds[0] < clip.bounds[2] &&
                   clip.bounds[1] < clip.bounds[3]);
    return valid_cull(rasterizer.cull) && valid_fill(rasterizer.fill) &&
           valid_shading(rasterizer.shading) &&
           valid_triangle_area_space(rasterizer.small_triangle_area_space) &&
           std::isfinite(rasterizer.small_triangle_area_threshold) &&
           rasterizer.small_triangle_area_threshold >= 0.0f &&
            (rasterizer.small_triangle_area_threshold == 0.0f ||
             rasterizer.small_triangle_area_space ==
                 NativePortTriangleAreaSpace::Submitted ||
             valid_extent(rasterizer.small_triangle_reference_extent)) &&
           valid_clip;
}

[[nodiscard]] bool valid_filter(const NativePortTextureFilter filter) noexcept {
    return filter == NativePortTextureFilter::Point ||
           filter == NativePortTextureFilter::Bilinear ||
           filter == NativePortTextureFilter::Trilinear ||
           filter == NativePortTextureFilter::Anisotropic;
}

[[nodiscard]] bool valid_texture_stage(
    const NativePortTextureStage stage) noexcept {
    return stage == NativePortTextureStage::Disabled ||
           stage == NativePortTextureStage::RequiredResolved;
}

[[nodiscard]] bool valid_address(
    const NativePortTextureAddress address) noexcept {
    return address == NativePortTextureAddress::Clamp ||
           address == NativePortTextureAddress::Wrap ||
           address == NativePortTextureAddress::Mirror;
}

[[nodiscard]] bool valid_sampler(
    const NativePortSamplerState& sampler) noexcept {
    return valid_filter(sampler.filter) && valid_address(sampler.address_u) &&
           valid_address(sampler.address_v) &&
           valid_address(sampler.address_w) &&
           std::isfinite(sampler.mip_lod_bias) &&
           std::isfinite(sampler.minimum_lod) &&
           std::isfinite(sampler.maximum_lod) &&
           sampler.mip_lod_bias >= -16.0f &&
           sampler.mip_lod_bias <= 15.99f && sampler.minimum_lod >= 0.0f &&
           sampler.maximum_lod >= sampler.minimum_lod &&
           sampler.maximum_lod <= 65'536.0f &&
           sampler.maximum_anisotropy >= 1u &&
           sampler.maximum_anisotropy <= 16u &&
           (sampler.filter == NativePortTextureFilter::Anisotropic ||
            sampler.maximum_anisotropy == 1u);
}

[[nodiscard]] bool valid_texture_combine(
    const NativePortTextureCombineMode combine) noexcept {
    return combine == NativePortTextureCombineMode::Modulate ||
           combine == NativePortTextureCombineMode::Replace ||
           combine == NativePortTextureCombineMode::Decal ||
           combine == NativePortTextureCombineMode::Add ||
           combine == NativePortTextureCombineMode::ModulateTextureAlpha;
}

[[nodiscard]] bool valid_texture_coordinates(
    const NativePortTextureCoordinateSource source) noexcept {
    return source == NativePortTextureCoordinateSource::Vertex ||
           source == NativePortTextureCoordinateSource::NormalSphere;
}

[[nodiscard]] bool valid_material(
    const NativePortMaterialState& material) noexcept {
    return finite_array(material.diffuse) && finite_array(material.ambient) &&
           finite_array(material.specular) && finite_array(material.emission) &&
           std::isfinite(material.specular_power) &&
           material.specular_power >= 0.0f &&
           material.specular_power <= 65'536.0f &&
           std::isfinite(material.mipmapped_pass_weight) &&
           material.mipmapped_pass_weight >= 0.0f &&
           material.mipmapped_pass_weight <= 1.0f &&
           valid_texture_combine(material.texture_combine) &&
           valid_texture_coordinates(material.texture_coordinates) &&
           (!material.specular_enabled || material.lighting_enabled);
}

[[nodiscard]] bool valid_lighting(
    const NativePortLightingState& lighting) noexcept {
    if (lighting.light_count > lighting.lights.size() ||
        !finite_array(lighting.ambient))
        return false;
    for (std::size_t index = 0u; index < lighting.light_count; ++index) {
        const auto& light = lighting.lights[index];
        if (!finite_array(light.direction) || !finite_array(light.color))
            return false;
        const auto length_squared =
            light.direction[0] * light.direction[0] +
            light.direction[1] * light.direction[1] +
            light.direction[2] * light.direction[2];
        if (!std::isfinite(length_squared) || length_squared <= 0.0f)
            return false;
    }
    return true;
}

[[nodiscard]] bool valid_fog_mode(const NativePortFogMode mode) noexcept {
    return mode == NativePortFogMode::Disabled ||
           mode == NativePortFogMode::VertexFactor ||
           mode == NativePortFogMode::Linear ||
           mode == NativePortFogMode::Exponential ||
           mode == NativePortFogMode::ExponentialSquared ||
           mode == NativePortFogMode::LookupTable ||
           mode == NativePortFogMode::LookupTablePrimary;
}

[[nodiscard]] bool valid_fog(const NativePortFogState& fog) noexcept {
    if (!valid_fog_mode(fog.mode) || !finite_array(fog.color) ||
        !std::isfinite(fog.start) || !std::isfinite(fog.end) ||
        !std::isfinite(fog.density) || fog.density < 0.0f)
        return false;
    if (fog.mode == NativePortFogMode::Linear && fog.end <= fog.start)
        return false;
    if (fog.mode != NativePortFogMode::LookupTable &&
        fog.mode != NativePortFogMode::LookupTablePrimary)
        return true;
    if (fog.density <= 0.0f || !finite_array(fog.lookup_table)) return false;
    return std::all_of(
        fog.lookup_table.begin(), fog.lookup_table.end(),
        [](const float value) { return value >= 0.0f && value <= 1.0f; });
}

[[nodiscard]] bool valid_color_clamp(
    const NativePortColorClampState& color_clamp) noexcept {
    if (!finite_array(color_clamp.minimum) ||
        !finite_array(color_clamp.maximum))
        return false;
    for (std::size_t component = 0u;
         component < color_clamp.minimum.size(); ++component) {
        if (color_clamp.minimum[component] < 0.0f ||
            color_clamp.maximum[component] > 1.0f ||
            color_clamp.minimum[component] > color_clamp.maximum[component])
            return false;
    }
    return true;
}

[[nodiscard]] bool valid_alpha_test(
    const NativePortAlphaTestState& alpha_test) noexcept {
    using Mode = NativePortAlphaTestState::Mode;
    if (!valid_compare(alpha_test.compare) ||
        !std::isfinite(alpha_test.reference) ||
        alpha_test.reference < 0.0f || alpha_test.reference > 1.0f ||
        (alpha_test.mode != Mode::FloatingPoint &&
         alpha_test.mode != Mode::Quantized8BitForceOpaque))
        return false;
    if (alpha_test.mode == Mode::Quantized8BitForceOpaque) {
        const float canonical_reference =
            static_cast<float>(alpha_test.reference_8bit) / 255.0f;
        return alpha_test.enabled &&
               alpha_test.compare ==
                   NativePortCompareOperation::GreaterEqual &&
               alpha_test.reference == canonical_reference;
    }
    return alpha_test.reference_8bit == 0u;
}

[[nodiscard]] bool identity_transform(
    const NativePortMatrix4x4& transform) noexcept {
    static constexpr std::array<float, 16u> identity{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f};
    return transform.values == identity;
}

[[nodiscard]] bool positive_affine_w_transform(
    const NativePortMatrix4x4& transform) noexcept {
    return transform.values[3u] == 0.0f &&
           transform.values[7u] == 0.0f &&
           transform.values[11u] == 0.0f &&
           transform.values[15u] > 0.0f;
}

[[nodiscard]] bool compatible_vertex_contract(
    const NativePortDrawPacket& packet) noexcept {
    switch (packet.vertex_space) {
    case NativePortVertexSpace::ObjectHomogeneous:
        return packet.depth_mapping.mode !=
                   NativePortDepthCoordinateMode::ReciprocalPositive &&
               packet.interpolation ==
                   NativePortInterpolationMode::PerspectiveCorrect &&
               packet.rasterizer.depth_clip_enabled;
    case NativePortVertexSpace::PvrScreenReciprocal:
        return packet.depth_mapping.mode ==
                   NativePortDepthCoordinateMode::ReciprocalPositive &&
               packet.interpolation ==
                   NativePortInterpolationMode::PvrScreenGouraud &&
               positive_affine_w_transform(packet.transform) &&
               packet.rasterizer.small_triangle_area_space ==
                   NativePortTriangleAreaSpace::Submitted &&
               !packet.rasterizer.depth_clip_enabled;
    case NativePortVertexSpace::ClipHomogeneous:
        return packet.depth_mapping.mode !=
                   NativePortDepthCoordinateMode::ReciprocalPositive &&
               packet.interpolation ==
                   NativePortInterpolationMode::PerspectiveCorrect &&
               identity_transform(packet.transform);
    }
    return false;
}

[[nodiscard]] bool compatible_draw_class_contract(
    const NativePortDrawPacket& packet) noexcept {
    using AlphaMode = NativePortAlphaTestState::Mode;
    const bool quantized_punch =
        packet.alpha_test.mode == AlphaMode::Quantized8BitForceOpaque;
    const bool ordinary_blend_targets =
        packet.blend.source_buffer == NativePortBlendSource::Fragment &&
        packet.blend.destination_buffer ==
            NativePortBlendDestination::Framebuffer;
    switch (packet.draw_class) {
    case NativePortDrawClass::Opaque:
        return !quantized_punch &&
               ordinary_blend_targets &&
               packet.translucency ==
                   NativePortTranslucencyPolicy::NotApplicable;
    case NativePortDrawClass::PunchThrough:
        return ordinary_blend_targets && quantized_punch &&
               packet.alpha_test.enabled &&
               packet.alpha_test.compare ==
                   NativePortCompareOperation::GreaterEqual &&
               packet.depth.test_enabled && packet.depth.write_enabled &&
               packet.depth.compare ==
                   NativePortCompareOperation::GreaterEqual &&
               reciprocal_depth_mode(packet.depth_mapping.mode) &&
               packet.translucency ==
                   NativePortTranslucencyPolicy::NotApplicable;
    case NativePortDrawClass::Translucent:
        if (quantized_punch || !packet.blend.enabled ||
            packet.translucency ==
                NativePortTranslucencyPolicy::NotApplicable)
            return false;
        if (packet.translucency ==
            NativePortTranslucencyPolicy::Type2AutoSorted) {
            // PVR Type-2 stores the fixed-function blend factors with each
            // fragment and applies them after per-pixel depth sorting.  The
            // native resolver implements every public factor, but keeps the
            // operation and write-mask contract deliberately narrow.
            return packet.depth.test_enabled &&
                   packet.depth.compare ==
                       NativePortCompareOperation::GreaterEqual &&
                   reciprocal_depth_mode(packet.depth_mapping.mode) &&
                   packet.blend.color_operation ==
                       NativePortBlendOperation::Add &&
                   packet.blend.alpha_operation ==
                       NativePortBlendOperation::Add &&
                   packet.blend.color_write_mask == 0x0Fu &&
                   valid_type2_autosort_contract(packet.type2_autosort);
        }
        if (packet.translucency ==
            NativePortTranslucencyPolicy::StableDepthSorted)
            return ordinary_blend_targets && packet.depth.test_enabled &&
                   !packet.depth.write_enabled &&
                   packet.depth.compare ==
                       NativePortCompareOperation::GreaterEqual &&
                   reciprocal_depth_mode(packet.depth_mapping.mode);
        return ordinary_blend_targets;
    case NativePortDrawClass::Overlay:
        return ordinary_blend_targets && !quantized_punch &&
               !packet.depth.test_enabled &&
               !packet.depth.write_enabled &&
               packet.translucency ==
                   NativePortTranslucencyPolicy::NotApplicable &&
               packet.batch.semantic != NativePortDrawBatchClass::Scene3D;
    }
    return false;
}

void validate_graphics_config(const NativePortGraphicsConfig& config) {
    const auto maximum_draw_payload =
        static_cast<std::uint64_t>(sizeof(NativePortGraphicsDrawPayload)) +
        alignof(NativePortVertex) - 1u +
        static_cast<std::uint64_t>(config.maximum_transient_vertices) *
            sizeof(NativePortVertex) +
        alignof(std::uint32_t) - 1u +
        static_cast<std::uint64_t>(config.maximum_transient_indices) *
            sizeof(std::uint32_t);
    const auto configured_present_payload =
        static_cast<std::uint64_t>(
            sizeof(NativePortGraphicsPresentImagePayload)) +
        static_cast<std::uint64_t>(config.output_extent.width) *
            config.output_extent.height * 4u;
    if (config.contract_version != native_port_graphics_contract_version ||
        config.title.empty() ||
        config.title.size() > maximum_graphics_title_bytes ||
        !valid_extent(config.output_extent) ||
        !valid_extent(config.render_extent) ||
        !valid_viewport_policy(config.game_viewport.policy) ||
        !valid_viewport_policy(config.ui_viewport.policy) ||
        !valid_camera_policy(config.camera_aspect_policy) ||
        !valid_aspect(config.game_viewport.aspect) ||
        !valid_aspect(config.ui_viewport.aspect) ||
        !valid_aspect(config.explicit_camera_aspect) ||
        config.maximum_textures == 0u ||
        config.maximum_textures > 1'048'576u ||
        config.maximum_texture_bytes < 4u ||
        config.maximum_texture_bytes > 16ull * 1024u * 1024u * 1024u ||
        config.maximum_meshes == 0u ||
        config.maximum_meshes > 1'048'576u ||
        config.maximum_mesh_bytes < sizeof(NativePortVertex) ||
        config.maximum_mesh_bytes > 16ull * 1024u * 1024u * 1024u ||
        config.maximum_transient_vertices < 3u ||
        config.maximum_transient_indices < 3u ||
        config.maximum_pipeline_states == 0u ||
        config.maximum_pipeline_states > 65'536u ||
        config.maximum_type2_fragment_nodes == 0u ||
        config.maximum_type2_fragment_nodes > 67'108'864u ||
        config.maximum_type2_fragments_per_pixel == 0u ||
        config.maximum_type2_fragments_per_pixel > 256u ||
        config.maximum_render_commands_per_frame < 2u ||
        config.maximum_render_commands_per_frame > 1'048'576u ||
        config.maximum_render_payload_bytes_per_frame == 0u ||
        config.maximum_render_resource_payload_bytes_per_command == 0u ||
        config.maximum_render_resource_payload_bytes_per_command >
            config.maximum_render_payload_bytes_per_frame ||
        maximum_draw_payload >
            config.maximum_render_payload_bytes_per_frame ||
        configured_present_payload >
            config.maximum_render_payload_bytes_per_frame ||
        config.maximum_transient_vertices >
            native_port_graphics_command_stream_max_vertices ||
        config.maximum_transient_indices >
            native_port_graphics_command_stream_max_indices ||
        config.maximum_transient_vertices >
            std::numeric_limits<std::uint32_t>::max() /
                sizeof(NativePortVertex) ||
        config.maximum_transient_indices >
            std::numeric_limits<std::uint32_t>::max() /
                sizeof(std::uint32_t))
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidConfig, 0u, "config");
}

void validate_frame_pacing_config(
    const NativePortFramePacingConfig& config) {
    if (config.contract_version !=
            native_port_frame_pacing_contract_version ||
        config.simulation_rate_hz == 0u ||
        config.simulation_rate_hz > maximum_native_frame_rate_hz ||
        config.presentation_rate_hz < config.simulation_rate_hz ||
        config.presentation_rate_hz > maximum_native_frame_rate_hz ||
        config.maximum_presentation_rate_hz <
            config.presentation_rate_hz ||
        config.maximum_presentation_rate_hz >
            maximum_native_frame_rate_hz)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidConfig,
            0u,
            "frame-pacing-config");
}

[[nodiscard]] NativePortGraphicsConfig desktop_graphics_config(
    NativePortGraphicsConfig graphics,
    const NativePortFramePacingConfig& pacing) {
    validate_frame_pacing_config(pacing);
    // Software deadlines own cadence when pacing is active.  Combining them
    // with an unrelated monitor-vblank interval would double-throttle on
    // 50/60/120/144-Hz displays and make title time display-dependent.
    if (pacing.enabled) graphics.synchronize_present = sonic::presentation::settings().vsync==1;
    return graphics;
}

void advance_frame_deadline(std::uint64_t& deadline,
                            std::uint64_t& remainder,
                            const std::uint32_t rate_hz) noexcept {
    const auto whole = nanoseconds_per_second / rate_hz;
    const auto fraction = nanoseconds_per_second % rate_hz;
    if (deadline > std::numeric_limits<std::uint64_t>::max() - whole) {
        deadline = std::numeric_limits<std::uint64_t>::max();
        remainder = 0u;
        return;
    }
    deadline += whole;
    remainder += fraction;
    if (remainder >= rate_hz) {
        remainder -= rate_hz;
        if (deadline != std::numeric_limits<std::uint64_t>::max()) ++deadline;
    }
}

void wait_until_monotonic_nanoseconds(const std::uint64_t deadline) {
    const auto maximum_count = static_cast<std::uint64_t>(
        std::numeric_limits<std::chrono::nanoseconds::rep>::max());
    const auto bounded = std::min(deadline, maximum_count);
    std::this_thread::sleep_until(
        std::chrono::steady_clock::time_point(
            std::chrono::nanoseconds(static_cast<
                std::chrono::nanoseconds::rep>(bounded))));
}

void acquire_frame_pacing_timer_resolution(
    const NativePortFramePacingConfig& config) {
#ifdef _WIN32
    if (config.enabled) {
        constexpr UINT requested_period_milliseconds = 1u;
        const auto result = timeBeginPeriod(requested_period_milliseconds);
        if (result != TIMERR_NOERROR)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::UnsupportedHost,
                static_cast<std::uint32_t>(result),
                "frame-pacing-timer-resolution");
    }
#else
    static_cast<void>(config);
#endif
}

void release_frame_pacing_timer_resolution(
    const NativePortFramePacingConfig& config) noexcept {
#ifdef _WIN32
    if (config.enabled) {
        constexpr UINT requested_period_milliseconds = 1u;
        static_cast<void>(timeEndPeriod(requested_period_milliseconds));
    }
#else
    static_cast<void>(config);
#endif
}

[[nodiscard]] NativePortPixelRect fit_aspect(
    const NativePortPixelRect outer,
    const NativePortAspectRatio aspect) noexcept {
    if (outer.width == 0u || outer.height == 0u ||
        !valid_aspect(aspect))
        return {};
    const auto available_scaled =
        static_cast<std::uint64_t>(outer.width) * aspect.denominator;
    const auto desired_scaled =
        static_cast<std::uint64_t>(outer.height) * aspect.numerator;
    NativePortPixelRect result = outer;
    if (available_scaled > desired_scaled) {
        result.width = static_cast<std::uint32_t>(
            std::max<std::uint64_t>(
                1u,
                static_cast<std::uint64_t>(outer.height) *
                    aspect.numerator / aspect.denominator));
        result.x += (outer.width - result.width) / 2u;
    } else if (available_scaled < desired_scaled) {
        result.height = static_cast<std::uint32_t>(
            std::max<std::uint64_t>(
                1u,
                static_cast<std::uint64_t>(outer.width) *
                    aspect.denominator / aspect.numerator));
        result.y += (outer.height - result.height) / 2u;
    }
    return result;
}

[[nodiscard]] NativePortPixelRect configured_viewport(
    const NativePortExtent render,
    const NativePortViewportConfig config) noexcept {
    const NativePortPixelRect full{0u, 0u, render.width, render.height};
    return config.policy == NativePortViewportPolicy::FullRender
               ? full
               : fit_aspect(full, config.aspect);
}

[[nodiscard]] NativePortExtent texture_mip_extent(
    const NativePortExtent extent,
    const std::uint32_t level) noexcept {
    return {std::max(extent.width >> level, 1u),
            std::max(extent.height >> level, 1u)};
}

[[nodiscard]] std::uint64_t checked_texture_bytes(
    const NativePortTextureConfig& config) {
    if (!valid_extent(config.extent) || config.mip_levels == 0u ||
        config.mip_levels > static_cast<std::uint32_t>(
                                std::bit_width(std::max(
                                    config.extent.width,
                                    config.extent.height))) ||
        (config.dynamic && config.mip_levels != 1u))
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidResource, 0u, "texture-extent");
    const bool zero_content_identity = std::ranges::all_of(
        config.provenance.content_sha256,
        [](const std::uint8_t byte) { return byte == 0u; });
    const bool zero_decoded_payload_identity = std::ranges::all_of(
        config.provenance.decoded_rgba8_sha256,
        [](const std::uint8_t byte) { return byte == 0u; });
    if (config.provenance.content_identity_bound
            ? config.provenance.generation == 0u || zero_content_identity
            : config.provenance.generation != 0u ||
                  config.provenance.archive_ordinal != 0u ||
                  !zero_content_identity ||
                  config.provenance.global_index_bound ||
                  config.provenance.global_index != 0u)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidResource,
            0u,
            "texture-provenance");
    if (config.provenance.global_index_bound == false &&
        config.provenance.global_index != 0u)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidResource,
            0u,
            "texture-global-index");
    if (config.provenance.decoded_payload_identity_bound
            ? zero_decoded_payload_identity ||
                  config.provenance.decoded_extent != config.extent ||
                  config.provenance.decoded_mip_levels != config.mip_levels
            : !zero_decoded_payload_identity ||
                  config.provenance.source_pixel_format != 0u ||
                  config.provenance.source_data_format != 0u ||
                  config.provenance.source_memory_layout ||
                  config.provenance.decoded_extent != NativePortExtent{} ||
                  config.provenance.decoded_mip_levels != 0u)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidResource,
            0u,
            "texture-decoded-payload-provenance");
    std::uint64_t result = 0u;
    for (std::uint32_t level = 0u; level < config.mip_levels; ++level) {
        const auto extent = texture_mip_extent(config.extent, level);
        const auto level_bytes =
            static_cast<std::uint64_t>(extent.width) * extent.height * 4u;
        if (level_bytes > std::numeric_limits<std::uint64_t>::max() - result)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidResource,
                0u,
                "texture-byte-size");
        result += level_bytes;
    }
    return result;
}

void validate_image(const NativePortImageView& image,
                    const NativePortExtent expected,
                    const NativePortTextureFormat expected_format) {
    if (!valid_extent(image.extent) || image.extent.width != expected.width ||
        image.extent.height != expected.height ||
        !valid_texture_format(image.format) ||
        !valid_texture_format(expected_format) ||
        image.format != expected_format)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidResource, 0u, "image-layout");
    const auto row_bytes = static_cast<std::uint64_t>(image.extent.width) * 4u;
    if (image.stride_bytes < row_bytes)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidResource, 0u, "image-stride");
    const auto required =
        static_cast<std::uint64_t>(image.stride_bytes) *
            (image.extent.height - 1u) +
        row_bytes;
    if (required > image.pixels.size())
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidResource, 0u, "image-bytes");
    const bool implicit_aspect = image.display_aspect_numerator == 0u &&
                                 image.display_aspect_denominator == 0u;
    const bool explicit_aspect = valid_aspect(
        {image.display_aspect_numerator,
         image.display_aspect_denominator});
    if (!implicit_aspect && !explicit_aspect)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidResource,
            0u,
            "image-display-aspect");
}

void validate_texture_images(
    const NativePortTextureConfig& config,
    const std::span<const NativePortImageView> images) {
    if (images.empty()) return;
    if (images.size() != config.mip_levels)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidResource,
            0u,
            "texture-mip-count");
    for (std::uint32_t level = 0u; level < config.mip_levels; ++level)
        validate_image(images[level],
                       texture_mip_extent(config.extent, level),
                       config.format);
}

void saturating_increment(std::uint64_t& value) noexcept {
    if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

void saturating_add(std::uint64_t& value, const std::uint64_t addition) noexcept {
    value = addition > std::numeric_limits<std::uint64_t>::max() - value
                ? std::numeric_limits<std::uint64_t>::max()
                : value + addition;
}

} // namespace

#ifdef _WIN32

namespace {

using Microsoft::WRL::ComPtr;

extern const wchar_t native_graphics_window_class[];

constexpr std::uint32_t draw_flag_vertex_color = 1u << 0u;
constexpr std::uint32_t draw_flag_secondary_color = 1u << 1u;
constexpr std::uint32_t draw_flag_lighting = 1u << 2u;
constexpr std::uint32_t draw_flag_specular = 1u << 3u;
constexpr std::uint32_t draw_flag_normal_sphere = 1u << 4u;
constexpr std::uint32_t draw_flag_reciprocal_depth = 1u << 5u;
constexpr std::uint32_t draw_flag_primary_alpha = 1u << 6u;
constexpr std::uint32_t draw_flag_texture_alpha = 1u << 7u;
constexpr std::uint32_t draw_texture_combine_shift = 8u;
constexpr std::uint32_t draw_alpha_test_enabled = 1u << 8u;
constexpr std::uint32_t draw_alpha_test_quantized_8bit = 1u << 9u;
constexpr std::uint32_t draw_alpha_test_reference_shift = 16u;
constexpr std::uint32_t draw_flag_homogeneous_reciprocal_clip = 1u << 16u;
constexpr std::uint32_t draw_flag_texture_present = 1u << 17u;
constexpr std::uint32_t draw_flag_pvr_screen_gouraud = 1u << 18u;
constexpr std::uint32_t draw_flag_clip_homogeneous = 1u << 19u;
constexpr std::uint32_t draw_flag_color_clamp = 1u << 20u;
constexpr std::uint32_t draw_flag_type_two_autosort_capture = 1u << 21u;
constexpr std::uint32_t draw_flag_flat_last_vertex = 1u << 22u;
constexpr std::uint32_t draw_flag_bump_mapping = 1u << 23u;
[[nodiscard]] ComPtr<ID3DBlob> compile_shader(const char* entry,
                                               const char* target);
[[nodiscard]] ComPtr<ID3DBlob> compile_type_two_shader(
    const char* entry,
    const char* target,
    std::uint32_t maximum_type2_fragments_per_pixel);
[[nodiscard]] ComPtr<ID3DBlob> compile_shader_source(
    const char* source,
    std::size_t source_size,
    const char* entry,
    const char* target,
    const char* source_name);
[[nodiscard]] std::wstring utf8_to_wide(std::string_view value);
[[nodiscard]] std::string wide_to_utf8(std::wstring_view value);
[[nodiscard]] DXGI_FORMAT texture_format(
    NativePortTextureFormat format) noexcept;
[[nodiscard]] D3D11_PRIMITIVE_TOPOLOGY primitive_topology(
    NativePortPrimitiveTopology topology) noexcept;
[[nodiscard]] D3D11_BLEND blend_factor(
    NativePortBlendFactor factor) noexcept;
[[nodiscard]] D3D11_BLEND_OP blend_operation(
    NativePortBlendOperation operation) noexcept;
[[nodiscard]] D3D11_COMPARISON_FUNC comparison_function(
    NativePortCompareOperation compare) noexcept;
[[nodiscard]] D3D11_TEXTURE_ADDRESS_MODE texture_address_mode(
    NativePortTextureAddress address) noexcept;
[[nodiscard]] D3D11_FILTER texture_filter(
    NativePortTextureFilter filter) noexcept;
[[nodiscard]] UINT next_buffer_capacity(UINT required) noexcept;

} // namespace

class NativePortGraphicsBackend final {
  private:
    struct GeometryCapabilities final {
        bool positive_depth_coordinates = true;
        bool nonnegative_fog_coordinates = true;
        bool nonzero_normals = true;
        bool unit_position_w = true;
        bool positive_position_w = true;
        float depth_coordinate_min =
            std::numeric_limits<float>::infinity();
        float depth_coordinate_max =
            -std::numeric_limits<float>::infinity();
    };

    struct GeometryPreprocessStats final {
        std::size_t input_vertices = 0u;
        std::size_t input_indices = 0u;
        std::size_t input_triangles = 0u;
        std::size_t output_vertices = 0u;
        std::size_t output_triangles = 0u;
        std::size_t rejected_triangles = 0u;
        bool applied = false;
    };

    struct GraphicsBreadcrumbRecord final {
        std::uint64_t frame = 0u;
        std::uint64_t draw = 0u;
        std::uint64_t batch_identity = 0u;
        std::uint64_t global_digest = 0u;
        std::uint64_t frame_digest = 0u;
        std::uint64_t layer_digest = 0u;
        std::uint64_t material_identity = 0u;
        std::uint64_t origin_identity = 0u;
        std::uint64_t model_identity = 0u;
        std::uint64_t texture_handle = 0u;
        std::uint64_t texture_list_identity = 0u;
        std::uint64_t texture_list_epoch = 0u;
        std::uint64_t last_writer_identity = 0u;
        std::uint64_t last_writer_sequence = 0u;
        std::uint64_t expected_asset_identity = 0u;
        std::uint64_t resolved_asset_identity = 0u;
        std::uint64_t texture_generation = 0u;
        std::array<std::uint8_t, 32u> texture_content_sha256{};
        NativePortTexturePayloadSha256 texture_decoded_rgba8_sha256{};
        std::uint32_t texture_archive_ordinal = 0u;
        std::uint32_t texture_global_index = 0u;
        std::uint32_t texture_list_index = 0u;
        std::uint32_t texture_list_count = 0u;
        std::uint32_t mesh_index = 0u;
        std::uint32_t primitive_index = 0u;
        std::uint32_t submission_order = 0u;
        std::uint32_t flags = 0u;
        std::uint32_t texture_width = 0u;
        std::uint32_t texture_height = 0u;
        std::uint32_t texture_mip_levels = 0u;
        std::uint8_t texture_source_pixel_format = 0u;
        std::uint8_t texture_source_data_format = 0u;
        std::uint8_t batch_semantic = 0u;
        std::uint8_t draw_class = 0u;
        std::uint8_t logical_use = 0u;
        std::uint8_t origin = 0u;
        std::uint8_t intent = 0u;
        std::uint8_t resolver = 0u;
        std::uint8_t last_writer = 0u;
        std::uint8_t reserved = 0u;
        std::uint8_t reserved_2 = 0u;
    };

    struct GraphicsBreadcrumbFileHeader final {
        std::array<char, 8u> magic{'K', 'A', 'T', 'G', 'F', 'X', 'B', '2'};
        std::uint32_t schema_version = 2u;
        std::uint32_t header_size = sizeof(GraphicsBreadcrumbFileHeader);
        std::uint32_t record_size = sizeof(GraphicsBreadcrumbRecord);
        std::uint32_t mode = 0u;
        std::uint64_t capacity = 0u;
        std::uint64_t record_count = 0u;
        std::uint64_t first_record = 0u;
        std::uint64_t dropped_records = 0u;
        std::uint64_t final_digest = 0u;
    };

    static_assert(sizeof(GraphicsBreadcrumbRecord) == 256u);
    static_assert(sizeof(GraphicsBreadcrumbFileHeader) == 64u);

    struct MeshSlot final {
        std::uint64_t vulkan_mesh = 0;
        ComPtr<ID3D11Buffer> vertex_buffer;
        ComPtr<ID3D11Buffer> index_buffer;
        NativePortPrimitiveTopology source_topology =
            NativePortPrimitiveTopology::TriangleList;
        NativePortPrimitiveTopology gpu_topology =
            NativePortPrimitiveTopology::TriangleList;
        NativePortShadingMode source_shading =
            NativePortShadingMode::Smooth;
        float source_small_triangle_area_threshold = 0.0f;
        GeometryCapabilities capabilities;
        std::uint64_t byte_size = 0u;
        std::uint32_t vertex_count = 0u;
        std::uint32_t index_count = 0u;
        std::uint32_t generation = 0u;
        bool live = false;
    };

    enum class GpuTimingState : std::uint8_t {
        Free,
        Recording,
        Pending,
        DiscardPending,
    };

    struct GpuTimingSlot final {
        ComPtr<ID3D11Query> disjoint;
        ComPtr<ID3D11Query> begin;
        ComPtr<ID3D11Query> end;
        GpuTimingState state = GpuTimingState::Free;
    };

    static constexpr std::size_t gpu_timing_query_count = 4u;
    static constexpr std::size_t no_gpu_timing_query =
        std::numeric_limits<std::size_t>::max();

  public:
    NativePortGraphicsBackend(
        const NativePortGraphicsConfig& config,
        NativePortRuntimeOptionsBridge* const runtime_options)
        : title_storage_(copy_validated_graphics_title(config.title)),
          development_state_directory_storage_(
              copy_development_state_directory(
                  config.development_state_directory)),
          config_(config), owner_thread_(std::this_thread::get_id()),
          runtime_options_(runtime_options) {
        config_.title = title_storage_;
        config_.development_state_directory =
            development_state_directory_storage_;
        inject_present_failure_once_ = present_failure_test_requested();
        inject_present_busy_once_ = present_busy_test_requested();
        validate_graphics_config(config_);
        initialize_present_policy();
        initialize_frame_capture();
        initialize_graphics_diagnostics();
        create_window();
        refresh_display_rate();
        refresh_layout();
        try {
            if (sonic::rendering::selected_renderer == sonic::rendering::Renderer::Vulkan) {
                vulkan_ = std::make_unique<sonic::rendering::VulkanRenderer>(window_, config_);
                config_.maximum_type2_fragment_nodes = vulkan_->type_two_node_capacity();
            } else {
                create_device(); create_pipeline(); create_render_surface(); create_white_texture();
                ensure_performance_overlay_pipeline();
                if(config_.maximum_transient_vertices) ensure_vertex_buffer(1);
                if(config_.maximum_transient_indices) ensure_index_buffer(1);
            }
        } catch (...) {
            destroy_window();
            throw;
        }
        if (config_.telemetry != nullptr) {
            telemetry_writer_.emplace(config_.telemetry->make_writer());
            initialize_gpu_timing_queries();
        }
        if (config_.initially_visible) show();
        if (sonic::rendering::selected_window_mode != sonic::rendering::WindowMode::Windowed &&
            !background_test_mode_requested() && !toggle_window_mode())
            fail(NativePortGraphicsFailure::WindowCreation,GetLastError(),"fullscreen-window");
    }

    ~NativePortGraphicsBackend() noexcept {
        if (std::this_thread::get_id() != owner_thread_) std::terminate();
        abandon_gpu_timing_frame();
        retire_gpu_timing_queries_nonblocking();
        stop_render_submit_telemetry();
        flush_render_telemetry();
        flush_graphics_breadcrumbs();
        if (context_) {
            context_->ClearState();
            context_->Flush();
        }
        if (d3d_exclusive_ && swap_chain_) swap_chain_->SetFullscreenState(FALSE,nullptr);
        vulkan_.reset();
        destroy_window();
    }

    void publish_telemetry() noexcept {
        retire_gpu_timing_queries_nonblocking();
        flush_render_telemetry();
    }

    void show() {
        require_owner_thread();
        if (window_ == nullptr)
            fail(NativePortGraphicsFailure::WindowCreation, 0u, "window-closed");
        if (background_test_mode_requested()) {
            ShowWindow(window_, SW_HIDE);
            return;
        }
        ShowWindow(window_, SW_SHOWNORMAL);
        if (IsWindowVisible(window_) == FALSE)
            ShowWindow(window_, SW_SHOWNORMAL);
        UpdateWindow(window_);
    }

    void poll_events() {
        require_owner_thread();
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE) != FALSE) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if(display_change_pending_) {
            display_change_pending_=false;
            display_rate_dirty_=true;
            if(!fullscreen_.display_changed(window_))
                std::fprintf(stderr,"SONIC_DISPLAY_RECOVERY failed win32=%lu\n",GetLastError());
            // Re-query surface/present-mode support even when two monitors
            // happen to have equal pixel dimensions. Guest render size stays.
            if(vulkan_)vulkan_->resize(output_extent_);
            else if(flip_swap_chain_) swap_chain_resize_required_=true;
        }
        apply_pending_resize();
        if(display_rate_dirty_)refresh_display_rate();
        update_runtime_options_menu();
    }

    [[nodiscard]] NativePortLifecycleState lifecycle_state() const {
        require_owner_thread();
        if (close_requested_ || window_ == nullptr)
            return NativePortLifecycleState::Shutdown;
        if (minimized_) return NativePortLifecycleState::Paused;
        return NativePortLifecycleState::Running;
    }

    [[nodiscard]] NativePortGraphicsLayout layout() const {
        require_owner_thread();
        return cached_layout_;
    }

    [[nodiscard]] std::uint32_t effective_presentation_rate(std::uint32_t manual) const noexcept {
        return config_.synchronize_present ? display_rate_hz_ : manual;
    }
    [[nodiscard]] bool nonblocking_output_enabled() const noexcept {
        return allow_nonblocking_present_ && (vulkan_ || flip_swap_chain_) &&
            runtime_options_ && runtime_options_->independent_presentation_enabled.load(
                std::memory_order_acquire);
    }
    [[nodiscard]] bool driver_paces_output() const noexcept {
        return config_.synchronize_present && !nonblocking_output_enabled() && !minimized_ &&
            window_ && IsWindowVisible(window_);
    }

    [[nodiscard]] NativePortTextureHandle create_texture(
        const NativePortTextureConfig& config,
        const std::span<const NativePortImageView> initial_mip_levels) {
        require_owner_thread();
        if (!valid_texture_format(config.format) || !config.shader_resource)
            fail(NativePortGraphicsFailure::InvalidResource,
                 0u,
                 "texture-not-shader-resource");
        const auto byte_size = checked_texture_bytes(config);
        if (byte_size > config_.maximum_texture_bytes - texture_bytes_)
            fail(NativePortGraphicsFailure::ResourceLimit,
                 0u,
                 "texture-byte-budget");
        validate_texture_images(config, initial_mip_levels);

        std::uint32_t index = 0u;
        if (!free_texture_slots_.empty()) {
            index = free_texture_slots_.back();
            free_texture_slots_.pop_back();
        } else {
            if (texture_slots_.size() >= config_.maximum_textures)
                fail(NativePortGraphicsFailure::ResourceLimit,
                     0u,
                     "texture-count-budget");
            index = static_cast<std::uint32_t>(texture_slots_.size());
            texture_slots_.emplace_back();
        }

        auto& slot = texture_slots_[index];
        try {
            if (vulkan_) slot.vulkan_texture = vulkan_->create_texture(config);
            else {
            D3D11_TEXTURE2D_DESC description{};
            description.Width = config.extent.width;
            description.Height = config.extent.height;
            description.MipLevels = config.mip_levels;
            description.ArraySize = 1u;
            description.Format = texture_format(config.format);
            description.SampleDesc.Count = 1u;
            description.Usage = config.dynamic ? D3D11_USAGE_DYNAMIC
                                               : D3D11_USAGE_DEFAULT;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            description.CPUAccessFlags =
                config.dynamic ? D3D11_CPU_ACCESS_WRITE : 0u;
            const auto result = device_->CreateTexture2D(
                &description, nullptr, slot.texture.GetAddressOf());
            if (FAILED(result))
                fail(NativePortGraphicsFailure::ResourceCreation,
                     static_cast<std::uint32_t>(result),
                     "texture-create");
            const auto view_result = device_->CreateShaderResourceView(
                slot.texture.Get(), nullptr, slot.view.GetAddressOf());
            if (FAILED(view_result))
                fail(NativePortGraphicsFailure::ResourceCreation,
                     static_cast<std::uint32_t>(view_result),
                     "texture-view");
}
            slot.config = config;
            slot.byte_size = byte_size;
            slot.live = true;
            if (slot.generation == 0u) slot.generation = 1u;
            for (std::uint32_t level = 0u;
                 level < initial_mip_levels.size(); ++level)
                upload_texture(slot, initial_mip_levels[level], level);
        } catch (...) {
            if (vulkan_) vulkan_->destroy_texture(slot.vulkan_texture);
            slot.vulkan_texture = 0;
            slot.texture.Reset();
            slot.view.Reset();
            slot.live = false;
            slot.byte_size = 0u;
            free_texture_slots_.push_back(index);
            throw;
        }
        texture_bytes_ += byte_size;
        ++live_textures_;
        return make_texture_handle(index, slot.generation);
    }

    void update_texture(const NativePortTextureHandle handle,
                        const NativePortImageView& pixels) {
        require_owner_thread();
        auto& slot = resolve_texture(handle);
        if (slot.config.mip_levels != 1u)
            fail(NativePortGraphicsFailure::InvalidResource,
                 0u,
                 "texture-single-level-update");
        validate_image(pixels, slot.config.extent, slot.config.format);
        upload_texture(slot, pixels, 0u);
    }

    void update_texture(
        const NativePortTextureHandle handle,
        const std::span<const NativePortImageView> mip_levels) {
        require_owner_thread();
        auto& slot = resolve_texture(handle);
        validate_texture_images(slot.config, mip_levels);
        if (mip_levels.empty())
            fail(NativePortGraphicsFailure::InvalidResource,
                 0u,
                 "texture-empty-mip-update");
        for (std::uint32_t level = 0u; level < mip_levels.size(); ++level)
            upload_texture(slot, mip_levels[level], level);
    }

    void destroy_texture(const NativePortTextureHandle handle) {
        require_owner_thread();
        auto& slot = resolve_texture(handle);
        const auto index = texture_handle_index(handle);
        const bool retire_generation =
            slot.generation == std::numeric_limits<std::uint32_t>::max();
        // Match immutable-mesh lifetime semantics: reserve free-list storage
        // before invalidating a reusable texture, and permanently retire the
        // last representable generation. Wrapping it back to one would make
        // the first handle ever issued for this slot valid again.
        if (!retire_generation) free_texture_slots_.push_back(index);
        if (image_texture_ == handle) image_texture_ = {};
        const auto* const destroyed_view = slot.view.Get();
        if (bound_shader_resource_valid_ &&
            bound_shader_resource_ == destroyed_view) {
            ID3D11ShaderResourceView* no_view = nullptr;
            context_->PSSetShaderResources(0u, 1u, &no_view);
            bound_shader_resource_ = nullptr;
            bound_shader_resource_valid_ = false;
        }
        if (vulkan_) vulkan_->destroy_texture(slot.vulkan_texture);
        slot.vulkan_texture = 0;
        texture_bytes_ -= slot.byte_size;
        if (live_textures_ != 0u) --live_textures_;
        slot.texture.Reset();
        slot.view.Reset();
        slot.config = {};
        slot.byte_size = 0u;
        slot.live = false;
        if (!retire_generation) ++slot.generation;
    }

    [[nodiscard]] NativePortMeshHandle create_mesh(
        const NativePortMeshConfig& mesh) {
        require_owner_thread();
        if (!valid_shading(mesh.shading) ||
            !std::isfinite(mesh.small_triangle_area_threshold) ||
            mesh.small_triangle_area_threshold != 0.0f)
            fail(NativePortGraphicsFailure::InvalidResource,
                 0u,
                 "mesh-transform-dependent-triangle-filter");

        const auto remaining_mesh_bytes =
            config_.maximum_mesh_bytes - mesh_bytes_;
        const auto persistent_vertex_limit = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining_mesh_bytes,
                                    std::numeric_limits<UINT>::max()) /
            sizeof(NativePortVertex));
        const auto persistent_index_limit = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining_mesh_bytes,
                                    std::numeric_limits<UINT>::max()) /
            sizeof(std::uint32_t));

        static_cast<void>(validate_geometry(
            mesh.vertices,
            mesh.indices,
            mesh.topology,
            persistent_vertex_limit,
            persistent_index_limit,
            NativePortGraphicsFailure::InvalidResource,
            "mesh-layout",
            "mesh-index",
            "mesh-vertex",
            "mesh-topology"));

        auto vertices = mesh.vertices;
        auto indices = mesh.indices;
        auto topology = mesh.topology;
        std::vector<NativePortVertex> prepared;
        NativePortRasterizerState preprocessing;
        preprocessing.shading = mesh.shading;
        preprocessing.small_triangle_area_threshold = 0.0f;
        preprocessing.small_triangle_area_space =
            NativePortTriangleAreaSpace::Submitted;
        static_cast<void>(preprocess_geometry(
            vertices,
            indices,
            topology,
            preprocessing,
            NativePortMatrix4x4{},
            NativePortVertexSpace::ObjectHomogeneous,
            persistent_vertex_limit,
            prepared,
            NativePortGraphicsFailure::InvalidResource,
            "mesh-triangle-preprocess-topology",
            "mesh-triangle-preprocess-budget"));

        const auto capabilities = validate_geometry(
            vertices,
            indices,
            topology,
            persistent_vertex_limit,
            persistent_index_limit,
            NativePortGraphicsFailure::InvalidResource,
            "mesh-prepared-layout",
            "mesh-prepared-index",
            "mesh-prepared-vertex",
            "mesh-prepared-topology");

        const auto vertex_bytes =
            static_cast<std::uint64_t>(vertices.size_bytes());
        const auto index_bytes =
            static_cast<std::uint64_t>(indices.size_bytes());
        if (vertex_bytes > std::numeric_limits<std::uint64_t>::max() -
                               index_bytes)
            fail(NativePortGraphicsFailure::ResourceLimit,
                 0u,
                 "mesh-byte-overflow");
        const auto byte_size = vertex_bytes + index_bytes;
        if (byte_size > config_.maximum_mesh_bytes - mesh_bytes_)
            fail(NativePortGraphicsFailure::ResourceLimit,
                 0u,
                 "mesh-byte-budget");

        const bool reuse_slot = !free_mesh_slots_.empty();
        const auto index = reuse_slot
                               ? free_mesh_slots_.back()
                               : static_cast<std::uint32_t>(mesh_slots_.size());
        if (!reuse_slot && mesh_slots_.size() >= config_.maximum_meshes)
            fail(NativePortGraphicsFailure::ResourceLimit,
                 0u,
                 "mesh-count-budget");
        const auto generation = reuse_slot ? mesh_slots_[index].generation : 1u;

        if (vulkan_) {
            const auto handle = vulkan_->create_mesh(vertices, indices);
            if (reuse_slot) free_mesh_slots_.pop_back();
            else mesh_slots_.emplace_back();
            mesh_slots_[index].vulkan_mesh = handle;
        } else {
        auto vertex_buffer = create_immutable_buffer(
            vertices.data(),
            vertices.size_bytes(),
            D3D11_BIND_VERTEX_BUFFER,
            "mesh-vertex-buffer");
        auto index_buffer = create_immutable_buffer(
            indices.data(),
            indices.size_bytes(),
            D3D11_BIND_INDEX_BUFFER,
            "mesh-index-buffer");

        if (reuse_slot)
            free_mesh_slots_.pop_back();
        else
            mesh_slots_.emplace_back();
        auto& slot = mesh_slots_[index];
        slot.vertex_buffer = std::move(vertex_buffer);
        slot.index_buffer = std::move(index_buffer);
}
        auto& slot = mesh_slots_[index];
        slot.source_topology = mesh.topology;
        slot.gpu_topology = topology;
        slot.source_shading = mesh.shading;
        slot.source_small_triangle_area_threshold =
            mesh.small_triangle_area_threshold;
        slot.capabilities = capabilities;
        slot.byte_size = byte_size;
        slot.vertex_count = static_cast<std::uint32_t>(vertices.size());
        slot.index_count = static_cast<std::uint32_t>(indices.size());
        slot.generation = generation;
        slot.live = true;
        mesh_bytes_ += byte_size;
        ++live_meshes_;
        saturating_add(snapshot_.uploaded_bytes, byte_size);
        saturating_add(snapshot_.persistent_geometry_uploaded_bytes,
                       byte_size);
        return make_mesh_handle(index, slot.generation);
    }

    void destroy_mesh(const NativePortMeshHandle handle) {
        require_owner_thread();
        auto& slot = resolve_mesh(handle);
        const auto index = mesh_handle_index(handle);
        const bool retire_generation =
            slot.generation == std::numeric_limits<std::uint32_t>::max();
        // Allocate free-list storage before invalidating a reusable resource
        // so an allocation failure leaves ownership unchanged. A slot whose
        // final generation was observed is retired permanently instead of
        // wrapping and making an ancient handle valid again.
        if (!retire_generation) free_mesh_slots_.push_back(index);
        if (vulkan_) vulkan_->destroy_mesh(slot.vulkan_mesh);
        slot.vulkan_mesh = 0;
        mesh_bytes_ -= slot.byte_size;
        if (live_meshes_ != 0u) --live_meshes_;
        slot.vertex_buffer.Reset();
        slot.index_buffer.Reset();
        slot.source_topology = NativePortPrimitiveTopology::TriangleList;
        slot.gpu_topology = NativePortPrimitiveTopology::TriangleList;
        slot.source_shading = NativePortShadingMode::Smooth;
        slot.source_small_triangle_area_threshold = 0.0f;
        slot.capabilities = {};
        slot.byte_size = 0u;
        slot.vertex_count = 0u;
        slot.index_count = 0u;
        slot.live = false;
        if (!retire_generation) ++slot.generation;
    }

    void begin_frame(const NativePortFrameConfig& frame) {
        require_owner_thread();
        poll_events();
        // WM_CLOSE requests lifecycle shutdown; it deliberately leaves the
        // window and GPU resources alive. A request can arrive after the
        // simulation accepted work but before its lazy BeginFrame reaches
        // this thread. Finish that work normally so the host-stop owner can
        // rendezvous at a completed frame instead of reporting a crash.
        if (window_ == nullptr)
            fail(NativePortGraphicsFailure::InvalidFrame, 0u, "window-missing");
        if (frame_open_)
            fail(NativePortGraphicsFailure::InvalidFrame, 0u, "frame-already-open");
        if (!std::all_of(frame.clear_color.begin(),
                         frame.clear_color.end(),
                         [](const float value) { return std::isfinite(value); }) ||
            !std::isfinite(frame.clear_depth) || frame.clear_depth < 0.0f ||
            frame.clear_depth > 1.0f)
            fail(NativePortGraphicsFailure::InvalidFrame, 0u, "frame-clear");
        if (!valid_depth_buffer_convention(frame.depth_buffer) ||
            (frame.depth_buffer == NativePortDepthBufferConvention::Forward
                 ? frame.clear_depth != 1.0f
                 : frame.clear_depth != 0.0f))
            fail(NativePortGraphicsFailure::InvalidFrame,
                 0u,
                 "frame-depth-contract");
        start_render_submit_telemetry();
        begin_gpu_timing_frame();
        if (vulkan_) vulkan_->begin_frame(frame);
        else {
        context_->OMSetRenderTargets(
            1u, render_target_.GetAddressOf(), depth_view_.Get());
        context_->ClearRenderTargetView(render_target_.Get(),
                                        frame.clear_color.data());
        context_->ClearDepthStencilView(
            depth_view_.Get(), D3D11_CLEAR_DEPTH, frame.clear_depth, 0u);
}
        vertex_buffer_write_offset_ = 0u;
        index_buffer_write_offset_ = 0u;
        vertex_buffer_discarded_ = false;
        index_buffer_discarded_ = false;
        invalidate_draw_state_shadow();
        frame_depth_buffer_ = frame.depth_buffer;
        frame_batch_active_ = false;
        frame_batch_identity_ = 0u;
        frame_batch_semantic_ = NativePortDrawBatchClass::Scene3D;
        frame_batch_phase_ = NativePortDrawClass::Opaque;
        frame_batch_submission_order_ = 0u;
        type2_subpass_active_ = false;
        type2_gather_active_ = false;
        type2_batch_identity_ = 0u;
        type2_closed_batch_valid_ = false;
        type2_closed_batch_identity_ = 0u;
        type2_non_type2_batch_valid_ = false;
        type2_non_type2_batch_identity_ = 0u;
        type_two_uavs_bound_ = false;
        graphics_frame_digest_ = graphics_digest_seed ^
            (snapshot_.presented_frames + 1u);
        frame_open_ = true;
        saturating_increment(snapshot_.begun_frames);
    }

    void draw(const NativePortDrawPacket& packet) {
        require_owner_thread();
        const bool type2 = packet.translucency ==
            NativePortTranslucencyPolicy::Type2AutoSorted;
        // A fixed-function pass identity owns one complete translucent list,
        // even when several host semantic batches contribute to it. A second
        // policy under that identity would create independently resolved runs
        // and is therefore not equivalent to the PVR list.
        if (!type2 &&
            packet.draw_class == NativePortDrawClass::Translucent &&
            type2_closed_batch_valid_ &&
            packet.batch.identity == type2_closed_batch_identity_) {
            try {
                fail(NativePortGraphicsFailure::InvalidDraw,
                     0u,
                     "type2-policy-mix");
            } catch (const NativePortGraphicsError& error) {
                record_graphics_contract_failure(packet, error.failure());
                throw;
            }
        }
        if (type2 && type2_subpass_active_ &&
            packet.batch.identity != type2_batch_identity_) {
            // A new authenticated pass identity is an explicit list boundary.
            // Resolve the previous Type-2 list before admitting the next one.
            flush_type2_translucency();
        }
        if (!type2 && type2_subpass_active_) {
            if (packet.batch.identity == type2_batch_identity_ &&
                packet.draw_class == NativePortDrawClass::Translucent) {
                try {
                    fail(NativePortGraphicsFailure::InvalidDraw,
                         0u,
                         "type2-policy-mix");
                } catch (const NativePortGraphicsError& error) {
                    record_graphics_contract_failure(packet, error.failure());
                    throw;
                }
            }
            flush_type2_translucency();
        }
        draw_immediate(packet, type2);
    }

    void draw_immediate(const NativePortDrawPacket& packet,
                        const bool type2_gather) {
        require_owner_thread();
        try {
        if (!frame_open_)
            fail(NativePortGraphicsFailure::InvalidDraw, 0u, "draw-outside-frame");
        auto* const mesh_slot = packet.mesh != NativePortMeshHandle{}
                                    ? &resolve_mesh(packet.mesh)
                                    : nullptr;
        validate_draw(packet, mesh_slot);
        const auto* const texture_slot = packet.texture
            ? &resolve_texture(packet.texture)
            : nullptr;
        validate_draw_batch_sequence(packet);
        if (type2_gather) {
            validate_type2_scene_admission(packet);
            if (!type2_subpass_active_) begin_type2_subpass(packet);
        } else if (packet.draw_class == NativePortDrawClass::Translucent) {
            type2_non_type2_batch_valid_ = true;
            type2_non_type2_batch_identity_ = packet.batch.identity;
        }

        std::uint32_t type2_draw_sequence = 0u;
        if (type2_gather) {
            if (type2_list_draw_sequence_ ==
                std::numeric_limits<std::uint32_t>::max())
                fail(NativePortGraphicsFailure::ResourceLimit,
                     type2_list_draw_sequence_,
                     "type2-draw-sequence-overflow");
            type2_draw_sequence = type2_list_draw_sequence_++;
        }

        // Lives through preprocessing and GPU upload; never aliases prepared_vertices_.
        std::array<NativePortVertex, 4u> ui_texture_edge_vertices;
        auto vertices = packet.vertices;
        auto indices = packet.indices;
        auto topology = packet.topology;
        GeometryPreprocessStats preprocess_stats;
        if (mesh_slot != nullptr) {
            topology = mesh_slot->gpu_topology;
            require_geometry_capabilities(packet, mesh_slot->capabilities);
        } else {
            const auto source_vertices = vertices;
            const auto source_indices = indices;
            const auto source_topology = topology;
            auto capabilities = validate_geometry(
                vertices,
                indices,
                topology,
                config_.maximum_transient_vertices,
                config_.maximum_transient_indices,
                NativePortGraphicsFailure::InvalidDraw,
                "draw-layout",
                "draw-index",
                "draw-vertex",
                "draw-topology");
            if (texture_slot != nullptr && detail::prepare_ui_texture_edges(
                    packet, texture_slot->config.extent,
                    cached_layout_.ui_viewport.height, ui_texture_edge_vertices))
                vertices = ui_texture_edge_vertices;
            const bool preprocessing_required =
                packet.rasterizer.shading ==
                    NativePortShadingMode::FlatLastVertex ||
                packet.rasterizer.small_triangle_area_threshold > 0.0f;
            // The preprocessing helper also computes drawstream statistics,
            // but otherwise its smooth/zero-threshold branch is a guaranteed
            // no-op. Keep the exact diagnostic record when drawstream capture
            // is armed while avoiding that work on the normal draw hotpath.
            if (preprocessing_required || drawstream_enabled_)
                preprocess_stats = preprocess_geometry(
                    vertices,
                    indices,
                    topology,
                    packet.rasterizer,
                    packet.transform,
                    packet.vertex_space,
                    config_.maximum_transient_vertices,
                    prepared_vertices_,
                    NativePortGraphicsFailure::InvalidDraw,
                    "draw-triangle-preprocess-topology",
                    "draw-triangle-preprocess-budget");
            if (vertices.empty()) {
                const auto& current_layout = cached_layout_;
                const auto viewport_rect =
                    packet.viewport == NativePortViewportTarget::Ui
                        ? current_layout.ui_viewport
                        : current_layout.game_viewport;
                if (graphics_diagnostic_mode_ !=
                    NativePortGraphicsDiagnosticMode::Off)
                    record_graphics_diagnostic(
                        packet, texture_slot, false, true);
                if (drawstream_enabled_)
                    capture_drawstream(packet,
                                       source_vertices,
                                       source_indices,
                                       source_topology,
                                       viewport_rect,
                                       texture_slot,
                                       nullptr,
                                       &preprocess_stats,
                                       false,
                                       "preprocessed-empty");
                return;
            }
            if (preprocessing_required) {
                capabilities = validate_geometry(
                    vertices,
                    indices,
                    topology,
                    config_.maximum_transient_vertices,
                    config_.maximum_transient_indices,
                    NativePortGraphicsFailure::InvalidDraw,
                    "draw-prepared-layout",
                    "draw-prepared-index",
                    "draw-prepared-vertex",
                    "draw-prepared-topology");
            }
            require_geometry_capabilities(packet, capabilities);
        }

        ID3D11Buffer* bound_vertex_buffer = nullptr;
        ID3D11Buffer* bound_index_buffer = nullptr;
        UINT vertex_buffer_offset = 0u;
        UINT index_buffer_offset = 0u;
        UINT vertex_count = 0u;
        UINT index_count = 0u;
if (!vulkan_) {
        if (mesh_slot != nullptr) {
            bound_vertex_buffer = mesh_slot->vertex_buffer.Get();
            bound_index_buffer = mesh_slot->index_buffer.Get();
            vertex_count = mesh_slot->vertex_count;
            index_count = mesh_slot->index_count;
        } else {
            ensure_vertex_buffer(vertices.size_bytes());
            vertex_buffer_offset = upload_dynamic(
                vertex_buffer_.Get(), vertex_buffer_capacity_,
                vertex_buffer_write_offset_, vertex_buffer_discarded_,
                vertices.data(), vertices.size_bytes(), "vertex-buffer-map");
            bound_vertex_buffer = vertex_buffer_.Get();
            vertex_count = static_cast<UINT>(vertices.size());
            if (!indices.empty()) {
                ensure_index_buffer(indices.size_bytes());
                index_buffer_offset = upload_dynamic(
                    index_buffer_.Get(), index_buffer_capacity_,
                    index_buffer_write_offset_, index_buffer_discarded_,
                    indices.data(), indices.size_bytes(), "index-buffer-map");
                bound_index_buffer = index_buffer_.Get();
                index_count = static_cast<UINT>(indices.size());
            }
        }
}

        const auto& current_layout = cached_layout_;
        const auto viewport_rect =
            packet.viewport == NativePortViewportTarget::Ui
                ? current_layout.ui_viewport
                : current_layout.game_viewport;
        DrawConstants constants{};
        constants.transform = packet.transform.values;
        constants.normal_transform = packet.normal_transform.values;
        constants.material_diffuse = packet.material.diffuse;
        constants.material_ambient = packet.material.ambient;
        constants.material_specular = packet.material.specular;
        constants.material_emission = packet.material.emission;
        constants.scene_ambient = packet.lighting.ambient;
        constants.fog_color = packet.fog.color;
        constants.color_clamp_minimum = packet.color_clamp.minimum;
        constants.color_clamp_maximum = packet.color_clamp.maximum;
        if ((packet.fog.mode == NativePortFogMode::LookupTable ||
             packet.fog.mode == NativePortFogMode::LookupTablePrimary) &&
            (!fog_lookup_table_valid_ ||
             last_fog_lookup_table_ != packet.fog.lookup_table)) {
            FogTableConstants fog_constants{};
            std::memcpy(fog_constants.lookup_table.data(),
                        packet.fog.lookup_table.data(),
                        sizeof(packet.fog.lookup_table));
            if (!vulkan_) context_->UpdateSubresource(fog_table_constants_.Get(),
                                        0u,
                                        nullptr,
                                        &fog_constants,
                                        0u,
                                        0u);
            last_fog_lookup_table_ = packet.fog.lookup_table;
            fog_lookup_table_valid_ = true;
        }
        constants.fog_parameters = {
            packet.fog.start, packet.fog.end, packet.fog.density, 0.0f};
        constants.depth_parameters = {
            packet.depth_mapping.reciprocal_scale,
            packet.depth_mapping.logarithm_divisor,
            0.0f,
            0.0f};
        constants.material_parameters = {
            packet.material.specular_power,
            packet.alpha_test.reference,
            texture_slot != nullptr && texture_slot->config.mip_levels > 1u &&
                    packet.draw_class != NativePortDrawClass::PunchThrough
                ? packet.material.mipmapped_pass_weight : 1.0f,
            0.0f};
        const auto& logical_clip = packet.rasterizer.logical_clip;
        if (logical_clip.mode !=
            NativePortLogicalClipMode::Disabled) {
            constants.logical_clip_transform = {
                static_cast<float>(viewport_rect.width) /
                    static_cast<float>(logical_clip.logical_extent.width),
                static_cast<float>(viewport_rect.height) /
                    static_cast<float>(logical_clip.logical_extent.height),
                static_cast<float>(viewport_rect.x),
                static_cast<float>(viewport_rect.y)};
            constants.logical_clip_bounds = logical_clip.bounds;
            constants.logical_clip_parameters = {
                static_cast<std::uint32_t>(logical_clip.mode),
                logical_clip.logical_extent.width,
                logical_clip.logical_extent.height,
                0u};
        }
        std::uint32_t material_flags = 0u;
        if (packet.material.use_vertex_color)
            material_flags |= draw_flag_vertex_color;
        if (packet.material.use_secondary_color)
            material_flags |= draw_flag_secondary_color;
        if (packet.material.lighting_enabled)
            material_flags |= draw_flag_lighting;
        if (packet.material.specular_enabled)
            material_flags |= draw_flag_specular;
        if (packet.material.texture_coordinates ==
            NativePortTextureCoordinateSource::NormalSphere)
            material_flags |= draw_flag_normal_sphere;
        if (packet.depth_mapping.mode ==
                NativePortDepthCoordinateMode::ReciprocalPositive ||
            packet.depth_mapping.mode ==
                NativePortDepthCoordinateMode::ReciprocalPositiveHomogeneousClip)
            material_flags |= draw_flag_reciprocal_depth;
        if (packet.depth_mapping.mode ==
            NativePortDepthCoordinateMode::ReciprocalPositiveHomogeneousClip)
            material_flags |= draw_flag_homogeneous_reciprocal_clip;
        if (packet.material.use_primary_alpha)
            material_flags |= draw_flag_primary_alpha;
        if (packet.material.use_texture_alpha)
            material_flags |= draw_flag_texture_alpha;
        if (packet.material.bump_mapping)
            material_flags |= draw_flag_bump_mapping;
        if (packet.texture)
            material_flags |= draw_flag_texture_present;
        if (packet.interpolation ==
            NativePortInterpolationMode::PvrScreenGouraud)
            material_flags |= draw_flag_pvr_screen_gouraud;
        if (packet.vertex_space ==
            NativePortVertexSpace::ClipHomogeneous)
            material_flags |= draw_flag_clip_homogeneous;
        if (packet.color_clamp.enabled)
            material_flags |= draw_flag_color_clamp;
        // Persistent geometry was expanded using source_shading; validation
        // requires the packet to retain that exact preprocessing contract.
        if ((mesh_slot != nullptr ? mesh_slot->source_shading
                                  : packet.rasterizer.shading) ==
            NativePortShadingMode::FlatLastVertex)
            material_flags |= draw_flag_flat_last_vertex;
        if (type2_gather)
            material_flags |= draw_flag_type_two_autosort_capture;
        material_flags |=
            static_cast<std::uint32_t>(packet.material.texture_combine)
            << draw_texture_combine_shift;
        auto alpha_test_flags =
            static_cast<std::uint32_t>(packet.alpha_test.compare);
        if (packet.alpha_test.enabled)
            alpha_test_flags |= draw_alpha_test_enabled;
        if (packet.alpha_test.mode ==
            NativePortAlphaTestState::Mode::Quantized8BitForceOpaque) {
            alpha_test_flags |= draw_alpha_test_quantized_8bit;
            alpha_test_flags |=
                static_cast<std::uint32_t>(
                    packet.alpha_test.reference_8bit)
                << draw_alpha_test_reference_shift;
        }
        constants.pipeline_flags = {
            material_flags,
            packet.lighting.light_count,
            static_cast<std::uint32_t>(packet.fog.mode),
            alpha_test_flags};
        if (type2_gather) {
            // Type-2 blending is performed only after per-pixel sorting, so
            // the capture must preserve the exact factors of this packet.
            // submission_order is only monotone within one semantic batch;
            // this renderer-owned sequence is unique across the complete
            // authenticated Type-2 list, including semantic transitions.
            constants.type_two_parameters = {
                pack_type2_blend_factors(packet.blend),
                pack_type2_blend_buffers(packet.blend),
                type2_draw_sequence,
                config_.maximum_type2_fragment_nodes};
        }
        for (std::size_t index = 0u;
             index < packet.lighting.light_count; ++index) {
            const auto& light = packet.lighting.lights[index];
            constants.light_directions[index] = {
                light.direction[0], light.direction[1], light.direction[2],
                0.0f};
            constants.light_colors[index] = light.color;
        }
        if (!vulkan_ && (!draw_constants_valid_ ||
            std::memcmp(&last_draw_constants_,
                        &constants,
                        sizeof(constants)) != 0)) {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            const auto result = context_->Map(draw_constants_.Get(),
                                              0u,
                                              D3D11_MAP_WRITE_DISCARD,
                                              0u,
                                              &mapped);
            if (FAILED(result))
                fail(NativePortGraphicsFailure::DeviceLost,
                     static_cast<std::uint32_t>(result),
                     "draw-constants-map");
            std::memcpy(mapped.pData, &constants, sizeof(constants));
            context_->Unmap(draw_constants_.Get(), 0u);
            last_draw_constants_ = constants;
            draw_constants_valid_ = true;
        }

        if (graphics_diagnostic_mode_ !=
            NativePortGraphicsDiagnosticMode::Off)
            record_graphics_diagnostic(packet, texture_slot, true, false);
        if (drawstream_enabled_)
            capture_drawstream(
                packet,
                vertices,
                indices,
                topology,
                viewport_rect,
                texture_slot,
                mesh_slot,
                mesh_slot == nullptr ? &preprocess_stats : nullptr,
                true,
                nullptr);
        if (vulkan_) {
            vulkan_->draw(packet, vertices, indices, topology,
                mesh_slot ? mesh_slot->vulkan_mesh : 0,
                texture_slot ? texture_slot->vulkan_texture : 0, viewport_rect,
                std::as_bytes(std::span(&constants, 1)), type2_gather);
            if (!mesh_slot) {
                const auto uploaded = vertices.size_bytes() + indices.size_bytes();
                saturating_add(snapshot_.uploaded_bytes, uploaded);
                saturating_add(snapshot_.transient_geometry_uploaded_bytes, uploaded);
            }
        } else {
        set_viewport(viewport_rect);

        // Type-2 capture must not mutate the depth buffer while the list is
        // being gathered.  The PVR/Flycast per-pixel path publishes every
        // fragment that passes the completed opaque/punch front depth and
        // only then orders the complete list.  Honouring a polygon's Z write
        // directly in submission order makes a nearer transparent UI quad
        // reject a later, farther quad before the resolver can see it.  That
        // produces rectangular holes behind transparent texels (Character
        // Select's Play Select, name and percentage layers).  Preserve the
        // packet's GEQUAL test against opaque/punch depth, but defer all
        // translucent ordering to the PPLL resolver.
        auto* const blend = resolve_blend_state(packet.blend);
        auto effective_depth_state = packet.depth;
        if (type2_gather) effective_depth_state.write_enabled = false;
        auto* const depth = resolve_depth_state(effective_depth_state);
        auto host_rasterizer = packet.rasterizer;
        // Flat-last-vertex and small-triangle semantics were resolved by the
        // bounded vertex preprocessing above. They do not create distinct
        // host rasterizer objects.
        host_rasterizer.shading = NativePortShadingMode::Smooth;
        host_rasterizer.small_triangle_area_threshold = 0.0f;
        host_rasterizer.small_triangle_area_space =
            NativePortTriangleAreaSpace::Submitted;
        host_rasterizer.small_triangle_reference_extent = {};
        // Logical clipping is a pixel-stage contract.  It must not multiply
        // identical native rasterizer objects or consume the pipeline-state
        // budget as clip rectangles vary between draws.
        host_rasterizer.logical_clip = {};
        auto* const rasterizer = resolve_rasterizer_state(host_rasterizer);
        auto* const sampler = resolve_sampler_state(packet.sampler);
        constexpr std::array blend_factor{0.0f, 0.0f, 0.0f, 0.0f};
        if (!bound_blend_valid_ || bound_blend_ != blend) {
            context_->OMSetBlendState(
                blend, blend_factor.data(), 0xFFFFFFFFu);
            bound_blend_ = blend;
            bound_blend_valid_ = true;
        }
        if (!bound_depth_valid_ || bound_depth_ != depth) {
            context_->OMSetDepthStencilState(depth, 0u);
            bound_depth_ = depth;
            bound_depth_valid_ = true;
        }
        if (!bound_rasterizer_valid_ || bound_rasterizer_ != rasterizer) {
            context_->RSSetState(rasterizer);
            bound_rasterizer_ = rasterizer;
            bound_rasterizer_valid_ = true;
        }
        if (!draw_pipeline_bound_ || draw_pipeline_type_two_ != type2_gather) {
            context_->IASetInputLayout(input_layout_.Get());
            context_->VSSetShader(draw_vertex_shader_.Get(), nullptr, 0u);
            context_->VSSetConstantBuffers(
                0u, 1u, draw_constants_.GetAddressOf());
            context_->PSSetShader(
                type2_gather ? type_two_capture_pixel_shader_.Get()
                             : draw_pixel_shader_.Get(),
                nullptr,
                0u);
            const std::array<ID3D11Buffer*, 2u> pixel_constant_buffers{
                draw_constants_.Get(), fog_table_constants_.Get()};
            context_->PSSetConstantBuffers(
                0u,
                static_cast<UINT>(pixel_constant_buffers.size()),
                pixel_constant_buffers.data());
            ID3D11ShaderResourceView* no_resource = nullptr;
            context_->PSSetShaderResources(1u, 1u, &no_resource);
            draw_pipeline_bound_ = true;
            draw_pipeline_type_two_ = type2_gather;
        }
        const auto host_topology = primitive_topology(topology);
        if (!bound_topology_valid_ || bound_topology_ != host_topology) {
            context_->IASetPrimitiveTopology(host_topology);
            bound_topology_ = host_topology;
            bound_topology_valid_ = true;
        }
        const UINT stride = sizeof(NativePortVertex);
        if (!bound_vertex_buffer_valid_ ||
            bound_vertex_buffer_ != bound_vertex_buffer ||
            bound_vertex_buffer_offset_ != vertex_buffer_offset) {
            context_->IASetVertexBuffers(
                0u, 1u, &bound_vertex_buffer, &stride,
                &vertex_buffer_offset);
            bound_vertex_buffer_ = bound_vertex_buffer;
            bound_vertex_buffer_offset_ = vertex_buffer_offset;
            bound_vertex_buffer_valid_ = true;
        }
        if (index_count != 0u &&
            (!bound_index_buffer_valid_ ||
             bound_index_buffer_ != bound_index_buffer ||
             bound_index_buffer_offset_ != index_buffer_offset)) {
            context_->IASetIndexBuffer(
                bound_index_buffer, DXGI_FORMAT_R32_UINT,
                index_buffer_offset);
            bound_index_buffer_ = bound_index_buffer;
            bound_index_buffer_offset_ = index_buffer_offset;
            bound_index_buffer_valid_ = true;
        }
        if (!bound_sampler_valid_ || bound_sampler_ != sampler) {
            context_->PSSetSamplers(0u, 1u, &sampler);
            bound_sampler_ = sampler;
            bound_sampler_valid_ = true;
        }
        auto* view = texture_slot != nullptr ? texture_slot->view.Get()
                                             : white_view_.Get();
        if (!bound_shader_resource_valid_ ||
            bound_shader_resource_ != view) {
            context_->PSSetShaderResources(0u, 1u, &view);
            bound_shader_resource_ = view;
            bound_shader_resource_valid_ = true;
        }
        if (index_count == 0u)
            context_->Draw(vertex_count, 0u);
        else
            context_->DrawIndexed(index_count, 0u, 0);
}
        saturating_increment(snapshot_.draw_calls);
        if (mesh_slot != nullptr)
            saturating_increment(snapshot_.persistent_mesh_draw_calls);
        } catch (const NativePortGraphicsError& error) {
            // Draw validation is a single public contract boundary. Preserve
            // the exact packet identity for every typed failure, including
            // failures raised before texture resolution or diagnostic capture.
            // Thread ownership is checked above so this bounded snapshot is
            // never mutated from a foreign thread.
            record_graphics_contract_failure(packet, error.failure());
            throw;
        }
    }

    void flush_type2_translucency() {
        require_owner_thread();
        if (!frame_open_)
            fail(NativePortGraphicsFailure::InvalidFrame,
                 0u,
                 "type2-flush-outside-frame");
        if (!type2_subpass_active_) return;
        try {
            resolve_type2_subpass();
        } catch (...) {
            // A rejected/overflowed Type-2 list must never be followed by a
            // second attempt using partially retained UAV state. The frame
            // remains fail-closed and the original typed error is preserved.
            unbind_type2_subpass();
            if (!vulkan_) context_->OMSetRenderTargets(
                1u, render_target_.GetAddressOf(), depth_view_.Get());
            type2_subpass_active_ = false;
            type2_gather_active_ = false;
            type2_batch_identity_ = 0u;
            type_two_uavs_bound_ = false;
            invalidate_draw_state_shadow();
            throw;
        }
        type2_subpass_active_ = false;
        type2_closed_batch_identity_ = type2_batch_identity_;
        type2_closed_batch_valid_ = true;
        type2_batch_identity_ = 0u;
        type2_gather_active_ = false;
        type_two_uavs_bound_ = false;
        invalidate_draw_state_shadow();
    }

    void complete_frame() {
        require_owner_thread();
        if (!frame_open_)
            fail(NativePortGraphicsFailure::InvalidFrame, 0u, "present-without-frame");
        try {
            flush_type2_translucency();
            end_gpu_timing_frame();
            stop_render_submit_telemetry();
            flush_render_telemetry();
        } catch (...) {
            abort_frame_after_command_failure();
            throw;
        }
        // Both resources belong to this immediate context. Its ordered GPU
        // stream finishes the working image before a subsequent composite;
        // no CPU fence or full-image copy is required.
        if (vulkan_) vulkan_->complete_frame();
        completed_host_image_ = false;
        completed_host_texture_.Reset();
        completed_host_view_.Reset();
        render_texture_.Swap(completed_texture_);
        render_target_.Swap(completed_target_);
        render_view_.Swap(completed_view_);
        frame_open_ = false;
        snapshot_.frame_open = false;
        completed_frame_available_ = true;
    }

    void present() {
        complete_frame();
        try {
            static_cast<void>(repeat_present("present"));
        } catch (...) {
            completed_frame_available_ = false;
            abort_frame_after_command_failure();
            throw;
        }
    }

    [[nodiscard]] bool completed_frame_ready() const noexcept {
        return completed_frame_available_ && !frame_open_;
    }

    [[nodiscard]] bool completed_image_available() const noexcept {
        return completed_frame_available_;
    }

    // Only the autonomous render-owner deadline may suspend an open frame.
    // The public repeat command still requires a closed frame. Keep the last
    // completed texture immutable and never resolve or publish the work here.
    NativePortBackendPresentOutcome present_completed_image_on_deadline(
        const char* const operation) {
        if (!frame_open_) return repeat_present(operation);
        require_owner_thread();
        if (!completed_frame_available_)
            fail(NativePortGraphicsFailure::InvalidFrame, 0u,
                 "repeat-without-completed-frame");
        try {
            end_gpu_timing_frame();
            stop_render_submit_telemetry();
            unbind_type2_subpass();
            start_render_submit_telemetry();
            begin_gpu_timing_frame();
            const auto outcome = present_completed_frame(operation);
            restore_working_frame_attachments();
            // Composite/overlay/resize changed the pipeline. The next draw
            // restores its own viewport, shaders, textures and fixed state.
            invalidate_draw_state_shadow();
            start_render_submit_telemetry();
            begin_gpu_timing_frame();
            return outcome;
        } catch (...) {
            abort_frame_after_command_failure();
            throw;
        }
    }

    // A batched parallel frame is atomic at the facade boundary. Once one
    // backend command fails, no later command from that frame may run and the
    // partially rendered surface must never become repeat-presentable. This
    // owner-thread rollback deliberately does not resolve Type-2 or present.
    void abort_frame_after_command_failure() noexcept {
        if (std::this_thread::get_id() != owner_thread_) std::terminate();
        abandon_gpu_timing_frame();
        stop_render_submit_telemetry();
        flush_render_telemetry();
        completed_frame_available_ = false;
        if (!frame_open_) return;
        if (vulkan_) { try { vulkan_->abort_frame(); } catch (...) {} }
        unbind_type2_subpass();
        if (context_)
            context_->OMSetRenderTargets(
                1u, render_target_.GetAddressOf(), depth_view_.Get());
        frame_open_ = false;
        completed_frame_available_ = false;
        snapshot_.frame_open = false;
        frame_batch_active_ = false;
        frame_batch_identity_ = 0u;
        frame_batch_semantic_ = NativePortDrawBatchClass::Scene3D;
        frame_batch_phase_ = NativePortDrawClass::Opaque;
        frame_batch_submission_order_ = 0u;
        type2_subpass_active_ = false;
        type2_gather_active_ = false;
        type2_batch_identity_ = 0u;
        type2_closed_batch_valid_ = false;
        type2_closed_batch_identity_ = 0u;
        type2_non_type2_batch_valid_ = false;
        type2_non_type2_batch_identity_ = 0u;
        type_two_uavs_bound_ = false;
        invalidate_draw_state_shadow();
    }

    NativePortBackendPresentOutcome repeat_present(
        const char* const operation = "repeat-present") {
        require_owner_thread();
        if (frame_open_ || !completed_frame_available_)
            fail(NativePortGraphicsFailure::InvalidFrame,
                 0u,
                 "repeat-without-completed-frame");
        start_render_submit_telemetry();
        begin_gpu_timing_frame();
        return present_completed_frame(operation);
    }

  private:
    void validate_type2_scene_admission(
        const NativePortDrawPacket& packet) {
        if (!vulkan_ && (feature_level_ < D3D_FEATURE_LEVEL_11_0 ||
            type_two_capture_pixel_shader_ == nullptr ||
            type_two_resolve_pixel_shader_ == nullptr))
            fail(NativePortGraphicsFailure::UnsupportedHost,
                 0u,
                 "type2-ppll-feature-level");
        // PVR Type-2 ordering belongs to the complete translucent list, not
        // to a host semantic batch or viewport. Character Select contributes
        // both its 3D model and reciprocal-screen UI to the same per-pixel
        // list. Semantic/viewport compatibility is already proved by the
        // ordinary packet validators.
        if (packet.draw_class != NativePortDrawClass::Translucent)
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "type2-list-contract");
        if (type2_closed_batch_valid_ &&
            packet.batch.identity == type2_closed_batch_identity_)
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "type2-scene-reentry");
        if (type2_non_type2_batch_valid_ &&
            packet.batch.identity == type2_non_type2_batch_identity_)
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "type2-policy-mix");
        if (type2_subpass_active_ &&
            packet.batch.identity != type2_batch_identity_)
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "type2-scene-boundary");
    }

    void ensure_type2_resources() {
        if (type2_resources_ready_) return;
        if (feature_level_ < D3D_FEATURE_LEVEL_11_0)
            fail(NativePortGraphicsFailure::UnsupportedHost,
                 0u,
                 "type2-ppll-feature-level");
        const auto width = config_.render_extent.width;
        const auto height = config_.render_extent.height;

        D3D11_TEXTURE2D_DESC base_description{};
        base_description.Width = width;
        base_description.Height = height;
        base_description.MipLevels = 1u;
        base_description.ArraySize = 1u;
        base_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        base_description.SampleDesc.Count = 1u;
        base_description.Usage = D3D11_USAGE_DEFAULT;
        base_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        auto result = device_->CreateTexture2D(
            &base_description, nullptr, type_two_base_texture_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "type2-base-texture");
        result = device_->CreateShaderResourceView(
            type_two_base_texture_.Get(),
            nullptr,
            type_two_base_view_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "type2-base-view");

        // A pixel shader with UAV side effects is not guaranteed to run only
        // after the ordinary DSV test.  Preserve the completed opaque/punch
        // depth in a separate shader-readable texture so the Type-2 gather
        // can reject hidden fragments before publishing a PPLL node.  This is
        // the native equivalent of the PVR OIT front-depth test; it is not a
        // second rendering pass or a guest-side emulation path.
        D3D11_TEXTURE2D_DESC depth_copy_description{};
        depth_copy_description.Width = width;
        depth_copy_description.Height = height;
        depth_copy_description.MipLevels = 1u;
        depth_copy_description.ArraySize = 1u;
        depth_copy_description.Format = DXGI_FORMAT_R24G8_TYPELESS;
        depth_copy_description.SampleDesc.Count = 1u;
        depth_copy_description.Usage = D3D11_USAGE_DEFAULT;
        depth_copy_description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        result = device_->CreateTexture2D(
            &depth_copy_description,
            nullptr,
            type_two_depth_texture_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "type2-depth-texture");
        D3D11_SHADER_RESOURCE_VIEW_DESC depth_copy_view_description{};
        depth_copy_view_description.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        depth_copy_view_description.ViewDimension =
            D3D11_SRV_DIMENSION_TEXTURE2D;
        depth_copy_view_description.Texture2D.MostDetailedMip = 0u;
        depth_copy_view_description.Texture2D.MipLevels = 1u;
        result = device_->CreateShaderResourceView(
            type_two_depth_texture_.Get(),
            &depth_copy_view_description,
            type_two_depth_view_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "type2-depth-view");

        const auto create_uint_texture = [&](ComPtr<ID3D11Texture2D>& texture,
                                             ComPtr<ID3D11ShaderResourceView>*
                                                 const shader_view,
                                             ComPtr<ID3D11UnorderedAccessView>&
                                                 unordered_view,
                                             const char* const operation) {
            D3D11_TEXTURE2D_DESC description{};
            description.Width = width;
            description.Height = height;
            description.MipLevels = 1u;
            description.ArraySize = 1u;
            description.Format = DXGI_FORMAT_R32_UINT;
            description.SampleDesc.Count = 1u;
            description.Usage = D3D11_USAGE_DEFAULT;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE |
                                    D3D11_BIND_UNORDERED_ACCESS;
            auto create_result = device_->CreateTexture2D(
                &description, nullptr, texture.GetAddressOf());
            if (FAILED(create_result))
                fail(NativePortGraphicsFailure::ResourceCreation,
                     static_cast<std::uint32_t>(create_result),
                     operation);
            D3D11_SHADER_RESOURCE_VIEW_DESC shader_description{};
            shader_description.Format = DXGI_FORMAT_R32_UINT;
            shader_description.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            shader_description.Texture2D.MostDetailedMip = 0u;
            shader_description.Texture2D.MipLevels = 1u;
            create_result = device_->CreateShaderResourceView(
                texture.Get(), &shader_description, shader_view->GetAddressOf());
            if (FAILED(create_result))
                fail(NativePortGraphicsFailure::ResourceCreation,
                     static_cast<std::uint32_t>(create_result),
                     operation);
            D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_description{};
            unordered_description.Format = DXGI_FORMAT_R32_UINT;
            unordered_description.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            create_result = device_->CreateUnorderedAccessView(
                texture.Get(),
                &unordered_description,
                unordered_view.GetAddressOf());
            if (FAILED(create_result))
                fail(NativePortGraphicsFailure::ResourceCreation,
                     static_cast<std::uint32_t>(create_result),
                     operation);
        };
        create_uint_texture(type_two_head_texture_, &type_two_head_view_,
                            type_two_head_uav_, "type2-head-texture");
        create_uint_texture(type_two_count_texture_, &type_two_count_view_,
                            type_two_count_uav_, "type2-count-texture");

        const auto node_bytes =
            static_cast<std::uint64_t>(config_.maximum_type2_fragment_nodes) *
            sizeof(TypeTwoFragmentGpu);
        if (node_bytes > std::numeric_limits<UINT>::max())
            fail(NativePortGraphicsFailure::ResourceLimit,
                 0u,
                 "type2-node-byte-budget");
        D3D11_BUFFER_DESC node_description{};
        node_description.ByteWidth = static_cast<UINT>(node_bytes);
        node_description.Usage = D3D11_USAGE_DEFAULT;
        node_description.BindFlags = D3D11_BIND_SHADER_RESOURCE |
                                     D3D11_BIND_UNORDERED_ACCESS;
        node_description.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        node_description.StructureByteStride = sizeof(TypeTwoFragmentGpu);
        result = device_->CreateBuffer(
            &node_description, nullptr, type_two_fragment_buffer_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "type2-fragment-buffer");
        D3D11_SHADER_RESOURCE_VIEW_DESC node_shader_description{};
        node_shader_description.Format = DXGI_FORMAT_UNKNOWN;
        node_shader_description.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        node_shader_description.Buffer.FirstElement = 0u;
        node_shader_description.Buffer.NumElements =
            config_.maximum_type2_fragment_nodes;
        result = device_->CreateShaderResourceView(
            type_two_fragment_buffer_.Get(),
            &node_shader_description,
            type_two_fragment_view_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "type2-fragment-view");
        D3D11_UNORDERED_ACCESS_VIEW_DESC node_unordered_description{};
        node_unordered_description.Format = DXGI_FORMAT_UNKNOWN;
        node_unordered_description.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        node_unordered_description.Buffer.FirstElement = 0u;
        node_unordered_description.Buffer.NumElements =
            config_.maximum_type2_fragment_nodes;
        result = device_->CreateUnorderedAccessView(
            type_two_fragment_buffer_.Get(),
            &node_unordered_description,
            type_two_fragment_uav_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "type2-fragment-uav");

        D3D11_BUFFER_DESC status_description{};
        status_description.ByteWidth = sizeof(std::uint32_t) * 4u;
        status_description.Usage = D3D11_USAGE_DEFAULT;
        status_description.BindFlags =
            D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        status_description.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        status_description.StructureByteStride = sizeof(std::uint32_t);
        result = device_->CreateBuffer(
            &status_description, nullptr, type_two_status_buffer_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "type2-status-buffer");
        D3D11_UNORDERED_ACCESS_VIEW_DESC status_unordered_description{};
        status_unordered_description.Format = DXGI_FORMAT_UNKNOWN;
        status_unordered_description.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        status_unordered_description.Buffer.FirstElement = 0u;
        status_unordered_description.Buffer.NumElements = 4u;
        result = device_->CreateUnorderedAccessView(
            type_two_status_buffer_.Get(),
            &status_unordered_description,
            type_two_status_uav_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "type2-status-uav");
        D3D11_SHADER_RESOURCE_VIEW_DESC status_view_description{};
        status_view_description.Format = DXGI_FORMAT_UNKNOWN;
        status_view_description.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        status_view_description.Buffer.FirstElement = 0u;
        status_view_description.Buffer.NumElements = 4u;
        result = device_->CreateShaderResourceView(
            type_two_status_buffer_.Get(),
            &status_view_description,
            type_two_status_view_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "type2-status-view");
        type2_resources_ready_ = true;
    }

    void begin_type2_subpass(const NativePortDrawPacket& packet) {
        if (vulkan_) vulkan_->begin_type_two();
        else {
        ensure_type2_resources();
        // Preserve the opaque/punch color and depth before publishing any
        // Type-2 nodes.  The copied depth is sampled manually in the gather
        // shader because D3D11 does not order UAV side effects after the DSV
        // test unless early depth is used, while our logarithmic SV_Depth is a
        // pixel-stage value and cannot use that shortcut.
        context_->OMSetRenderTargets(0u, nullptr, nullptr);
        std::array<ID3D11ShaderResourceView*, 6u> no_resources{};
        context_->PSSetShaderResources(
            0u,
            static_cast<UINT>(no_resources.size()),
            no_resources.data());
        context_->CopyResource(type_two_base_texture_.Get(),
                               render_texture_.Get());
        context_->CopyResource(type_two_depth_texture_.Get(),
                               depth_texture_.Get());
        // Resolve binds Type-2 resources through t1..t4. Clear the complete
        // range (including the depth copy at t5) before rebinding the same
        // resources as outputs on a later scene.
        bound_shader_resource_ = nullptr;
        bound_shader_resource_valid_ = false;
        constexpr std::array<std::uint32_t, 4u> empty_heads{
            0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
        constexpr std::array<std::uint32_t, 4u> zero_values{
            0u, 0u, 0u, 0u};
        context_->ClearUnorderedAccessViewUint(
            type_two_head_uav_.Get(), empty_heads.data());
        context_->ClearUnorderedAccessViewUint(
            type_two_count_uav_.Get(), zero_values.data());
        context_->ClearUnorderedAccessViewUint(
            type_two_status_uav_.Get(), zero_values.data());
        std::array<ID3D11UnorderedAccessView*, 4u> unordered_views{
            type_two_head_uav_.Get(),
            type_two_fragment_uav_.Get(),
            type_two_count_uav_.Get(),
            type_two_status_uav_.Get()};
        context_->OMSetRenderTargetsAndUnorderedAccessViews(
            0u,
            nullptr,
            depth_view_.Get(),
            1u,
            static_cast<UINT>(unordered_views.size()),
            unordered_views.data(),
            nullptr);
        context_->PSSetShaderResources(
            5u, 1u, type_two_depth_view_.GetAddressOf());
        type_two_uavs_bound_ = true;
}
        type2_subpass_active_ = true;
        type2_gather_active_ = true;
        type2_batch_identity_ = packet.batch.identity;
        type2_list_draw_sequence_ = 0u;
    }

    void unbind_type2_subpass() {
        if (vulkan_) return;
        ID3D11ShaderResourceView* no_depth_resource = nullptr;
        context_->PSSetShaderResources(5u, 1u, &no_depth_resource);
        if (!type_two_uavs_bound_) return;
        std::array<ID3D11UnorderedAccessView*, 4u> no_views{
            nullptr, nullptr, nullptr, nullptr};
        context_->OMSetRenderTargetsAndUnorderedAccessViews(
            0u,
            nullptr,
            nullptr,
            1u,
            static_cast<UINT>(no_views.size()),
            no_views.data(),
            nullptr);
        type_two_uavs_bound_ = false;
    }

    void restore_working_frame_attachments() {
        if (vulkan_) return;
        if (type2_subpass_active_) {
            // Rebind the existing gather, including its live node counters.
            // begin_type2_subpass() would clear the list and copy a new base.
            std::array<ID3D11UnorderedAccessView*, 4u> unordered_views{
                type_two_head_uav_.Get(), type_two_fragment_uav_.Get(),
                type_two_count_uav_.Get(), type_two_status_uav_.Get()};
            context_->OMSetRenderTargetsAndUnorderedAccessViews(
                0u, nullptr, depth_view_.Get(), 1u,
                static_cast<UINT>(unordered_views.size()),
                unordered_views.data(), nullptr);
            context_->PSSetShaderResources(
                5u, 1u, type_two_depth_view_.GetAddressOf());
            type_two_uavs_bound_ = true;
        } else {
            context_->OMSetRenderTargets(
                1u, render_target_.GetAddressOf(), depth_view_.Get());
        }
    }

    void resolve_type2_subpass() {
        if (vulkan_) { vulkan_->resolve_type_two(); return; }
        if (!type2_resources_ready_ || !type2_gather_active_)
            fail(NativePortGraphicsFailure::InvalidFrame,
                 0u,
                 "type2-subpass-state");
        // End all UAV writes before binding the same status buffer as a
        // shader input. The resolve reads its current live-node bound on the
        // GPU; a CPU readback here would serialize every transparency batch.
        unbind_type2_subpass();

        context_->OMSetRenderTargets(
            1u, render_target_.GetAddressOf(), nullptr);
        set_viewport({0u,
                      0u,
                      config_.render_extent.width,
                      config_.render_extent.height});
        NativePortBlendState blend;
        NativePortDepthState depth;
        depth.test_enabled = false;
        depth.write_enabled = false;
        NativePortRasterizerState rasterizer;
        rasterizer.cull = NativePortCullMode::None;
        constexpr std::array blend_factor{0.0f, 0.0f, 0.0f, 0.0f};
        context_->OMSetBlendState(
            resolve_blend_state(blend), blend_factor.data(), 0xFFFFFFFFu);
        context_->OMSetDepthStencilState(resolve_depth_state(depth), 0u);
        context_->RSSetState(resolve_rasterizer_state(rasterizer));
        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(composite_vertex_shader_.Get(), nullptr, 0u);
        context_->PSSetShader(type_two_resolve_pixel_shader_.Get(), nullptr, 0u);
        TypeTwoResolveConstants constants;
        constants.parameters = {
            config_.maximum_type2_fragments_per_pixel,
            0u,
            0u,
            config_.maximum_type2_fragment_nodes};
        context_->UpdateSubresource(type_two_resolve_constants_.Get(),
                                    0u,
                                    nullptr,
                                    &constants,
                                    0u,
                                    0u);
        context_->PSSetConstantBuffers(
            2u, 1u, type_two_resolve_constants_.GetAddressOf());
        ID3D11ShaderResourceView* resources[5u]{
            type_two_base_view_.Get(),
            type_two_head_view_.Get(),
            type_two_fragment_view_.Get(),
            type_two_count_view_.Get(),
            type_two_status_view_.Get()};
        context_->PSSetShaderResources(1u, 5u, resources);
        context_->Draw(3u, 0u);
        std::array<ID3D11ShaderResourceView*, 6u> no_resources{};
        context_->PSSetShaderResources(
            0u,
            static_cast<UINT>(no_resources.size()),
            no_resources.data());
        context_->OMSetRenderTargets(
            1u, render_target_.GetAddressOf(), depth_view_.Get());
    }

    void start_render_submit_telemetry() noexcept {
        stop_render_submit_telemetry();
        if (telemetry_writer_.has_value())
            render_submit_timer_.emplace(
                *telemetry_writer_, NativePortTelemetryStage::RenderSubmit);
    }

    void stop_render_submit_telemetry() noexcept {
        if (!render_submit_timer_.has_value()) return;
        render_submit_timer_->stop();
        render_submit_timer_.reset();
    }

    void flush_render_telemetry() noexcept {
        if (telemetry_writer_.has_value() && config_.telemetry != nullptr)
            config_.telemetry->publish(*telemetry_writer_);
    }

    void disable_gpu_timing() noexcept {
        for (auto& slot : gpu_timing_queries_) {
            slot.disjoint.Reset();
            slot.begin.Reset();
            slot.end.Reset();
            slot.state = GpuTimingState::Free;
        }
        active_gpu_timing_query_ = no_gpu_timing_query;
        gpu_timing_enabled_ = false;
    }

    void initialize_gpu_timing_queries() noexcept {
        if (vulkan_) return;
        if (device_ == nullptr || context_ == nullptr ||
            !telemetry_writer_.has_value())
            return;
        D3D11_QUERY_DESC disjoint_description{};
        disjoint_description.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
        D3D11_QUERY_DESC timestamp_description{};
        timestamp_description.Query = D3D11_QUERY_TIMESTAMP;
        for (auto& slot : gpu_timing_queries_) {
            if (FAILED(device_->CreateQuery(
                    &disjoint_description, slot.disjoint.GetAddressOf())) ||
                FAILED(device_->CreateQuery(
                    &timestamp_description, slot.begin.GetAddressOf())) ||
                FAILED(device_->CreateQuery(
                    &timestamp_description, slot.end.GetAddressOf()))) {
                disable_gpu_timing();
                return;
            }
        }
        gpu_timing_enabled_ = true;
    }

    void retire_gpu_timing_queries_nonblocking() noexcept {
        if (!gpu_timing_enabled_ || context_ == nullptr ||
            !telemetry_writer_.has_value())
            return;
        for (auto& slot : gpu_timing_queries_) {
            if (slot.state != GpuTimingState::Pending &&
                slot.state != GpuTimingState::DiscardPending)
                continue;
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
            const auto disjoint_result = context_->GetData(
                slot.disjoint.Get(), &disjoint, sizeof(disjoint),
                D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (disjoint_result == S_FALSE) continue;
            if (FAILED(disjoint_result)) {
                disable_gpu_timing();
                return;
            }
            if (slot.state == GpuTimingState::DiscardPending) {
                slot.state = GpuTimingState::Free;
                continue;
            }
            UINT64 begin = 0u;
            UINT64 end = 0u;
            const auto begin_result = context_->GetData(
                slot.begin.Get(), &begin, sizeof(begin),
                D3D11_ASYNC_GETDATA_DONOTFLUSH);
            const auto end_result = context_->GetData(
                slot.end.Get(), &end, sizeof(end),
                D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (begin_result == S_FALSE || end_result == S_FALSE) continue;
            if (FAILED(begin_result) || FAILED(end_result)) {
                disable_gpu_timing();
                return;
            }
            if (!disjoint.Disjoint && disjoint.Frequency != 0u &&
                end >= begin) {
                const auto elapsed =
                    static_cast<long double>(end - begin) *
                    static_cast<long double>(nanoseconds_per_second) /
                    static_cast<long double>(disjoint.Frequency);
                const auto maximum = static_cast<long double>(
                    std::numeric_limits<std::uint64_t>::max());
                const auto elapsed_ns = elapsed >= maximum
                                            ? std::numeric_limits<
                                                  std::uint64_t>::max()
                                            : static_cast<std::uint64_t>(
                                                  elapsed);
                telemetry_writer_->add(
                    NativePortTelemetryStage::GpuTime, elapsed_ns, 1u);
            }
            slot.state = GpuTimingState::Free;
        }
    }

    void begin_gpu_timing_frame() noexcept {
        retire_gpu_timing_queries_nonblocking();
        if (!gpu_timing_enabled_ || context_ == nullptr ||
            active_gpu_timing_query_ != no_gpu_timing_query)
            return;
        for (std::size_t offset = 0u; offset < gpu_timing_query_count;
             ++offset) {
            const auto index = (next_gpu_timing_query_ + offset) %
                gpu_timing_query_count;
            auto& slot = gpu_timing_queries_[index];
            if (slot.state != GpuTimingState::Free) continue;
            context_->Begin(slot.disjoint.Get());
            context_->End(slot.begin.Get());
            slot.state = GpuTimingState::Recording;
            active_gpu_timing_query_ = index;
            next_gpu_timing_query_ = (index + 1u) % gpu_timing_query_count;
            return;
        }
    }

    void end_gpu_timing_frame(const bool discard = false) noexcept {
        if (!gpu_timing_enabled_ || context_ == nullptr ||
            active_gpu_timing_query_ == no_gpu_timing_query)
            return;
        auto& slot = gpu_timing_queries_[active_gpu_timing_query_];
        context_->End(slot.end.Get());
        context_->End(slot.disjoint.Get());
        slot.state = discard ? GpuTimingState::DiscardPending
                             : GpuTimingState::Pending;
        active_gpu_timing_query_ = no_gpu_timing_query;
    }

    void abandon_gpu_timing_frame() noexcept {
        end_gpu_timing_frame(true);
    }

    void ensure_performance_overlay_pipeline() {
        if (performance_overlay_pixel_shader_ != nullptr &&
            performance_overlay_constants_ != nullptr)
            return;
        const auto pixel_bytecode = compile_shader(
            "performance_overlay_pixel_main", "ps_4_0");
        auto result = device_->CreatePixelShader(
            pixel_bytecode->GetBufferPointer(),
            pixel_bytecode->GetBufferSize(),
            nullptr,
            performance_overlay_pixel_shader_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "performance-overlay-pixel-shader");
        D3D11_BUFFER_DESC description{};
        description.ByteWidth = sizeof(PerformanceOverlayConstants);
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        result = device_->CreateBuffer(
            &description, nullptr,
            performance_overlay_constants_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "performance-overlay-constants");
    }

    void draw_performance_overlay() {
        if (runtime_options_ == nullptr ||
            !runtime_options_->performance_overlay_enabled.load(
                std::memory_order_acquire))
            return;
        constexpr std::uint32_t overlay_origin = 12u;
        if (output_extent_.width <= overlay_origin ||
            output_extent_.height <= overlay_origin)
            return;
        ensure_performance_overlay_pipeline();

        const auto output_fps = std::isfinite(runtime_menu_output_fps_)
                                    ? std::clamp(runtime_menu_output_fps_,
                                                 0.0, 999.9)
                                    : 0.0;
        const auto simulation_fps =
            std::isfinite(runtime_menu_simulation_fps_)
                ? std::clamp(runtime_menu_simulation_fps_, 0.0, 999.9)
                : 0.0;
        if (output_fps != performance_overlay_output_fps_ ||
            simulation_fps != performance_overlay_simulation_fps_) {
            PerformanceOverlayConstants constants{};
            char output_text[16]{};
            char simulation_text[16]{};
            static_cast<void>(std::snprintf(
                output_text, std::size(output_text), "FPS %5.1f", output_fps));
            static_cast<void>(std::snprintf(
                simulation_text, std::size(simulation_text), "SIM %5.1f",
                simulation_fps));
            rasterize_performance_overlay_text(
                constants, std::string_view(output_text), 2u);
            rasterize_performance_overlay_text(
                constants, std::string_view(simulation_text), 17u);

            D3D11_MAPPED_SUBRESOURCE mapped{};
            const auto map_result = context_->Map(
                performance_overlay_constants_.Get(), 0u,
                D3D11_MAP_WRITE_DISCARD, 0u, &mapped);
            if (FAILED(map_result))
                fail(NativePortGraphicsFailure::ResourceCreation,
                     static_cast<std::uint32_t>(map_result),
                     "performance-overlay-map");
            std::memcpy(mapped.pData, &constants, sizeof(constants));
            context_->Unmap(performance_overlay_constants_.Get(), 0u);
            performance_overlay_output_fps_ = output_fps;
            performance_overlay_simulation_fps_ = simulation_fps;
        }

        NativePortBlendState blend;
        blend.enabled = true;
        blend.source_color = NativePortBlendFactor::SourceAlpha;
        blend.destination_color = NativePortBlendFactor::InverseSourceAlpha;
        blend.source_alpha = NativePortBlendFactor::One;
        blend.destination_alpha = NativePortBlendFactor::InverseSourceAlpha;
        NativePortDepthState depth;
        depth.test_enabled = false;
        depth.write_enabled = false;
        NativePortRasterizerState rasterizer;
        rasterizer.cull = NativePortCullMode::None;
        constexpr std::array blend_factor{0.0f, 0.0f, 0.0f, 0.0f};
        set_viewport({overlay_origin,
                      overlay_origin,
                      std::min(performance_overlay_width,
                               output_extent_.width - overlay_origin),
                      std::min(performance_overlay_height,
                               output_extent_.height - overlay_origin)});
        context_->OMSetBlendState(
            resolve_blend_state(blend), blend_factor.data(), 0xFFFFFFFFu);
        context_->OMSetDepthStencilState(resolve_depth_state(depth), 0u);
        context_->RSSetState(resolve_rasterizer_state(rasterizer));
        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(composite_vertex_shader_.Get(), nullptr, 0u);
        context_->PSSetShader(
            performance_overlay_pixel_shader_.Get(), nullptr, 0u);
        auto* const constant_buffer = performance_overlay_constants_.Get();
        context_->PSSetConstantBuffers(2u, 1u, &constant_buffer);
        context_->Draw(3u, 0u);
        ID3D11Buffer* no_buffer = nullptr;
        context_->PSSetConstantBuffers(2u, 1u, &no_buffer);
        invalidate_draw_state_shadow();
    }

    NativePortBackendPresentOutcome present_completed_frame(
        const char* const operation) {
        poll_events();
        // Resize may have restored an open gather while polling events.
        // Composite output must have no working-frame UAVs bound.
        if (frame_open_) unbind_type2_subpass();
        update_runtime_options_menu();
        if (minimized_) {
            end_gpu_timing_frame();
            stop_render_submit_telemetry();
            flush_render_telemetry();
            snapshot_.occluded = true;
            return NativePortBackendPresentOutcome::Occluded;
        }
        if (vulkan_) {
            if (inject_present_failure_once_) {
                inject_present_failure_once_ = false;
                fail(NativePortGraphicsFailure::DeviceLost, static_cast<std::uint32_t>(E_FAIL), operation);
            }
            const bool nonblocking = nonblocking_output_enabled();
            if (nonblocking && inject_present_busy_once_) {
                inject_present_busy_once_ = false;
                return NativePortBackendPresentOutcome::Deferred;
            }
            PerformanceOverlayConstants overlay{};
            const bool show_overlay = runtime_options_ && runtime_options_->performance_overlay_enabled.load(std::memory_order_acquire);
            if (show_overlay) {
                char output[16]{}, simulation[16]{};
                std::snprintf(output, sizeof(output), "FPS %5.1f", std::clamp(runtime_menu_output_fps_, 0.0, 999.9));
                std::snprintf(simulation, sizeof(simulation), "SIM %5.1f", std::clamp(runtime_menu_simulation_fps_, 0.0, 999.9));
                rasterize_performance_overlay_text(overlay, output, 2u);
                rasterize_performance_overlay_text(overlay, simulation, 17u);
            }
            const auto presented = vulkan_->present(completed_output_viewport(), nonblocking,
                show_overlay ? std::as_bytes(std::span(&overlay, 1)) : std::span<const std::byte>{});
            stop_render_submit_telemetry(); flush_render_telemetry();
            if (!presented) return NativePortBackendPresentOutcome::Deferred;
            snapshot_.occluded = false;
            capture_completed_frame(snapshot_.presented_frames + 1u);
            saturating_increment(snapshot_.presented_frames);
            maybe_checkpoint_graphics_breadcrumbs();
            return NativePortBackendPresentOutcome::Presented;
        }
        context_->OMSetRenderTargets(
            1u, swap_chain_target_.GetAddressOf(), nullptr);
        constexpr std::array black{0.0f, 0.0f, 0.0f, 1.0f};
        context_->ClearRenderTargetView(swap_chain_target_.Get(), black.data());
        set_viewport(completed_output_viewport());
        constexpr std::array blend_factor{0.0f, 0.0f, 0.0f, 0.0f};
        NativePortBlendState composite_blend;
        NativePortDepthState composite_depth;
        composite_depth.test_enabled = false;
        composite_depth.write_enabled = false;
        NativePortRasterizerState composite_rasterizer;
        composite_rasterizer.cull = NativePortCullMode::None;
        NativePortSamplerState composite_sampler;
        context_->OMSetBlendState(
            resolve_blend_state(composite_blend), blend_factor.data(),
            0xFFFFFFFFu);
        context_->OMSetDepthStencilState(
            resolve_depth_state(composite_depth), 0u);
        context_->RSSetState(resolve_rasterizer_state(composite_rasterizer));
        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(composite_vertex_shader_.Get(), nullptr, 0u);
        context_->PSSetShader(composite_pixel_shader_.Get(), nullptr, 0u);
        auto* const sampler = resolve_sampler_state(composite_sampler);
        context_->PSSetSamplers(0u, 1u, &sampler);
        auto* const completed_source = completed_host_image_ ? completed_host_view_.Get() : completed_view_.Get();
        context_->PSSetShaderResources(0u, 1u, &completed_source);
        context_->Draw(3u, 0u);
        ID3D11ShaderResourceView* no_view = nullptr;
        context_->PSSetShaderResources(0u, 1u, &no_view);
        draw_performance_overlay();
        invalidate_draw_state_shadow();
        end_gpu_timing_frame();
        stop_render_submit_telemetry();
        std::optional<NativePortTelemetryTimer> present_wait_timer;
        if (telemetry_writer_.has_value())
            present_wait_timer.emplace(
                *telemetry_writer_, NativePortTelemetryStage::PresentWait);
        if (inject_present_failure_once_) {
            inject_present_failure_once_ = false;
            fail(NativePortGraphicsFailure::DeviceLost,
                 static_cast<std::uint32_t>(E_FAIL),
                 operation);
        }
        // DXGI's DO_NOT_WAIT result is an output-deadline miss, not device
        // loss. Keep this to the software-paced flip model path; direct and
        // SerialReference presentation retain their established contract.
        // Flycast's D3D11 presenter uses the same flag/result pair in
        // core/rend/dx11/dx11context.cpp.
        // VSync controls scanout, not whether this shared render owner may
        // sleep while the simulation awaits a resource/completion reply.
        const bool nonblocking = nonblocking_output_enabled();
        // A single queued frame causes DO_NOT_WAIT to reject otherwise
        // useful output deadlines while DWM still owns the preceding image.
        // Bound independent output to two frames; the serial path keeps one.
        // Change this only on a policy transition, never query COM per frame.
        const auto maximum_latency = nonblocking ? present_queue_limit_ : 1u;
        if (dxgi_device_ && applied_present_queue_limit_ != maximum_latency) {
            const auto latency_result =
                dxgi_device_->SetMaximumFrameLatency(maximum_latency);
            if (FAILED(latency_result))
                fail(NativePortGraphicsFailure::DeviceLost,
                     static_cast<std::uint32_t>(latency_result),
                     "present-queue-limit");
            applied_present_queue_limit_ = maximum_latency;
        }
        HRESULT result = S_OK;
        if (nonblocking && inject_present_busy_once_) {
            inject_present_busy_once_ = false;
            result = DXGI_ERROR_WAS_STILL_DRAWING;
        } else {
            result = swap_chain_->Present(
                config_.synchronize_present ? 1u : 0u,
                nonblocking ? DXGI_PRESENT_DO_NOT_WAIT : 0u);
        }
        if (present_wait_timer.has_value()) present_wait_timer->stop();
        flush_render_telemetry();
        if (nonblocking && result == DXGI_ERROR_WAS_STILL_DRAWING)
            return NativePortBackendPresentOutcome::Deferred;
        if (result == DXGI_STATUS_OCCLUDED) {
            snapshot_.occluded = true;
            // Explicit diagnostics must remain useful for bounded background
            // gates even when DXGI declines presentation. The immutable
            // render target is complete here; begun_frames is the logical
            // native frame identity and does not advance on repeat_present.
            capture_completed_frame(snapshot_.begun_frames);
            return NativePortBackendPresentOutcome::Occluded;
        }
        if (FAILED(result)) {
            const auto removed = device_->GetDeviceRemovedReason();
            fail(NativePortGraphicsFailure::DeviceLost,
                 static_cast<std::uint32_t>(FAILED(removed) ? removed : result),
                 operation);
        }
        snapshot_.occluded = false;
        capture_completed_frame(snapshot_.presented_frames + 1u);
        saturating_increment(snapshot_.presented_frames);
        maybe_checkpoint_graphics_breadcrumbs();
        return NativePortBackendPresentOutcome::Presented;
    }

  public:

    void present_image(const NativePortImageView& image,
                       const NativePortViewportTarget viewport,
                       const NativePortImageFit fit,
                       const bool defer_presentation = false) {
        require_owner_thread();
        if (!valid_viewport_target(viewport) || !valid_image_fit(fit))
            fail(NativePortGraphicsFailure::InvalidFrame,
                 0u,
                 "image-presentation-policy");
        validate_image(image, image.extent, image.format);
        if (!image_texture_ || image_texture_extent_.width != image.extent.width ||
            image_texture_extent_.height != image.extent.height ||
            image_texture_format_ != image.format) {
            if (image_texture_) destroy_texture(image_texture_);
            NativePortTextureConfig texture_config;
            texture_config.extent = image.extent;
            texture_config.format = image.format;
            texture_config.dynamic = true;
            image_texture_ = create_texture(
                texture_config, std::span<const NativePortImageView>{});
            image_texture_extent_ = image.extent;
            image_texture_format_ = image.format;
        }
        update_texture(image_texture_, image);
        begin_frame({});

        // Opaque host menus are already rasterized at client resolution. Keep
        // their pixels out of the lower-resolution game framebuffer entirely.
        // The backend owns image_texture_; it changes only within this atomic
        // PresentImage command. Ordinary frame completion restores game output.
        if (viewport == NativePortViewportTarget::Ui &&
            fit == NativePortImageFit::Stretch &&
            image.extent == cached_layout_.output_extent &&
            image.format == NativePortTextureFormat::Rgba8Unorm) {
            complete_frame();
            const auto& slot = resolve_texture(image_texture_);
            if (vulkan_) vulkan_->complete_host_image(slot.vulkan_texture);
            else {
                completed_host_texture_ = slot.texture;
                completed_host_view_ = slot.view;
            }
            completed_host_extent_ = image.extent;
            completed_host_image_ = true;
            if (!defer_presentation) static_cast<void>(repeat_present("present-image"));
            return;
        }

        // Output-sized host dialogs are rasterized in client coordinates. They
        // span the window even when the original game's viewport is 4:3.
        // This scoped override never changes guest draws or movie fitting.
        struct RestoreUiViewport {
            NativePortGraphicsLayout& layout;
            NativePortPixelRect saved;
            ~RestoreUiViewport() { layout.ui_viewport = saved; }
        } restore_ui_viewport{cached_layout_, cached_layout_.ui_viewport};
        if (viewport == NativePortViewportTarget::Ui &&
            fit == NativePortImageFit::Stretch &&
            image.extent == cached_layout_.output_extent)
            cached_layout_.ui_viewport = {0u, 0u, config_.render_extent.width,
                                          config_.render_extent.height};
        const auto& current_layout = cached_layout_;
        const auto target = viewport == NativePortViewportTarget::Ui
                                ? current_layout.ui_viewport
                                : current_layout.game_viewport;
        auto destination = target;
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 1.0f;
        float v1 = 1.0f;
        const NativePortAspectRatio source_aspect =
            image.display_aspect_numerator != 0u
                ? NativePortAspectRatio{image.display_aspect_numerator,
                                        image.display_aspect_denominator}
                : NativePortAspectRatio{image.extent.width,
                                        image.extent.height};
        if (fit == NativePortImageFit::Contain) {
            destination = fit_aspect(target, source_aspect);
        } else if (fit == NativePortImageFit::Cover &&
                   target.width != 0u && target.height != 0u) {
            const auto target_ratio =
                static_cast<double>(target.width) / target.height;
            const auto source_ratio =
                static_cast<double>(source_aspect.numerator) /
                source_aspect.denominator;
            if (source_ratio > target_ratio) {
                const auto visible = static_cast<float>(target_ratio / source_ratio);
                u0 = (1.0f - visible) * 0.5f;
                u1 = 1.0f - u0;
            } else if (source_ratio < target_ratio) {
                const auto visible = static_cast<float>(source_ratio / target_ratio);
                v0 = (1.0f - visible) * 0.5f;
                v1 = 1.0f - v0;
            }
        }

        const auto relative_x = static_cast<float>(destination.x - target.x);
        const auto relative_y = static_cast<float>(destination.y - target.y);
        const auto left = -1.0f + 2.0f * relative_x / target.width;
        const auto right = -1.0f +
                           2.0f * (relative_x + destination.width) /
                               target.width;
        const auto top = 1.0f - 2.0f * relative_y / target.height;
        const auto bottom = 1.0f -
                            2.0f * (relative_y + destination.height) /
                                target.height;
        const std::array vertices{
            NativePortVertex{{left, top, 0.0f}, {u0, v0}},
            NativePortVertex{{right, top, 0.0f}, {u1, v0}},
            NativePortVertex{{right, bottom, 0.0f}, {u1, v1}},
            NativePortVertex{{left, bottom, 0.0f}, {u0, v1}},
        };
        constexpr std::array<std::uint32_t, 6u> indices{0u, 1u, 2u, 0u, 2u, 3u};
        NativePortDrawPacket packet;
        packet.vertices = vertices;
        packet.indices = indices;
        packet.texture = image_texture_;
        packet.texture_stage = NativePortTextureStage::RequiredResolved;
        packet.viewport = viewport;
        packet.draw_class = NativePortDrawClass::Overlay;
        packet.batch.identity = 1u;
        packet.batch.semantic =
            viewport == NativePortViewportTarget::Ui
                ? NativePortDrawBatchClass::UiOverlay
                : NativePortDrawBatchClass::GameOverlay;
        packet.batch.submission_order = 0u;
        packet.depth.test_enabled = false;
        packet.depth.write_enabled = false;
        packet.rasterizer.cull = NativePortCullMode::None;
        draw(packet);
        if (defer_presentation) complete_frame();
        else present();
    }

    [[nodiscard]] NativePortGraphicsSnapshot snapshot() const {
        require_owner_thread();
        auto result = snapshot_;
        result.live_textures = live_textures_;
        result.live_meshes = live_meshes_;
        result.persistent_mesh_bytes = mesh_bytes_;
        result.hardware_accelerated = vulkan_ != nullptr || device_ != nullptr;
        result.frame_open = frame_open_;
        return result;
    }

  private:
    struct DrawConstants final {
        std::array<float, 16u> transform{};
        std::array<float, 16u> normal_transform{};
        std::array<float, 4u> material_diffuse{};
        std::array<float, 4u> material_ambient{};
        std::array<float, 4u> material_specular{};
        std::array<float, 4u> material_emission{};
        std::array<float, 4u> scene_ambient{};
        std::array<float, 4u> fog_color{};
        std::array<float, 4u> color_clamp_minimum{};
        std::array<float, 4u> color_clamp_maximum{};
        std::array<std::array<float, 4u>,
                   native_port_maximum_directional_lights> light_directions{};
        std::array<std::array<float, 4u>,
                   native_port_maximum_directional_lights> light_colors{};
        std::array<float, 4u> fog_parameters{};
        std::array<float, 4u> depth_parameters{};
        std::array<float, 4u> material_parameters{};
        std::array<std::uint32_t, 4u> pipeline_flags{};
        // x stores the packet's four fixed-function blend factors, y stores
        // source/destination secondary selectors, z is the stable packet
        // submission sequence and w is the bounded allocator capacity. These
        // values are consumed only by the Type-2 capture shader; ordinary
        // draws keep them zero.
        std::array<std::uint32_t, 4u> type_two_parameters{};
        // Pixel-stage logical clipping maps the selected viewport back to the
        // packet's logical raster extent.  x/y are viewport pixels per logical
        // pixel and z/w are the viewport origin; the bounds are left/top/right/
        // bottom logical pixels and the mode lives in logical_clip_parameters.x.
        std::array<float, 4u> logical_clip_transform{};
        std::array<float, 4u> logical_clip_bounds{};
        std::array<std::uint32_t, 4u> logical_clip_parameters{};
    };

    static_assert(sizeof(DrawConstants) % 16u == 0u);

    struct TypeTwoFragmentGpu final {
        std::uint32_t color = 0u;
        float depth = 0.0f;
        std::uint32_t sequence = 0u;
        std::uint32_t primitive = 0u;
        std::uint32_t next = 0xFFFFFFFFu;
        std::uint32_t blend_factors = 0u;
    };

    static_assert(sizeof(TypeTwoFragmentGpu) == 24u);

    struct TypeTwoResolveConstants final {
        std::array<std::uint32_t, 4u> parameters{};
    };

    static_assert(sizeof(TypeTwoResolveConstants) == 16u);

    struct FogTableConstants final {
        std::array<std::array<float, 4u>,
                   native_port_fog_table_entries / 4u> lookup_table{};
    };

    static_assert(sizeof(FogTableConstants) % 16u == 0u);

    struct BlendStateSlot final {
        NativePortBlendState key;
        ComPtr<ID3D11BlendState> value;
    };

    struct DepthStateSlot final {
        NativePortDepthState key;
        ComPtr<ID3D11DepthStencilState> value;
    };

    struct RasterizerStateSlot final {
        NativePortRasterizerState key;
        ComPtr<ID3D11RasterizerState> value;
    };

    struct SamplerStateSlot final {
        NativePortSamplerState key;
        ComPtr<ID3D11SamplerState> value;
    };

    std::unique_ptr<sonic::rendering::VulkanRenderer> vulkan_;

    struct TextureSlot final {
        std::uint64_t vulkan_texture = 0;
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> view;
        NativePortTextureConfig config;
        std::uint64_t byte_size = 0u;
        std::uint32_t generation = 1u;
        bool live = false;
    };

    static LRESULT CALLBACK window_proc(HWND window,
                                        const UINT message,
                                        const WPARAM word,
                                        const LPARAM data) noexcept {
        auto* self = reinterpret_cast<NativePortGraphicsBackend*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create =
                reinterpret_cast<const CREATESTRUCTW*>(data);
            self = static_cast<NativePortGraphicsBackend*>(
                create->lpCreateParams);
            SetWindowLongPtrW(
                window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self == nullptr) return DefWindowProcW(window, message, word, data);
        if(sonic::input::window_message(window,message,word,data))return 0;
        switch (message) {
        case WM_SYSKEYDOWN:
            if (word == VK_RETURN &&
                (static_cast<std::uintptr_t>(data) & (std::uintptr_t{1u} << 29u))) {
                if ((static_cast<std::uintptr_t>(data) & (std::uintptr_t{1u} << 30u)) == 0u &&
                    !self->toggle_window_mode())
                    std::fprintf(stderr,"SONIC_FULLSCREEN_ERROR win32=%lu\n",GetLastError());
                return 0;
            }
            return DefWindowProcW(window, message, word, data);
        case WM_SYSCHAR:
        case WM_SYSKEYUP:
            if (word == VK_RETURN) return 0;
            return DefWindowProcW(window, message, word, data);
        case WM_KEYDOWN:
            if (word == VK_F1 && (GetKeyState(VK_CONTROL) & 0x8000)) {
                if ((static_cast<std::uintptr_t>(data) &
                     (std::uintptr_t{1u} << 30u)) == 0u)
                    static_cast<void>(self->handle_runtime_menu_command(
                        runtime_menu_performance_overlay));
                return 0;
            }
            if (word == VK_F6) {
                if ((static_cast<std::uintptr_t>(data) &
                     (std::uintptr_t{1u} << 30u)) == 0u)
                    static_cast<void>(self->handle_runtime_menu_command(
                        runtime_menu_keyboard_controls));
                return 0;
            }
            if (word == VK_F5 || word == VK_F9) {
                if ((static_cast<std::uintptr_t>(data) &
                     (std::uintptr_t{1u} << 30u)) == 0u)
                    self->quick_development_state(
                        word == VK_F5
                            ? NativePortDevelopmentStateOperation::Save
                            : NativePortDevelopmentStateOperation::Load);
                return 0;
            }
            return DefWindowProcW(window, message, word, data);
        case WM_COMMAND:
            if (HIWORD(word) == 0u &&
                self->handle_runtime_menu_command(
                    static_cast<UINT>(LOWORD(word))))
                return 0;
            return DefWindowProcW(window, message, word, data);
        case WM_CLOSE:
            self->close_requested_ = true;
            return 0;
        case WM_WINDOWPOSCHANGED:
            self->display_rate_dirty_=true;
            return DefWindowProcW(window, message, word, data);
        case WM_DISPLAYCHANGE:
            self->display_change_pending_=true;
            return 0;
        case WM_ACTIVATEAPP:
            // DXGI can relinquish exclusive ownership on focus loss even
            // without changing pixel dimensions. Rebind before next Present.
            if(self->d3d_exclusive_ && self->flip_swap_chain_)
                self->swap_chain_resize_required_=true;
            return DefWindowProcW(window, message, word, data);
        case WM_DPICHANGED:
            if(!self->fullscreen_.active()&&data) {
                const auto& r=*reinterpret_cast<const RECT*>(data);
                SetWindowPos(window,nullptr,r.left,r.top,r.right-r.left,r.bottom-r.top,
                    SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOOWNERZORDER);
            }
            self->display_change_pending_=true;
            return 0;
        case WM_SIZE:
            self->minimized_ = word == SIZE_MINIMIZED;
            if (!self->minimized_) {
                self->pending_output_extent_ = {
                    static_cast<std::uint32_t>(LOWORD(data)),
                    static_cast<std::uint32_t>(HIWORD(data))};
            }
            return 0;
        case WM_DESTROY:
            self->close_requested_ = true;
            return 0;
        case WM_ERASEBKGND:
            return 1;
        default:
            return DefWindowProcW(window, message, word, data);
        }
    }

    bool toggle_window_mode() noexcept {
        if (d3d_exclusive_) {
            if (swap_chain_->SetFullscreenState(FALSE,nullptr)!=S_OK) return false;
            d3d_exclusive_=false;
            swap_chain_resize_required_=flip_swap_chain_;
        }
        if (!fullscreen_.toggle(window_)) return false;
        if (fullscreen_.active() && swap_chain_ &&
            sonic::rendering::selected_window_mode==sonic::rendering::WindowMode::Fullscreen &&
            !background_test_mode_requested()) {
            const auto result=swap_chain_->SetFullscreenState(TRUE,nullptr);
            d3d_exclusive_=result==S_OK;
            if(d3d_exclusive_) swap_chain_resize_required_=flip_swap_chain_;
            if (!d3d_exclusive_)
                std::fprintf(stderr,"SONIC_FULLSCREEN_FALLBACK backend=d3d11 mode=borderless result=%ld\n",result);
        }
        return true;
    }

    [[nodiscard]] bool handle_runtime_menu_command(
        const UINT command) noexcept {
        if (runtime_options_ == nullptr) return false;
        if (command == runtime_menu_keyboard_controls) {
            if (config_.keyboard_controls == nullptr) return true;
            const auto enabled = !config_.keyboard_controls->enabled.load(
                std::memory_order_acquire);
            config_.keyboard_controls->enabled.store(
                enabled, std::memory_order_release);
            update_runtime_options_menu(true);
            return true;
        }
        if (command == runtime_menu_quick_save_state ||
            command == runtime_menu_quick_load_state) {
            quick_development_state(
                command == runtime_menu_quick_save_state
                    ? NativePortDevelopmentStateOperation::Save
                    : NativePortDevelopmentStateOperation::Load);
            return true;
        }
        if (command == runtime_menu_save_state ||
            command == runtime_menu_load_state) {
            choose_development_state_path(
                command == runtime_menu_save_state
                    ? NativePortDevelopmentStateOperation::Save
                    : NativePortDevelopmentStateOperation::Load);
            update_runtime_options_menu(true);
            return true;
        }
        if (command == runtime_menu_performance_overlay) {
            const auto enabled =
                !runtime_options_->performance_overlay_enabled.load(
                    std::memory_order_acquire);
            runtime_options_->performance_overlay_enabled.store(
                enabled, std::memory_order_release);
            update_runtime_options_menu(true);
            return true;
        }
        if (command < runtime_menu_rate_first ||
            command > runtime_menu_rate_last)
            return false;
        const auto index = static_cast<std::size_t>(
            command - runtime_menu_rate_first);
        const auto rate_hz = runtime_presentation_rate_choices[index];
        const auto simulation_rate_hz =
            runtime_options_->simulation_rate_hz.load(
                std::memory_order_acquire);
        const auto maximum_rate_hz =
            runtime_options_->maximum_presentation_rate_hz.load(
                std::memory_order_acquire);
        if (sonic::presentation::settings().vsync==1 ||
            !runtime_options_->frame_pacing_enabled.load(
                std::memory_order_acquire) ||
            rate_hz < simulation_rate_hz || rate_hz > maximum_rate_hz)
            return true;
        runtime_options_->requested_presentation_rate_hz.store(
            rate_hz, std::memory_order_release);
        update_runtime_options_menu(true);
        return true;
    }

    [[nodiscard]] bool publish_development_state_request(
        const NativePortDevelopmentStateOperation operation,
        const std::string_view path) {
        if (runtime_options_ == nullptr) return false;
        const auto published = runtime_options_->state_request_publication.load(
            std::memory_order_acquire);
        if (runtime_options_->state_request_consumed.load(
                std::memory_order_acquire) != published)
            return false;
        if (path.empty() || path.find('\0') != std::string_view::npos ||
            path.size() >= runtime_options_->state_request_path.size())
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidConfig, 0u,
                "development-state-path-size");
        if (published == std::numeric_limits<std::uint64_t>::max())
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::RenderThreadContract, 0u,
                "development-state-request-sequence");
        std::memcpy(runtime_options_->state_request_path.data(),
                    path.data(), path.size());
        runtime_options_->state_request_path[path.size()] = '\0';
        runtime_options_->state_request_path_bytes =
            static_cast<std::uint32_t>(path.size());
        runtime_options_->state_request_operation = operation;
        runtime_options_->state_request_publication.store(
            published + 1u, std::memory_order_release);
        return true;
    }

    void quick_development_state(
        const NativePortDevelopmentStateOperation operation) noexcept {
        if (runtime_options_ == nullptr) return;
        const auto published = runtime_options_->state_request_publication.load(
            std::memory_order_acquire);
        if (runtime_options_->state_request_consumed.load(
                std::memory_order_acquire) != published) {
            std::fputs("KATANA_QUICK_STATE ignored=request-pending\n", stderr);
            return;
        }
        if (config_.development_state_directory.empty()) {
            std::fputs("KATANA_QUICK_STATE rejected=directory-empty\n", stderr);
            return;
        }
        try {
            const std::filesystem::path directory(
                utf8_to_wide(config_.development_state_directory));
            const auto path = directory / L"quicksave.kstate";
            const auto utf8_path = wide_to_utf8(path.native());
            if (operation == NativePortDevelopmentStateOperation::Save) {
                std::error_code error;
                std::filesystem::create_directories(directory, error);
                if (error) {
                    std::fputs("KATANA_QUICK_STATE rejected=directory-create\n", stderr);
                    return;
                }
            }
            if (!publish_development_state_request(operation, utf8_path))
                std::fputs("KATANA_QUICK_STATE ignored=request-pending\n", stderr);
        } catch (...) {
            std::fputs("KATANA_QUICK_STATE rejected=request-create\n", stderr);
        }
    }

    void choose_development_state_path(
        const NativePortDevelopmentStateOperation operation) noexcept {
        try {
            const auto published =
                runtime_options_->state_request_publication.load(
                    std::memory_order_acquire);
            if (runtime_options_->state_request_consumed.load(
                    std::memory_order_acquire) != published) {
                MessageBoxW(window_,
                            L"Der vorherige Save-State-Auftrag wird noch verarbeitet.",
                            L"KatanaRecomp",
                            MB_OK | MB_ICONINFORMATION);
                return;
            }

            std::wstring initial_directory;
            if (!config_.development_state_directory.empty()) {
                std::error_code error;
                const auto directory = std::filesystem::path(
                    utf8_to_wide(config_.development_state_directory));
                if (operation == NativePortDevelopmentStateOperation::Save)
                    std::filesystem::create_directories(directory, error);
                if (error)
                    throw NativePortGraphicsError(
                        NativePortGraphicsFailure::InvalidConfig,
                        static_cast<std::uint32_t>(error.value()),
                        "development-state-directory-create");
                initial_directory = utf8_to_wide(
                    config_.development_state_directory);
            }

            std::vector<wchar_t> path(32'768u, L'\0');
            if (operation == NativePortDevelopmentStateOperation::Save) {
                constexpr std::wstring_view default_name =
                    L"sonic-adventure-state.kstate";
                std::copy(default_name.begin(),
                          default_name.end(),
                          path.begin());
            }
            OPENFILENAMEW dialog{};
            dialog.lStructSize = sizeof(dialog);
            dialog.hwndOwner = window_;
            dialog.lpstrFilter =
                L"Katana Save States (*.kstate)\0*.kstate\0Alle Dateien\0*.*\0";
            dialog.lpstrFile = path.data();
            dialog.nMaxFile = static_cast<DWORD>(path.size());
            dialog.lpstrInitialDir = initial_directory.empty()
                                         ? nullptr
                                         : initial_directory.c_str();
            dialog.lpstrDefExt = L"kstate";
            dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR |
                           OFN_PATHMUSTEXIST;
            BOOL selected = FALSE;
            if (operation == NativePortDevelopmentStateOperation::Save) {
                dialog.Flags |= OFN_OVERWRITEPROMPT;
                selected = GetSaveFileNameW(&dialog);
            } else {
                dialog.Flags |= OFN_FILEMUSTEXIST;
                selected = GetOpenFileNameW(&dialog);
            }
            if (selected == FALSE) {
                if (const auto error = CommDlgExtendedError(); error != 0u)
                    MessageBoxW(window_,
                                L"Der Save-State-Dateidialog ist fehlgeschlagen.",
                                L"KatanaRecomp",
                                MB_OK | MB_ICONERROR);
                return;
            }

            const auto utf8_path = wide_to_utf8(path.data());
            if (!publish_development_state_request(operation, utf8_path))
                MessageBoxW(window_,
                            L"Der vorherige Save-State-Auftrag wird noch verarbeitet.",
                            L"KatanaRecomp", MB_OK | MB_ICONINFORMATION);
        } catch (...) {
            MessageBoxW(window_,
                        L"Der Save-State-Auftrag konnte nicht erstellt werden.",
                        L"KatanaRecomp",
                        MB_OK | MB_ICONERROR);
        }
    }

    void create_runtime_options_menu() {
        auto* const menu_bar = CreateMenu();
        if (menu_bar == nullptr)
            fail(NativePortGraphicsFailure::WindowCreation,
                 GetLastError(),
                 "runtime-menu-bar");
        auto* const options_menu = CreatePopupMenu();
        if (options_menu == nullptr) {
            const auto code = GetLastError();
            DestroyMenu(menu_bar);
            fail(NativePortGraphicsFailure::WindowCreation,
                 code,
                 "runtime-options-menu");
        }
        const auto fail_menu = [&](const char* const operation) {
            const auto code = GetLastError();
            DestroyMenu(options_menu);
            DestroyMenu(menu_bar);
            fail(NativePortGraphicsFailure::WindowCreation,
                 code == 0u ? 1u : code,
                 operation);
        };
        if (AppendMenuW(options_menu,
                        MF_STRING | MF_GRAYED,
                        runtime_menu_output_fps,
                        L"Ausgabe: -- FPS (60 Hz)") == FALSE)
            fail_menu("runtime-menu-output-fps");
        if (AppendMenuW(options_menu,
                        MF_STRING | MF_GRAYED,
                        runtime_menu_simulation_fps,
                        L"Simulation: -- FPS (30 Hz)") == FALSE)
            fail_menu("runtime-menu-simulation-fps");
        if (AppendMenuW(options_menu,
                        MF_STRING,
                        runtime_menu_performance_overlay,
                        L"FPS im Spiel anzeigen") == FALSE)
            fail_menu("runtime-menu-performance-overlay");
        if (AppendMenuW(options_menu, MF_SEPARATOR, 0u, nullptr) == FALSE)
            fail_menu("runtime-menu-separator");
        if (config_.keyboard_controls != nullptr &&
            AppendMenuW(options_menu, MF_STRING, runtime_menu_keyboard_controls,
                        L"P1 Tastatur: Pfeile, Leertaste=A, B=B\tF6") == FALSE)
            fail_menu("runtime-menu-keyboard-controls");
        if (AppendMenuW(options_menu,
                        MF_STRING,
                        runtime_menu_save_state,
                        L"Save State...") == FALSE)
            fail_menu("runtime-menu-save-state");
        if (AppendMenuW(options_menu,
                        MF_STRING,
                        runtime_menu_load_state,
                        L"Load State...") == FALSE)
            fail_menu("runtime-menu-load-state");
        if (AppendMenuW(options_menu, MF_STRING, runtime_menu_quick_save_state,
                        L"Quick Save\tF5") == FALSE)
            fail_menu("runtime-menu-quick-save-state");
        if (AppendMenuW(options_menu, MF_STRING, runtime_menu_quick_load_state,
                        L"Quick Load\tF9") == FALSE)
            fail_menu("runtime-menu-quick-load-state");
        if (AppendMenuW(options_menu, MF_SEPARATOR, 0u, nullptr) == FALSE)
            fail_menu("runtime-menu-state-separator");
        for (std::size_t index = 0u;
             index < runtime_presentation_rate_choices.size();
             ++index) {
            wchar_t label[32]{};
            static_cast<void>(std::swprintf(
                label,
                std::size(label),
                L"%u Hz",
                runtime_presentation_rate_choices[index]));
            if (AppendMenuW(options_menu,
                            MF_STRING,
                            runtime_menu_rate_first +
                                static_cast<UINT>(index),
                            label) == FALSE)
                fail_menu("runtime-menu-rate");
        }
        if (AppendMenuW(menu_bar,
                        MF_POPUP,
                        reinterpret_cast<UINT_PTR>(options_menu),
                        L"&Optionen") == FALSE)
            fail_menu("runtime-menu-options");
        menu_bar_ = menu_bar;
        options_menu_ = options_menu;
    }

    void update_runtime_options_menu(const bool force = false) noexcept {
        if (window_ == nullptr || options_menu_ == nullptr ||
            runtime_options_ == nullptr)
            return;
        const auto elapsed = std::chrono::steady_clock::now()
                                 .time_since_epoch();
        const auto signed_now =
            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed)
                .count();
        const auto now = signed_now <= 0
                             ? 0u
                             : static_cast<std::uint64_t>(signed_now);
        const auto output_frames = snapshot_.presented_frames;
        const auto simulation_frames =
            runtime_options_->simulation_frames.load(
                std::memory_order_acquire);
        constexpr std::uint64_t sample_interval_nanoseconds = 500'000'000u;
        if (runtime_menu_sample_nanoseconds_ == 0u) {
            runtime_menu_sample_nanoseconds_ = now;
            runtime_menu_output_frames_ = output_frames;
            runtime_menu_simulation_frames_ = simulation_frames;
        } else {
            const auto interval = now >= runtime_menu_sample_nanoseconds_
                                      ? now - runtime_menu_sample_nanoseconds_
                                      : 0u;
            if (interval < sample_interval_nanoseconds) {
                if (!force) return;
            } else {
                runtime_menu_output_fps_ =
                    static_cast<double>(
                        output_frames - runtime_menu_output_frames_) *
                    static_cast<double>(nanoseconds_per_second) /
                    static_cast<double>(interval);
                runtime_menu_simulation_fps_ =
                    static_cast<double>(
                        simulation_frames - runtime_menu_simulation_frames_) *
                    static_cast<double>(nanoseconds_per_second) /
                    static_cast<double>(interval);
                runtime_menu_sample_nanoseconds_ = now;
                runtime_menu_output_frames_ = output_frames;
                runtime_menu_simulation_frames_ = simulation_frames;
            }
        }

        const auto simulation_rate_hz =
            runtime_options_->simulation_rate_hz.load(
                std::memory_order_acquire);
        const auto presentation_rate_hz = effective_presentation_rate(
            runtime_options_->presentation_rate_hz.load(std::memory_order_acquire));
        const auto maximum_rate_hz =
            runtime_options_->maximum_presentation_rate_hz.load(
                std::memory_order_acquire);
        const auto requested_rate_hz =
            runtime_options_->requested_presentation_rate_hz.load(
                std::memory_order_acquire);
        const auto pacing_enabled =
            runtime_options_->frame_pacing_enabled.load(
                std::memory_order_acquire);
        wchar_t output_label[96]{};
        wchar_t simulation_label[96]{};
        static_cast<void>(std::swprintf(output_label,
                                        std::size(output_label),
                                        L"Ausgabe: %.1f FPS (%u Hz)",
                                        runtime_menu_output_fps_,
                                        presentation_rate_hz));
        static_cast<void>(std::swprintf(simulation_label,
                                        std::size(simulation_label),
                                        L"Simulation: %.1f FPS (%u Hz)",
                                        runtime_menu_simulation_fps_,
                                        simulation_rate_hz));
        static_cast<void>(ModifyMenuW(options_menu_,
                                      runtime_menu_output_fps,
                                      MF_BYCOMMAND | MF_STRING | MF_GRAYED,
                                      runtime_menu_output_fps,
                                      output_label));
        static_cast<void>(ModifyMenuW(options_menu_,
                                      runtime_menu_simulation_fps,
                                      MF_BYCOMMAND | MF_STRING | MF_GRAYED,
                                      runtime_menu_simulation_fps,
                                      simulation_label));
        const auto overlay_enabled =
            runtime_options_->performance_overlay_enabled.load(
                std::memory_order_acquire);
        static_cast<void>(CheckMenuItem(
            options_menu_,
            runtime_menu_performance_overlay,
            MF_BYCOMMAND | (overlay_enabled ? MF_CHECKED : MF_UNCHECKED)));
        if (config_.keyboard_controls != nullptr) {
            const auto enabled = config_.keyboard_controls->enabled.load(
                std::memory_order_acquire);
            static_cast<void>(CheckMenuItem(
                options_menu_, runtime_menu_keyboard_controls,
                MF_BYCOMMAND | (enabled ? MF_CHECKED : MF_UNCHECKED)));
        }
        const auto state_request_pending =
            runtime_options_->state_request_publication.load(
                std::memory_order_acquire) !=
            runtime_options_->state_request_consumed.load(
                std::memory_order_acquire);
        const auto state_menu_flags =
            MF_BYCOMMAND |
            (state_request_pending ? MF_GRAYED : MF_ENABLED);
        static_cast<void>(EnableMenuItem(
            options_menu_, runtime_menu_save_state, state_menu_flags));
        static_cast<void>(EnableMenuItem(
            options_menu_, runtime_menu_load_state, state_menu_flags));
        UINT selected = 0u;
        for (std::size_t index = 0u;
             index < runtime_presentation_rate_choices.size();
             ++index) {
            const auto rate_hz = runtime_presentation_rate_choices[index];
            const auto command = runtime_menu_rate_first +
                static_cast<UINT>(index);
            const auto enabled = pacing_enabled && sonic::presentation::settings().vsync!=1 &&
                rate_hz >= simulation_rate_hz &&
                rate_hz <= maximum_rate_hz;
            static_cast<void>(EnableMenuItem(
                options_menu_,
                command,
                MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED)));
            if (enabled && rate_hz == requested_rate_hz)
                selected = command;
        }
        if (selected != 0u)
            static_cast<void>(CheckMenuRadioItem(options_menu_,
                                                 runtime_menu_rate_first,
                                                 runtime_menu_rate_last,
                                                 selected,
                                                 MF_BYCOMMAND));
        // The retained developer command state is private. Player settings
        // live in the in-game Options screen; no native menu bar is attached.
    }

    void refresh_display_rate() noexcept {
        display_rate_dirty_=false;
        MONITORINFOEXW info{};info.cbSize=sizeof(info);
        DEVMODEW mode{};mode.dmSize=sizeof(mode);
        // Query only at startup or after window/display changes. Hidden or
        // occluded windows need a bounded cadence even if Present returns early.
        if(GetMonitorInfoW(MonitorFromWindow(window_,MONITOR_DEFAULTTONEAREST),&info)&&
            EnumDisplaySettingsW(info.szDevice,ENUM_CURRENT_SETTINGS,&mode)&&
            mode.dmDisplayFrequency>=20&&mode.dmDisplayFrequency<=1000)
            display_rate_hz_=mode.dmDisplayFrequency;
        if(sonic::presentation::settings().vsync==1&&display_rate_reported_!=display_rate_hz_) {
            std::fprintf(stderr,"SONIC_VSYNC_CLOCK display_hz=%u visible=%u manual_cap=ignored\n",
                display_rate_hz_,unsigned(window_&&IsWindowVisible(window_)));
            display_rate_reported_=display_rate_hz_;
        }
    }

    void create_window() {
        const auto instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW existing{};
        existing.cbSize = sizeof(existing);
        if (GetClassInfoExW(instance, native_graphics_window_class, &existing) ==
            FALSE) {
            WNDCLASSEXW window_class{};
            window_class.cbSize = sizeof(window_class);
            window_class.lpfnWndProc =
                &NativePortGraphicsBackend::window_proc;
            window_class.hInstance = instance;
            window_class.hCursor =
                LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
            window_class.lpszClassName = native_graphics_window_class;
            if (RegisterClassExW(&window_class) == 0u &&
                GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                fail(NativePortGraphicsFailure::WindowCreation,
                     GetLastError(),
                     "window-class");
        }
        RECT bounds{0,
                    0,
                    static_cast<LONG>(config_.output_extent.width),
                    static_cast<LONG>(config_.output_extent.height)};
        const auto style = config_.resizable
                               ? WS_OVERLAPPEDWINDOW
                               : WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                                     WS_MINIMIZEBOX;
        const auto title = utf8_to_wide(config_.title);
        create_runtime_options_menu();
        if (AdjustWindowRect(&bounds, style, FALSE) == FALSE) {
            const auto code = GetLastError();
            DestroyMenu(menu_bar_);
            menu_bar_ = nullptr;
            options_menu_ = nullptr;
            fail(NativePortGraphicsFailure::WindowCreation,
                 code,
                 "window-rect");
        }
        window_ = CreateWindowExW(0,
                                  native_graphics_window_class,
                                  title.c_str(),
                                  style,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  bounds.right - bounds.left,
                                  bounds.bottom - bounds.top,
                                  nullptr,
                                  nullptr,
                                  instance,
                                  this);
        if (window_ == nullptr) {
            const auto code = GetLastError();
            DestroyMenu(menu_bar_);
            menu_bar_ = nullptr;
            options_menu_ = nullptr;
            fail(NativePortGraphicsFailure::WindowCreation,
                 code,
                 "window-create");
        }
        sonic::input::window_created(window_);
        output_extent_ = config_.output_extent;
        pending_output_extent_ = output_extent_;
        update_runtime_options_menu(true);
    }

    void destroy_window() noexcept {
        if (window_ == nullptr) return;
        (void)sonic::input::window_message(window_,WM_DESTROY,0,0);
        fullscreen_.attach_menu_before_destroy(window_);
        SetWindowLongPtrW(window_, GWLP_USERDATA, 0);
        DestroyWindow(window_);
        window_ = nullptr;
        // The command menu is never attached, including fullscreen toggles,
        // so it is owned here rather than destroyed by DestroyWindow.
        if (menu_bar_) DestroyMenu(menu_bar_);
        menu_bar_ = nullptr;
        options_menu_ = nullptr;
    }

    void create_device() {
        constexpr std::array requested_levels{
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        D3D_FEATURE_LEVEL selected = D3D_FEATURE_LEVEL_10_0;
        const auto attempt = [&](DXGI_SWAP_EFFECT effect,
                                 const UINT buffer_count,
                                 const bool include_11_1) {
            swap_chain_.Reset();
            device_.Reset();
            context_.Reset();
            DXGI_SWAP_CHAIN_DESC description{};
            description.BufferDesc.Width = output_extent_.width;
            description.BufferDesc.Height = output_extent_.height;
            description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.SampleDesc.Count = 1u;
            description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            description.BufferCount = buffer_count;
            description.OutputWindow = window_;
            description.Windowed = TRUE;
            description.SwapEffect = effect;
            const auto first = include_11_1 ? 0u : 1u;
            return D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                D3D11_CREATE_DEVICE_SINGLETHREADED |
                    D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                requested_levels.data() + first,
                static_cast<UINT>(requested_levels.size() - first),
                D3D11_SDK_VERSION,
                &description,
                swap_chain_.GetAddressOf(),
                device_.GetAddressOf(),
                &selected,
                context_.GetAddressOf());
        };
        auto result = attempt(DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 2u, true);
        if (result == E_INVALIDARG)
            result = attempt(DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 2u, false);
        flip_swap_chain_ = SUCCEEDED(result);
        if (FAILED(result)) {
            swap_chain_.Reset();
            device_.Reset();
            context_.Reset();
            result = attempt(DXGI_SWAP_EFFECT_DISCARD, 1u, true);
            if (result == E_INVALIDARG)
                result = attempt(DXGI_SWAP_EFFECT_DISCARD, 1u, false);
        }
        if (FAILED(result))
            fail(NativePortGraphicsFailure::HardwareDeviceUnavailable,
                 static_cast<std::uint32_t>(result),
                 "d3d11-device");
        feature_level_ = selected;
        // Alt+Enter is owned by our window handler for both backends. DXGI's
        // implicit toggle would bypass the required flip-buffer invalidation.
        ComPtr<IDXGIFactory> factory;
        result=swap_chain_->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));
        if(SUCCEEDED(result)) result=factory->MakeWindowAssociation(window_,DXGI_MWA_NO_ALT_ENTER);
        if(FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),"swap-chain-window-association");
        if (SUCCEEDED(device_.As(&dxgi_device_))) {
            if (SUCCEEDED(dxgi_device_->SetMaximumFrameLatency(1u)))
                applied_present_queue_limit_ = 1u;
        }
        create_swap_chain_target();
    }

    void create_swap_chain_target() {
        ComPtr<ID3D11Texture2D> back_buffer;
        const auto result = swap_chain_->GetBuffer(
            0u,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(back_buffer.GetAddressOf()));
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "swap-chain-buffer");
        const auto view_result = device_->CreateRenderTargetView(
            back_buffer.Get(), nullptr, swap_chain_target_.GetAddressOf());
        if (FAILED(view_result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(view_result),
                 "swap-chain-target");
    }

    void create_pipeline() {
        sonic::startup::phase("Preparing graphics...",0,feature_level_>=D3D_FEATURE_LEVEL_11_0?7:5);
        const auto draw_vertex_bytecode =
            compile_shader("draw_vertex_main", "vs_4_0");
        const auto draw_pixel_bytecode =
            compile_shader("draw_pixel_main", "ps_4_0");
        const auto composite_vertex_bytecode =
            compile_shader("composite_vertex_main", "vs_4_0");
        const auto composite_pixel_bytecode =
            compile_shader("composite_pixel_main", "ps_4_0");
        ComPtr<ID3DBlob> type_two_capture_bytecode;
        ComPtr<ID3DBlob> type_two_resolve_bytecode;
        if (feature_level_ >= D3D_FEATURE_LEVEL_11_0) {
            type_two_capture_bytecode = compile_type_two_shader(
                "draw_type_two_capture_main",
                "ps_5_0",
                config_.maximum_type2_fragments_per_pixel);
            type_two_resolve_bytecode = compile_type_two_shader(
                "type_two_resolve_pixel_main",
                "ps_5_0",
                config_.maximum_type2_fragments_per_pixel);
        }
        auto check = [&](const HRESULT result, const char* operation) {
            if (FAILED(result))
                fail(NativePortGraphicsFailure::ResourceCreation,
                     static_cast<std::uint32_t>(result),
                     operation);
        };
        check(device_->CreateVertexShader(
                  draw_vertex_bytecode->GetBufferPointer(),
                  draw_vertex_bytecode->GetBufferSize(),
                  nullptr,
                  draw_vertex_shader_.GetAddressOf()),
              "draw-vertex-shader");
        check(device_->CreatePixelShader(
                  draw_pixel_bytecode->GetBufferPointer(),
                  draw_pixel_bytecode->GetBufferSize(),
                  nullptr,
                  draw_pixel_shader_.GetAddressOf()),
              "draw-pixel-shader");
        check(device_->CreateVertexShader(
                  composite_vertex_bytecode->GetBufferPointer(),
                  composite_vertex_bytecode->GetBufferSize(),
                  nullptr,
                  composite_vertex_shader_.GetAddressOf()),
              "composite-vertex-shader");
        check(device_->CreatePixelShader(
                  composite_pixel_bytecode->GetBufferPointer(),
                  composite_pixel_bytecode->GetBufferSize(),
                  nullptr,
                  composite_pixel_shader_.GetAddressOf()),
              "composite-pixel-shader");
        if (type_two_capture_bytecode) {
            check(device_->CreatePixelShader(
                      type_two_capture_bytecode->GetBufferPointer(),
                      type_two_capture_bytecode->GetBufferSize(),
                      nullptr,
                      type_two_capture_pixel_shader_.GetAddressOf()),
                  "type-two-capture-pixel-shader");
            check(device_->CreatePixelShader(
                      type_two_resolve_bytecode->GetBufferPointer(),
                      type_two_resolve_bytecode->GetBufferSize(),
                      nullptr,
                      type_two_resolve_pixel_shader_.GetAddressOf()),
                  "type-two-resolve-pixel-shader");
        }

        constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 8u> elements{
            D3D11_INPUT_ELEMENT_DESC{
                "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u,
                static_cast<UINT>(offsetof(NativePortVertex, position)),
                D3D11_INPUT_PER_VERTEX_DATA, 0u},
            D3D11_INPUT_ELEMENT_DESC{
                "TEXCOORD", 0u, DXGI_FORMAT_R32G32_FLOAT, 0u,
                static_cast<UINT>(offsetof(NativePortVertex, texture_coordinate)),
                D3D11_INPUT_PER_VERTEX_DATA, 0u},
            D3D11_INPUT_ELEMENT_DESC{
                "COLOR", 0u, DXGI_FORMAT_R32G32B32A32_FLOAT, 0u,
                static_cast<UINT>(offsetof(NativePortVertex, color)),
                D3D11_INPUT_PER_VERTEX_DATA, 0u},
            D3D11_INPUT_ELEMENT_DESC{
                "NORMAL", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u,
                static_cast<UINT>(offsetof(NativePortVertex, normal)),
                D3D11_INPUT_PER_VERTEX_DATA, 0u},
            D3D11_INPUT_ELEMENT_DESC{
                "COLOR", 1u, DXGI_FORMAT_R32G32B32A32_FLOAT, 0u,
                static_cast<UINT>(offsetof(NativePortVertex, secondary_color)),
                D3D11_INPUT_PER_VERTEX_DATA, 0u},
            D3D11_INPUT_ELEMENT_DESC{
                "TEXCOORD", 1u, DXGI_FORMAT_R32_FLOAT, 0u,
                static_cast<UINT>(offsetof(NativePortVertex, fog_coordinate)),
                D3D11_INPUT_PER_VERTEX_DATA, 0u},
            D3D11_INPUT_ELEMENT_DESC{
                "TEXCOORD", 2u, DXGI_FORMAT_R32_FLOAT, 0u,
                static_cast<UINT>(offsetof(NativePortVertex, depth_coordinate)),
                D3D11_INPUT_PER_VERTEX_DATA, 0u},
            D3D11_INPUT_ELEMENT_DESC{
                "POSITION", 1u, DXGI_FORMAT_R32_FLOAT, 0u,
                static_cast<UINT>(offsetof(NativePortVertex, position_w)),
                D3D11_INPUT_PER_VERTEX_DATA, 0u},
        };
        check(device_->CreateInputLayout(
                  elements.data(),
                  static_cast<UINT>(elements.size()),
                  draw_vertex_bytecode->GetBufferPointer(),
                  draw_vertex_bytecode->GetBufferSize(),
                  input_layout_.GetAddressOf()),
              "draw-input-layout");

        D3D11_BUFFER_DESC constant_description{};
        constant_description.ByteWidth = sizeof(DrawConstants);
        constant_description.Usage = D3D11_USAGE_DYNAMIC;
        constant_description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constant_description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        check(device_->CreateBuffer(
                  &constant_description, nullptr, draw_constants_.GetAddressOf()),
              "draw-constants");
        FogTableConstants initial_fog_constants{};
        D3D11_SUBRESOURCE_DATA initial_fog_data{};
        initial_fog_data.pSysMem = &initial_fog_constants;
        constant_description.ByteWidth = sizeof(FogTableConstants);
        constant_description.Usage = D3D11_USAGE_DEFAULT;
        constant_description.CPUAccessFlags = 0u;
        check(device_->CreateBuffer(&constant_description,
                                    &initial_fog_data,
                                    fog_table_constants_.GetAddressOf()),
              "fog-table-constants");
        if (type_two_capture_bytecode) {
            D3D11_BUFFER_DESC type_two_constant_description{};
            type_two_constant_description.ByteWidth =
                sizeof(TypeTwoResolveConstants);
            type_two_constant_description.Usage = D3D11_USAGE_DEFAULT;
            type_two_constant_description.BindFlags =
                D3D11_BIND_CONSTANT_BUFFER;
            check(device_->CreateBuffer(&type_two_constant_description,
                                        nullptr,
                                        type_two_resolve_constants_.GetAddressOf()),
                  "type-two-resolve-constants");
        }
    }

    [[nodiscard]] std::size_t pipeline_state_count() const noexcept {
        return blend_states_.size() + depth_states_.size() +
               rasterizer_states_.size() + sampler_states_.size();
    }

    void require_pipeline_state_budget(const char* const operation) {
        if (pipeline_state_count() >= config_.maximum_pipeline_states)
            fail(NativePortGraphicsFailure::ResourceLimit, 0u, operation);
    }

    [[nodiscard]] ID3D11BlendState* resolve_blend_state(
        const NativePortBlendState& key) {
        if (last_blend_key_valid_ && last_blend_key_ == key)
            return last_blend_state_;
        const auto existing = std::find_if(
            blend_states_.begin(), blend_states_.end(),
            [&](const BlendStateSlot& slot) { return slot.key == key; });
        if (existing != blend_states_.end()) {
            last_blend_key_ = key;
            last_blend_key_valid_ = true;
            last_blend_state_ = existing->value.Get();
            return last_blend_state_;
        }
        require_pipeline_state_budget("blend-state-budget");
        D3D11_BLEND_DESC description{};
        auto& target = description.RenderTarget[0];
        target.BlendEnable = key.enabled ? TRUE : FALSE;
        target.SrcBlend = blend_factor(key.source_color);
        target.DestBlend = blend_factor(key.destination_color);
        target.BlendOp = blend_operation(key.color_operation);
        target.SrcBlendAlpha = blend_factor(key.source_alpha);
        target.DestBlendAlpha = blend_factor(key.destination_alpha);
        target.BlendOpAlpha = blend_operation(key.alpha_operation);
        target.RenderTargetWriteMask =
            ((key.color_write_mask & 0x01u) != 0u
                 ? D3D11_COLOR_WRITE_ENABLE_RED
                 : 0u) |
            ((key.color_write_mask & 0x02u) != 0u
                 ? D3D11_COLOR_WRITE_ENABLE_GREEN
                 : 0u) |
            ((key.color_write_mask & 0x04u) != 0u
                 ? D3D11_COLOR_WRITE_ENABLE_BLUE
                 : 0u) |
            ((key.color_write_mask & 0x08u) != 0u
                 ? D3D11_COLOR_WRITE_ENABLE_ALPHA
                 : 0u);
        ComPtr<ID3D11BlendState> state;
        const auto result =
            device_->CreateBlendState(&description, state.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result), "blend-state");
        blend_states_.push_back({key, std::move(state)});
        last_blend_key_ = key;
        last_blend_key_valid_ = true;
        last_blend_state_ = blend_states_.back().value.Get();
        return last_blend_state_;
    }

    [[nodiscard]] ID3D11DepthStencilState* resolve_depth_state(
        const NativePortDepthState& key) {
        if (last_depth_key_valid_ && last_depth_key_ == key)
            return last_depth_state_;
        const auto existing = std::find_if(
            depth_states_.begin(), depth_states_.end(),
            [&](const DepthStateSlot& slot) { return slot.key == key; });
        if (existing != depth_states_.end()) {
            last_depth_key_ = key;
            last_depth_key_valid_ = true;
            last_depth_state_ = existing->value.Get();
            return last_depth_state_;
        }
        require_pipeline_state_budget("depth-state-budget");
        D3D11_DEPTH_STENCIL_DESC description{};
        description.DepthEnable = key.test_enabled ? TRUE : FALSE;
        description.DepthWriteMask = key.write_enabled
                                         ? D3D11_DEPTH_WRITE_MASK_ALL
                                         : D3D11_DEPTH_WRITE_MASK_ZERO;
        description.DepthFunc = comparison_function(key.compare);
        ComPtr<ID3D11DepthStencilState> state;
        const auto result = device_->CreateDepthStencilState(
            &description, state.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result), "depth-state");
        depth_states_.push_back({key, std::move(state)});
        last_depth_key_ = key;
        last_depth_key_valid_ = true;
        last_depth_state_ = depth_states_.back().value.Get();
        return last_depth_state_;
    }

    [[nodiscard]] ID3D11RasterizerState* resolve_rasterizer_state(
        const NativePortRasterizerState& key) {
        if (last_rasterizer_key_valid_ && last_rasterizer_key_ == key)
            return last_rasterizer_state_;
        const auto existing = std::find_if(
            rasterizer_states_.begin(), rasterizer_states_.end(),
            [&](const RasterizerStateSlot& slot) { return slot.key == key; });
        if (existing != rasterizer_states_.end()) {
            last_rasterizer_key_ = key;
            last_rasterizer_key_valid_ = true;
            last_rasterizer_state_ = existing->value.Get();
            return last_rasterizer_state_;
        }
        require_pipeline_state_budget("rasterizer-state-budget");
        D3D11_RASTERIZER_DESC description{};
        description.FillMode = key.fill == NativePortFillMode::Wireframe
                                   ? D3D11_FILL_WIREFRAME
                                   : D3D11_FILL_SOLID;
        description.CullMode = key.cull == NativePortCullMode::Front
                                   ? D3D11_CULL_FRONT
                                   : key.cull == NativePortCullMode::Back
                                         ? D3D11_CULL_BACK
                                         : D3D11_CULL_NONE;
        description.FrontCounterClockwise =
            key.front_counter_clockwise ? TRUE : FALSE;
        description.DepthClipEnable = key.depth_clip_enabled ? TRUE : FALSE;
        description.ScissorEnable = TRUE;
        ComPtr<ID3D11RasterizerState> state;
        const auto result = device_->CreateRasterizerState(
            &description, state.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result), "rasterizer-state");
        rasterizer_states_.push_back({key, std::move(state)});
        last_rasterizer_key_ = key;
        last_rasterizer_key_valid_ = true;
        last_rasterizer_state_ = rasterizer_states_.back().value.Get();
        return last_rasterizer_state_;
    }

    [[nodiscard]] ID3D11SamplerState* resolve_sampler_state(
        const NativePortSamplerState& key) {
        if (last_sampler_key_valid_ && last_sampler_key_ == key)
            return last_sampler_state_;
        const auto existing = std::find_if(
            sampler_states_.begin(), sampler_states_.end(),
            [&](const SamplerStateSlot& slot) { return slot.key == key; });
        if (existing != sampler_states_.end()) {
            last_sampler_key_ = key;
            last_sampler_key_valid_ = true;
            last_sampler_state_ = existing->value.Get();
            return last_sampler_state_;
        }
        require_pipeline_state_budget("sampler-state-budget");
        D3D11_SAMPLER_DESC description{};
        description.Filter = texture_filter(key.filter);
        description.AddressU = texture_address_mode(key.address_u);
        description.AddressV = texture_address_mode(key.address_v);
        description.AddressW = texture_address_mode(key.address_w);
        description.MipLODBias = key.mip_lod_bias;
        description.MaxAnisotropy = key.maximum_anisotropy;
        description.ComparisonFunc = D3D11_COMPARISON_NEVER;
        description.MinLOD = key.minimum_lod;
        description.MaxLOD = key.maximum_lod;
        ComPtr<ID3D11SamplerState> state;
        const auto result = device_->CreateSamplerState(
            &description, state.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result), "sampler-state");
        sampler_states_.push_back({key, std::move(state)});
        last_sampler_key_ = key;
        last_sampler_key_valid_ = true;
        last_sampler_state_ = sampler_states_.back().value.Get();
        return last_sampler_state_;
    }

    void create_render_surface() {
        D3D11_TEXTURE2D_DESC color_description{};
        color_description.Width = config_.render_extent.width;
        color_description.Height = config_.render_extent.height;
        color_description.MipLevels = 1u;
        color_description.ArraySize = 1u;
        color_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        color_description.SampleDesc.Count = 1u;
        color_description.Usage = D3D11_USAGE_DEFAULT;
        color_description.BindFlags =
            D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        auto result = device_->CreateTexture2D(
            &color_description, nullptr, render_texture_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "render-texture");
        result = device_->CreateRenderTargetView(
            render_texture_.Get(), nullptr, render_target_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "render-target");
        result = device_->CreateShaderResourceView(
            render_texture_.Get(), nullptr, render_view_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "render-view");

        result = device_->CreateTexture2D(
            &color_description, nullptr, completed_texture_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result), "completed-texture");
        result = device_->CreateRenderTargetView(
            completed_texture_.Get(), nullptr, completed_target_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result), "completed-target");
        result = device_->CreateShaderResourceView(
            completed_texture_.Get(), nullptr, completed_view_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result), "completed-view");

        D3D11_TEXTURE2D_DESC depth_description{};
        depth_description.Width = config_.render_extent.width;
        depth_description.Height = config_.render_extent.height;
        depth_description.MipLevels = 1u;
        depth_description.ArraySize = 1u;
        // Keep the scene depth in the D24/S8 typeless family so Type-2 can
        // copy it into a shader-readable front-depth snapshot without a
        // format conversion or CPU readback.
        depth_description.Format = DXGI_FORMAT_R24G8_TYPELESS;
        depth_description.SampleDesc.Count = 1u;
        depth_description.Usage = D3D11_USAGE_DEFAULT;
        depth_description.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        result = device_->CreateTexture2D(
            &depth_description, nullptr, depth_texture_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "depth-texture");
        D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_description{};
        depth_view_description.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depth_view_description.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        depth_view_description.Texture2D.MipSlice = 0u;
        result = device_->CreateDepthStencilView(
            depth_texture_.Get(),
            &depth_view_description,
            depth_view_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "depth-view");
    }

    void create_white_texture() {
        constexpr std::uint32_t white = 0xFFFFFFFFu;
        D3D11_TEXTURE2D_DESC description{};
        description.Width = 1u;
        description.Height = 1u;
        description.MipLevels = 1u;
        description.ArraySize = 1u;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1u;
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = &white;
        data.SysMemPitch = sizeof(white);
        auto result = device_->CreateTexture2D(
            &description, &data, white_texture_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "white-texture");
        result = device_->CreateShaderResourceView(
            white_texture_.Get(), nullptr, white_view_.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "white-view");
    }

    void apply_pending_resize() {
        if (pending_output_extent_.width == 0u ||
            pending_output_extent_.height == 0u ||
            (!swap_chain_resize_required_ && pending_output_extent_.width == output_extent_.width &&
             pending_output_extent_.height == output_extent_.height))
            return;
        if (!valid_extent(pending_output_extent_))
            fail(NativePortGraphicsFailure::ResourceLimit,
                 0u,
                 "swap-chain-resize-extent");
        if (vulkan_) {
            vulkan_->resize(pending_output_extent_);
            output_extent_ = pending_output_extent_; refresh_layout();
            saturating_increment(snapshot_.swap_chain_resizes); return;
        }
        unbind_type2_subpass();
        ID3D11ShaderResourceView* no_view = nullptr;
        context_->PSSetShaderResources(0u, 1u, &no_view);
        context_->OMSetRenderTargets(0u, nullptr, nullptr);
        invalidate_draw_state_shadow();
        swap_chain_target_.Reset();
        const auto result = swap_chain_->ResizeBuffers(
            0u,
            pending_output_extent_.width,
            pending_output_extent_.height,
            DXGI_FORMAT_UNKNOWN,
            0u);
        if (FAILED(result))
            fail(NativePortGraphicsFailure::DeviceLost,
                 static_cast<std::uint32_t>(result),
                 "swap-chain-resize");
        swap_chain_resize_required_=false;
        output_extent_ = pending_output_extent_;
        refresh_layout();
        create_swap_chain_target();
        // poll_events() is a public lifecycle boundary and may observe a
        // resize between begin_frame() and draw(). ResizeBuffers requires all
        // targets to be unbound, so restore the unchanged native render
        // surface when that frame is still open.
        if (frame_open_)
            restore_working_frame_attachments();
        saturating_increment(snapshot_.swap_chain_resizes);
    }

    void refresh_layout() noexcept {
        cached_layout_.output_extent = output_extent_;
        cached_layout_.render_extent = config_.render_extent;
        cached_layout_.output_viewport = fit_aspect(
            {0u, 0u, output_extent_.width, output_extent_.height},
            {config_.render_extent.width, config_.render_extent.height});
        cached_layout_.game_viewport =
            configured_viewport(config_.render_extent, config_.game_viewport);
        cached_layout_.ui_viewport =
            configured_viewport(config_.render_extent, config_.ui_viewport);
        switch (config_.camera_aspect_policy) {
        case NativePortCameraAspectPolicy::GameViewport:
            cached_layout_.camera_aspect =
                cached_layout_.game_viewport.height != 0u
                    ? static_cast<float>(cached_layout_.game_viewport.width) /
                          static_cast<float>(cached_layout_.game_viewport.height)
                    : 1.0f;
            break;
        case NativePortCameraAspectPolicy::OutputSurface:
            cached_layout_.camera_aspect =
                output_extent_.height != 0u
                    ? static_cast<float>(output_extent_.width) /
                          static_cast<float>(output_extent_.height)
                    : 1.0f;
            break;
        case NativePortCameraAspectPolicy::Explicit:
            cached_layout_.camera_aspect =
                static_cast<float>(config_.explicit_camera_aspect.numerator) /
                static_cast<float>(config_.explicit_camera_aspect.denominator);
            break;
        }
    }

    [[nodiscard]] GeometryCapabilities validate_geometry(
        const std::span<const NativePortVertex> vertices,
        const std::span<const std::uint32_t> indices,
        const NativePortPrimitiveTopology topology,
        const std::size_t maximum_vertices,
        const std::size_t maximum_indices,
        const NativePortGraphicsFailure failure,
        const char* const layout_operation,
        const char* const index_operation,
        const char* const vertex_operation,
        const char* const topology_operation) {
        if (!valid_topology(topology) || vertices.empty() ||
            vertices.size() > maximum_vertices ||
            indices.size() > maximum_indices ||
            vertices.size_bytes() > std::numeric_limits<UINT>::max() ||
            indices.size_bytes() > std::numeric_limits<UINT>::max())
            fail(failure, 0u, layout_operation);
        for (const auto index : indices) {
            if (index >= vertices.size()) fail(failure, 0u, index_operation);
        }

        GeometryCapabilities capabilities;
        // Authored vertex semantics are a developer audit. Bounds, indices,
        // topology and GPU resource ownership remain checked in both modes.
        if (sonic::diagnostics::runtime_checks_enabled()) for (const auto& vertex : vertices) {
            if (!finite_array(vertex.position) ||
                !finite_array(vertex.texture_coordinate) ||
                !finite_array(vertex.color) || !finite_array(vertex.normal) ||
                !finite_array(vertex.secondary_color) ||
                !std::isfinite(vertex.fog_coordinate) ||
                !std::isfinite(vertex.depth_coordinate) ||
                !std::isfinite(vertex.position_w))
                fail(failure, 0u, vertex_operation);
            capabilities.positive_depth_coordinates =
                capabilities.positive_depth_coordinates &&
                vertex.depth_coordinate > 0.0f;
            capabilities.nonnegative_fog_coordinates =
                capabilities.nonnegative_fog_coordinates &&
                vertex.fog_coordinate >= 0.0f;
            const auto length_squared =
                vertex.normal[0] * vertex.normal[0] +
                vertex.normal[1] * vertex.normal[1] +
                vertex.normal[2] * vertex.normal[2];
            capabilities.nonzero_normals =
                capabilities.nonzero_normals &&
                std::isfinite(length_squared) && length_squared > 0.0f;
            capabilities.unit_position_w =
                capabilities.unit_position_w && vertex.position_w == 1.0f;
            capabilities.positive_position_w =
                capabilities.positive_position_w && vertex.position_w > 0.0f;
            capabilities.depth_coordinate_min = std::min(
                capabilities.depth_coordinate_min, vertex.depth_coordinate);
            capabilities.depth_coordinate_max = std::max(
                capabilities.depth_coordinate_max, vertex.depth_coordinate);
        }

        const auto element_count =
            indices.empty() ? vertices.size() : indices.size();
        const bool valid_count = [&] {
            switch (topology) {
            case NativePortPrimitiveTopology::PointList:
                return element_count >= 1u;
            case NativePortPrimitiveTopology::LineList:
                return element_count >= 2u && element_count % 2u == 0u;
            case NativePortPrimitiveTopology::LineStrip:
                return element_count >= 2u;
            case NativePortPrimitiveTopology::TriangleList:
                return element_count >= 3u && element_count % 3u == 0u;
            case NativePortPrimitiveTopology::TriangleStrip:
                return element_count >= 3u;
            }
            return false;
        }();
        if (!valid_count) fail(failure, 0u, topology_operation);
        return capabilities;
    }

    void require_geometry_capabilities(
        const NativePortDrawPacket& packet,
        const GeometryCapabilities& capabilities) {
        if (!sonic::diagnostics::runtime_checks_enabled()) return;
        if ((packet.fog.mode != NativePortFogMode::Disabled &&
             packet.depth_mapping.mode !=
                 NativePortDepthCoordinateMode::
                     ReciprocalPositiveHomogeneousClip &&
             !capabilities.nonnegative_fog_coordinates) ||
            ((packet.vertex_space ==
                  NativePortVertexSpace::PvrScreenReciprocal ||
              packet.depth_mapping.mode ==
                  NativePortDepthCoordinateMode::ReciprocalPositive) &&
             !capabilities.positive_depth_coordinates))
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "draw-vertex");
        if ((packet.material.lighting_enabled ||
             packet.material.texture_coordinates ==
                 NativePortTextureCoordinateSource::NormalSphere) &&
            !capabilities.nonzero_normals)
            fail(NativePortGraphicsFailure::InvalidDraw, 0u, "draw-normal");
        if ((packet.vertex_space ==
                 NativePortVertexSpace::ClipHomogeneous &&
             !capabilities.positive_position_w) ||
            (packet.vertex_space !=
                 NativePortVertexSpace::ClipHomogeneous &&
             !capabilities.unit_position_w))
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "draw-position-w");

        // ReciprocalPositiveHomogeneousClip deliberately accepts object
        // vertices outside the homogeneous clip volume.  The host GPU clips
        // those primitives and the shader reconstructs reciprocal W only for
        // surviving fragments.  Requiring every submitted W to be positive
        // both contradicted that contract and made persistent meshes
        // impossible to validate because their CPU vertex span is absent.
        // PVR screen-space draws carry an explicit reciprocal coordinate and
        // retain the exact data-dependent range check below.
        if (packet.depth_mapping.mode ==
            NativePortDepthCoordinateMode::ReciprocalPositive) {
            const double minimum_reciprocal =
                capabilities.depth_coordinate_min;
            const double maximum_reciprocal =
                capabilities.depth_coordinate_max;
            const auto map_depth = [&](const double reciprocal) {
                return std::log2(
                           1.0 + packet.depth_mapping.reciprocal_scale *
                                     reciprocal) /
                       packet.depth_mapping.logarithm_divisor;
            };
            const double minimum_depth = map_depth(minimum_reciprocal);
            const double maximum_depth = map_depth(maximum_reciprocal);
            if (!std::isfinite(minimum_depth) ||
                !std::isfinite(maximum_depth) || minimum_depth < 0.0 ||
                maximum_depth > 1.0 || maximum_depth < minimum_depth)
                fail(NativePortGraphicsFailure::InvalidDraw,
                     0u,
                     "draw-reciprocal-depth-range");
        }
    }

    void validate_draw(const NativePortDrawPacket& packet,
                       const MeshSlot* const mesh_slot) {
        if (!valid_viewport_target(packet.viewport) ||
            !valid_vertex_space(packet.vertex_space) ||
            !valid_draw_class(packet.draw_class) ||
            !valid_draw_batch_class(packet.batch.semantic) ||
            packet.batch.identity == 0u ||
            !valid_translucency_policy(packet.translucency) ||
            !valid_draw_diagnostics(packet.diagnostics) ||
            !valid_interpolation(packet.interpolation) ||
            !valid_texture_stage(packet.texture_stage) ||
            !valid_topology(packet.topology) ||
            !valid_blend(packet.blend) || !valid_depth(packet.depth) ||
            !valid_depth_mapping(packet.depth_mapping) ||
            !valid_rasterizer(packet.rasterizer) ||
            !valid_sampler(packet.sampler) ||
            !valid_material(packet.material) ||
            !valid_lighting(packet.lighting) || !valid_fog(packet.fog) ||
            !valid_color_clamp(packet.color_clamp) ||
            !valid_alpha_test(packet.alpha_test) ||
            !finite_array(packet.transform.values) ||
            !finite_array(packet.normal_transform.values))
            fail(NativePortGraphicsFailure::InvalidDraw, 0u, "draw-layout");
        if (packet.texture_stage == NativePortTextureStage::Disabled &&
            packet.texture)
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "draw-texture-stage-disabled");
        if (packet.texture_stage ==
                NativePortTextureStage::RequiredResolved &&
            !packet.texture) {
            fail(NativePortGraphicsFailure::MissingRequiredTexture,
                 0u,
                 "draw-texture-required");
        }
        if (!compatible_vertex_contract(packet))
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "draw-vertex-space-contract");
        if (!compatible_draw_class_contract(packet))
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "draw-class-contract");
        if (!compatible_depth_contract(
                frame_depth_buffer_, packet.depth, packet.depth_mapping))
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "draw-depth-contract");

        if (mesh_slot != nullptr) {
            if (!packet.vertices.empty() || !packet.indices.empty() ||
                packet.topology != mesh_slot->source_topology ||
                packet.rasterizer.shading != mesh_slot->source_shading ||
                packet.rasterizer.small_triangle_area_threshold !=
                    mesh_slot->source_small_triangle_area_threshold ||
                packet.rasterizer.small_triangle_area_space !=
                    NativePortTriangleAreaSpace::Submitted ||
                packet.rasterizer.small_triangle_reference_extent !=
                    NativePortExtent{})
                fail(NativePortGraphicsFailure::InvalidDraw,
                     0u,
                     "draw-mesh-contract");
        }
    }

    void validate_draw_batch_sequence(const NativePortDrawPacket& packet) {
        const auto semantic_rank =
            static_cast<std::uint8_t>(packet.batch.semantic);
        const auto phase_rank = static_cast<std::uint8_t>(packet.draw_class);
        const bool semantic_viewport_compatible =
            packet.batch.semantic == NativePortDrawBatchClass::Scene3D ||
            (packet.batch.semantic == NativePortDrawBatchClass::GameOverlay &&
             packet.viewport == NativePortViewportTarget::Game) ||
            ((packet.batch.semantic == NativePortDrawBatchClass::UiOverlay ||
              packet.batch.semantic == NativePortDrawBatchClass::FontOverlay) &&
             packet.viewport == NativePortViewportTarget::Ui);
        if (!semantic_viewport_compatible)
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "draw-batch-contract");

        if (!frame_batch_active_) {
            frame_batch_active_ = true;
            frame_batch_identity_ = packet.batch.identity;
            frame_batch_semantic_ = packet.batch.semantic;
            frame_batch_phase_ = packet.draw_class;
            frame_batch_submission_order_ = packet.batch.submission_order;
            return;
        }

        const auto previous_phase_rank =
            static_cast<std::uint8_t>(frame_batch_phase_);
        if (phase_rank < previous_phase_rank)
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "draw-phase-order");
        if (phase_rank > previous_phase_rank) {
            frame_batch_identity_ = packet.batch.identity;
            frame_batch_semantic_ = packet.batch.semantic;
            frame_batch_phase_ = packet.draw_class;
            frame_batch_submission_order_ = packet.batch.submission_order;
            return;
        }

        const auto previous_semantic_rank =
            static_cast<std::uint8_t>(frame_batch_semantic_);
        if (semantic_rank < previous_semantic_rank)
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "draw-batch-order");
        if (semantic_rank > previous_semantic_rank) {
            frame_batch_identity_ = packet.batch.identity;
            frame_batch_semantic_ = packet.batch.semantic;
            frame_batch_submission_order_ = packet.batch.submission_order;
            return;
        }

        // One list phase owns at most one batch for each semantic layer.
        // Viewport, vertex-space, interpolation and depth mapping remain
        // per-draw state. Different semantic layers may share an authenticated
        // fixed-function pass identity, but two identities cannot both claim
        // the same semantic/phase pair.
        if (packet.batch.identity != frame_batch_identity_)
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "draw-batch-order");
        if (packet.batch.submission_order <= frame_batch_submission_order_)
            fail(NativePortGraphicsFailure::InvalidDraw,
                 0u,
                 "draw-phase-order");
        frame_batch_submission_order_ = packet.batch.submission_order;
    }

    [[nodiscard]] GeometryPreprocessStats preprocess_geometry(
        std::span<const NativePortVertex>& vertices,
        std::span<const std::uint32_t>& indices,
        NativePortPrimitiveTopology& topology,
        const NativePortRasterizerState& rasterizer,
        const NativePortMatrix4x4& transform,
        const NativePortVertexSpace vertex_space,
        const std::size_t maximum_output_vertices,
        std::vector<NativePortVertex>& destination,
        const NativePortGraphicsFailure topology_failure,
        const char* const topology_operation,
        const char* const budget_operation) {
        GeometryPreprocessStats stats;
        stats.input_vertices = vertices.size();
        stats.input_indices = indices.size();
        const auto element_count =
            indices.empty() ? vertices.size() : indices.size();
        if (topology == NativePortPrimitiveTopology::TriangleList) {
            stats.input_triangles = element_count / 3u;
        } else if (topology == NativePortPrimitiveTopology::TriangleStrip &&
                   element_count >= 3u) {
            stats.input_triangles = element_count - 2u;
        }
        const bool preprocess_triangles =
            rasterizer.shading == NativePortShadingMode::FlatLastVertex ||
            rasterizer.small_triangle_area_threshold > 0.0f;
        if (!preprocess_triangles) {
            stats.output_vertices = vertices.size();
            stats.output_triangles = stats.input_triangles;
            return stats;
        }
        stats.applied = true;
        if (topology != NativePortPrimitiveTopology::TriangleList &&
            topology != NativePortPrimitiveTopology::TriangleStrip)
            fail(topology_failure, 0u, topology_operation);
        const auto triangle_count = stats.input_triangles;
        stats.input_triangles = triangle_count;
        if (triangle_count > maximum_output_vertices / 3u)
            fail(NativePortGraphicsFailure::ResourceLimit,
                 0u,
                 budget_operation);
        destination.clear();
        destination.reserve(triangle_count * 3u);
        const auto source_index = [&](const std::size_t element) {
            return indices.empty() ? static_cast<std::uint32_t>(element)
                                   : indices[element];
        };
        const auto triangle_area_position =
            [&](const NativePortVertex& vertex,
                std::array<double, 2u>& result) {
                if (rasterizer.small_triangle_area_space ==
                    NativePortTriangleAreaSpace::Submitted) {
                    result = {vertex.position[0], vertex.position[1]};
                    return true;
                }

                const std::array<double, 4u> source{
                    vertex.position[0], vertex.position[1],
                    vertex.position[2], vertex.position_w};
                std::array<double, 4u> clip{};
                if (vertex_space ==
                    NativePortVertexSpace::ClipHomogeneous) {
                    clip = source;
                } else {
                    auto object_source = source;
                    object_source[3u] = 1.0;
                    for (std::size_t column = 0u; column < 4u; ++column) {
                        for (std::size_t row = 0u; row < 4u; ++row) {
                            clip[column] += object_source[row] *
                                transform.values[row * 4u + column];
                        }
                    }
                }
                if (!std::ranges::all_of(
                        clip,
                        [](const double value) {
                            return std::isfinite(value);
                        }) ||
                    clip[3u] <= 0.0)
                    return false;

                const auto ndc_x = clip[0u] / clip[3u];
                const auto ndc_y = clip[1u] / clip[3u];
                result = {
                    (ndc_x + 1.0) * 0.5 *
                        rasterizer.small_triangle_reference_extent.width,
                    (1.0 - ndc_y) * 0.5 *
                        rasterizer.small_triangle_reference_extent.height};
                return std::ranges::all_of(
                    result,
                    [](const double value) { return std::isfinite(value); });
            };
        const auto append_triangle =
            [&](const std::size_t first,
                const std::size_t second,
                const std::size_t third,
                const std::size_t provoking) {
                NativePortVertex a = vertices[source_index(first)];
                NativePortVertex b = vertices[source_index(second)];
                NativePortVertex c = vertices[source_index(third)];
                std::array<double, 2u> area_a{};
                std::array<double, 2u> area_b{};
                std::array<double, 2u> area_c{};
                if (rasterizer.small_triangle_area_threshold > 0.0f &&
                    triangle_area_position(a, area_a) &&
                    triangle_area_position(b, area_b) &&
                    triangle_area_position(c, area_c)) {
                    const auto determinant =
                        (area_b[0u] - area_a[0u]) *
                            (area_c[1u] - area_a[1u]) -
                        (area_b[1u] - area_a[1u]) *
                            (area_c[0u] - area_a[0u]);
                    if (std::abs(determinant) <
                        rasterizer.small_triangle_area_threshold) {
                        ++stats.rejected_triangles;
                        return;
                    }
                }
                if (rasterizer.shading ==
                    NativePortShadingMode::FlatLastVertex) {
                    const auto& flat = vertices[source_index(provoking)];
                    a.color = b.color = c.color = flat.color;
                    a.secondary_color = b.secondary_color =
                        c.secondary_color = flat.secondary_color;
                    a.normal = b.normal = c.normal = flat.normal;
                    // D3D's nointerpolation value comes from the first vertex.
                    // Rotate cyclically to preserve winding and all varying
                    // coordinates while selecting the original last vertex.
                    destination.push_back(c);
                    destination.push_back(a);
                    destination.push_back(b);
                    return;
                }
                destination.push_back(a);
                destination.push_back(b);
                destination.push_back(c);
            };
        if (topology == NativePortPrimitiveTopology::TriangleList) {
            for (std::size_t element = 0u; element < element_count;
                 element += 3u)
                append_triangle(
                    element, element + 1u, element + 2u, element + 2u);
        } else {
            for (std::size_t element = 0u; element + 2u < element_count;
                 ++element) {
                if ((element & 1u) == 0u)
                    append_triangle(element,
                                    element + 1u,
                                    element + 2u,
                                    element + 2u);
                else
                    append_triangle(element + 1u,
                                    element,
                                    element + 2u,
                                    element + 2u);
            }
        }
        vertices = destination;
        indices = {};
        topology = NativePortPrimitiveTopology::TriangleList;
        stats.output_vertices = destination.size();
        stats.output_triangles = destination.size() / 3u;
        return stats;
    }

    [[nodiscard]] ComPtr<ID3D11Buffer> create_immutable_buffer(
        const void* const source,
        const std::size_t byte_size,
        const UINT bind_flags,
        const char* const operation) {
        if (byte_size == 0u) return {};
        if (source == nullptr || byte_size > std::numeric_limits<UINT>::max())
            fail(NativePortGraphicsFailure::InvalidResource, 0u, operation);
        D3D11_BUFFER_DESC description{};
        description.ByteWidth = static_cast<UINT>(byte_size);
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags = bind_flags;
        D3D11_SUBRESOURCE_DATA initial_data{};
        initial_data.pSysMem = source;
        ComPtr<ID3D11Buffer> buffer;
        const auto result = device_->CreateBuffer(
            &description, &initial_data, buffer.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 operation);
        return buffer;
    }

    void ensure_vertex_buffer(const std::size_t byte_size) {
        if (byte_size <= vertex_buffer_capacity_) return;
        const auto required = static_cast<UINT>(byte_size);
        const auto maximum = static_cast<UINT>(
            config_.maximum_transient_vertices * sizeof(NativePortVertex));
        const auto preferred = std::min(maximum,
                                        minimum_dynamic_vertex_buffer_bytes);
        const auto capacity = next_buffer_capacity(std::max(required, preferred));
        D3D11_BUFFER_DESC description{};
        description.ByteWidth = capacity;
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ComPtr<ID3D11Buffer> replacement;
        const auto result = device_->CreateBuffer(
            &description, nullptr, replacement.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "vertex-buffer");
        vertex_buffer_ = std::move(replacement);
        vertex_buffer_capacity_ = capacity;
        vertex_buffer_write_offset_ = 0u;
        vertex_buffer_discarded_ = false;
    }

    void ensure_index_buffer(const std::size_t byte_size) {
        if (byte_size <= index_buffer_capacity_) return;
        const auto required = static_cast<UINT>(byte_size);
        const auto maximum = static_cast<UINT>(
            config_.maximum_transient_indices * sizeof(std::uint32_t));
        const auto preferred = std::min(maximum,
                                        minimum_dynamic_index_buffer_bytes);
        const auto capacity = next_buffer_capacity(std::max(required, preferred));
        D3D11_BUFFER_DESC description{};
        description.ByteWidth = capacity;
        description.Usage = D3D11_USAGE_DYNAMIC;
        description.BindFlags = D3D11_BIND_INDEX_BUFFER;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        ComPtr<ID3D11Buffer> replacement;
        const auto result = device_->CreateBuffer(
            &description, nullptr, replacement.GetAddressOf());
        if (FAILED(result))
            fail(NativePortGraphicsFailure::ResourceCreation,
                 static_cast<std::uint32_t>(result),
                 "index-buffer");
        index_buffer_ = std::move(replacement);
        index_buffer_capacity_ = capacity;
        index_buffer_write_offset_ = 0u;
        index_buffer_discarded_ = false;
    }

    [[nodiscard]] UINT upload_dynamic(ID3D11Buffer* const buffer,
                                      const UINT capacity,
                                      UINT& write_offset,
                                      bool& discarded,
                                      const void* const source,
                                      const std::size_t byte_size,
                                      const char* const operation) {
        const auto required = static_cast<UINT>(byte_size);
        if (required > capacity || write_offset > capacity)
            fail(NativePortGraphicsFailure::ResourceLimit, 0u, operation);

        D3D11_MAP map_type = D3D11_MAP_WRITE_NO_OVERWRITE;
        UINT upload_offset = write_offset;
        if (!discarded || required > capacity - write_offset) {
            map_type = D3D11_MAP_WRITE_DISCARD;
            upload_offset = 0u;
            write_offset = 0u;
            discarded = true;
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const auto result = context_->Map(
            buffer, 0u, map_type, 0u, &mapped);
        if (FAILED(result))
            fail(NativePortGraphicsFailure::DeviceLost,
                  static_cast<std::uint32_t>(result),
                  operation);
        std::memcpy(static_cast<std::uint8_t*>(mapped.pData) + upload_offset,
                    source,
                    byte_size);
        context_->Unmap(buffer, 0u);
        saturating_add(snapshot_.uploaded_bytes, byte_size);
        saturating_add(snapshot_.transient_geometry_uploaded_bytes,
                       byte_size);
        write_offset = upload_offset + required;
        return upload_offset;
    }

    void upload_texture(TextureSlot& slot,
                        const NativePortImageView& image,
                        const std::uint32_t mip_level) {
        const auto row_bytes =
            static_cast<std::size_t>(image.extent.width) * 4u;
        const auto subresource = D3D11CalcSubresource(
            mip_level, 0u, slot.config.mip_levels);
        if (vulkan_) {
            vulkan_->upload_texture(slot.vulkan_texture, image, mip_level);
        } else if (slot.config.dynamic) {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            const auto result = context_->Map(
                slot.texture.Get(),
                subresource,
                D3D11_MAP_WRITE_DISCARD,
                0u,
                &mapped);
            if (FAILED(result))
                fail(NativePortGraphicsFailure::DeviceLost,
                     static_cast<std::uint32_t>(result),
                     "texture-map");
            if (mapped.RowPitch < row_bytes) {
                context_->Unmap(slot.texture.Get(), subresource);
                fail(NativePortGraphicsFailure::InvalidResource,
                     0u,
                     "texture-row-pitch");
            }
            for (std::uint32_t row = 0u; row < image.extent.height; ++row) {
                const auto source_row =
                    image.bottom_up ? image.extent.height - 1u - row : row;
                std::memcpy(
                    static_cast<std::byte*>(mapped.pData) +
                        static_cast<std::size_t>(row) * mapped.RowPitch,
                    image.pixels.data() +
                        static_cast<std::size_t>(source_row) * image.stride_bytes,
                    row_bytes);
            }
            context_->Unmap(slot.texture.Get(), subresource);
        } else if (!image.bottom_up) {
            context_->UpdateSubresource(slot.texture.Get(),
                                        subresource,
                                        nullptr,
                                        image.pixels.data(),
                                        image.stride_bytes,
                                        0u);
        } else {
            std::vector<std::byte> normalized(
                row_bytes * image.extent.height);
            for (std::uint32_t row = 0u; row < image.extent.height; ++row) {
                const auto source_row = image.extent.height - 1u - row;
                std::memcpy(normalized.data() +
                                static_cast<std::size_t>(row) * row_bytes,
                            image.pixels.data() +
                                static_cast<std::size_t>(source_row) *
                                    image.stride_bytes,
                            row_bytes);
            }
            context_->UpdateSubresource(slot.texture.Get(),
                                        subresource,
                                        nullptr,
                                        normalized.data(),
                                        static_cast<UINT>(row_bytes),
                                        0u);
        }
        saturating_add(snapshot_.uploaded_bytes,
                       static_cast<std::uint64_t>(row_bytes) *
                           image.extent.height);
    }

    void invalidate_draw_state_shadow() noexcept {
        bound_viewport_valid_ = false;
        bound_blend_valid_ = false;
        bound_depth_valid_ = false;
        bound_rasterizer_valid_ = false;
        bound_topology_valid_ = false;
        bound_vertex_buffer_valid_ = false;
        bound_index_buffer_valid_ = false;
        bound_sampler_valid_ = false;
        bound_shader_resource_valid_ = false;
        draw_pipeline_bound_ = false;
        draw_pipeline_type_two_ = false;
    }

    void set_viewport(const NativePortPixelRect rect) {
        if (rect.width == 0u || rect.height == 0u)
            fail(NativePortGraphicsFailure::InvalidFrame, 0u, "viewport-empty");
        if (bound_viewport_valid_ && bound_viewport_.x == rect.x &&
            bound_viewport_.y == rect.y &&
            bound_viewport_.width == rect.width &&
            bound_viewport_.height == rect.height)
            return;
        D3D11_VIEWPORT viewport{};
        viewport.TopLeftX = static_cast<float>(rect.x);
        viewport.TopLeftY = static_cast<float>(rect.y);
        viewport.Width = static_cast<float>(rect.width);
        viewport.Height = static_cast<float>(rect.height);
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        context_->RSSetViewports(1u, &viewport);
        const D3D11_RECT scissor{
            static_cast<LONG>(rect.x),
            static_cast<LONG>(rect.y),
            static_cast<LONG>(rect.x + rect.width),
            static_cast<LONG>(rect.y + rect.height)};
        context_->RSSetScissorRects(1u, &scissor);
        bound_viewport_ = rect;
        bound_viewport_valid_ = true;
    }

    [[nodiscard]] std::wstring environment_value(
        const wchar_t* const name) const {
        SetLastError(ERROR_SUCCESS);
        const auto required = GetEnvironmentVariableW(name, nullptr, 0u);
        if (required == 0u) {
            const auto error = GetLastError();
            if (error == ERROR_SUCCESS || error == ERROR_ENVVAR_NOT_FOUND)
                return {};
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidConfig,
                static_cast<std::uint32_t>(error),
                "graphics-capture-environment");
        }
        std::wstring value(required, L'\0');
        const auto written =
            GetEnvironmentVariableW(name, value.data(), required);
        if (written == 0u || written >= required)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidConfig,
                static_cast<std::uint32_t>(GetLastError()),
                "graphics-capture-environment");
        value.resize(written);
        return value;
    }

    [[nodiscard]] std::uint64_t environment_unsigned(
        const wchar_t* const name,
        const std::uint64_t fallback,
        const char* const operation) const {
        const auto value = environment_value(name);
        if (value.empty()) return fallback;
        std::uint64_t result = 0u;
        for (const auto character : value) {
            if (character < L'0' || character > L'9')
                throw NativePortGraphicsError(
                    NativePortGraphicsFailure::InvalidConfig,
                    1u,
                    operation);
            const auto digit =
                static_cast<std::uint64_t>(character - L'0');
            if (result >
                (std::numeric_limits<std::uint64_t>::max() - digit) / 10u)
                throw NativePortGraphicsError(
                    NativePortGraphicsFailure::InvalidConfig,
                    1u,
                    operation);
            result = result * 10u + digit;
        }
        return result;
    }

    void initialize_present_policy() {
        // Cold diagnostic controls let the same identity-bound binary compare
        // queue policies without another AOT export. Invalid values fail closed.
        const auto nonblocking = environment_unsigned(
            L"KATANA_PORT_NONBLOCKING_PRESENT", 1u, "present-policy");
        const auto queue_limit = environment_unsigned(
            L"KATANA_PORT_PRESENT_QUEUE_LIMIT", 2u, "present-queue-limit");
        if (nonblocking > 1u || queue_limit < 1u || queue_limit > 3u)
            fail(NativePortGraphicsFailure::InvalidConfig, 1u,
                 "present-policy");
        allow_nonblocking_present_ = nonblocking != 0u;
        present_queue_limit_ = static_cast<UINT>(queue_limit);
    }

    void initialize_frame_capture() {
        const auto drawstream = environment_value(
            L"KATANA_NATIVE_GRAPHICS_CAPTURE_DRAWSTREAM");
        if (!drawstream.empty() && drawstream != L"0" && drawstream != L"1")
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidConfig,
                1u,
                "graphics-drawstream-enabled");
        const auto directory = environment_value(
            L"KATANA_NATIVE_GRAPHICS_CAPTURE_DIRECTORY");
        if (directory.empty()) {
            if (drawstream == L"1")
                throw NativePortGraphicsError(
                    NativePortGraphicsFailure::InvalidConfig,
                    1u,
                    "graphics-drawstream-directory");
            return;
        }
        capture_directory_ = std::filesystem::path(directory);
        capture_start_frame_ = environment_unsigned(
            L"KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME",
            1u,
            "graphics-capture-start");
        capture_end_frame_ = environment_unsigned(
            L"KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME",
            std::numeric_limits<std::uint64_t>::max(),
            "graphics-capture-end");
        capture_interval_ = environment_unsigned(
            L"KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL",
            60u,
            "graphics-capture-interval");
        if (capture_start_frame_ == 0u || capture_interval_ == 0u ||
            capture_interval_ > 1'000'000u ||
            capture_end_frame_ < capture_start_frame_)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidConfig,
                1u,
                "graphics-capture-range");
        std::error_code error;
        std::filesystem::create_directories(capture_directory_, error);
        if (error)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::ResourceCreation,
                static_cast<std::uint32_t>(error.value()),
                "graphics-capture-directory");
        capture_enabled_ = true;
        if (drawstream == L"1") {
            constexpr std::uint64_t maximum_drawstream_frame_span = 3u;
            const auto maximum_bounded_end =
                capture_start_frame_ >
                        std::numeric_limits<std::uint64_t>::max() -
                            (maximum_drawstream_frame_span - 1u)
                    ? std::numeric_limits<std::uint64_t>::max()
                    : capture_start_frame_ +
                          maximum_drawstream_frame_span - 1u;
            drawstream_end_frame_ = std::min(
                capture_end_frame_, maximum_bounded_end);
            drawstream_maximum_draws_per_frame_ = environment_unsigned(
                L"KATANA_NATIVE_GRAPHICS_CAPTURE_DRAWSTREAM_MAX_DRAWS",
                1'024u,
                "graphics-drawstream-maximum-draws");
            if (drawstream_maximum_draws_per_frame_ == 0u ||
                drawstream_maximum_draws_per_frame_ > 16'384u)
                throw NativePortGraphicsError(
                    NativePortGraphicsFailure::InvalidConfig,
                    1u,
                    "graphics-drawstream-maximum-draws");
            drawstream_directory_ = capture_directory_;
            drawstream_enabled_ = true;
        }
    }

    void initialize_graphics_diagnostics() {
        const auto requested = environment_value(
            L"KATANA_NATIVE_GRAPHICS_DIAGNOSTICS_MODE");
        if (requested.empty() || requested == L"off") {
            graphics_diagnostic_mode_ = NativePortGraphicsDiagnosticMode::Off;
            snapshot_.diagnostic_mode = graphics_diagnostic_mode_;
            return;
        }
        if (requested == L"digest")
            graphics_diagnostic_mode_ =
                NativePortGraphicsDiagnosticMode::Digest;
        else if (requested == L"breadcrumbs")
            graphics_diagnostic_mode_ =
                NativePortGraphicsDiagnosticMode::Breadcrumbs;
        else if (requested == L"armed-capture")
            graphics_diagnostic_mode_ =
                NativePortGraphicsDiagnosticMode::ArmedCapture;
        else
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidConfig,
                1u,
                "graphics-diagnostics-mode");

        snapshot_.diagnostic_mode = graphics_diagnostic_mode_;
        graphics_digest_ = graphics_digest_seed;
        snapshot_.diagnostic_digest = graphics_digest_;
        if (graphics_diagnostic_mode_ ==
            NativePortGraphicsDiagnosticMode::Digest)
            return;

        const auto directory = environment_value(
            L"KATANA_NATIVE_GRAPHICS_DIAGNOSTICS_DIRECTORY");
        if (directory.empty())
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidConfig,
                1u,
                "graphics-diagnostics-directory");
        graphics_diagnostic_directory_ = std::filesystem::path(directory);
        std::error_code error;
        std::filesystem::create_directories(
            graphics_diagnostic_directory_, error);
        if (error)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::ResourceCreation,
                static_cast<std::uint32_t>(error.value()),
                "graphics-diagnostics-directory");

        const auto breadcrumb_filename =
            std::filesystem::path(L"graphics-breadcrumbs-v2.bin");
        graphics_breadcrumb_output_path_ =
            graphics_diagnostic_directory_ / breadcrumb_filename;
        auto breadcrumb_temporary_filename = breadcrumb_filename;
        breadcrumb_temporary_filename +=
            L".tmp-" +
            std::to_wstring(graphics_diagnostic_process_id());
        graphics_breadcrumb_temporary_path_ =
            graphics_diagnostic_directory_ / breadcrumb_temporary_filename;

        constexpr std::uint64_t maximum_breadcrumb_capacity = 262'144u;
        const auto capacity = environment_unsigned(
            L"KATANA_NATIVE_GRAPHICS_DIAGNOSTICS_CAPACITY",
            32'768u,
            "graphics-diagnostics-capacity");
        if (capacity == 0u || capacity > maximum_breadcrumb_capacity)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidConfig,
                1u,
                "graphics-diagnostics-capacity");
        graphics_breadcrumbs_.resize(static_cast<std::size_t>(capacity));

        if (graphics_diagnostic_mode_ !=
            NativePortGraphicsDiagnosticMode::ArmedCapture)
            return;

        capture_start_frame_ = environment_unsigned(
            L"KATANA_NATIVE_GRAPHICS_CAPTURE_START_FRAME",
            1u,
            "graphics-capture-start");
        capture_end_frame_ = environment_unsigned(
            L"KATANA_NATIVE_GRAPHICS_CAPTURE_END_FRAME",
            std::numeric_limits<std::uint64_t>::max(),
            "graphics-capture-end");
        capture_interval_ = environment_unsigned(
            L"KATANA_NATIVE_GRAPHICS_CAPTURE_INTERVAL",
            60u,
            "graphics-capture-interval");
        if (capture_start_frame_ == 0u || capture_interval_ == 0u ||
            capture_interval_ > 1'000'000u ||
            capture_end_frame_ < capture_start_frame_)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidConfig,
                1u,
                "graphics-capture-range");
        constexpr std::uint64_t maximum_drawstream_frame_span = 3u;
        const auto maximum_bounded_end =
            capture_start_frame_ >
                    std::numeric_limits<std::uint64_t>::max() -
                        (maximum_drawstream_frame_span - 1u)
                ? std::numeric_limits<std::uint64_t>::max()
                : capture_start_frame_ + maximum_drawstream_frame_span - 1u;
        drawstream_end_frame_ =
            std::min(capture_end_frame_, maximum_bounded_end);
        drawstream_maximum_draws_per_frame_ = environment_unsigned(
            L"KATANA_NATIVE_GRAPHICS_CAPTURE_DRAWSTREAM_MAX_DRAWS",
            1'024u,
            "graphics-drawstream-maximum-draws");
        if (drawstream_maximum_draws_per_frame_ == 0u ||
            drawstream_maximum_draws_per_frame_ > 16'384u)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidConfig,
                1u,
                "graphics-drawstream-maximum-draws");
        drawstream_directory_ = graphics_diagnostic_directory_;
        drawstream_enabled_ = true;
    }

    void record_graphics_diagnostic(
        const NativePortDrawPacket& packet,
        const TextureSlot* const texture,
        const bool geometry_available,
        const bool rejected,
        const std::optional<NativePortGraphicsFailure> contract_failure =
            std::nullopt) noexcept {
        if (graphics_diagnostic_mode_ ==
            NativePortGraphicsDiagnosticMode::Off)
            return;

        const auto frame = snapshot_.begun_frames;
        const auto draw = snapshot_.diagnostic_draws + 1u;
        const auto& diagnostics = packet.diagnostics;
        const auto& binding = diagnostics.texture_binding;
        std::uint32_t record_flags = 0u;
        if (geometry_available) record_flags |= 1u << 0u;
        if (rejected) record_flags |= 1u << 1u;
        if (diagnostics.material_identity_bound) record_flags |= 1u << 2u;
        if (diagnostics.origin_identity_bound) record_flags |= 1u << 3u;
        if (diagnostics.model_identity_bound) record_flags |= 1u << 4u;
        if (diagnostics.texture_list_index_bound) record_flags |= 1u << 5u;
        if (binding.texture_list_bound) record_flags |= 1u << 6u;
        if (binding.last_writer_bound) record_flags |= 1u << 7u;
        if (binding.expected_asset_bound) record_flags |= 1u << 8u;
        if (binding.resolved_asset_bound) record_flags |= 1u << 9u;
        if (texture != nullptr &&
            texture->config.provenance.content_identity_bound)
            record_flags |= 1u << 10u;
        if (texture != nullptr &&
            texture->config.provenance.global_index_bound)
            record_flags |= 1u << 11u;
        if (binding.texture_list_epoch_bound) record_flags |= 1u << 12u;
        if (diagnostics.enabled) record_flags |= 1u << 13u;
        if (texture != nullptr &&
            texture->config.provenance.decoded_payload_identity_bound)
            record_flags |= 1u << 14u;
        if (contract_failure.has_value()) record_flags |= 1u << 15u;
        if (texture != nullptr &&
            texture->config.provenance.source_memory_layout)
            record_flags |= 1u << 16u;
        auto digest = graphics_digest_;
        auto frame_digest = graphics_frame_digest_;
        const auto mix = [&](const std::uint64_t value) {
            digest = mix_graphics_digest(digest, value);
            frame_digest = mix_graphics_digest(frame_digest, value);
        };
        mix(frame);
        mix(draw);
        mix(packet.batch.identity);
        mix(packet.batch.submission_order);
        mix(static_cast<std::uint64_t>(packet.batch.semantic));
        mix(static_cast<std::uint64_t>(packet.draw_class));
        mix(packet.texture.value);
        mix(static_cast<std::uint64_t>(packet.texture_stage));
        mix(static_cast<std::uint64_t>(packet.blend.source_buffer));
        mix(static_cast<std::uint64_t>(packet.blend.destination_buffer));
        mix(static_cast<std::uint64_t>(
            packet.rasterizer.logical_clip.mode));
        mix(packet.rasterizer.logical_clip.logical_extent.width);
        mix(packet.rasterizer.logical_clip.logical_extent.height);
        for (const auto value : packet.rasterizer.logical_clip.bounds)
            mix(std::bit_cast<std::uint32_t>(value));
        mix(std::bit_cast<std::uint32_t>(packet.material.mipmapped_pass_weight));
        mix(packet.material.bump_mapping);
        mix(diagnostics.material_identity);
        mix(diagnostics.origin_identity);
        mix(diagnostics.model_identity);
        mix(diagnostics.texture_list_index);
        mix(diagnostics.mesh_index);
        mix(diagnostics.primitive_index);
        mix(static_cast<std::uint64_t>(diagnostics.logical_use));
        mix(static_cast<std::uint64_t>(diagnostics.origin));
        mix(static_cast<std::uint64_t>(diagnostics.intent));
        mix(binding.texture_list_identity);
        mix(binding.texture_list_epoch);
        mix(binding.texture_list_count);
        mix(binding.last_writer_identity);
        mix(binding.last_writer_sequence);
        mix(binding.expected_asset_identity);
        mix(binding.resolved_asset_identity);
        mix(static_cast<std::uint64_t>(binding.resolver));
        mix(static_cast<std::uint64_t>(binding.last_writer));
        mix(record_flags);
        if (contract_failure.has_value())
            mix(static_cast<std::uint64_t>(*contract_failure));
        if (texture != nullptr) {
            mix(texture->config.provenance.generation);
            mix(texture->config.provenance.archive_ordinal);
            mix(texture->config.provenance.global_index);
            for (std::size_t offset = 0u;
                 offset < texture->config.provenance.content_sha256.size();
                 offset += sizeof(std::uint64_t)) {
                std::uint64_t word = 0u;
                std::memcpy(
                    &word,
                    texture->config.provenance.content_sha256.data() + offset,
                    sizeof(word));
                mix(word);
            }
            if (texture->config.provenance.decoded_payload_identity_bound) {
                mix(texture->config.provenance.source_pixel_format);
                mix(texture->config.provenance.source_data_format);
                mix(texture->config.provenance.decoded_extent.width);
                mix(texture->config.provenance.decoded_extent.height);
                mix(texture->config.provenance.decoded_mip_levels);
                for (std::size_t offset = 0u;
                     offset < texture->config.provenance
                                   .decoded_rgba8_sha256.size();
                     offset += sizeof(std::uint64_t)) {
                    std::uint64_t word = 0u;
                    std::memcpy(
                        &word,
                        texture->config.provenance.decoded_rgba8_sha256.data() +
                            offset,
                        sizeof(word));
                    mix(word);
                }
            }
        }
        mix(geometry_available ? 1u : 0u);
        mix(rejected ? 1u : 0u);
        graphics_digest_ = digest;
        graphics_frame_digest_ = frame_digest;
        snapshot_.diagnostic_digest = digest;
        saturating_increment(snapshot_.diagnostic_draws);

        if (graphics_diagnostic_mode_ ==
            NativePortGraphicsDiagnosticMode::Digest)
            return;

        auto layer_digest = mix_graphics_digest(
            graphics_digest_seed, packet.batch.identity);
        layer_digest = mix_graphics_digest(
            layer_digest, static_cast<std::uint64_t>(packet.batch.semantic));
        layer_digest = mix_graphics_digest(
            layer_digest, packet.diagnostics.origin_identity);
        layer_digest = mix_graphics_digest(
            layer_digest,
            packet.diagnostics.texture_binding.texture_list_identity);

        GraphicsBreadcrumbRecord record{};
        record.frame = frame;
        record.draw = draw;
        record.batch_identity = packet.batch.identity;
        record.global_digest = digest;
        record.frame_digest = frame_digest;
        record.layer_digest = layer_digest;
        record.material_identity = diagnostics.material_identity;
        record.origin_identity = diagnostics.origin_identity;
        record.model_identity = diagnostics.model_identity;
        record.texture_handle = packet.texture.value;
        record.texture_list_identity = binding.texture_list_identity;
        record.texture_list_epoch = binding.texture_list_epoch;
        record.last_writer_identity = binding.last_writer_identity;
        record.last_writer_sequence = binding.last_writer_sequence;
        record.expected_asset_identity = binding.expected_asset_identity;
        record.resolved_asset_identity = binding.resolved_asset_identity;
        record.texture_list_index = diagnostics.texture_list_index;
        record.texture_list_count = binding.texture_list_count;
        record.mesh_index = diagnostics.mesh_index;
        record.primitive_index = diagnostics.primitive_index;
        record.submission_order = packet.batch.submission_order;
        record.batch_semantic = static_cast<std::uint8_t>(packet.batch.semantic);
        record.draw_class = static_cast<std::uint8_t>(packet.draw_class);
        record.logical_use =
            static_cast<std::uint8_t>(diagnostics.logical_use);
        record.origin = static_cast<std::uint8_t>(diagnostics.origin);
        record.intent = static_cast<std::uint8_t>(diagnostics.intent);
        record.resolver = static_cast<std::uint8_t>(binding.resolver);
        record.last_writer = static_cast<std::uint8_t>(binding.last_writer);
        if (contract_failure.has_value())
            record.reserved =
                static_cast<std::uint8_t>(*contract_failure);
        record.flags = record_flags;
        if (texture != nullptr) {
            const auto& provenance = texture->config.provenance;
            record.texture_generation = provenance.generation;
            record.texture_content_sha256 = provenance.content_sha256;
            record.texture_decoded_rgba8_sha256 =
                provenance.decoded_rgba8_sha256;
            record.texture_archive_ordinal = provenance.archive_ordinal;
            record.texture_global_index = provenance.global_index;
            record.texture_width = provenance.decoded_extent.width;
            record.texture_height = provenance.decoded_extent.height;
            record.texture_mip_levels = provenance.decoded_mip_levels;
            record.texture_source_pixel_format = provenance.source_pixel_format;
            record.texture_source_data_format = provenance.source_data_format;
        }

        const auto capacity = graphics_breadcrumbs_.size();
        std::size_t destination = 0u;
        if (graphics_breadcrumb_count_ < capacity) {
            destination = (graphics_breadcrumb_first_ +
                           graphics_breadcrumb_count_) % capacity;
            ++graphics_breadcrumb_count_;
        } else {
            destination = graphics_breadcrumb_first_;
            graphics_breadcrumb_first_ =
                (graphics_breadcrumb_first_ + 1u) % capacity;
            saturating_increment(snapshot_.diagnostic_drops);
        }
        graphics_breadcrumbs_[destination] = record;
        graphics_breadcrumbs_dirty_ = true;
    }

    void record_graphics_contract_failure(
        const NativePortDrawPacket& packet,
        const NativePortGraphicsFailure failure) noexcept {
        NativePortGraphicsContractFailureWitness witness;
        witness.frame = snapshot_.begun_frames;
        const auto remaining_draw_sequence =
            std::numeric_limits<std::uint64_t>::max() -
            snapshot_.draw_calls;
        witness.draw_sequence =
            snapshot_.contract_failures >= remaining_draw_sequence
                ? std::numeric_limits<std::uint64_t>::max()
                : snapshot_.draw_calls + snapshot_.contract_failures + 1u;
        witness.batch_identity = packet.batch.identity;
        witness.diagnostics = packet.diagnostics;
        witness.submission_order = packet.batch.submission_order;
        witness.failure = failure;
        witness.batch_semantic = packet.batch.semantic;
        witness.draw_class = packet.draw_class;
        witness.valid = true;
        snapshot_.last_contract_failure = witness;
        saturating_increment(snapshot_.contract_failures);

        if (graphics_diagnostic_mode_ ==
            NativePortGraphicsDiagnosticMode::Off)
            return;
        record_graphics_diagnostic(packet, nullptr, false, true, failure);
        // The exception may immediately unwind the title. Persist the bounded
        // breadcrumb now instead of waiting for a periodic checkpoint.
        flush_graphics_breadcrumbs();
    }

    [[nodiscard]] bool write_graphics_breadcrumb_snapshot() noexcept {
        if (graphics_breadcrumb_output_path_.empty() ||
            graphics_breadcrumb_temporary_path_.empty())
            return false;
        try {
            std::ofstream output(
                graphics_breadcrumb_temporary_path_,
                std::ios::binary | std::ios::trunc);
            if (!output) return false;
            GraphicsBreadcrumbFileHeader header{};
            header.mode = static_cast<std::uint32_t>(graphics_diagnostic_mode_);
            header.capacity = graphics_breadcrumbs_.size();
            header.record_count = graphics_breadcrumb_count_;
            header.first_record = graphics_breadcrumb_first_;
            header.dropped_records = snapshot_.diagnostic_drops;
            header.final_digest = graphics_digest_;
            output.write(reinterpret_cast<const char*>(&header), sizeof(header));
            for (std::size_t index = 0u; index < graphics_breadcrumb_count_;
                 ++index) {
                const auto source = (graphics_breadcrumb_first_ + index) %
                    graphics_breadcrumbs_.size();
                output.write(
                    reinterpret_cast<const char*>(&graphics_breadcrumbs_[source]),
                    sizeof(GraphicsBreadcrumbRecord));
                if (!output) return false;
            }
            output.flush();
            if (!output) return false;
            output.close();
            if (!output) return false;
#ifdef _WIN32
            return MoveFileExW(
                       graphics_breadcrumb_temporary_path_.c_str(),
                       graphics_breadcrumb_output_path_.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
                   FALSE;
#else
            std::error_code error;
            std::filesystem::rename(
                graphics_breadcrumb_temporary_path_,
                graphics_breadcrumb_output_path_,
                error);
            return !error;
#endif
        } catch (...) {
            return false;
        }
    }

    void maybe_checkpoint_graphics_breadcrumbs() noexcept {
        if ((graphics_diagnostic_mode_ !=
                 NativePortGraphicsDiagnosticMode::Breadcrumbs &&
             graphics_diagnostic_mode_ !=
                 NativePortGraphicsDiagnosticMode::ArmedCapture) ||
            !graphics_breadcrumbs_dirty_ || graphics_breadcrumb_count_ == 0u)
            return;

        constexpr std::uint64_t checkpoint_frame_interval = 32u;
        constexpr std::uint64_t checkpoint_draw_interval = 4'096u;
        const auto frame_due =
            !graphics_breadcrumb_checkpoint_written_ ||
            (snapshot_.presented_frames >=
                 graphics_breadcrumb_last_checkpoint_frame_ &&
             snapshot_.presented_frames -
                     graphics_breadcrumb_last_checkpoint_frame_ >=
                 checkpoint_frame_interval);
        const auto draw_due =
            !graphics_breadcrumb_checkpoint_written_ ||
            (snapshot_.diagnostic_draws >=
                 graphics_breadcrumb_last_checkpoint_draw_ &&
             snapshot_.diagnostic_draws -
                     graphics_breadcrumb_last_checkpoint_draw_ >=
                 checkpoint_draw_interval);
        if (!frame_due && !draw_due) return;
        if (!write_graphics_breadcrumb_snapshot()) return;
        graphics_breadcrumbs_dirty_ = false;
        graphics_breadcrumb_checkpoint_written_ = true;
        graphics_breadcrumb_last_checkpoint_frame_ =
            snapshot_.presented_frames;
        graphics_breadcrumb_last_checkpoint_draw_ = snapshot_.diagnostic_draws;
    }

    void flush_graphics_breadcrumbs() noexcept {
        if (graphics_diagnostic_mode_ !=
                NativePortGraphicsDiagnosticMode::Breadcrumbs &&
            graphics_diagnostic_mode_ !=
                NativePortGraphicsDiagnosticMode::ArmedCapture)
            return;
        if (!write_graphics_breadcrumb_snapshot()) return;
        graphics_breadcrumbs_dirty_ = false;
        graphics_breadcrumb_checkpoint_written_ = true;
        graphics_breadcrumb_last_checkpoint_frame_ =
            snapshot_.presented_frames;
        graphics_breadcrumb_last_checkpoint_draw_ = snapshot_.diagnostic_draws;
    }

    [[nodiscard]] bool should_capture_frame(
        const std::uint64_t frame) const noexcept {
        return capture_enabled_ && frame >= capture_start_frame_ &&
               frame <= capture_end_frame_ &&
               (frame - capture_start_frame_) % capture_interval_ == 0u;
    }

    [[nodiscard]] bool should_capture_drawstream_frame(
        const std::uint64_t frame) const noexcept {
        return drawstream_enabled_ && frame >= capture_start_frame_ &&
               frame <= drawstream_end_frame_ &&
               (frame - capture_start_frame_) % capture_interval_ == 0u;
    }

    void begin_drawstream_frame(const std::uint64_t frame) {
        if (drawstream_frame_ == frame) return;
        if (drawstream_output_.is_open()) drawstream_output_.close();
        drawstream_output_.clear();
        drawstream_frame_ = frame;
        drawstream_draws_this_frame_ = 0u;
        drawstream_gpu_draws_this_frame_ = 0u;
        drawstream_truncation_reported_ = false;
        if (!should_capture_drawstream_frame(frame)) return;
        const auto output_path = drawstream_directory_ /
            (L"drawstream-frame-" + std::to_wstring(frame) + L".jsonl");
        drawstream_output_.open(
            output_path, std::ios::binary | std::ios::trunc);
        if (!drawstream_output_)
            fail(NativePortGraphicsFailure::ResourceCreation,
                 1u,
                 "graphics-drawstream-open");
        drawstream_output_ << std::setprecision(9);
    }

    void capture_drawstream(
        const NativePortDrawPacket& packet,
        const std::span<const NativePortVertex> vertices,
        const std::span<const std::uint32_t> indices,
        const NativePortPrimitiveTopology topology,
        const NativePortPixelRect viewport,
        const TextureSlot* const texture_slot,
        const MeshSlot* const mesh_slot,
        const GeometryPreprocessStats* const preprocessing,
        const bool geometry_available,
        const char* const rejection_reason) {
        if (!drawstream_enabled_) return;
        const auto frame = snapshot_.presented_frames + 1u;
        begin_drawstream_frame(frame);
        if (!drawstream_output_.is_open()) return;
        if (drawstream_draws_this_frame_ >=
            drawstream_maximum_draws_per_frame_) {
            if (!drawstream_truncation_reported_) {
                drawstream_output_
                    << "{\"schema\":\"katana-native-drawstream-v2\","
                       "\"frame\":"
                    << frame
                    << ",\"truncated\":true,\"reason\":"
                       "\"draw-budget\"}\n";
                drawstream_truncation_reported_ = true;
            }
            return;
        }

        constexpr std::size_t maximum_vertices_examined = 8'192u;
        constexpr std::size_t maximum_elements_examined = 24'576u;
        const auto examined_vertices =
            std::min(vertices.size(), maximum_vertices_examined);
        bool geometry_truncated = vertices.size() > examined_vertices;

        const auto clip_position = [&](const NativePortVertex& vertex) {
            std::array<double, 4u> result{};
            const std::array<double, 4u> source{
                vertex.position[0], vertex.position[1], vertex.position[2],
                packet.vertex_space == NativePortVertexSpace::ClipHomogeneous
                    ? vertex.position_w
                    : 1.0};
            if (packet.vertex_space == NativePortVertexSpace::ClipHomogeneous)
                return source;
            for (std::size_t column = 0u; column < 4u; ++column) {
                for (std::size_t row = 0u; row < 4u; ++row)
                    result[column] += source[row] *
                        packet.transform.values[row * 4u + column];
            }
            return result;
        };

        std::array<double, 4u> submitted_min{};
        std::array<double, 4u> submitted_max{};
        std::array<double, 4u> clip_min{};
        std::array<double, 4u> clip_max{};
        std::array<double, 2u> ndc_min{};
        std::array<double, 2u> ndc_max{};
        std::array<double, 2u> pixel_min{};
        std::array<double, 2u> pixel_max{};
        std::array<double, 2u> uv_min{};
        std::array<double, 2u> uv_max{};
        std::array<double, 2u> depth_range{};
        std::array<double, 2u> alpha_range{};
        std::array<double, 4u> color_min{};
        std::array<double, 4u> color_max{};
        std::array<double, 4u> secondary_min{};
        std::array<double, 4u> secondary_max{};
        std::array<double, 3u> normal_min{};
        std::array<double, 3u> normal_max{};
        bool bounds_initialized = false;
        bool projected_bounds_initialized = false;
        std::size_t invalid_clip_positions = 0u;
        std::size_t positive_clip_w = 0u;
        std::size_t nonpositive_clip_w = 0u;
        for (std::size_t index = 0u; index < examined_vertices; ++index) {
            const auto& vertex = vertices[index];
            const auto clip = clip_position(vertex);
            const std::array<double, 4u> submitted{
                vertex.position[0], vertex.position[1], vertex.position[2],
                vertex.position_w};
            const std::array<double, 2u> uv{
                vertex.texture_coordinate[0], vertex.texture_coordinate[1]};
            if (!bounds_initialized) {
                submitted_min = submitted_max = submitted;
                clip_min = clip_max = clip;
                uv_min = uv_max = uv;
                depth_range = {vertex.depth_coordinate,
                               vertex.depth_coordinate};
                alpha_range = {vertex.color[3u], vertex.color[3u]};
                for (std::size_t component = 0u; component < 4u;
                     ++component) {
                    color_min[component] = color_max[component] =
                        vertex.color[component];
                    secondary_min[component] = secondary_max[component] =
                        vertex.secondary_color[component];
                }
                for (std::size_t component = 0u; component < 3u;
                     ++component)
                    normal_min[component] = normal_max[component] =
                        vertex.normal[component];
                bounds_initialized = true;
            } else {
                for (std::size_t component = 0u; component < 4u; ++component) {
                    submitted_min[component] = std::min(
                        submitted_min[component], submitted[component]);
                    submitted_max[component] = std::max(
                        submitted_max[component], submitted[component]);
                    clip_min[component] =
                        std::min(clip_min[component], clip[component]);
                    clip_max[component] =
                        std::max(clip_max[component], clip[component]);
                }
                for (std::size_t component = 0u; component < 2u; ++component) {
                    uv_min[component] =
                        std::min(uv_min[component], uv[component]);
                    uv_max[component] =
                        std::max(uv_max[component], uv[component]);
                }
                depth_range[0u] = std::min(
                    depth_range[0u],
                    static_cast<double>(vertex.depth_coordinate));
                depth_range[1u] = std::max(
                    depth_range[1u],
                    static_cast<double>(vertex.depth_coordinate));
                alpha_range[0u] = std::min(
                    alpha_range[0u],
                    static_cast<double>(vertex.color[3u]));
                alpha_range[1u] = std::max(
                    alpha_range[1u],
                    static_cast<double>(vertex.color[3u]));
                for (std::size_t component = 0u; component < 4u;
                     ++component) {
                    color_min[component] = std::min(
                        color_min[component],
                        static_cast<double>(vertex.color[component]));
                    color_max[component] = std::max(
                        color_max[component],
                        static_cast<double>(vertex.color[component]));
                    secondary_min[component] = std::min(
                        secondary_min[component],
                        static_cast<double>(
                            vertex.secondary_color[component]));
                    secondary_max[component] = std::max(
                        secondary_max[component],
                        static_cast<double>(
                            vertex.secondary_color[component]));
                }
                for (std::size_t component = 0u; component < 3u;
                     ++component) {
                    normal_min[component] = std::min(
                        normal_min[component],
                        static_cast<double>(vertex.normal[component]));
                    normal_max[component] = std::max(
                        normal_max[component],
                        static_cast<double>(vertex.normal[component]));
                }
            }
            if (!std::ranges::all_of(
                    clip, [](const double value) {
                        return std::isfinite(value);
                    }) || clip[3u] == 0.0) {
                ++invalid_clip_positions;
                continue;
            }
            if (clip[3u] > 0.0)
                ++positive_clip_w;
            else
                ++nonpositive_clip_w;
            const std::array<double, 2u> ndc{
                clip[0u] / clip[3u], clip[1u] / clip[3u]};
            const std::array<double, 2u> pixel{
                viewport.x + (ndc[0u] + 1.0) * 0.5 * viewport.width,
                viewport.y + (1.0 - ndc[1u]) * 0.5 * viewport.height};
            if (!projected_bounds_initialized) {
                ndc_min = ndc_max = ndc;
                pixel_min = pixel_max = pixel;
                projected_bounds_initialized = true;
            } else {
                for (std::size_t component = 0u; component < 2u; ++component) {
                    ndc_min[component] =
                        std::min(ndc_min[component], ndc[component]);
                    ndc_max[component] =
                        std::max(ndc_max[component], ndc[component]);
                    pixel_min[component] =
                        std::min(pixel_min[component], pixel[component]);
                    pixel_max[component] =
                        std::max(pixel_max[component], pixel[component]);
                }
            }
        }

        std::size_t winding_positive = 0u;
        std::size_t winding_negative = 0u;
        std::size_t winding_degenerate = 0u;
        std::size_t winding_unprojectable = 0u;
        const auto source_element_count =
            indices.empty() ? vertices.size() : indices.size();
        const auto examined_elements =
            std::min(source_element_count, maximum_elements_examined);
        geometry_truncated = geometry_truncated ||
                             source_element_count > examined_elements;
        const auto source_index = [&](const std::size_t element) {
            return indices.empty() ? static_cast<std::uint32_t>(element)
                                   : indices[element];
        };
        const auto projected_xy = [&](const std::uint32_t index,
                                      std::array<double, 2u>& result) {
            if (index >= examined_vertices) return false;
            const auto clip = clip_position(vertices[index]);
            if (!std::ranges::all_of(
                    clip, [](const double value) {
                        return std::isfinite(value);
                    }) || clip[3u] == 0.0)
                return false;
            result = {clip[0u] / clip[3u], clip[1u] / clip[3u]};
            return std::ranges::all_of(
                result, [](const double value) {
                    return std::isfinite(value);
                });
        };
        const auto classify_triangle = [&](const std::size_t first,
                                           const std::size_t second,
                                           const std::size_t third) {
            std::array<double, 2u> a{};
            std::array<double, 2u> b{};
            std::array<double, 2u> c{};
            if (!projected_xy(source_index(first), a) ||
                !projected_xy(source_index(second), b) ||
                !projected_xy(source_index(third), c)) {
                ++winding_unprojectable;
                return;
            }
            const auto determinant =
                (b[0u] - a[0u]) * (c[1u] - a[1u]) -
                (b[1u] - a[1u]) * (c[0u] - a[0u]);
            if (determinant > 1.0e-12)
                ++winding_positive;
            else if (determinant < -1.0e-12)
                ++winding_negative;
            else
                ++winding_degenerate;
        };
        if (topology == NativePortPrimitiveTopology::TriangleList) {
            for (std::size_t element = 0u; element + 2u < examined_elements;
                 element += 3u)
                classify_triangle(element, element + 1u, element + 2u);
        } else if (topology == NativePortPrimitiveTopology::TriangleStrip) {
            for (std::size_t element = 0u; element + 2u < examined_elements;
                 ++element) {
                if ((element & 1u) == 0u)
                    classify_triangle(element, element + 1u, element + 2u);
                else
                    classify_triangle(element + 1u, element, element + 2u);
            }
        }

        const auto determinant3 = [&] {
            const auto& m = packet.transform.values;
            return static_cast<double>(m[0u]) *
                       (static_cast<double>(m[5u]) * m[10u] -
                        static_cast<double>(m[6u]) * m[9u]) -
                   static_cast<double>(m[1u]) *
                       (static_cast<double>(m[4u]) * m[10u] -
                        static_cast<double>(m[6u]) * m[8u]) +
                   static_cast<double>(m[2u]) *
                       (static_cast<double>(m[4u]) * m[9u] -
                        static_cast<double>(m[5u]) * m[8u]);
        }();
        const auto determinant4 = [&] {
            std::array<std::array<double, 4u>, 4u> matrix{};
            for (std::size_t row = 0u; row < 4u; ++row)
                for (std::size_t column = 0u; column < 4u; ++column)
                    matrix[row][column] =
                        packet.transform.values[row * 4u + column];
            double determinant = 1.0;
            for (std::size_t pivot = 0u; pivot < 4u; ++pivot) {
                auto selected = pivot;
                for (std::size_t row = pivot + 1u; row < 4u; ++row)
                    if (std::abs(matrix[row][pivot]) >
                        std::abs(matrix[selected][pivot]))
                        selected = row;
                if (matrix[selected][pivot] == 0.0) return 0.0;
                if (selected != pivot) {
                    std::swap(matrix[selected], matrix[pivot]);
                    determinant = -determinant;
                }
                const auto divisor = matrix[pivot][pivot];
                determinant *= divisor;
                for (std::size_t row = pivot + 1u; row < 4u; ++row) {
                    const auto factor = matrix[row][pivot] / divisor;
                    for (std::size_t column = pivot + 1u; column < 4u;
                         ++column)
                        matrix[row][column] -=
                            factor * matrix[pivot][column];
                }
            }
            return determinant;
        }();

        auto& output = drawstream_output_;
        const auto write_array = [&output](const auto& values) {
            output << '[';
            for (std::size_t index = 0u; index < values.size(); ++index) {
                if (index != 0u) output << ',';
                output << values[index];
            }
            output << ']';
        };
        const auto vertex_count = mesh_slot != nullptr
            ? mesh_slot->vertex_count
            : vertices.size();
        const auto index_count = mesh_slot != nullptr
            ? mesh_slot->index_count
            : indices.size();
        output << "{\"schema\":\"katana-native-draw-v2\",\"frame\":"
               << frame << ",\"record\":" << drawstream_draws_this_frame_
               << ",\"draw\":";
        if (geometry_available)
            output << drawstream_gpu_draws_this_frame_;
        else
            output << "null";
        output << ",\"diagnostic_only\":"
               << (geometry_available ? "false" : "true")
               << ",\"vertex_space\":"
               << static_cast<unsigned>(packet.vertex_space)
               << ",\"draw_class\":"
               << static_cast<unsigned>(packet.draw_class)
               << ",\"batch_identity\":" << packet.batch.identity
               << ",\"batch_semantic\":"
               << static_cast<unsigned>(packet.batch.semantic)
               << ",\"submission_order\":"
               << packet.batch.submission_order
               << ",\"translucency_policy\":"
               << static_cast<unsigned>(packet.translucency)
               << ",\"interpolation\":"
               << static_cast<unsigned>(packet.interpolation)
               << ",\"viewport_target\":"
               << static_cast<unsigned>(packet.viewport)
               << ",\"viewport\":[" << viewport.x << ',' << viewport.y
               << ',' << viewport.width << ',' << viewport.height << ']'
               << ",\"topology\":" << static_cast<unsigned>(topology)
               << ",\"vertices\":" << vertex_count
               << ",\"indices\":" << index_count
               << ",\"mesh\":" << packet.mesh.value
               << ",\"texture\":" << packet.texture.value
               << ",\"texture_stage\":"
               << static_cast<unsigned>(packet.texture_stage)
               << ",\"diagnostics\":{\"material_identity\":"
               << packet.diagnostics.material_identity
               << ",\"origin_identity\":"
               << packet.diagnostics.origin_identity
               << ",\"model_identity\":"
               << packet.diagnostics.model_identity
               << ",\"texture_list_index\":"
               << packet.diagnostics.texture_list_index
               << ",\"mesh_index\":" << packet.diagnostics.mesh_index
               << ",\"primitive_index\":"
               << packet.diagnostics.primitive_index
               << ",\"logical_use\":"
               << static_cast<unsigned>(packet.diagnostics.logical_use)
               << ",\"origin\":"
               << static_cast<unsigned>(packet.diagnostics.origin)
               << ",\"intent\":"
               << static_cast<unsigned>(packet.diagnostics.intent)
               << ",\"texture_list_identity\":"
               << packet.diagnostics.texture_binding.texture_list_identity
               << ",\"texture_list_epoch\":"
               << packet.diagnostics.texture_binding.texture_list_epoch
               << ",\"texture_list_count\":"
               << packet.diagnostics.texture_binding.texture_list_count
               << ",\"last_writer_identity\":"
               << packet.diagnostics.texture_binding.last_writer_identity
               << ",\"last_writer_sequence\":"
               << packet.diagnostics.texture_binding.last_writer_sequence
               << ",\"expected_asset_identity\":"
               << packet.diagnostics.texture_binding.expected_asset_identity
               << ",\"resolved_asset_identity\":"
               << packet.diagnostics.texture_binding.resolved_asset_identity
               << ",\"resolver\":"
               << static_cast<unsigned>(
                      packet.diagnostics.texture_binding.resolver)
               << ",\"last_writer\":"
               << static_cast<unsigned>(
                      packet.diagnostics.texture_binding.last_writer)
               << ",\"material_identity_bound\":"
               << (packet.diagnostics.material_identity_bound ? "true"
                                                               : "false")
               << ",\"texture_list_index_bound\":"
               << (packet.diagnostics.texture_list_index_bound ? "true"
                                                                : "false")
               << '}'
               << ",\"geometry_available\":"
               << (geometry_available ? "true" : "false")
               << ",\"geometry_truncated\":"
               << (geometry_truncated ? "true" : "false")
               << ",\"invalid_clip_positions\":"
               << invalid_clip_positions
               << ",\"positive_clip_w\":" << positive_clip_w
               << ",\"nonpositive_clip_w\":" << nonpositive_clip_w
               << ",\"winding_positive\":" << winding_positive
               << ",\"winding_negative\":" << winding_negative
               << ",\"winding_degenerate\":" << winding_degenerate
               << ",\"winding_unprojectable\":"
               << winding_unprojectable;
        if (rejection_reason != nullptr)
            output << ",\"reason\":\"" << rejection_reason << '\"';
        if (preprocessing != nullptr) {
            output << ",\"preprocessing\":{\"applied\":"
                   << (preprocessing->applied ? "true" : "false")
                   << ",\"input_vertices\":"
                   << preprocessing->input_vertices
                   << ",\"input_indices\":"
                   << preprocessing->input_indices
                   << ",\"input_triangles\":"
                   << preprocessing->input_triangles
                   << ",\"output_vertices\":"
                   << preprocessing->output_vertices
                   << ",\"output_triangles\":"
                   << preprocessing->output_triangles
                   << ",\"rejected_triangles\":"
                   << preprocessing->rejected_triangles
                   << ",\"threshold\":"
                   << packet.rasterizer.small_triangle_area_threshold
                   << ",\"area_space\":"
                   << static_cast<unsigned>(
                          packet.rasterizer.small_triangle_area_space)
                   << '}';
        }
        if (bounds_initialized) {
            output << ",\"submitted_min\":";
            write_array(submitted_min);
            output << ",\"submitted_max\":";
            write_array(submitted_max);
            output << ",\"clip_min\":";
            write_array(clip_min);
            output << ",\"clip_max\":";
            write_array(clip_max);
            output << ",\"uv_min\":";
            write_array(uv_min);
            output << ",\"uv_max\":";
            write_array(uv_max);
            output << ",\"depth_range\":";
            write_array(depth_range);
            output << ",\"vertex_alpha_range\":";
            write_array(alpha_range);
            output << ",\"vertex_color_min\":";
            write_array(color_min);
            output << ",\"vertex_color_max\":";
            write_array(color_max);
            output << ",\"secondary_color_min\":";
            write_array(secondary_min);
            output << ",\"secondary_color_max\":";
            write_array(secondary_max);
            output << ",\"normal_min\":";
            write_array(normal_min);
            output << ",\"normal_max\":";
            write_array(normal_max);
        }
        if (projected_bounds_initialized) {
            output << ",\"ndc_min\":";
            write_array(ndc_min);
            output << ",\"ndc_max\":";
            write_array(ndc_max);
            output << ",\"pixel_min\":";
            write_array(pixel_min);
            output << ",\"pixel_max\":";
            write_array(pixel_max);
        }
        output << ",\"transform_determinant3\":" << determinant3
               << ",\"transform_determinant4\":" << determinant4
               << ",\"transform\":";
        write_array(packet.transform.values);
        output << ",\"normal_transform\":";
        write_array(packet.normal_transform.values);
        output << ",\"state\":{\"cull\":"
               << static_cast<unsigned>(packet.rasterizer.cull)
               << ",\"front_ccw\":"
               << (packet.rasterizer.front_counter_clockwise ? "true"
                                                              : "false")
               << ",\"depth_clip\":"
               << (packet.rasterizer.depth_clip_enabled ? "true" : "false")
               << ",\"logical_clip\":{\"mode\":"
               << static_cast<unsigned>(packet.rasterizer.logical_clip.mode)
               << ",\"logical_width\":"
               << packet.rasterizer.logical_clip.logical_extent.width
               << ",\"logical_height\":"
               << packet.rasterizer.logical_clip.logical_extent.height
               << ",\"bounds\":["
               << packet.rasterizer.logical_clip.bounds[0] << ','
               << packet.rasterizer.logical_clip.bounds[1] << ','
               << packet.rasterizer.logical_clip.bounds[2] << ','
               << packet.rasterizer.logical_clip.bounds[3] << "]}"
               << ",\"depth_test\":"
               << (packet.depth.test_enabled ? "true" : "false")
               << ",\"depth_write\":"
               << (packet.depth.write_enabled ? "true" : "false")
               << ",\"depth_compare\":"
               << static_cast<unsigned>(packet.depth.compare)
               << ",\"depth_mapping\":"
               << static_cast<unsigned>(packet.depth_mapping.mode)
               << ",\"blend\":"
               << (packet.blend.enabled ? "true" : "false")
               << ",\"blend_source\":"
               << static_cast<unsigned>(packet.blend.source_color)
               << ",\"blend_destination\":"
               << static_cast<unsigned>(packet.blend.destination_color)
               << ",\"blend_source_alpha\":"
               << static_cast<unsigned>(packet.blend.source_alpha)
               << ",\"blend_destination_alpha\":"
               << static_cast<unsigned>(packet.blend.destination_alpha)
               << ",\"blend_source_buffer\":"
               << static_cast<unsigned>(packet.blend.source_buffer)
               << ",\"blend_destination_buffer\":"
               << static_cast<unsigned>(packet.blend.destination_buffer)
               << ",\"blend_color_operation\":"
               << static_cast<unsigned>(packet.blend.color_operation)
               << ",\"blend_alpha_operation\":"
               << static_cast<unsigned>(packet.blend.alpha_operation)
               << ",\"texture_combine\":"
               << static_cast<unsigned>(packet.material.texture_combine)
               << ",\"alpha_test\":"
               << (packet.alpha_test.enabled ? "true" : "false")
               << ",\"alpha_mode\":"
               << static_cast<unsigned>(packet.alpha_test.mode)
               << ",\"alpha_compare\":"
               << static_cast<unsigned>(packet.alpha_test.compare)
               << ",\"alpha_reference\":"
               << packet.alpha_test.reference
               << ",\"alpha_reference_8bit\":"
               << static_cast<unsigned>(packet.alpha_test.reference_8bit)
               << "}";
        output << ",\"material\":{\"diffuse\":";
        write_array(packet.material.diffuse);
        output << ",\"ambient\":";
        write_array(packet.material.ambient);
        output << ",\"specular\":";
        write_array(packet.material.specular);
        output << ",\"emission\":";
        write_array(packet.material.emission);
        output << ",\"specular_power\":"
               << packet.material.specular_power
               << ",\"texture_coordinates\":"
               << static_cast<unsigned>(packet.material.texture_coordinates)
               << ",\"use_vertex_color\":"
               << (packet.material.use_vertex_color ? "true" : "false")
               << ",\"use_primary_alpha\":"
               << (packet.material.use_primary_alpha ? "true" : "false")
               << ",\"use_texture_alpha\":"
               << (packet.material.use_texture_alpha ? "true" : "false")
               << ",\"use_secondary_color\":"
               << (packet.material.use_secondary_color ? "true" : "false")
               << ",\"lighting_enabled\":"
               << (packet.material.lighting_enabled ? "true" : "false")
               << ",\"specular_enabled\":"
               << (packet.material.specular_enabled ? "true" : "false")
               << ",\"mipmapped_pass_weight\":"
               << packet.material.mipmapped_pass_weight
               << ",\"bump_mapping\":"
               << (packet.material.bump_mapping ? "true" : "false")
               << "}";
        output << ",\"lighting\":{\"ambient\":";
        write_array(packet.lighting.ambient);
        output << ",\"light_count\":" << packet.lighting.light_count
               << ",\"lights\":[";
        for (std::size_t index = 0u; index < packet.lighting.light_count;
             ++index) {
            if (index != 0u) output << ',';
            output << "{\"direction\":";
            write_array(packet.lighting.lights[index].direction);
            output << ",\"color\":";
            write_array(packet.lighting.lights[index].color);
            output << '}';
        }
        output << "]}";
        output << ",\"fog\":{\"mode\":"
               << static_cast<unsigned>(packet.fog.mode)
               << ",\"color\":";
        write_array(packet.fog.color);
        output << ",\"start\":" << packet.fog.start
               << ",\"end\":" << packet.fog.end
               << ",\"density\":" << packet.fog.density << "}";
        output << ",\"color_clamp\":{\"enabled\":"
               << (packet.color_clamp.enabled ? "true" : "false")
               << ",\"minimum\":";
        write_array(packet.color_clamp.minimum);
        output << ",\"maximum\":";
        write_array(packet.color_clamp.maximum);
        output << "}";
        output << ",\"sampler\":{\"filter\":"
               << static_cast<unsigned>(packet.sampler.filter)
               << ",\"address_u\":"
               << static_cast<unsigned>(packet.sampler.address_u)
               << ",\"address_v\":"
               << static_cast<unsigned>(packet.sampler.address_v)
               << ",\"address_w\":"
               << static_cast<unsigned>(packet.sampler.address_w)
               << ",\"mip_lod_bias\":" << packet.sampler.mip_lod_bias
               << ",\"minimum_lod\":" << packet.sampler.minimum_lod
               << ",\"maximum_lod\":" << packet.sampler.maximum_lod
               << ",\"maximum_anisotropy\":"
               << packet.sampler.maximum_anisotropy << "}";
        if (texture_slot != nullptr) {
            const auto& texture = *texture_slot;
            output << ",\"texture_state\":{\"format\":"
                   << static_cast<unsigned>(texture.config.format)
                   << ",\"width\":" << texture.config.extent.width
                   << ",\"height\":" << texture.config.extent.height
                   << ",\"mips\":" << texture.config.mip_levels
                   << ",\"dynamic\":"
                   << (texture.config.dynamic ? "true" : "false")
                   << ",\"content_identity_bound\":"
                   << (texture.config.provenance.content_identity_bound
                           ? "true"
                           : "false")
                   << ",\"generation\":"
                   << texture.config.provenance.generation
                   << ",\"archive_ordinal\":"
                   << texture.config.provenance.archive_ordinal
                   << ",\"global_index_bound\":"
                   << (texture.config.provenance.global_index_bound
                           ? "true"
                           : "false")
                   << ",\"global_index\":"
                   << texture.config.provenance.global_index;
            if (texture.config.provenance.content_identity_bound) {
                output << ",\"content_sha256\":\"" << std::hex
                       << std::setfill('0');
                for (const auto byte :
                     texture.config.provenance.content_sha256)
                    output << std::setw(2)
                           << static_cast<unsigned>(byte);
                output << std::dec << std::setfill(' ') << '\"';
            }
            output << ",\"decoded_payload_identity_bound\":"
                   << (texture.config.provenance
                               .decoded_payload_identity_bound
                           ? "true"
                           : "false")
                   << ",\"source_pixel_format\":"
                   << static_cast<unsigned>(
                          texture.config.provenance.source_pixel_format)
                   << ",\"source_data_format\":"
                   << static_cast<unsigned>(
                          texture.config.provenance.source_data_format)
                   << ",\"decoded_width\":"
                   << texture.config.provenance.decoded_extent.width
                   << ",\"decoded_height\":"
                   << texture.config.provenance.decoded_extent.height
                   << ",\"source_memory_layout\":"
                   << (texture.config.provenance.source_memory_layout
                           ? "true" : "false")
                   << ",\"decoded_mip_levels\":"
                   << texture.config.provenance.decoded_mip_levels;
            if (texture.config.provenance.decoded_payload_identity_bound) {
                output << ",\"decoded_rgba8_sha256\":\"" << std::hex
                       << std::setfill('0');
                for (const auto byte : texture.config.provenance
                                          .decoded_rgba8_sha256)
                    output << std::setw(2)
                           << static_cast<unsigned>(byte);
                output << std::dec << std::setfill(' ') << '\"';
            }
            output << '}';
        }
        output << "}\n";
        if (!output)
            fail(NativePortGraphicsFailure::ResourceCreation,
                 1u,
                 "graphics-drawstream-write");
        if (geometry_available) ++drawstream_gpu_draws_this_frame_;
        ++drawstream_draws_this_frame_;
    }

    void capture_completed_frame(const std::uint64_t frame) {
        if (!should_capture_frame(frame)) return;
        const auto extent = completed_host_image_ ? completed_host_extent_ : config_.render_extent;
        const auto width = extent.width;
        const auto height = extent.height;
        const auto row_bytes = static_cast<std::size_t>(width) * 4u;
        const auto pixel_bytes = row_bytes * static_cast<std::size_t>(height);
        if (vulkan_) capture_pixels_ = vulkan_->capture_bgra_bottom_up();
        else {
        auto* const source_texture = completed_host_image_ ? completed_host_texture_.Get() : completed_texture_.Get();
        D3D11_TEXTURE2D_DESC description{};
        source_texture->GetDesc(&description);
        if (capture_readback_) {
            D3D11_TEXTURE2D_DESC previous{};
            capture_readback_->GetDesc(&previous);
            if (previous.Width != description.Width || previous.Height != description.Height ||
                previous.Format != description.Format) capture_readback_.Reset();
        }
        if (!capture_readback_) {
            description.Usage = D3D11_USAGE_STAGING;
            description.BindFlags = 0u;
            description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            description.MiscFlags = 0u;
            const auto result = device_->CreateTexture2D(
                &description, nullptr, capture_readback_.GetAddressOf());
            if (FAILED(result))
                fail(NativePortGraphicsFailure::ResourceCreation,
                     static_cast<std::uint32_t>(result),
                     "graphics-capture-readback");
        }

        context_->CopyResource(capture_readback_.Get(), source_texture);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const auto map_result = context_->Map(
            capture_readback_.Get(), 0u, D3D11_MAP_READ, 0u, &mapped);
        if (FAILED(map_result))
            fail(NativePortGraphicsFailure::DeviceLost,
                 static_cast<std::uint32_t>(map_result),
                 "graphics-capture-map");
        struct Unmap final {
            ID3D11DeviceContext* context = nullptr;
            ID3D11Resource* resource = nullptr;
            ~Unmap() {
                if (context != nullptr && resource != nullptr)
                    context->Unmap(resource, 0u);
            }
        } unmap{context_.Get(), capture_readback_.Get()};

        capture_pixels_.resize(pixel_bytes);
        const auto* const source =
            static_cast<const std::uint8_t*>(mapped.pData);
        for (std::uint32_t output_y = 0u; output_y < height; ++output_y) {
            const auto source_y = height - output_y - 1u;
            const auto* const source_row =
                source + static_cast<std::size_t>(source_y) * mapped.RowPitch;
            auto* const output_row =
                capture_pixels_.data() +
                static_cast<std::size_t>(output_y) * row_bytes;
            for (std::uint32_t x = 0u; x < width; ++x) {
                output_row[x * 4u + 0u] = source_row[x * 4u + 2u];
                output_row[x * 4u + 1u] = source_row[x * 4u + 1u];
                output_row[x * 4u + 2u] = source_row[x * 4u + 0u];
                output_row[x * 4u + 3u] = source_row[x * 4u + 3u];
            }
        }
        unmap.context->Unmap(unmap.resource, 0u);
        unmap.context = nullptr;
        unmap.resource = nullptr;
        }

        constexpr std::uint32_t header_bytes = 54u;
        if (pixel_bytes >
            std::numeric_limits<std::uint32_t>::max() - header_bytes)
            fail(NativePortGraphicsFailure::ResourceLimit,
                 1u,
                 "graphics-capture-size");
        std::array<std::uint8_t, header_bytes> header{};
        const auto store_u16 = [&](const std::size_t offset,
                                   const std::uint16_t value) {
            header[offset] = static_cast<std::uint8_t>(value);
            header[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
        };
        const auto store_u32 = [&](const std::size_t offset,
                                   const std::uint32_t value) {
            for (std::size_t byte = 0u; byte < 4u; ++byte)
                header[offset + byte] = static_cast<std::uint8_t>(
                    value >> static_cast<unsigned>(byte * 8u));
        };
        header[0] = static_cast<std::uint8_t>('B');
        header[1] = static_cast<std::uint8_t>('M');
        store_u32(2u, header_bytes + static_cast<std::uint32_t>(pixel_bytes));
        store_u32(10u, header_bytes);
        store_u32(14u, 40u);
        store_u32(18u, width);
        store_u32(22u, height);
        store_u16(26u, 1u);
        store_u16(28u, 32u);
        store_u32(34u, static_cast<std::uint32_t>(pixel_bytes));

        const auto output_path = capture_directory_ /
            (L"frame-" + std::to_wstring(frame) + L".bmp");
        std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
        if (!output)
            fail(NativePortGraphicsFailure::ResourceCreation,
                 1u,
                 "graphics-capture-open");
        output.write(reinterpret_cast<const char*>(header.data()),
                     static_cast<std::streamsize>(header.size()));
        output.write(reinterpret_cast<const char*>(capture_pixels_.data()),
                     static_cast<std::streamsize>(capture_pixels_.size()));
        if (!output)
            fail(NativePortGraphicsFailure::ResourceCreation,
                 1u,
                 "graphics-capture-write");
    }

    [[nodiscard]] static NativePortTextureHandle make_texture_handle(
        const std::uint32_t index,
        const std::uint32_t generation) noexcept {
        return {static_cast<std::uint64_t>(generation) << 32u |
                (static_cast<std::uint64_t>(index) + 1u)};
    }

    [[nodiscard]] static std::uint32_t texture_handle_index(
        const NativePortTextureHandle handle) noexcept {
        return static_cast<std::uint32_t>(handle.value) - 1u;
    }

    [[nodiscard]] TextureSlot& resolve_texture(
        const NativePortTextureHandle handle) {
        if (!handle || static_cast<std::uint32_t>(handle.value) == 0u)
            fail(NativePortGraphicsFailure::InvalidResource,
                 0u,
                 "texture-handle");
        const auto index = texture_handle_index(handle);
        const auto generation = static_cast<std::uint32_t>(handle.value >> 32u);
        if (index >= texture_slots_.size() || generation == 0u ||
            !texture_slots_[index].live ||
            texture_slots_[index].generation != generation)
            fail(NativePortGraphicsFailure::InvalidResource,
                 0u,
                 "texture-stale");
        return texture_slots_[index];
    }

    [[nodiscard]] static NativePortMeshHandle make_mesh_handle(
        const std::uint32_t index,
        const std::uint32_t generation) noexcept {
        return {static_cast<std::uint64_t>(generation) << 32u |
                (static_cast<std::uint64_t>(index) + 1u)};
    }

    [[nodiscard]] static std::uint32_t mesh_handle_index(
        const NativePortMeshHandle handle) noexcept {
        return static_cast<std::uint32_t>(handle.value) - 1u;
    }

    [[nodiscard]] MeshSlot& resolve_mesh(const NativePortMeshHandle handle) {
        if (!handle || static_cast<std::uint32_t>(handle.value) == 0u)
            fail(NativePortGraphicsFailure::InvalidResource,
                 0u,
                 "mesh-handle");
        const auto index = mesh_handle_index(handle);
        const auto generation = static_cast<std::uint32_t>(handle.value >> 32u);
        if (index >= mesh_slots_.size() || generation == 0u ||
            !mesh_slots_[index].live ||
            mesh_slots_[index].generation != generation)
            fail(NativePortGraphicsFailure::InvalidResource,
                 0u,
                 "mesh-stale");
        return mesh_slots_[index];
    }

    void require_owner_thread() const {
        if (std::this_thread::get_id() != owner_thread_)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::ThreadViolation,
                0u,
                "thread");
    }

    [[noreturn]] void fail(const NativePortGraphicsFailure failure,
                           const std::uint32_t code,
                           const char* const operation) {
        snapshot_.platform_error_code = code == 0u ? 1u : code;
        throw NativePortGraphicsError(
            failure, snapshot_.platform_error_code, operation);
    }

    std::string title_storage_;
    std::string development_state_directory_storage_;
    NativePortGraphicsConfig config_;
    std::thread::id owner_thread_;
    NativePortRuntimeOptionsBridge* runtime_options_ = nullptr;
    std::optional<NativePortTelemetryWriter> telemetry_writer_;
    std::optional<NativePortTelemetryTimer> render_submit_timer_;
    std::array<GpuTimingSlot, gpu_timing_query_count> gpu_timing_queries_{};
    std::size_t next_gpu_timing_query_ = 0u;
    std::size_t active_gpu_timing_query_ = no_gpu_timing_query;
    bool gpu_timing_enabled_ = false;
    HWND window_ = nullptr;
    sonic::rendering::WindowFullscreen fullscreen_;
    bool d3d_exclusive_ = false;
    HMENU menu_bar_ = nullptr;
    HMENU options_menu_ = nullptr;
    std::uint64_t runtime_menu_sample_nanoseconds_ = 0u;
    std::uint64_t runtime_menu_output_frames_ = 0u;
    std::uint64_t runtime_menu_simulation_frames_ = 0u;
    double runtime_menu_output_fps_ = 0.0;
    double runtime_menu_simulation_fps_ = 0.0;
    double performance_overlay_output_fps_ = -1.0;
    double performance_overlay_simulation_fps_ = -1.0;
    NativePortExtent output_extent_;
    NativePortExtent pending_output_extent_;
    NativePortGraphicsLayout cached_layout_;
    bool close_requested_ = false;
    bool minimized_ = false;
    bool display_change_pending_ = false;
    bool display_rate_dirty_ = true;
    std::uint32_t display_rate_hz_ = 60u;
    std::uint32_t display_rate_reported_ = 0u;
    bool frame_open_ = false;
    bool completed_frame_available_ = false;
    bool completed_host_image_ = false;
    NativePortExtent completed_host_extent_{};
    ComPtr<ID3D11Texture2D> completed_host_texture_;
    ComPtr<ID3D11ShaderResourceView> completed_host_view_;
    [[nodiscard]] NativePortPixelRect completed_output_viewport() const noexcept {
        return completed_host_image_ ? NativePortPixelRect{0,0,output_extent_.width,output_extent_.height}
                                     : cached_layout_.output_viewport;
    }
    bool inject_present_failure_once_ = false;
    bool inject_present_busy_once_ = false;
    bool flip_swap_chain_ = false;
    bool swap_chain_resize_required_ = false;
    bool allow_nonblocking_present_ = true;
    UINT present_queue_limit_ = 2u;
    UINT applied_present_queue_limit_ = 0u;
    NativePortDepthBufferConvention frame_depth_buffer_ =
        NativePortDepthBufferConvention::Forward;

    ComPtr<ID3D11Device> device_;
    ComPtr<IDXGIDevice1> dxgi_device_;
    ComPtr<ID3D11DeviceContext> context_;
    D3D_FEATURE_LEVEL feature_level_ = D3D_FEATURE_LEVEL_10_0;
    ComPtr<IDXGISwapChain> swap_chain_;
    ComPtr<ID3D11RenderTargetView> swap_chain_target_;
    ComPtr<ID3D11Texture2D> render_texture_;
    ComPtr<ID3D11RenderTargetView> render_target_;
    ComPtr<ID3D11ShaderResourceView> render_view_;
    ComPtr<ID3D11Texture2D> completed_texture_;
    ComPtr<ID3D11RenderTargetView> completed_target_;
    ComPtr<ID3D11ShaderResourceView> completed_view_;
    ComPtr<ID3D11Texture2D> capture_readback_;
    ComPtr<ID3D11Texture2D> depth_texture_;
    ComPtr<ID3D11DepthStencilView> depth_view_;
    ComPtr<ID3D11VertexShader> draw_vertex_shader_;
    ComPtr<ID3D11PixelShader> draw_pixel_shader_;
    ComPtr<ID3D11PixelShader> type_two_capture_pixel_shader_;
    ComPtr<ID3D11VertexShader> composite_vertex_shader_;
    ComPtr<ID3D11PixelShader> composite_pixel_shader_;
    ComPtr<ID3D11PixelShader> performance_overlay_pixel_shader_;
    ComPtr<ID3D11PixelShader> type_two_resolve_pixel_shader_;
    ComPtr<ID3D11InputLayout> input_layout_;
    ComPtr<ID3D11Buffer> draw_constants_;
    ComPtr<ID3D11Buffer> fog_table_constants_;
    ComPtr<ID3D11Buffer> type_two_resolve_constants_;
    ComPtr<ID3D11Buffer> performance_overlay_constants_;
    ComPtr<ID3D11Texture2D> type_two_base_texture_;
    ComPtr<ID3D11ShaderResourceView> type_two_base_view_;
    ComPtr<ID3D11Texture2D> type_two_depth_texture_;
    ComPtr<ID3D11ShaderResourceView> type_two_depth_view_;
    ComPtr<ID3D11Texture2D> type_two_head_texture_;
    ComPtr<ID3D11ShaderResourceView> type_two_head_view_;
    ComPtr<ID3D11UnorderedAccessView> type_two_head_uav_;
    ComPtr<ID3D11Texture2D> type_two_count_texture_;
    ComPtr<ID3D11ShaderResourceView> type_two_count_view_;
    ComPtr<ID3D11UnorderedAccessView> type_two_count_uav_;
    ComPtr<ID3D11Buffer> type_two_fragment_buffer_;
    ComPtr<ID3D11ShaderResourceView> type_two_fragment_view_;
    ComPtr<ID3D11UnorderedAccessView> type_two_fragment_uav_;
    ComPtr<ID3D11Buffer> type_two_status_buffer_;
    ComPtr<ID3D11UnorderedAccessView> type_two_status_uav_;
    ComPtr<ID3D11ShaderResourceView> type_two_status_view_;
    ComPtr<ID3D11Buffer> vertex_buffer_;
    ComPtr<ID3D11Buffer> index_buffer_;
    UINT vertex_buffer_capacity_ = 0u;
    UINT index_buffer_capacity_ = 0u;
    UINT vertex_buffer_write_offset_ = 0u;
    UINT index_buffer_write_offset_ = 0u;
    bool vertex_buffer_discarded_ = false;
    bool index_buffer_discarded_ = false;
    std::array<float, native_port_fog_table_entries> last_fog_lookup_table_{};
    bool fog_lookup_table_valid_ = false;
    DrawConstants last_draw_constants_{};
    bool draw_constants_valid_ = false;
    NativePortBlendState last_blend_key_{};
    NativePortDepthState last_depth_key_{};
    NativePortRasterizerState last_rasterizer_key_{};
    NativePortSamplerState last_sampler_key_{};
    ID3D11BlendState* last_blend_state_ = nullptr;
    ID3D11DepthStencilState* last_depth_state_ = nullptr;
    ID3D11RasterizerState* last_rasterizer_state_ = nullptr;
    ID3D11SamplerState* last_sampler_state_ = nullptr;
    bool last_blend_key_valid_ = false;
    bool last_depth_key_valid_ = false;
    bool last_rasterizer_key_valid_ = false;
    bool last_sampler_key_valid_ = false;
    NativePortPixelRect bound_viewport_{};
    ID3D11BlendState* bound_blend_ = nullptr;
    ID3D11DepthStencilState* bound_depth_ = nullptr;
    ID3D11RasterizerState* bound_rasterizer_ = nullptr;
    ID3D11SamplerState* bound_sampler_ = nullptr;
    ID3D11Buffer* bound_vertex_buffer_ = nullptr;
    ID3D11Buffer* bound_index_buffer_ = nullptr;
    ID3D11ShaderResourceView* bound_shader_resource_ = nullptr;
    UINT bound_vertex_buffer_offset_ = 0u;
    UINT bound_index_buffer_offset_ = 0u;
    D3D11_PRIMITIVE_TOPOLOGY bound_topology_ =
        D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    bool bound_viewport_valid_ = false;
    bool bound_blend_valid_ = false;
    bool bound_depth_valid_ = false;
    bool bound_rasterizer_valid_ = false;
    bool bound_sampler_valid_ = false;
    bool bound_vertex_buffer_valid_ = false;
    bool bound_index_buffer_valid_ = false;
    bool bound_shader_resource_valid_ = false;
    bool bound_topology_valid_ = false;
    bool draw_pipeline_bound_ = false;
    bool draw_pipeline_type_two_ = false;
    bool frame_batch_active_ = false;
    std::uint64_t frame_batch_identity_ = 0u;
    NativePortDrawBatchClass frame_batch_semantic_ =
        NativePortDrawBatchClass::Scene3D;
    NativePortDrawClass frame_batch_phase_ = NativePortDrawClass::Opaque;
    std::uint32_t frame_batch_submission_order_ = 0u;
    bool type2_resources_ready_ = false;
    bool type2_subpass_active_ = false;
    bool type2_gather_active_ = false;
    bool type_two_uavs_bound_ = false;
    std::uint64_t type2_batch_identity_ = 0u;
    bool type2_closed_batch_valid_ = false;
    std::uint64_t type2_closed_batch_identity_ = 0u;
    bool type2_non_type2_batch_valid_ = false;
    std::uint64_t type2_non_type2_batch_identity_ = 0u;
    std::uint32_t type2_list_draw_sequence_ = 0u;
    std::vector<NativePortVertex> prepared_vertices_;
    std::vector<BlendStateSlot> blend_states_;
    std::vector<DepthStateSlot> depth_states_;
    std::vector<RasterizerStateSlot> rasterizer_states_;
    std::vector<SamplerStateSlot> sampler_states_;
    ComPtr<ID3D11Texture2D> white_texture_;
    ComPtr<ID3D11ShaderResourceView> white_view_;

    std::vector<TextureSlot> texture_slots_;
    std::vector<std::uint32_t> free_texture_slots_;
    std::vector<MeshSlot> mesh_slots_;
    std::vector<std::uint32_t> free_mesh_slots_;
    std::filesystem::path capture_directory_;
    std::filesystem::path drawstream_directory_;
    std::filesystem::path graphics_diagnostic_directory_;
    std::filesystem::path graphics_breadcrumb_output_path_;
    std::filesystem::path graphics_breadcrumb_temporary_path_;
    std::vector<std::uint8_t> capture_pixels_;
    std::vector<GraphicsBreadcrumbRecord> graphics_breadcrumbs_;
    std::size_t graphics_breadcrumb_first_ = 0u;
    std::size_t graphics_breadcrumb_count_ = 0u;
    std::uint64_t graphics_breadcrumb_last_checkpoint_frame_ = 0u;
    std::uint64_t graphics_breadcrumb_last_checkpoint_draw_ = 0u;
    std::uint64_t graphics_digest_ = graphics_digest_seed;
    std::uint64_t graphics_frame_digest_ = graphics_digest_seed;
    NativePortGraphicsDiagnosticMode graphics_diagnostic_mode_ =
        NativePortGraphicsDiagnosticMode::Off;
    std::uint64_t capture_start_frame_ = 1u;
    std::uint64_t capture_end_frame_ =
        std::numeric_limits<std::uint64_t>::max();
    std::uint64_t capture_interval_ = 60u;
    bool capture_enabled_ = false;
    std::ofstream drawstream_output_;
    std::uint64_t drawstream_frame_ = 0u;
    std::uint64_t drawstream_end_frame_ = 0u;
    std::uint64_t drawstream_maximum_draws_per_frame_ = 0u;
    std::uint64_t drawstream_draws_this_frame_ = 0u;
    std::uint64_t drawstream_gpu_draws_this_frame_ = 0u;
    bool drawstream_truncation_reported_ = false;
    bool drawstream_enabled_ = false;
    bool graphics_breadcrumbs_dirty_ = false;
    bool graphics_breadcrumb_checkpoint_written_ = false;
    std::uint64_t texture_bytes_ = 0u;
    std::uint64_t mesh_bytes_ = 0u;
    std::uint32_t live_textures_ = 0u;
    std::uint32_t live_meshes_ = 0u;
    NativePortTextureHandle image_texture_;
    NativePortExtent image_texture_extent_;
    NativePortTextureFormat image_texture_format_ =
        NativePortTextureFormat::Bgra8Unorm;
    NativePortGraphicsSnapshot snapshot_;
};

#else

#include "../../linux/native_port_graphics_sdl.inc"

#endif

class NativePortGraphicsDevice::Impl final {
  public:
    explicit Impl(const NativePortGraphicsConfig& config)
        : title_storage_(copy_validated_graphics_title(config.title)),
          development_state_directory_storage_(
              copy_development_state_directory(
                  config.development_state_directory)),
          config_(config), producer_thread_(std::this_thread::get_id()),
          requested_mode_(native_port_render_thread_enabled()
                              ? NativePortGraphicsExecutionMode::Parallel
                              : NativePortGraphicsExecutionMode::SerialReference),
          active_mode_(requested_mode_) {
        config_.title = title_storage_;
        config_.development_state_directory =
            development_state_directory_storage_;
        validate_graphics_config(config_);

        NativePortFrameQueueConfig queue_config;
        queue_config.maximum_commands_per_frame =
            config_.maximum_render_commands_per_frame;
        queue_config.maximum_payload_bytes_per_frame =
            config_.maximum_render_payload_bytes_per_frame;
        queue_config.enabled = true;
        queue_config.threading_mode =
            serial()
                ? NativePortFrameQueueConfig::ThreadingMode::SerialReference
                : NativePortFrameQueueConfig::ThreadingMode::ParallelSpsc;
        queue_ = std::make_unique<NativePortFrameQueue>(queue_config);
        if (!queue_->enabled())
            fail_facade("render-command-queue-disabled");
        observe_render_queue_depth();

        if (serial()) {
            serial_backend_ =
                std::make_unique<NativePortGraphicsBackend>(
                    config_, &runtime_options_);
            cache_initial_backend(*serial_backend_);
            bind_serial_queue_domains();
            return;
        }

#ifdef _WIN32
        consumer_wake_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (consumer_wake_event_ == nullptr)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::RenderThreadContract,
                GetLastError(),
                "render-consumer-wake-event");
#endif
        try {
            consumer_thread_ = std::thread([this] { consumer_main(); });
            wait_for_consumer_startup();
            bind_parallel_producer_domain();
        } catch (...) {
            close_consumer_wake_event();
            throw;
        }
    }

    ~Impl() noexcept {
        if (std::this_thread::get_id() != producer_thread_) std::terminate();
        presentation_shutdown_.store(true, std::memory_order_release);
        abort_open_batch();
        producer_frame_open_ = false;
        try {
            wait_for_all_replies();
        } catch (...) {
        }
        try {
            if (queue_->snapshot().lifecycle ==
                NativePortFrameQueueLifecycle::Running) {
                submit_standalone_sync(
                    [](NativePortGraphicsCommandWriter& writer) {
                        return writer.shutdown();
                    },
                    NativePortGraphicsFailure::RenderThreadContract,
                    "render-shutdown-encode");
            }
        } catch (...) {
        }
        request_consumer_shutdown();
        if (consumer_thread_.joinable()) consumer_thread_.join();
        close_consumer_wake_event();
        serial_backend_.reset();
    }

    void show() {
        submit_control(
            [](NativePortGraphicsCommandWriter& writer) {
                return writer.show();
            },
            NativePortGraphicsFailure::RenderThreadContract,
            "render-show-encode");
    }

    void poll_events() {
        if (!serial()) {
            require_producer_thread();
            retire_available_replies();
            acquire_consumer_state_mailbox();
            require_queue_healthy();
            return;
        }
        submit_control(
            [](NativePortGraphicsCommandWriter& writer) {
                return writer.poll_events();
            },
            NativePortGraphicsFailure::RenderThreadContract,
            "render-poll-encode");
    }

    void configure_runtime_options(
        const std::uint32_t simulation_rate_hz,
        const std::uint32_t presentation_rate_hz,
        const std::uint32_t maximum_presentation_rate_hz,
        const bool frame_pacing_enabled) {
        require_producer_thread();
        runtime_options_.simulation_rate_hz.store(
            simulation_rate_hz, std::memory_order_release);
        runtime_options_.maximum_presentation_rate_hz.store(
            maximum_presentation_rate_hz, std::memory_order_release);
        runtime_options_.presentation_rate_hz.store(
            presentation_rate_hz, std::memory_order_release);
        runtime_options_.requested_presentation_rate_hz.store(
            presentation_rate_hz, std::memory_order_release);
        runtime_options_.frame_pacing_enabled.store(
            frame_pacing_enabled, std::memory_order_release);
        runtime_options_.independent_presentation_enabled.store(
            frame_pacing_enabled && !serial(), std::memory_order_release);
        signal_consumer_noexcept();
    }

    [[nodiscard]] bool independent_presentation_enabled() const noexcept {
        return runtime_options_.independent_presentation_enabled.load(
            std::memory_order_acquire);
    }

    [[nodiscard]] std::uint64_t missed_presentation_deadlines() const noexcept {
        return published_missed_presentations_.load(std::memory_order_acquire);
    }

    void record_simulation_frame_nonblocking() noexcept {
        saturating_atomic_increment(runtime_options_.simulation_frames);
    }

    [[nodiscard]] std::uint32_t
    requested_presentation_rate_nonblocking() const noexcept {
        return runtime_options_.requested_presentation_rate_hz.load(
            std::memory_order_acquire);
    }

    void acknowledge_presentation_rate_nonblocking(
        const std::uint32_t presentation_rate_hz) noexcept {
        runtime_options_.presentation_rate_hz.store(
            presentation_rate_hz, std::memory_order_release);
        signal_consumer_noexcept();
    }

    [[nodiscard]] std::optional<NativePortDevelopmentStateRequest>
    take_development_state_request() {
        require_producer_thread();
        const auto published =
            runtime_options_.state_request_publication.load(
                std::memory_order_acquire);
        const auto consumed =
            runtime_options_.state_request_consumed.load(
                std::memory_order_relaxed);
        if (published == consumed) return take_development_state_probe_request();
        const auto path_bytes = runtime_options_.state_request_path_bytes;
        if (path_bytes == 0u ||
            path_bytes >= runtime_options_.state_request_path.size()) {
            runtime_options_.state_request_consumed.store(
                published, std::memory_order_release);
            signal_consumer_noexcept();
            fail_facade("development-state-request-payload");
        }
        NativePortDevelopmentStateRequest request;
        request.operation = runtime_options_.state_request_operation;
        request.path.assign(runtime_options_.state_request_path.data(),
                            path_bytes);
        request.sequence = published;
        runtime_options_.state_request_consumed.store(
            published, std::memory_order_release);
        signal_consumer_noexcept();
        return request;
    }

    // Producer-owned diagnostic requests exercise the same save/load handler as
    // F5/F9 without replacing host services or publishing into the UI's SPSC slot.
    // The simulation counter is host-monotonic and is deliberately not rewound.
    [[nodiscard]] std::optional<NativePortDevelopmentStateRequest>
    take_development_state_probe_request() {
        if (!development_probe_initialized_) {
            development_probe_initialized_ = true;
            const auto* mode = std::getenv("KATANA_NATIVE_DEVELOPMENT_STATE_PROBE");
            const auto* initial_state = std::getenv("KATANA_NATIVE_INPUT_START_STATE");
            if (initial_state != nullptr && *initial_state != 0) {
                if (mode != nullptr && *mode != 0)
                    fail_facade("development-state-probe-conflicting-start");
                development_initial_state_path_ = initial_state;
                development_probe_count_ = 1u;
            } else if (mode != nullptr && *mode != 0) {
                if (std::strcmp(mode, "roundtrip") == 0) development_probe_count_ = 3u;
                else if (std::strcmp(mode, "load") == 0) development_probe_count_ = 1u;
                else fail_facade("development-state-probe-mode");
                if (development_state_directory_storage_.empty())
                    fail_facade("development-state-probe-directory");
            }
        }
        if (development_probe_next_ >= development_probe_count_)
            return std::nullopt;
        const auto frames = runtime_options_.simulation_frames.load(
            std::memory_order_relaxed);
        if (frames < (development_initial_state_path_.empty()
                          ? 600u + 180u * development_probe_next_ : 1u))
            return std::nullopt;
        const auto operation = development_probe_count_ == 3u && development_probe_next_ == 0u
            ? NativePortDevelopmentStateOperation::Save
            : NativePortDevelopmentStateOperation::Load;
        const auto directory = std::filesystem::u8path(development_state_directory_storage_);
        if (operation == NativePortDevelopmentStateOperation::Save) {
            std::error_code error;
            std::filesystem::create_directories(directory, error);
            if (error) fail_facade("development-state-probe-create-directory");
        }
        const auto utf8 = (development_initial_state_path_.empty()
            ? directory / "quicksave.kstate"
            : std::filesystem::u8path(development_initial_state_path_)).u8string();
        NativePortDevelopmentStateRequest request;
        request.operation = operation;
        request.path.assign(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        request.sequence = 0x4000000000000000ull + ++development_probe_next_;
        std::fprintf(stderr,
            "KATANA_NATIVE_DEVELOPMENT_PROBE operation=%s sequence=%llu simulation_frames=%llu\n",
            operation == NativePortDevelopmentStateOperation::Save ? "save" : "load",
            static_cast<unsigned long long>(request.sequence),
            static_cast<unsigned long long>(frames));
        return request;
    }

    [[nodiscard]] NativePortLifecycleState lifecycle_state() {
        require_producer_thread();
        retire_available_replies();
        acquire_consumer_state_mailbox();
        require_queue_healthy();
        return cached_lifecycle_;
    }

    [[nodiscard]] NativePortGraphicsLayout layout() {
        require_producer_thread();
        retire_available_replies();
        acquire_consumer_state_mailbox();
        require_queue_healthy();
        return cached_layout_;
    }

    [[nodiscard]] NativePortTextureHandle create_texture(
        const NativePortTextureConfig& config,
        const std::span<const NativePortImageView> initial_mip_levels) {
        require_producer_thread();
        require_resource_payload(
            native_port_graphics_encoded_create_texture_size(
                config, initial_mip_levels));
        const NativePortTextureHandle logical{next_logical_handle(
            next_texture_handle_, "texture-handle-exhausted")};
        submit_resource_sync(
            [&](NativePortGraphicsCommandWriter& writer) {
                return writer.create_texture(
                    logical, config, initial_mip_levels);
            },
            NativePortGraphicsFailure::InvalidResource,
            "texture-command-encode");
        return logical;
    }

    void update_texture(const NativePortTextureHandle texture,
                        const NativePortImageView& pixels) {
        update_texture(
            texture, std::span<const NativePortImageView>(&pixels, 1u));
    }

    void update_texture(
        const NativePortTextureHandle texture,
        const std::span<const NativePortImageView> mip_levels) {
        require_resource_payload(
            native_port_graphics_encoded_update_texture_size(mip_levels));
        submit_resource_sync(
            [&](NativePortGraphicsCommandWriter& writer) {
                return writer.update_texture(texture, mip_levels);
            },
            NativePortGraphicsFailure::InvalidResource,
            "texture-update-command-encode");
    }

    void destroy_texture(const NativePortTextureHandle texture) {
        submit_resource_sync(
            [&](NativePortGraphicsCommandWriter& writer) {
                return writer.destroy_texture(texture);
            },
            NativePortGraphicsFailure::InvalidResource,
            "texture-destroy-command-encode");
    }

    [[nodiscard]] NativePortMeshHandle create_mesh(
        const NativePortMeshConfig& config) {
        require_producer_thread();
        require_resource_payload(
            native_port_graphics_encoded_create_mesh_size(config));
        const NativePortMeshHandle logical{next_logical_handle(
            next_mesh_handle_, "mesh-handle-exhausted")};
        submit_resource_sync(
            [&](NativePortGraphicsCommandWriter& writer) {
                return writer.create_mesh(logical, config);
            },
            NativePortGraphicsFailure::InvalidResource,
            "mesh-command-encode");
        return logical;
    }

    void destroy_mesh(const NativePortMeshHandle mesh) {
        submit_resource_sync(
            [&](NativePortGraphicsCommandWriter& writer) {
                return writer.destroy_mesh(mesh);
            },
            NativePortGraphicsFailure::InvalidResource,
            "mesh-destroy-command-encode");
    }

    void begin_frame(const NativePortFrameConfig& config) {
        require_producer_thread();
        if (producer_frame_open_)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidFrame,
                1u,
                "frame-already-open");

        begin_batch();
        try {
            if(sonic::presentation::Settings::interpolation && sonic::motion::submitted_frame.enabled)
                motion_annotations_.push_back({batch_command_count_,sonic::motion::submitted_frame,{}});
            append_open(
                [&](NativePortGraphicsCommandWriter& writer) {
                    return writer.begin_frame(config);
                },
                NativePortGraphicsFailure::InvalidFrame,
                "frame-command-encode");
            producer_frame_open_ = true;
        } catch (...) {
            producer_frame_open_ = false;
            throw;
        }
    }

    void draw(const NativePortDrawPacket& packet) {
        require_producer_thread();
        if (!producer_frame_open_)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidDraw,
                1u,
                "draw-outside-frame");
        if(sonic::presentation::Settings::interpolation &&
           (sonic::motion::submitted_draw.enabled||sonic::motion::submitted_draw.world))
            motion_annotations_.push_back({batch_command_count_,{},sonic::motion::submitted_draw});
        append_open(
            [&](NativePortGraphicsCommandWriter& writer) {
                return writer.draw(packet);
            },
            NativePortGraphicsFailure::InvalidDraw,
            "draw-command-encode");
    }

    void flush_type2_translucency() {
        require_producer_thread();
        if (!producer_frame_open_)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidFrame,
                1u,
                "type2-flush-outside-frame");
        append_open(
            [](NativePortGraphicsCommandWriter& writer) {
                return writer.flush_type2_translucency();
            },
            NativePortGraphicsFailure::InvalidFrame,
            "type2-flush-command-encode");
    }

    void present() {
        require_producer_thread();
        if (!producer_frame_open_)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidFrame,
                1u,
                "present-without-frame");
        try {
            append_open(
                [](NativePortGraphicsCommandWriter& writer) {
                    return writer.present();
                },
                NativePortGraphicsFailure::InvalidFrame,
                "present-command-encode");
            batch_render_completion_ = {batch_command_count_ - 1u,
                                        sonic::render_completion::capture()};
            // Retire the preceding submitted frame before publishing this one.
            // The just-published frame remains asynchronous, so simulation can
            // build the next frame while the consumer renders it. This admits
            // at most one waiting frame with the depth-2 queue.
            publish_open_batch(false);
            producer_frame_open_ = false;
        } catch (...) {
            producer_frame_open_ = false;
            throw;
        }
    }

    void repeat_present() {
        require_producer_thread();
        if (producer_frame_open_)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidFrame,
                1u,
                "repeat-present-with-open-frame");
        submit_standalone_sync(
            [](NativePortGraphicsCommandWriter& writer) {
                return writer.repeat_present();
            },
            NativePortGraphicsFailure::InvalidFrame,
            "repeat-present-command-encode");
    }

    void repeat_present_async() {
        require_producer_thread();
        if (producer_frame_open_)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidFrame,
                1u,
                "repeat-present-with-open-frame");
        submit_standalone_async(
            [](NativePortGraphicsCommandWriter& writer) {
                return writer.repeat_present();
            },
            NativePortGraphicsFailure::InvalidFrame,
            "repeat-present-command-encode");
    }

    void present_image(const NativePortImageView& image,
                       const NativePortViewportTarget viewport,
                       const NativePortImageFit fit) {
        require_producer_thread();
        if (producer_frame_open_)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidFrame,
                1u,
                "present-image-with-open-frame");
        require_resource_payload(
            native_port_graphics_encoded_present_image_size(image));
        submit_standalone_sync(
            [&](NativePortGraphicsCommandWriter& writer) {
                return writer.present_image(image, viewport, fit);
            },
            NativePortGraphicsFailure::InvalidFrame,
            "present-image-command-encode");
    }

    void finish() {
        require_producer_thread();
        if (producer_frame_open_ || batch_writer_.has_value())
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidFrame,
                1u,
                "render-finish-open-frame");
        // Quicksave/restore also use this reusable drain. Pause idle repeats
        // only until the ordered fence completes; do not terminate the clock.
        presentation_paused_.store(true, std::memory_order_release);
        signal_consumer_noexcept();
        wait_for_all_replies();
        acquire_consumer_state_mailbox();
        require_queue_healthy();
        // This final ordered fence executes on the backend owner and returns
        // a sequence-bound snapshot without changing the normal parallel
        // lifecycle polling path.
        submit_standalone_sync(
            [](NativePortGraphicsCommandWriter& writer) {
                return writer.poll_events();
            },
            NativePortGraphicsFailure::RenderThreadContract,
            "render-finish-encode");
        acquire_consumer_state_mailbox();
        require_queue_healthy();
        presentation_paused_.store(false, std::memory_order_release);
        signal_consumer_noexcept();
    }

    [[nodiscard]] NativePortGraphicsSnapshot snapshot() {
        require_producer_thread();
        if (!producer_frame_open_)
            wait_for_all_replies();
        else
            retire_available_replies();
        acquire_consumer_state_mailbox();
        require_queue_healthy();
        auto result = cached_snapshot_;
        if (producer_frame_open_) result.frame_open = true;
        decorate_snapshot(result);
        return result;
    }

    [[nodiscard]] std::uint64_t
    presented_frames_nonblocking() const noexcept {
        return published_presented_frames_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::uint64_t
    completed_drawn_frames_nonblocking() const noexcept {
        return published_completed_drawn_frames_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::uint64_t
    repeated_presentations_nonblocking() const noexcept {
        return published_repeated_presentations_.load(
            std::memory_order_acquire);
    }

    [[nodiscard]] bool frame_recording_open_nonblocking() const noexcept {
        return producer_frame_open_;
    }

  private:
    enum class StartupState : std::uint8_t { Pending, Ready, Failed };

    struct BackendError final {
        bool valid = false;
        NativePortGraphicsFailure failure =
            NativePortGraphicsFailure::RenderThreadContract;
        std::uint32_t platform_error_code = 0u;
        std::uint64_t operation_id = 0u;
    };
    static_assert(std::is_trivially_copyable_v<BackendError>);

    struct ConsumerStateMailbox final {
        std::atomic_flag lock = ATOMIC_FLAG_INIT;
        std::atomic<std::uint64_t> publication{0u};
        std::uint64_t state_revision = 0u;
        NativePortGraphicsLayout layout;
        NativePortLifecycleState lifecycle =
            NativePortLifecycleState::Running;
        BackendError error;
    };

    struct ReplySlot final {
        std::atomic<std::uint64_t> ready_sequence{0u};
        BackendError error;
        NativePortGraphicsSnapshot snapshot;
        NativePortGraphicsLayout layout;
        NativePortLifecycleState lifecycle =
            NativePortLifecycleState::Running;
        std::uint64_t state_revision = 0u;
        std::uint32_t failed_ordinal = 0u;
    };

    struct RenderCompletion final {
        std::uint64_t ordinal = 0u;
        sonic::render_completion::Ticket ticket;
    };

    static void saturating_add_value(
        std::uint64_t& value, const std::uint64_t amount) noexcept {
        value = amount > std::numeric_limits<std::uint64_t>::max() - value
                    ? std::numeric_limits<std::uint64_t>::max()
                    : value + amount;
    }

    class ScopedProducerWait final {
      public:
        ScopedProducerWait(std::uint64_t& total, const bool enabled) noexcept
            : total_(enabled ? &total : nullptr),
              started_(enabled ? std::chrono::steady_clock::now()
                               : std::chrono::steady_clock::time_point{}) {}

        ~ScopedProducerWait() noexcept {
            if (total_ == nullptr) return;
            const auto elapsed = std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started_)
                                     .count();
            if (elapsed > 0)
                saturating_add_value(
                    *total_, static_cast<std::uint64_t>(elapsed));
        }

        ScopedProducerWait(const ScopedProducerWait&) = delete;
        ScopedProducerWait& operator=(const ScopedProducerWait&) = delete;

      private:
        std::uint64_t* total_ = nullptr;
        std::chrono::steady_clock::time_point started_;
    };

    [[nodiscard]] bool serial() const noexcept {
        return active_mode_ ==
               NativePortGraphicsExecutionMode::SerialReference;
    }

    static void saturating_atomic_add(
        std::atomic<std::uint64_t>& value,
        const std::uint64_t amount = 1u) noexcept {
        auto current = value.load(std::memory_order_relaxed);
        for (;;) {
            const auto next =
                amount > std::numeric_limits<std::uint64_t>::max() - current
                    ? std::numeric_limits<std::uint64_t>::max()
                    : current + amount;
            if (value.compare_exchange_weak(
                    current, next, std::memory_order_relaxed,
                    std::memory_order_relaxed))
                return;
        }
    }

    [[nodiscard]] static std::uint64_t next_logical_handle(
        std::uint64_t& next,
        const char* const operation) {
        if (next == 0u || next == std::numeric_limits<std::uint64_t>::max())
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::ResourceLimit, 1u, operation);
        return next++;
    }

    [[nodiscard]] static bool is_resource_command(
        const NativePortGraphicsCommandKind kind) noexcept {
        switch (kind) {
        case NativePortGraphicsCommandKind::CreateTexture:
        case NativePortGraphicsCommandKind::UpdateTexture:
        case NativePortGraphicsCommandKind::DestroyTexture:
        case NativePortGraphicsCommandKind::CreateMesh:
        case NativePortGraphicsCommandKind::DestroyMesh:
            return true;
        default:
            return false;
        }
    }

    void require_resource_payload(
        const std::optional<NativePortGraphicsCommandPayloadRequirement>&
            requirement) const {
        const auto capacity = requirement.has_value()
                                  ? requirement->capacity_from(0u)
                                  : std::nullopt;
        if (!capacity.has_value() ||
            *capacity >
                config_.maximum_render_resource_payload_bytes_per_command)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::ResourceLimit, 1u,
                "render-resource-command-capacity");
    }

    [[noreturn]] static void fail_facade(const char* const operation) {
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::RenderThreadContract, 1u, operation);
    }

    [[nodiscard]] static BackendError facade_error(
        const char* const operation) noexcept {
        return {true, NativePortGraphicsFailure::RenderThreadContract, 1u,
                native_port_graphics_operation_id(operation)};
    }

    [[nodiscard]] static BackendError capture_backend_error(
        const NativePortGraphicsError& error) noexcept {
        return {true, error.failure(), error.platform_error_code(),
                error.operation_id()};
    }

    [[noreturn]] static void throw_backend_error(const BackendError error) {
        throw NativePortGraphicsError(error.failure,
                                      error.platform_error_code,
                                      error.operation_id);
    }

    void require_producer_thread() const {
        if (std::this_thread::get_id() != producer_thread_)
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::ThreadViolation,
                1u,
                "render-producer-thread");
    }

    void lock_consumer_state_mailbox() noexcept {
        while (consumer_state_mailbox_.lock.test_and_set(
            std::memory_order_acquire))
            consumer_state_mailbox_.lock.wait(
                true, std::memory_order_relaxed);
    }

    void unlock_consumer_state_mailbox() noexcept {
        consumer_state_mailbox_.lock.clear(std::memory_order_release);
        consumer_state_mailbox_.lock.notify_one();
    }

    [[nodiscard]] std::uint64_t next_consumer_state_revision() noexcept {
        if (consumer_state_revision_ ==
            std::numeric_limits<std::uint64_t>::max())
            std::terminate();
        return ++consumer_state_revision_;
    }

    void publish_consumer_state_mailbox(
        const NativePortGraphicsLayout& layout,
        const NativePortLifecycleState lifecycle) noexcept {
        publish_consumer_state_mailbox(layout, lifecycle, BackendError{});
    }

    void publish_consumer_state_mailbox(
        const NativePortGraphicsLayout& layout,
        const NativePortLifecycleState lifecycle,
        const BackendError error) noexcept {
        const auto revision = next_consumer_state_revision();
        lock_consumer_state_mailbox();
        consumer_state_mailbox_.layout = layout;
        consumer_state_mailbox_.lifecycle = lifecycle;
        consumer_state_mailbox_.state_revision = revision;
        if (error.valid && !consumer_state_mailbox_.error.valid)
            consumer_state_mailbox_.error = error;
        const auto publication =
            consumer_state_mailbox_.publication.load(
                std::memory_order_relaxed);
        consumer_state_mailbox_.publication.store(
            publication == std::numeric_limits<std::uint64_t>::max()
                ? 1u
                : publication + 1u,
            std::memory_order_release);
        unlock_consumer_state_mailbox();
    }

    void publish_consumer_error_mailbox(
        const BackendError error) noexcept {
        lock_consumer_state_mailbox();
        if (!consumer_state_mailbox_.error.valid)
            consumer_state_mailbox_.error = error;
        const auto publication =
            consumer_state_mailbox_.publication.load(
                std::memory_order_relaxed);
        consumer_state_mailbox_.publication.store(
            publication == std::numeric_limits<std::uint64_t>::max()
                ? 1u
                : publication + 1u,
            std::memory_order_release);
        unlock_consumer_state_mailbox();
    }

    void acquire_consumer_state_mailbox() {
        if (serial()) return;
        auto publication = consumer_state_mailbox_.publication.load(
            std::memory_order_acquire);
        if (publication == acquired_state_mailbox_publication_) return;
        NativePortGraphicsLayout layout;
        NativePortLifecycleState lifecycle =
            NativePortLifecycleState::Running;
        BackendError error;
        std::uint64_t revision = 0u;
        lock_consumer_state_mailbox();
        publication = consumer_state_mailbox_.publication.load(
            std::memory_order_relaxed);
        if (publication != acquired_state_mailbox_publication_) {
            layout = consumer_state_mailbox_.layout;
            lifecycle = consumer_state_mailbox_.lifecycle;
            revision = consumer_state_mailbox_.state_revision;
            error = consumer_state_mailbox_.error;
            consumer_state_mailbox_.error = {};
        }
        unlock_consumer_state_mailbox();
        if (publication == acquired_state_mailbox_publication_) return;
        acquired_state_mailbox_publication_ = publication;
        if (revision > cached_state_revision_) {
            cached_layout_ = layout;
            cached_lifecycle_ = lifecycle;
            cached_state_revision_ = revision;
        }
        if (error.valid) throw_backend_error(error);
    }

    void close_consumer_wake_event() noexcept {
#ifdef _WIN32
        if (consumer_wake_event_ == nullptr) return;
        CloseHandle(consumer_wake_event_);
        consumer_wake_event_ = nullptr;
#endif
    }

    void signal_consumer() {
#ifdef _WIN32
        if (!serial() && consumer_wake_event_ != nullptr &&
            SetEvent(consumer_wake_event_) == FALSE) {
            const auto code = GetLastError();
            queue_->report_producer_error(
                NativePortFrameQueueError::ProducerException,
                last_submitted_sequence_);
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::RenderThreadContract,
                code == 0u ? 1u : code,
                "render-consumer-wake");
        }
#else
        signal_consumer_noexcept();
#endif
    }

    void signal_consumer_noexcept() noexcept {
#ifdef _WIN32
        if (!serial() && consumer_wake_event_ != nullptr)
            static_cast<void>(SetEvent(consumer_wake_event_));
#else
        if (!serial()) {
            {
                std::lock_guard guard(consumer_wake_mutex_);
                consumer_wake_pending_ = true;
            }
            consumer_wake_condition_.notify_one();
        }
#endif
    }

    void request_consumer_shutdown() noexcept {
        queue_->request_shutdown();
        signal_consumer_noexcept();
    }

    [[nodiscard]] bool pump_consumer_events(
        NativePortGraphicsBackend& backend) noexcept {
        try {
            backend.poll_events();
            publish_consumer_state_mailbox(
                backend.layout(), backend.lifecycle_state());
            return true;
        } catch (const NativePortGraphicsError& error) {
            publish_consumer_error_mailbox(capture_backend_error(error));
        } catch (...) {
            publish_consumer_error_mailbox(
                facade_error("render-consumer-message-pump"));
        }
        queue_->report_consumer_error(
            NativePortFrameQueueError::ConsumerException, 0u);
        return false;
    }

    void cache_initial_backend(NativePortGraphicsBackend& backend) {
        cached_snapshot_ = backend.snapshot();
        cached_layout_ = backend.layout();
        cached_lifecycle_ = backend.lifecycle_state();
        published_presented_frames_.store(
            cached_snapshot_.presented_frames, std::memory_order_release);
    }

    void bind_serial_queue_domains() {
        auto producer = queue_->try_begin_produce();
        if (!producer.has_value()) throw_queue_failure("serial-producer-bind");
        producer->abort();
        static_cast<void>(queue_->try_begin_consume());
        const auto queue_snapshot = queue_->snapshot();
        if (queue_snapshot.producer_thread_identity == 0u ||
            queue_snapshot.consumer_thread_identity == 0u ||
            queue_snapshot.producer_thread_identity !=
                queue_snapshot.consumer_thread_identity)
            fail_facade("serial-render-thread-domains");
    }

    void bind_parallel_producer_domain() {
        auto producer = queue_->try_begin_produce();
        if (!producer.has_value()) {
            request_consumer_shutdown();
            consumer_thread_.join();
            throw_queue_failure("render-producer-bind");
        }
        producer->abort();
        const auto queue_snapshot = queue_->snapshot();
        if (queue_snapshot.producer_thread_identity == 0u ||
            queue_snapshot.consumer_thread_identity == 0u ||
            queue_snapshot.producer_thread_identity ==
                queue_snapshot.consumer_thread_identity) {
            queue_->report_producer_error(
                NativePortFrameQueueError::ThreadDomainOverlap, 0u);
            request_consumer_shutdown();
            consumer_thread_.join();
            fail_facade("render-thread-domains");
        }
    }

    void wait_for_consumer_startup() {
        auto state = startup_state_.load(std::memory_order_acquire);
        while (state == StartupState::Pending) {
            startup_state_.wait(state, std::memory_order_acquire);
            state = startup_state_.load(std::memory_order_acquire);
        }
        if (state == StartupState::Failed) {
            consumer_thread_.join();
            if (startup_error_.valid) throw_backend_error(startup_error_);
            fail_facade("render-consumer-startup");
        }
        cached_snapshot_ = startup_snapshot_;
        cached_layout_ = startup_layout_;
        cached_lifecycle_ = startup_lifecycle_;
        cached_state_revision_ = startup_state_revision_;
        acquire_consumer_state_mailbox();
        published_presented_frames_.store(
            cached_snapshot_.presented_frames, std::memory_order_release);
    }

    void consumer_main() noexcept {
        std::unique_ptr<NativePortGraphicsBackend> backend;
        try {
            backend = std::make_unique<NativePortGraphicsBackend>(
                config_, &runtime_options_);
            static_cast<void>(queue_->try_begin_consume());
            startup_snapshot_ = backend->snapshot();
            startup_layout_ = backend->layout();
            startup_lifecycle_ = backend->lifecycle_state();
            startup_state_revision_ = next_consumer_state_revision();
            lock_consumer_state_mailbox();
            consumer_state_mailbox_.layout = startup_layout_;
            consumer_state_mailbox_.lifecycle = startup_lifecycle_;
            consumer_state_mailbox_.state_revision = startup_state_revision_;
            consumer_state_mailbox_.publication.store(
                1u, std::memory_order_release);
            unlock_consumer_state_mailbox();
            startup_state_.store(StartupState::Ready,
                                 std::memory_order_release);
            startup_state_.notify_all();
        } catch (const NativePortGraphicsError& error) {
            startup_error_ = capture_backend_error(error);
            startup_state_.store(StartupState::Failed,
                                 std::memory_order_release);
            startup_state_.notify_all();
            return;
        } catch (...) {
            startup_error_ = facade_error("render-consumer-startup");
            startup_state_.store(StartupState::Failed,
                                 std::memory_order_release);
            startup_state_.notify_all();
            return;
        }

        for (;;) {
            if (auto lease = queue_->try_begin_consume(); lease.has_value()) {
                if (consume_lease(*backend, *lease)) break;
                // Drain queued resources/geometry before repeating an old
                // image. Producer replies must not wait behind idle output.
                continue;
            }
            // ShowWindow/DispatchMessage can deliver resize state
            // synchronously without leaving a queued message. Pump exactly
            // once while transitioning to idle, publish that state, then
            // block on the queue event or the next Windows message.
            if (!pump_consumer_events(*backend)) break;
            const auto lifecycle = queue_->snapshot().lifecycle;
            if (lifecycle == NativePortFrameQueueLifecycle::Disabled ||
                lifecycle == NativePortFrameQueueLifecycle::Stopped ||
                lifecycle == NativePortFrameQueueLifecycle::Failed)
                break;
            if (!service_idle_presentation(*backend)) break;
#ifdef _WIN32
            const HANDLE handles[]{consumer_wake_event_};
            const auto wait_result = MsgWaitForMultipleObjectsEx(
                1u, handles, presentation_wait_milliseconds(*backend),
                QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (wait_result == WAIT_TIMEOUT) continue;
            if (wait_result == WAIT_OBJECT_0) continue;
            if (wait_result == WAIT_OBJECT_0 + 1u) {
                if (!pump_consumer_events(*backend)) break;
                continue;
            }
            const auto code = GetLastError();
            publish_consumer_error_mailbox(
                {true,
                 NativePortGraphicsFailure::RenderThreadContract,
                 code == 0u ? 1u : code,
                 native_port_graphics_operation_id(
                     "render-consumer-message-wait")});
            queue_->report_consumer_error(
                NativePortFrameQueueError::ConsumerException, 0u);
            break;
#else
            // A static host menu produces no new draw packets. Waiting only
            // on the command queue starves SDL input and repeat presentation.
            // Keep the window owner pumping at bounded latency while letting
            // newly published work and shutdown wake it immediately.
            std::unique_lock wake_lock(consumer_wake_mutex_);
            consumer_wake_condition_.wait_for(wake_lock,
                std::chrono::milliseconds(std::min(8u, presentation_wait_milliseconds(*backend))),
                [this] { return consumer_wake_pending_; });
            consumer_wake_pending_ = false;
#endif
        }
        request_consumer_shutdown();
        backend.reset();
    }

    [[nodiscard]] static std::uint64_t presentation_now() noexcept {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    void update_presentation_deadline(const NativePortGraphicsBackend& backend) noexcept {
        const auto manual = runtime_options_.presentation_rate_hz.load(std::memory_order_acquire);
        const auto rate = backend.effective_presentation_rate(manual);
        if(consumer_presentation_manual_rate_!=manual||consumer_presentation_rate_!=rate) {
            if(sonic::presentation::settings().vsync==1)
                std::fprintf(stderr,"SONIC_PRESENT_CLOCK vsync=on saved_cap=%u display_hz=%u cap_applied=0\n",manual,rate);
            consumer_presentation_manual_rate_=manual;
        }
        if (consumer_presentation_rate_ == rate &&
            consumer_presentation_deadline_ != 0u) return;
        consumer_presentation_rate_ = rate;
        consumer_presentation_remainder_ = 0u;
        // The first available complete image is output immediately. A later
        // rate change starts a fresh output epoch without touching title time.
        const bool started = consumer_presentation_deadline_ != 0u;
        consumer_presentation_deadline_ = presentation_now();
        if (started)
            advance_frame_deadline(consumer_presentation_deadline_,
                consumer_presentation_remainder_, rate);
    }

    void advance_presentation_deadline(const std::uint64_t now) noexcept {
        advance_frame_deadline(consumer_presentation_deadline_,
            consumer_presentation_remainder_, consumer_presentation_rate_);
        while (consumer_presentation_deadline_ <= now) {
            saturating_atomic_add(published_missed_presentations_);
            advance_frame_deadline(consumer_presentation_deadline_,
                consumer_presentation_remainder_, consumer_presentation_rate_);
        }
    }

    void present_on_consumer_deadline(NativePortGraphicsBackend& backend,
                                      const bool wait,
                                      const char* const operation = "repeat-present") {
        update_presentation_deadline(backend);
        auto now = presentation_now();
        if (wait && sonic::presentation::settings().gameplay_timing==0u) {
            // Original title/movie/UI commands already own their frame rate.
            // Do not quantize 25/30/50-Hz delivery onto a second 60-Hz clock.
            consumer_presentation_deadline_=now;
            consumer_presentation_remainder_=0u;
        }
        const bool wait_for_output = wait &&
            (sonic::presentation::settings().gameplay_timing==0u ||
             !backend.nonblocking_output_enabled());
        if (wait_for_output && now < consumer_presentation_deadline_) {
            wait_until_monotonic_nanoseconds(consumer_presentation_deadline_);
            now = presentation_now();
        }
        if (now < consumer_presentation_deadline_) return;
        render_motion_sample(backend,now);
        if (!backend.completed_image_available()) {
            // There is no output authority before the first complete image.
            saturating_atomic_add(published_missed_presentations_);
            advance_presentation_deadline(now);
            return;
        }
        const auto before = backend.snapshot().presented_frames;
        const auto outcome = backend.present_completed_image_on_deadline(operation);
        const auto after = backend.snapshot().presented_frames;
        if (outcome == NativePortBackendPresentOutcome::Deferred) {
            // The completed image remains authoritative and will be retried
            // only when the next software deadline is reached.
            saturating_atomic_add(published_missed_presentations_);
        } else if (after > before) {
            if (consumer_completed_image_presented_ && !motion_output_changed_)
                saturating_add_value(consumer_repeated_presentations_, after - before);
            consumer_completed_image_presented_ = true;
            motion_output_changed_=false;
        }
        backend.publish_telemetry();
        published_repeated_presentations_.store(
            consumer_repeated_presentations_, std::memory_order_release);
        published_presented_frames_.store(after, std::memory_order_release);
        if(backend.driver_paces_output() && outcome==NativePortBackendPresentOutcome::Presented) {
            // VSync owns visible output pacing. Do not add a software FPS cap
            // after blocking Present/FIFO acquire, which would skip vblanks.
            consumer_presentation_deadline_=presentation_now();
            consumer_presentation_remainder_=0u;
        } else {
            // Occluded/hidden windows and deferred presents may return at once.
            // Keep these bounded at the display cadence, never a busy retry loop.
            advance_presentation_deadline(presentation_now());
        }
    }

    [[nodiscard]] bool autonomous_presentation_active(
        const NativePortGraphicsBackend& backend) const noexcept {
        return backend.lifecycle_state() == NativePortLifecycleState::Running &&
            independent_presentation_enabled() &&
            (sonic::presentation::settings().gameplay_timing!=0u || !consumer_completed_image_presented_) &&
            !presentation_shutdown_.load(std::memory_order_acquire) &&
            !presentation_paused_.load(std::memory_order_acquire) &&
            !consumer_presentation_faulted_ &&
            queue_->snapshot().lifecycle == NativePortFrameQueueLifecycle::Running;
    }

    [[nodiscard]] bool service_idle_presentation(
        NativePortGraphicsBackend& backend) noexcept {
        if (backend.lifecycle_state() != NativePortLifecycleState::Running) {
            // A paused/closed window cannot authorize autonomous output.
            // Resume starts a new epoch; window downtime is not a missed frame.
            consumer_presentation_deadline_ = 0u;
            consumer_presentation_remainder_ = 0u;
            return true;
        }
        if (!autonomous_presentation_active(backend)) return true;
        // No output epoch before the first completed image. A later prefix
        // retains that immutable image while its own working list is open.
        if (consumer_presentation_deadline_ == 0u &&
            !backend.completed_image_available()) return true;
        try {
            present_on_consumer_deadline(backend, false);
            return true;
        } catch (const NativePortGraphicsError& error) {
            publish_consumer_error_mailbox(capture_backend_error(error));
        } catch (...) {
            publish_consumer_error_mailbox(facade_error("render-idle-presentation"));
        }
        queue_->report_consumer_error(
            NativePortFrameQueueError::ConsumerException, 0u);
        return false;
    }

    [[nodiscard]] std::uint32_t presentation_wait_milliseconds(
        const NativePortGraphicsBackend& backend) const noexcept {
        if (backend.lifecycle_state() != NativePortLifecycleState::Running ||
            !independent_presentation_enabled() ||
            (sonic::presentation::settings().gameplay_timing==0u && consumer_completed_image_presented_) ||
            presentation_shutdown_.load(std::memory_order_acquire) ||
            presentation_paused_.load(std::memory_order_acquire) ||
            consumer_presentation_faulted_ || consumer_presentation_deadline_ == 0u)
            return std::numeric_limits<std::uint32_t>::max();
        const auto now = presentation_now();
        if (now >= consumer_presentation_deadline_) return 0u;
        return static_cast<std::uint32_t>(std::min<std::uint64_t>(
            (consumer_presentation_deadline_ - now + 999'999u) / 1'000'000u,
            1'000u));
    }

    [[nodiscard]] bool consume_lease(
        NativePortGraphicsBackend& backend,
        NativePortFrameReadLease& lease) noexcept {
        const auto sequence = lease.sequence();
        // Producer writes this sidecar before queue publication (release).
        // Acquiring the matching read lease makes it visible; the reply must
        // be retired before this depth-2 slot can be reused by the producer.
        const auto render_completion = render_completions_[
            static_cast<std::size_t>((sequence - 1u) % render_completions_.size())];
        std::vector<sonic::motion::Annotation> annotations;
        if constexpr(sonic::presentation::Settings::interpolation) {
            const std::lock_guard lock(motion_annotations_mutex_);
            const auto it=motion_published_annotations_.find(sequence);
            if(it!=motion_published_annotations_.end()){
                annotations=std::move(it->second);motion_published_annotations_.erase(it);
            }
        }
        std::size_t annotation_index=0;
        observe_render_queue_depth();
        NativePortGraphicsCommandReader reader(lease);
        if (!reader.valid() || reader.size() == 0u) {
            const auto error = facade_error("render-command-frame");
            lease.fail(NativePortFrameQueueError::InvalidCommandRange);
            observe_render_queue_depth();
            publish_backend_reply(backend, sequence, error, 0u);
            return true;
        }

        BackendError first_error;
        std::uint32_t first_failed_ordinal = 0u;
        bool shutdown = false;
        bool terminal = false;
        const auto command_count = reader.size();
        saturating_atomic_add(consumed_commands_, command_count);
        last_consumed_sequence_.store(sequence, std::memory_order_release);
        while (auto command = reader.next()) {
            const auto ordinal = command->ordinal;
            try {
                const sonic::motion::Annotation* annotation=nullptr;
                if(annotation_index<annotations.size()&&annotations[annotation_index].ordinal==ordinal)
                    annotation=&annotations[annotation_index++];
                shutdown = execute_command(backend, *command,annotation) || shutdown;
                if (render_completion.ticket && render_completion.ordinal == ordinal) {
                    if (command->kind != NativePortGraphicsCommandKind::Present ||
                        !render_completion.ticket.complete())
                        fail_facade("guest-render-completion-order");
                }
                // Long draw leases must not monopolize the immediate context
                // for several output periods. Bound clock/atomic checks to
                // one per 16 commands and keep any failure inside this lease's
                // normal atomic abort/skip-rest-of-frame handling.
                if (command->kind == NativePortGraphicsCommandKind::Draw &&
                    (ordinal & 15u) == 15u &&
                    backend.nonblocking_output_enabled() &&
                    autonomous_presentation_active(backend) &&
                    backend.completed_image_available())
                    present_on_consumer_deadline(backend, false);
                saturating_atomic_add(executed_commands_);
                last_executed_sequence_.store(sequence,
                                              std::memory_order_release);
            } catch (const NativePortGraphicsError& error) {
                if (!first_error.valid) {
                    first_error = capture_backend_error(error);
                    first_failed_ordinal = ordinal;
                }
                saturating_atomic_add(failed_commands_);
                last_failed_sequence_.store(sequence,
                                            std::memory_order_release);
                last_failed_ordinal_.store(ordinal,
                                           std::memory_order_release);
                const auto skipped = command_count -
                    (static_cast<std::size_t>(ordinal) + 1u);
                saturating_atomic_add(skipped_commands_, skipped);
                if (!is_resource_command(command->kind)) {
                    consumer_drawn_frame_open_ = false;
                    backend.abort_frame_after_command_failure();
                }
                break;
            } catch (...) {
                first_error = facade_error("render-consumer-exception");
                first_failed_ordinal = ordinal;
                saturating_atomic_add(failed_commands_);
                last_failed_sequence_.store(sequence,
                                            std::memory_order_release);
                last_failed_ordinal_.store(ordinal,
                                           std::memory_order_release);
                const auto skipped = command_count -
                    (static_cast<std::size_t>(ordinal) + 1u);
                saturating_atomic_add(skipped_commands_, skipped);
                backend.abort_frame_after_command_failure();
                consumer_drawn_frame_open_ = false;
                terminal = true;
                break;
            }
        }

        if (first_error.valid) {
            consumer_presentation_faulted_ = true;
            motion_history_.abort();motion_buffering_=false;
        }
        if (terminal) {
            lease.fail(NativePortFrameQueueError::ConsumerException);
            observe_render_queue_depth();
            publish_backend_reply(
                backend, sequence, first_error, first_failed_ordinal);
            return true;
        }
        if (!lease.complete()) {
            observe_render_queue_depth();
            publish_backend_reply(
                backend, sequence,
                facade_error("render-command-complete"),
                first_failed_ordinal);
            return true;
        }
        observe_render_queue_depth();
        publish_backend_reply(
            backend, sequence, first_error, first_failed_ordinal);
        if (shutdown) queue_->request_shutdown();
        return shutdown;
    }

    void publish_backend_reply(
        NativePortGraphicsBackend& backend,
        const std::uint64_t sequence,
        BackendError error,
        const std::uint32_t failed_ordinal) noexcept {
        auto& slot = reply_slots_[
            static_cast<std::size_t>((sequence - 1u) %
                                     reply_slots_.size())];
        auto ready = slot.ready_sequence.load(std::memory_order_acquire);
        while (ready != 0u) {
            slot.ready_sequence.wait(ready, std::memory_order_acquire);
            ready = slot.ready_sequence.load(std::memory_order_acquire);
        }
        backend.publish_telemetry();
        slot.state_revision = 0u;
        try {
            slot.snapshot = backend.snapshot();
            slot.layout = backend.layout();
            slot.lifecycle = backend.lifecycle_state();
            slot.state_revision = next_consumer_state_revision();
        } catch (const NativePortGraphicsError& snapshot_error) {
            error = capture_backend_error(snapshot_error);
        } catch (...) {
            error = facade_error("render-backend-snapshot");
        }
        slot.error = error;
        slot.failed_ordinal = failed_ordinal;
        published_repeated_presentations_.store(
            consumer_repeated_presentations_, std::memory_order_release);
        published_completed_drawn_frames_.store(
            consumer_completed_drawn_frames_, std::memory_order_release);
        published_presented_frames_.store(
            slot.snapshot.presented_frames, std::memory_order_release);
        slot.ready_sequence.store(sequence, std::memory_order_release);
        slot.ready_sequence.notify_all();
    }

    void consume_reply(const std::uint64_t sequence, const bool wait) {
        auto& slot = reply_slots_[
            static_cast<std::size_t>((sequence - 1u) %
                                     reply_slots_.size())];
        auto ready = slot.ready_sequence.load(std::memory_order_acquire);
        while (wait && ready == 0u) {
            slot.ready_sequence.wait(ready, std::memory_order_acquire);
            ready = slot.ready_sequence.load(std::memory_order_acquire);
        }
        if (ready == 0u) return;
        if (ready != sequence)
            fail_facade("render-command-reply-sequence");

        cached_snapshot_ = slot.snapshot;
        if (slot.state_revision > cached_state_revision_) {
            cached_layout_ = slot.layout;
            cached_lifecycle_ = slot.lifecycle;
            cached_state_revision_ = slot.state_revision;
        }
        cached_backend_reply_sequence_ = sequence;
        const auto error = slot.error;
        slot.error = {};
        slot.state_revision = 0u;
        slot.failed_ordinal = 0u;
        slot.ready_sequence.store(0u, std::memory_order_release);
        slot.ready_sequence.notify_all();
        last_retired_sequence_ = sequence;
        if (error.valid) throw_backend_error(error);
    }

    void retire_available_replies() {
        while (last_retired_sequence_ < last_submitted_sequence_) {
            const auto sequence = last_retired_sequence_ + 1u;
            auto& slot = reply_slots_[
                static_cast<std::size_t>((sequence - 1u) %
                                         reply_slots_.size())];
            if (slot.ready_sequence.load(std::memory_order_acquire) != sequence)
                return;
            consume_reply(sequence, false);
        }
    }

    void wait_for_all_replies() {
        if (last_retired_sequence_ >= last_submitted_sequence_) return;
        const ScopedProducerWait wait(render_producer_wait_ns_, !serial());
        while (last_retired_sequence_ < last_submitted_sequence_)
            consume_reply(last_retired_sequence_ + 1u, true);
    }

    void require_queue_healthy() const {
        if (queue_->snapshot().lifecycle ==
            NativePortFrameQueueLifecycle::Failed)
            throw_queue_failure("render-command-queue");
    }

    void begin_batch() {
        require_producer_thread();
        retire_available_replies();
        require_queue_healthy();
        auto lease = queue_->try_begin_produce();
        if (!lease.has_value()) {
            const ScopedProducerWait wait(
                render_producer_wait_ns_, !serial());
            lease = queue_->wait_begin_produce();
        }
        if (!lease.has_value()) {
            retire_available_replies();
            throw_queue_failure("render-command-begin");
        }
        batch_lease_.emplace(std::move(*lease));
        batch_writer_.emplace(*batch_lease_);
        batch_sequence_ = batch_lease_->sequence();
        batch_command_count_ = 0u;
    }

    template <typename Encoder>
    void append_open(Encoder&& encode,
                     const NativePortGraphicsFailure source_failure,
                     const char* const source_operation) {
        if (!batch_writer_.has_value()) fail_facade("render-command-batch");
        if (!std::forward<Encoder>(encode)(*batch_writer_)) {
            const auto queue_snapshot = queue_->snapshot();
            abort_open_batch();
            producer_frame_open_ = false;
            if (queue_snapshot.lifecycle ==
                NativePortFrameQueueLifecycle::Failed)
                throw_queue_failure("render-command-capacity");
            throw NativePortGraphicsError(
                source_failure, 1u, source_operation);
        }
        ++batch_command_count_;
    }

    void publish_open_batch(const bool wait_current) {
        if (!batch_writer_.has_value() || batch_command_count_ == 0u)
            fail_facade("render-command-empty-batch");
        try {
            // One prior submitted frame may execute while this arena is
            // recorded. Retiring it before publication bounds the pipeline to
            // one consumer frame plus one producer-owned frame.
            wait_for_all_replies();
        } catch (...) {
            abort_open_batch();
            throw;
        }

        const auto sequence = batch_sequence_;
        const auto command_count = batch_command_count_;
        render_completions_[static_cast<std::size_t>(
            (sequence - 1u) % render_completions_.size())] = batch_render_completion_;
        if(sonic::presentation::Settings::interpolation && !motion_annotations_.empty()){
            const std::lock_guard lock(motion_annotations_mutex_);
            motion_published_annotations_.emplace(sequence,std::move(motion_annotations_));
            motion_annotations_.clear();
        }
        if (!batch_writer_->publish()) {
            if constexpr(sonic::presentation::Settings::interpolation) {
                const std::lock_guard lock(motion_annotations_mutex_);motion_published_annotations_.erase(sequence);
            }
            abort_open_batch();
            observe_render_queue_depth();
            throw_queue_failure("render-command-publish");
        }
        observe_render_queue_depth();
        batch_writer_.reset();
        batch_lease_.reset();
        batch_sequence_ = 0u;
        batch_command_count_ = 0u;

        batch_render_completion_ = {};

        saturating_atomic_add(recorded_commands_, command_count);
        last_recorded_sequence_.store(sequence, std::memory_order_release);
        last_submitted_sequence_ = sequence;
        signal_consumer();

        if (serial()) {
            auto lease = queue_->wait_begin_consume();
            if (!lease.has_value())
                throw_queue_failure("serial-render-consume");
            static_cast<void>(consume_lease(*serial_backend_, *lease));
        }
        if (serial()) {
            consume_reply(sequence, true);
        } else if (wait_current) {
            const ScopedProducerWait wait(render_producer_wait_ns_, true);
            consume_reply(sequence, true);
        }
    }

    void abort_open_batch() noexcept {
        batch_render_completion_ = {};
        motion_annotations_.clear();
        if (batch_writer_.has_value()) batch_writer_->abort();
        batch_writer_.reset();
        batch_lease_.reset();
        batch_sequence_ = 0u;
        batch_command_count_ = 0u;
    }

    void observe_render_queue_depth() const noexcept {
        if (config_.telemetry == nullptr || queue_ == nullptr) return;
        const auto snapshot = queue_->snapshot();
        const auto depth =
            snapshot.producer_queue_position >=
                    snapshot.consumer_queue_position
                ? snapshot.producer_queue_position -
                      snapshot.consumer_queue_position
                : 0u;
        config_.telemetry->observe_render_queue_depth(depth);
    }

    template <typename Encoder>
    void submit_standalone_sync(
        Encoder&& encode,
        const NativePortGraphicsFailure source_failure,
        const char* const source_operation) {
        require_producer_thread();
        if (batch_writer_.has_value())
            fail_facade("render-standalone-with-open-batch");
        begin_batch();
        append_open(std::forward<Encoder>(encode),
                    source_failure, source_operation);
        publish_open_batch(true);
    }

    template <typename Encoder>
    void submit_standalone_async(
        Encoder&& encode,
        const NativePortGraphicsFailure source_failure,
        const char* const source_operation) {
        require_producer_thread();
        if (batch_writer_.has_value())
            fail_facade("render-standalone-with-open-batch");
        begin_batch();
        append_open(std::forward<Encoder>(encode),
                    source_failure, source_operation);
        publish_open_batch(false);
    }

    template <typename Encoder>
    void submit_control(
        Encoder&& encode,
        const NativePortGraphicsFailure source_failure,
        const char* const source_operation) {
        require_producer_thread();
        if (producer_frame_open_) {
            append_open(std::forward<Encoder>(encode),
                        source_failure, source_operation);
            return;
        }
        submit_standalone_sync(std::forward<Encoder>(encode),
                               source_failure, source_operation);
    }

    template <typename Encoder>
    void submit_resource_sync(
        Encoder&& encode,
        const NativePortGraphicsFailure source_failure,
        const char* const source_operation) {
        require_producer_thread();
        if (!producer_frame_open_) {
            submit_standalone_sync(std::forward<Encoder>(encode),
                                   source_failure, source_operation);
            return;
        }

        saturating_add_value(resource_fence_count_, 1u);
        const ScopedProducerWait resource_wait(
            render_resource_fence_wait_ns_, true);

        // Writer rejection invalidates an entire lease.  Retire the already
        // recorded Begin/Draw prefix first, execute the synchronous resource
        // operation in its own ordered lease, then continue the same backend
        // frame in the other preallocated arena.  This preserves Sonic's
        // historical lazy-resource error boundary without losing earlier
        // draws or closing the frame.
        try {
            if (batch_command_count_ != 0u) {
                publish_open_batch(true);
                saturating_add_value(frame_prefix_publications_, 1u);
            } else {
                abort_open_batch();
            }
        } catch (...) {
            producer_frame_open_ = false;
            throw;
        }

        try {
            submit_standalone_sync(std::forward<Encoder>(encode),
                                   source_failure, source_operation);
        } catch (...) {
            const auto resource_error = std::current_exception();
            try {
                begin_batch();
            } catch (...) {
                producer_frame_open_ = false;
                throw;
            }
            producer_frame_open_ = true;
            std::rethrow_exception(resource_error);
        }

        try {
            begin_batch();
        } catch (...) {
            producer_frame_open_ = false;
            throw;
        }
        producer_frame_open_ = true;
    }

    [[noreturn]] void throw_queue_failure(
        const char* const operation) const {
        const auto snapshot = queue_->snapshot();
        const bool capacity =
            snapshot.first_error ==
                NativePortFrameQueueError::CommandCapacityExceeded ||
            snapshot.first_error ==
                NativePortFrameQueueError::PayloadCapacityExceeded;
        throw NativePortGraphicsError(
            capacity ? NativePortGraphicsFailure::ResourceLimit
                     : NativePortGraphicsFailure::RenderThreadContract,
            snapshot.first_error == NativePortFrameQueueError::None
                ? 1u
                : static_cast<std::uint32_t>(snapshot.first_error),
            operation);
    }

    [[nodiscard]] NativePortTextureHandle resolve_texture_handle(
        const NativePortTextureHandle logical) const {
        const auto found = texture_handles_.find(logical.value);
        if (!logical || found == texture_handles_.end())
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidResource,
                1u,
                "texture-stale");
        return found->second;
    }

    [[nodiscard]] NativePortMeshHandle resolve_mesh_handle(
        const NativePortMeshHandle logical) const {
        const auto found = mesh_handles_.find(logical.value);
        if (!logical || found == mesh_handles_.end())
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidResource,
                1u,
                "mesh-stale");
        return found->second;
    }

    void create_texture_backend(
        NativePortGraphicsBackend& backend,
        const NativePortTextureHandle logical,
        const NativePortTextureConfig& config,
        const std::span<const NativePortImageView> initial_mip_levels) {
        if (!logical || texture_handles_.contains(logical.value))
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidResource,
                1u,
                "texture-handle");
        const auto actual = backend.create_texture(config, initial_mip_levels);
        try {
            const auto [ignored, inserted] =
                texture_handles_.emplace(logical.value, actual);
            static_cast<void>(ignored);
            if (!inserted)
                throw NativePortGraphicsError(
                    NativePortGraphicsFailure::InvalidResource,
                    1u,
                    "texture-handle");
        } catch (...) {
            backend.destroy_texture(actual);
            throw;
        }
    }

    void destroy_texture_backend(NativePortGraphicsBackend& backend,
                                 const NativePortTextureHandle logical) {
        const auto actual = resolve_texture_handle(logical);
        backend.destroy_texture(actual);
        texture_handles_.erase(logical.value);
    }

    void create_mesh_backend(NativePortGraphicsBackend& backend,
                             const NativePortMeshHandle logical,
                             const NativePortMeshConfig& config) {
        if (!logical || mesh_handles_.contains(logical.value))
            throw NativePortGraphicsError(
                NativePortGraphicsFailure::InvalidResource,
                1u,
                "mesh-handle");
        const auto actual = backend.create_mesh(config);
        try {
            const auto [ignored, inserted] =
                mesh_handles_.emplace(logical.value, actual);
            static_cast<void>(ignored);
            if (!inserted)
                throw NativePortGraphicsError(
                    NativePortGraphicsFailure::InvalidResource,
                    1u,
                    "mesh-handle");
        } catch (...) {
            backend.destroy_mesh(actual);
            throw;
        }
    }

    void destroy_mesh_backend(NativePortGraphicsBackend& backend,
                              const NativePortMeshHandle logical) {
        const auto actual = resolve_mesh_handle(logical);
        backend.destroy_mesh(actual);
        mesh_handles_.erase(logical.value);
    }

    void draw_backend(NativePortGraphicsBackend& backend,
                      const NativePortDrawPacket& source) {
        auto packet = source;
        if (packet.texture)
            packet.texture = resolve_texture_handle(packet.texture);
        if (packet.mesh) packet.mesh = resolve_mesh_handle(packet.mesh);
        backend.draw(packet);
    }

    // Resource mutations cannot overtake deferred draws that still reference
    // their old contents. Flush that prefix through the original path first.
    void drain_motion_prefix(NativePortGraphicsBackend& backend) {
        if constexpr(!sonic::presentation::Settings::interpolation)return;
        if(!motion_buffering_)return;
        backend.begin_frame(motion_history_.building().config);
        sonic::motion::History::render(motion_history_.building(),1.0f,
            [&](const auto& packet){draw_backend(backend,packet);},
            [&]{backend.flush_type2_translucency();});
        motion_buffering_=false;motion_history_.abort();
    }
    void render_motion_sample(NativePortGraphicsBackend& backend,std::uint64_t now,bool first=false) {
        if constexpr(!sonic::presentation::Settings::interpolation)return;
        if(const auto test_time=sonic::motion::test_time_ns.load();test_time&&background_test_mode_requested())now=test_time;
        if((!first&&!motion_history_.can_render())||(!first&&!motion_history_.animated())||
           (!first&&!backend.completed_frame_ready()))return;
        const float alpha=motion_history_.alpha(now);
        if(!first&&alpha<=motion_last_alpha_)return;
        backend.begin_frame(motion_history_.current().config);
        sonic::motion::History::render(motion_history_.current(),alpha,
            [&](const auto& packet){draw_backend(backend,packet);},
            [&]{backend.flush_type2_translucency();});
        backend.complete_frame();motion_last_alpha_=alpha;motion_output_changed_=true;
        if(sonic::motion::tracing()&&(first||motion_trace_samples_<32)){
            std::fprintf(stderr,"SONIC_MOTION frame=%llu source_ns=%llu matched=%zu draws=%zu world=%zu rejected_world=%zu alpha=%.4f native=%u camera=%u\n",
                static_cast<unsigned long long>(motion_history_.current().tag.sequence),
                static_cast<unsigned long long>(motion_history_.current().tag.time_ns),
                motion_history_.matched(),motion_history_.current().draws.size(),motion_history_.world_draws(),
                motion_history_.rejected_world_draws(),alpha,unsigned(first),unsigned(motion_history_.current().camera_motion));
            if(!first)++motion_trace_samples_;
        }
    }

    [[nodiscard]] bool execute_command(
        NativePortGraphicsBackend& backend,
        const NativePortGraphicsCommandView& command,
        const sonic::motion::Annotation* annotation=nullptr) {
        if(command.kind==NativePortGraphicsCommandKind::UpdateTexture||
           command.kind==NativePortGraphicsCommandKind::DestroyTexture||
           command.kind==NativePortGraphicsCommandKind::DestroyMesh){
            drain_motion_prefix(backend);motion_history_.invalidate();
        }
        switch (command.kind) {
        case NativePortGraphicsCommandKind::Show:
            backend.show();
            return false;
        case NativePortGraphicsCommandKind::PollEvents:
            backend.poll_events();
            return false;
        case NativePortGraphicsCommandKind::CreateTexture: {
            const auto& view =
                std::get<NativePortGraphicsCreateTextureView>(command.payload);
            std::array<NativePortImageView,
                       native_port_graphics_command_stream_max_mip_levels>
                images{};
            for (std::size_t index = 0u;
                 index < view.initial_mip_levels.size(); ++index)
                images[index] = view.initial_mip_levels[index];
            create_texture_backend(
                backend, view.texture, view.config,
                std::span<const NativePortImageView>(
                    images.data(), view.initial_mip_levels.size()));
            return false;
        }
        case NativePortGraphicsCommandKind::UpdateTexture: {
            const auto& view =
                std::get<NativePortGraphicsUpdateTextureView>(command.payload);
            std::array<NativePortImageView,
                       native_port_graphics_command_stream_max_mip_levels>
                images{};
            for (std::size_t index = 0u; index < view.mip_levels.size(); ++index)
                images[index] = view.mip_levels[index];
            backend.update_texture(
                resolve_texture_handle(view.texture),
                std::span<const NativePortImageView>(
                    images.data(), view.mip_levels.size()));
            return false;
        }
        case NativePortGraphicsCommandKind::DestroyTexture:
            destroy_texture_backend(
                backend,
                std::get<NativePortGraphicsDestroyTextureView>(command.payload)
                    .texture);
            return false;
        case NativePortGraphicsCommandKind::CreateMesh: {
            const auto& view =
                std::get<NativePortGraphicsCreateMeshView>(command.payload);
            NativePortMeshConfig config;
            config.vertices = view.vertices;
            config.indices = view.indices;
            config.topology = view.topology;
            config.shading = view.shading;
            config.small_triangle_area_threshold =
                view.small_triangle_area_threshold;
            create_mesh_backend(backend, view.mesh, config);
            return false;
        }
        case NativePortGraphicsCommandKind::DestroyMesh:
            destroy_mesh_backend(
                backend,
                std::get<NativePortGraphicsDestroyMeshView>(command.payload)
                    .mesh);
            return false;
        case NativePortGraphicsCommandKind::BeginFrame: {
            consumer_drawn_frame_open_ = false;
            const auto& frame=std::get<NativePortGraphicsBeginFrameView>(command.payload).config;
            if constexpr(sonic::presentation::Settings::interpolation) {
                motion_buffering_=annotation&&annotation->frame.enabled&&motion_history_.accepts(annotation->frame);
                if(motion_buffering_)motion_history_.begin(frame,annotation->frame);
                else{motion_history_.abort();backend.begin_frame(frame);}
            } else backend.begin_frame(frame);
            consumer_frame_start_draw_calls_ = backend.snapshot().draw_calls;
            consumer_drawn_frame_open_ = true;
            return false;
        }
        case NativePortGraphicsCommandKind::Draw:
            if(sonic::presentation::Settings::interpolation && motion_buffering_){
                if(motion_history_.add(std::get<NativePortGraphicsDrawView>(command.payload).packet,
                                      annotation?annotation->draw:sonic::motion::DrawTag{}))return false;
                drain_motion_prefix(backend);
            }
            draw_backend(
                backend,
                std::get<NativePortGraphicsDrawView>(command.payload).packet);
            return false;
        case NativePortGraphicsCommandKind::FlushType2:
            if(sonic::presentation::Settings::interpolation && motion_buffering_){if(motion_history_.flush())return false;drain_motion_prefix(backend);}
            backend.flush_type2_translucency();
            return false;
        case NativePortGraphicsCommandKind::Present: {
            const bool frame_open = consumer_drawn_frame_open_;
            consumer_drawn_frame_open_ = false;
            const bool motion_frame=sonic::presentation::Settings::interpolation && motion_buffering_;
            if(motion_frame){
                const auto test_time=sonic::motion::test_time_ns.load();
                motion_history_.commit(test_time&&background_test_mode_requested()?test_time:presentation_now());motion_buffering_=false;
                render_motion_sample(backend,presentation_now(),true);
            }
            if (independent_presentation_enabled()) {
                if(!motion_frame)backend.complete_frame();
                consumer_completed_image_presented_ = false;
                present_on_consumer_deadline(backend, true, "present");
                consumer_presentation_faulted_ = false;
            } else if(motion_frame)backend.repeat_present("present");
            else backend.present();
            if (frame_open &&
                backend.snapshot().draw_calls > consumer_frame_start_draw_calls_)
                saturating_add_value(consumer_completed_drawn_frames_, 1u);
            return false;
        }
        case NativePortGraphicsCommandKind::RepeatPresent: {
            if (independent_presentation_enabled()) {
                // Explicit commands retain the backend's strict InvalidFrame
                // contract. Only the autonomous idle timer may skip a prefix.
                if (!backend.completed_frame_ready()) backend.repeat_present();
                present_on_consumer_deadline(backend, true);
                return false;
            }
            const auto before = backend.snapshot().presented_frames;
            render_motion_sample(backend,presentation_now());
            backend.repeat_present();
            const auto after = backend.snapshot().presented_frames;
            if (after > before && !motion_output_changed_)
                saturating_add_value(
                    consumer_repeated_presentations_, after - before);
            motion_output_changed_=false;
            return false;
        }
        case NativePortGraphicsCommandKind::PresentImage: {
            drain_motion_prefix(backend);motion_history_.abort();
            const auto& view =
                std::get<NativePortGraphicsPresentImageView>(command.payload);
            consumer_drawn_frame_open_ = false;
            const auto before = backend.snapshot().draw_calls;
            backend.present_image(view.image, view.viewport, view.fit,
                                  independent_presentation_enabled());
            if (independent_presentation_enabled()) {
                consumer_completed_image_presented_ = false;
                present_on_consumer_deadline(backend, true, "present");
                consumer_presentation_faulted_ = false;
            }
            if (backend.snapshot().draw_calls > before)
                saturating_add_value(consumer_completed_drawn_frames_, 1u);
            return false;
        }
        case NativePortGraphicsCommandKind::Shutdown:
            return true;
        }
        fail_facade("render-command-kind");
    }

    void decorate_snapshot(NativePortGraphicsSnapshot& result) const noexcept {
        // Autonomous output is not accompanied by a command reply. Its
        // monotonic completion count must not be hidden by an older reply.
        result.presented_frames = published_presented_frames_.load(
            std::memory_order_acquire);
        result.requested_execution_mode = requested_mode_;
        result.active_execution_mode = active_mode_;
        result.recorded_commands =
            recorded_commands_.load(std::memory_order_acquire);
        result.consumed_commands =
            consumed_commands_.load(std::memory_order_acquire);
        result.executed_commands =
            executed_commands_.load(std::memory_order_acquire);
        result.failed_commands =
            failed_commands_.load(std::memory_order_acquire);
        result.skipped_commands =
            skipped_commands_.load(std::memory_order_acquire);
        result.last_recorded_queue_sequence =
            last_recorded_sequence_.load(std::memory_order_acquire);
        result.last_consumed_queue_sequence =
            last_consumed_sequence_.load(std::memory_order_acquire);
        result.last_executed_queue_sequence =
            last_executed_sequence_.load(std::memory_order_acquire);
        result.last_failed_queue_sequence =
            last_failed_sequence_.load(std::memory_order_acquire);
        result.last_failed_command_ordinal =
            last_failed_ordinal_.load(std::memory_order_acquire);
        result.backend_reply_sequence = cached_backend_reply_sequence_;
        const auto queue_snapshot = queue_->snapshot();
        result.producer_thread_identity =
            queue_snapshot.producer_thread_identity;
        result.consumer_thread_identity =
            queue_snapshot.consumer_thread_identity;
        result.frame_queue_producer_position =
            queue_snapshot.producer_queue_position;
        result.frame_queue_consumer_position =
            queue_snapshot.consumer_queue_position;
        result.render_producer_wait_ns = render_producer_wait_ns_;
        result.render_resource_fence_wait_ns =
            render_resource_fence_wait_ns_;
        result.resource_fence_count = resource_fence_count_;
        result.frame_prefix_publications = frame_prefix_publications_;
    }

    std::string title_storage_;
    std::string development_state_directory_storage_;
    NativePortGraphicsConfig config_;
    std::thread::id producer_thread_;
    std::vector<sonic::motion::Annotation> motion_annotations_;
    std::mutex motion_annotations_mutex_;
    std::unordered_map<std::uint64_t,std::vector<sonic::motion::Annotation>> motion_published_annotations_;
    sonic::motion::History motion_history_;
    bool motion_buffering_=false,motion_output_changed_=false;
    float motion_last_alpha_=1;
    unsigned motion_trace_samples_=0;
    NativePortGraphicsExecutionMode requested_mode_ =
        NativePortGraphicsExecutionMode::Parallel;
    NativePortGraphicsExecutionMode active_mode_ =
        NativePortGraphicsExecutionMode::Parallel;
    bool development_probe_initialized_ = false;
    std::string development_initial_state_path_;
    std::uint64_t development_probe_count_ = 0u;
    std::uint64_t development_probe_next_ = 0u;
    mutable NativePortRuntimeOptionsBridge runtime_options_;
    std::unique_ptr<NativePortGraphicsBackend> serial_backend_;
    std::unique_ptr<NativePortFrameQueue> queue_;
    std::thread consumer_thread_;
#ifdef _WIN32
    HANDLE consumer_wake_event_ = nullptr;
#else
    std::mutex consumer_wake_mutex_;
    std::condition_variable consumer_wake_condition_;
    bool consumer_wake_pending_ = false;
#endif
    ConsumerStateMailbox consumer_state_mailbox_;
    std::uint64_t consumer_state_revision_ = 0u;
    std::uint64_t acquired_state_mailbox_publication_ = 0u;
    std::atomic<StartupState> startup_state_{StartupState::Pending};
    BackendError startup_error_;
    NativePortGraphicsSnapshot startup_snapshot_;
    NativePortGraphicsLayout startup_layout_;
    NativePortLifecycleState startup_lifecycle_ =
        NativePortLifecycleState::Running;
    std::uint64_t startup_state_revision_ = 0u;
    std::array<ReplySlot, native_port_frame_queue_depth> reply_slots_;
    std::array<RenderCompletion, native_port_frame_queue_depth> render_completions_;
    RenderCompletion batch_render_completion_;
    std::optional<NativePortFrameWriteLease> batch_lease_;
    std::optional<NativePortGraphicsCommandWriter> batch_writer_;
    std::uint64_t batch_sequence_ = 0u;
    std::uint64_t batch_command_count_ = 0u;
    bool producer_frame_open_ = false;
    std::uint64_t last_submitted_sequence_ = 0u;
    std::uint64_t last_retired_sequence_ = 0u;
    std::uint64_t cached_backend_reply_sequence_ = 0u;
    std::uint64_t cached_state_revision_ = 0u;
    NativePortGraphicsSnapshot cached_snapshot_;
    NativePortGraphicsLayout cached_layout_;
    NativePortLifecycleState cached_lifecycle_ =
        NativePortLifecycleState::Running;
    std::atomic<std::uint64_t> published_presented_frames_{0u};
    std::atomic<std::uint64_t> published_repeated_presentations_{0u};
    std::atomic<std::uint64_t> published_completed_drawn_frames_{0u};
    std::atomic<std::uint64_t> published_missed_presentations_{0u};
    std::atomic<bool> presentation_shutdown_{false};
    std::atomic<bool> presentation_paused_{false};
    std::uint64_t consumer_presentation_deadline_ = 0u;
    std::uint64_t consumer_presentation_remainder_ = 0u;
    std::uint32_t consumer_presentation_rate_ = 0u;
    std::uint32_t consumer_presentation_manual_rate_ = 0u;
    bool consumer_completed_image_presented_ = false;
    bool consumer_presentation_faulted_ = false;
    std::uint64_t consumer_completed_drawn_frames_ = 0u;
    std::uint64_t consumer_frame_start_draw_calls_ = 0u;
    bool consumer_drawn_frame_open_ = false;
    std::uint64_t consumer_repeated_presentations_ = 0u;
    std::atomic<std::uint64_t> recorded_commands_{0u};
    std::atomic<std::uint64_t> consumed_commands_{0u};
    std::atomic<std::uint64_t> executed_commands_{0u};
    std::atomic<std::uint64_t> failed_commands_{0u};
    std::atomic<std::uint64_t> skipped_commands_{0u};
    std::atomic<std::uint64_t> last_recorded_sequence_{0u};
    std::atomic<std::uint64_t> last_consumed_sequence_{0u};
    std::atomic<std::uint64_t> last_executed_sequence_{0u};
    std::atomic<std::uint64_t> last_failed_sequence_{0u};
    std::atomic<std::uint32_t> last_failed_ordinal_{0u};
    std::uint64_t render_producer_wait_ns_ = 0u;
    std::uint64_t render_resource_fence_wait_ns_ = 0u;
    std::uint64_t resource_fence_count_ = 0u;
    std::uint64_t frame_prefix_publications_ = 0u;
    std::uint64_t next_texture_handle_ = 1u;
    std::uint64_t next_mesh_handle_ = 1u;
    std::unordered_map<std::uint64_t, NativePortTextureHandle>
        texture_handles_;
    std::unordered_map<std::uint64_t, NativePortMeshHandle> mesh_handles_;
};
NativePortGraphicsDevice::NativePortGraphicsDevice(
    const NativePortGraphicsConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

NativePortGraphicsDevice::~NativePortGraphicsDevice() = default;

void NativePortGraphicsDevice::show() {
    impl_->show();
}

void NativePortGraphicsDevice::poll_events() {
    impl_->poll_events();
}

void NativePortGraphicsDevice::configure_runtime_options(
    const std::uint32_t simulation_rate_hz,
    const std::uint32_t presentation_rate_hz,
    const std::uint32_t maximum_presentation_rate_hz,
    const bool frame_pacing_enabled) {
    impl_->configure_runtime_options(simulation_rate_hz,
                                     presentation_rate_hz,
                                     maximum_presentation_rate_hz,
                                     frame_pacing_enabled);
}

void NativePortGraphicsDevice::record_simulation_frame_nonblocking()
    noexcept {
    impl_->record_simulation_frame_nonblocking();
}

std::uint32_t
NativePortGraphicsDevice::requested_presentation_rate_nonblocking()
    const noexcept {
    return impl_->requested_presentation_rate_nonblocking();
}

void NativePortGraphicsDevice::acknowledge_presentation_rate_nonblocking(
    const std::uint32_t presentation_rate_hz) noexcept {
    impl_->acknowledge_presentation_rate_nonblocking(presentation_rate_hz);
}

NativePortLifecycleState
NativePortGraphicsDevice::lifecycle_state() const {
    return impl_->lifecycle_state();
}

NativePortGraphicsLayout NativePortGraphicsDevice::layout() const {
    return impl_->layout();
}

NativePortTextureHandle NativePortGraphicsDevice::create_texture(
    const NativePortTextureConfig& config,
    const NativePortImageView* const initial_pixels) {
    return initial_pixels != nullptr
               ? impl_->create_texture(
                     config,
                     std::span<const NativePortImageView>(initial_pixels, 1u))
               : impl_->create_texture(
                     config, std::span<const NativePortImageView>{});
}

NativePortTextureHandle NativePortGraphicsDevice::create_texture(
    const NativePortTextureConfig& config,
    const std::span<const NativePortImageView> initial_mip_levels) {
    return impl_->create_texture(config, initial_mip_levels);
}

void NativePortGraphicsDevice::update_texture(
    const NativePortTextureHandle texture,
    const NativePortImageView& pixels) {
    impl_->update_texture(texture, pixels);
}

void NativePortGraphicsDevice::update_texture(
    const NativePortTextureHandle texture,
    const std::span<const NativePortImageView> mip_levels) {
    impl_->update_texture(texture, mip_levels);
}

void NativePortGraphicsDevice::destroy_texture(
    const NativePortTextureHandle texture) {
    impl_->destroy_texture(texture);
}

NativePortMeshHandle NativePortGraphicsDevice::create_mesh(
    const NativePortMeshConfig& config) {
    return impl_->create_mesh(config);
}

void NativePortGraphicsDevice::destroy_mesh(
    const NativePortMeshHandle mesh) {
    impl_->destroy_mesh(mesh);
}

void NativePortGraphicsDevice::begin_frame(
    const NativePortFrameConfig& config) {
    impl_->begin_frame(config);
}

void NativePortGraphicsDevice::draw(const NativePortDrawPacket& packet) {
    impl_->draw(packet);
}

void NativePortGraphicsDevice::flush_type2_translucency() {
    impl_->flush_type2_translucency();
}

void NativePortGraphicsDevice::present() {
    impl_->present();
}

void NativePortGraphicsDevice::repeat_present() {
    impl_->repeat_present();
}

void NativePortGraphicsDevice::repeat_present_async() {
    impl_->repeat_present_async();
}

bool NativePortGraphicsDevice::independent_presentation_enabled() const noexcept {
    return impl_->independent_presentation_enabled();
}

std::uint64_t NativePortGraphicsDevice::missed_presentation_deadlines() const noexcept {
    return impl_->missed_presentation_deadlines();
}

void NativePortGraphicsDevice::present_image(
    const NativePortImageView& image,
    const NativePortViewportTarget viewport,
    const NativePortImageFit fit) {
    impl_->present_image(image, viewport, fit);
}

void NativePortGraphicsDevice::finish() {
    impl_->finish();
}

NativePortGraphicsSnapshot NativePortGraphicsDevice::snapshot() const {
    return impl_->snapshot();
}

std::uint64_t
NativePortGraphicsDevice::presented_frames_nonblocking() const noexcept {
    return impl_->presented_frames_nonblocking();
}

std::uint64_t
NativePortGraphicsDevice::completed_drawn_frames_nonblocking() const noexcept {
    return impl_->completed_drawn_frames_nonblocking();
}

std::uint64_t
NativePortGraphicsDevice::repeated_presentations_nonblocking() const noexcept {
    return impl_->repeated_presentations_nonblocking();
}

bool NativePortGraphicsDevice::frame_recording_open_nonblocking()
    const noexcept {
    return impl_->frame_recording_open_nonblocking();
}

std::optional<NativePortDevelopmentStateRequest>
NativePortGraphicsDevice::take_development_state_request() {
    return impl_->take_development_state_request();
}

NativePortDesktopHost::NativePortDesktopHost(
    const NativePortGraphicsConfig& graphics_config,
    const NativePortFramePacingConfig& frame_pacing_config)
    : graphics_(desktop_graphics_config(graphics_config,
                                        frame_pacing_config)),
      frame_pacing_config_(frame_pacing_config) {
    acquire_frame_pacing_timer_resolution(frame_pacing_config_);
    frame_pacing_snapshot_.simulation_rate_hz =
        frame_pacing_config_.simulation_rate_hz;
    frame_pacing_snapshot_.presentation_rate_hz =
        frame_pacing_config_.presentation_rate_hz;
    frame_pacing_snapshot_.enabled = frame_pacing_config_.enabled;
    graphics_.configure_runtime_options(
        frame_pacing_config_.simulation_rate_hz,
        frame_pacing_config_.presentation_rate_hz,
        frame_pacing_config_.maximum_presentation_rate_hz,
        frame_pacing_config_.enabled);
}

NativePortDesktopHost::~NativePortDesktopHost() {
    release_frame_pacing_timer_resolution(frame_pacing_config_);
}

std::uint64_t NativePortDesktopHost::monotonic_time_nanoseconds()
    const noexcept {
    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    const auto count = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           elapsed)
                           .count();
    return count <= 0 ? 0u : static_cast<std::uint64_t>(count);
}

NativePortLifecycleState NativePortDesktopHost::poll_lifecycle() {
    // AOT safepoints can ask for lifecycle state far more often than a host
    // event loop needs to enter User32. Bound message-pump latency to 1 ms
    // without turning every statically chained SH-4 block into a host syscall.
    constexpr std::uint64_t event_poll_interval_nanoseconds = 1'000'000u;
    const auto now = monotonic_time_nanoseconds();
    if (now >= next_event_poll_nanoseconds_) {
        graphics_.poll_events();
        apply_runtime_presentation_rate();
        next_event_poll_nanoseconds_ =
            now > std::numeric_limits<std::uint64_t>::max() -
                      event_poll_interval_nanoseconds
                ? std::numeric_limits<std::uint64_t>::max()
                : now + event_poll_interval_nanoseconds;
    }
    return graphics_.lifecycle_state();
}

std::optional<NativePortDevelopmentStateRequest>
NativePortDesktopHost::take_development_state_request() {
    return graphics_.take_development_state_request();
}

void NativePortDesktopHost::synchronize_simulation_boundary() {
    apply_runtime_presentation_rate();
    if (!frame_pacing_config_.enabled || !frame_pacing_started_)
        return;
    if (monotonic_time_nanoseconds() <
        next_simulation_deadline_nanoseconds_)
        wait_until_monotonic_nanoseconds(
            next_simulation_deadline_nanoseconds_);
}

void NativePortDesktopHost::begin_frame(const std::uint64_t frame_index) {
    static_cast<void>(frame_index);
    graphics_.begin_frame();
}

void NativePortDesktopHost::present_frame(const std::uint64_t frame_index) {
    static_cast<void>(frame_index);
    graphics_.record_simulation_frame_nonblocking();
    paced_present();
}

bool NativePortDesktopHost::title_cadence_available() const noexcept {
    return frame_pacing_config_.enabled &&
           graphics_.independent_presentation_enabled();
}

void NativePortDesktopHost::wait_until_title_deadline(
    const std::uint64_t deadline_nanoseconds) {
    if (monotonic_time_nanoseconds() < deadline_nanoseconds)
        wait_until_monotonic_nanoseconds(deadline_nanoseconds);
}

void NativePortDesktopHost::present_frame_after_title_cadence(
    const std::uint64_t frame_index) {
    static_cast<void>(frame_index);
    apply_runtime_presentation_rate();
    graphics_.record_simulation_frame_nonblocking();
    if (graphics_.frame_recording_open_nonblocking()) graphics_.present();
    // The independent render owner repeats an unchanged image by itself.
    saturating_increment(frame_pacing_snapshot_.simulation_frames);
    reconcile_presentations();
    // A later generic owner must start a fresh simulation epoch. The
    // independent presentation thread retains its own clock and phase.
    frame_pacing_started_ = false;
    next_simulation_deadline_nanoseconds_ = 0u;
    simulation_deadline_remainder_ = 0u;
}

std::uint64_t NativePortDesktopHost::presented_frames() const noexcept {
    return graphics_.presented_frames_nonblocking();
}

NativePortFramePacingSnapshot
NativePortDesktopHost::frame_pacing_snapshot() const noexcept {
    reconcile_presentations();
    return frame_pacing_snapshot_;
}

void NativePortDesktopHost::reconcile_presentations() const noexcept {
    if (graphics_.independent_presentation_enabled())
        frame_pacing_snapshot_.missed_presentation_deadlines =
            graphics_.missed_presentation_deadlines();
    const auto completed = graphics_.presented_frames_nonblocking();
    if (completed <= accounted_presented_frames_) return;
    const auto delta = completed - accounted_presented_frames_;
    const auto repeated = std::min(
        graphics_.repeated_presentations_nonblocking(), completed);
    const auto repeated_delta = std::min(
        delta,
        repeated > accounted_repeated_presentations_
            ? repeated - accounted_repeated_presentations_
            : 0u);
    saturating_add(frame_pacing_snapshot_.presentation_frames, delta);
    saturating_add(frame_pacing_snapshot_.repeated_presentations,
                   repeated_delta);
    accounted_presented_frames_ = completed;
    accounted_repeated_presentations_ = repeated;
}

void NativePortDesktopHost::apply_runtime_presentation_rate() {
    if (!frame_pacing_config_.enabled) return;
    const auto requested =
        graphics_.requested_presentation_rate_nonblocking();
    if (requested == frame_pacing_config_.presentation_rate_hz) return;
    if (!runtime_presentation_rate_choice(requested) ||
        requested < frame_pacing_config_.simulation_rate_hz ||
        requested > frame_pacing_config_.maximum_presentation_rate_hz) {
        graphics_.acknowledge_presentation_rate_nonblocking(
            frame_pacing_config_.presentation_rate_hz);
        return;
    }
    frame_pacing_config_.presentation_rate_hz = requested;
    frame_pacing_snapshot_.presentation_rate_hz = requested;
    graphics_.acknowledge_presentation_rate_nonblocking(requested);
    if (!frame_pacing_started_) return;
    const auto now = monotonic_time_nanoseconds();
    next_presentation_deadline_nanoseconds_ = now;
    presentation_deadline_remainder_ = 0u;
    advance_frame_deadline(next_presentation_deadline_nanoseconds_,
                           presentation_deadline_remainder_,
                           requested);
}

void NativePortDesktopHost::paced_present() {
    apply_runtime_presentation_rate();
    if (graphics_.independent_presentation_enabled()) {
        // Only the original simulation clock belongs to the title thread.
        // The render owner presents completed images and repeats on its own
        // output deadlines, including while this producer computes a frame.
        auto now = monotonic_time_nanoseconds();
        if (frame_pacing_started_ && now < next_simulation_deadline_nanoseconds_)
            wait_until_monotonic_nanoseconds(next_simulation_deadline_nanoseconds_);
        if (graphics_.frame_recording_open_nonblocking()) graphics_.present();
        saturating_increment(frame_pacing_snapshot_.simulation_frames);
        now = monotonic_time_nanoseconds();
        if (!frame_pacing_started_) {
            next_simulation_deadline_nanoseconds_ = now;
            simulation_deadline_remainder_ = 0u;
            frame_pacing_started_ = true;
        } else if (now > next_simulation_deadline_nanoseconds_) {
            saturating_increment(frame_pacing_snapshot_.late_simulation_frames);
        }
        advance_frame_deadline(next_simulation_deadline_nanoseconds_,
            simulation_deadline_remainder_, frame_pacing_config_.simulation_rate_hz);
        if (next_simulation_deadline_nanoseconds_ <= now) {
            // Missed title updates are never replayed to catch up.
            next_simulation_deadline_nanoseconds_ = now;
            simulation_deadline_remainder_ = 0u;
            advance_frame_deadline(next_simulation_deadline_nanoseconds_,
                simulation_deadline_remainder_, frame_pacing_config_.simulation_rate_hz);
        }
        reconcile_presentations();
        return;
    }
    const auto present_and_record = [this](const bool repeated) {
        const bool repeat_completed_frame =
            repeated || !graphics_.frame_recording_open_nonblocking();
        if (repeat_completed_frame) {
            graphics_.repeat_present_async();
            reconcile_presentations();
        } else {
            graphics_.present();
            // present() retires the preceding frame before publishing the
            // current one. Account whichever FIFO completions are visible;
            // the current asynchronous presentation remains pending.
            reconcile_presentations();
        }
    };

    if (!frame_pacing_config_.enabled) {
        present_and_record(false);
        saturating_increment(frame_pacing_snapshot_.simulation_frames);
        return;
    }

    auto now = monotonic_time_nanoseconds();
    if (!frame_pacing_started_) {
        // The first completed frame establishes both native epochs and is
        // visible immediately. Subsequent title updates run while the first
        // simulation interval elapses and are gated at this boundary.
        present_and_record(false);
        saturating_increment(frame_pacing_snapshot_.simulation_frames);
        now = monotonic_time_nanoseconds();
        next_simulation_deadline_nanoseconds_ = now;
        next_presentation_deadline_nanoseconds_ = now;
        advance_frame_deadline(
            next_simulation_deadline_nanoseconds_,
            simulation_deadline_remainder_,
            frame_pacing_config_.simulation_rate_hz);
        advance_frame_deadline(
            next_presentation_deadline_nanoseconds_,
            presentation_deadline_remainder_,
            frame_pacing_config_.presentation_rate_hz);
        frame_pacing_started_ = true;
        return;
    }

    const bool simulation_late =
        now > next_simulation_deadline_nanoseconds_;
    if (now >= next_simulation_deadline_nanoseconds_) {
        // A normal host wait resumes a little after its deadline. Preserve the
        // phase for that sub-frame lateness instead of accumulating scheduler
        // overshoot into title time. Only a complete additional missed
        // simulation interval reanchors the clocks; catch-up updates are never
        // issued.
        present_and_record(false);
        saturating_increment(frame_pacing_snapshot_.simulation_frames);
        now = monotonic_time_nanoseconds();
        advance_frame_deadline(
            next_simulation_deadline_nanoseconds_,
            simulation_deadline_remainder_,
            frame_pacing_config_.simulation_rate_hz);
        if (next_presentation_deadline_nanoseconds_ <= now)
            advance_frame_deadline(
                next_presentation_deadline_nanoseconds_,
                presentation_deadline_remainder_,
                frame_pacing_config_.presentation_rate_hz);
        while (next_presentation_deadline_nanoseconds_ <= now) {
            saturating_increment(
                frame_pacing_snapshot_.missed_presentation_deadlines);
            advance_frame_deadline(
                next_presentation_deadline_nanoseconds_,
                presentation_deadline_remainder_,
                frame_pacing_config_.presentation_rate_hz);
        }
        if (simulation_late)
            saturating_increment(
                frame_pacing_snapshot_.late_simulation_frames);
        if (next_simulation_deadline_nanoseconds_ <= now) {
            next_simulation_deadline_nanoseconds_ = now;
            next_presentation_deadline_nanoseconds_ = now;
            simulation_deadline_remainder_ = 0u;
            presentation_deadline_remainder_ = 0u;
            advance_frame_deadline(
                next_simulation_deadline_nanoseconds_,
                simulation_deadline_remainder_,
                frame_pacing_config_.simulation_rate_hz);
            advance_frame_deadline(
                next_presentation_deadline_nanoseconds_,
                presentation_deadline_remainder_,
                frame_pacing_config_.presentation_rate_hz);
        }
        return;
    }

    // A new title frame is ready before its next simulation boundary. Present
    // it on the next output deadline, then repeat only that completed GPU
    // frame on additional output deadlines. Title state does not advance
    // again until the simulation boundary below is reached.
    if (now > next_presentation_deadline_nanoseconds_) {
        saturating_increment(
            frame_pacing_snapshot_.missed_presentation_deadlines);
        present_and_record(false);
        now = monotonic_time_nanoseconds();
        advance_frame_deadline(
            next_presentation_deadline_nanoseconds_,
            presentation_deadline_remainder_,
            frame_pacing_config_.presentation_rate_hz);
        while (next_presentation_deadline_nanoseconds_ <= now) {
            saturating_increment(
                frame_pacing_snapshot_.missed_presentation_deadlines);
            advance_frame_deadline(
                next_presentation_deadline_nanoseconds_,
                presentation_deadline_remainder_,
                frame_pacing_config_.presentation_rate_hz);
        }
    } else {
        wait_until_monotonic_nanoseconds(
            next_presentation_deadline_nanoseconds_);
        present_and_record(false);
        advance_frame_deadline(
            next_presentation_deadline_nanoseconds_,
            presentation_deadline_remainder_,
            frame_pacing_config_.presentation_rate_hz);
    }
    saturating_increment(frame_pacing_snapshot_.simulation_frames);

    for (;;) {
        now = monotonic_time_nanoseconds();
        while (next_presentation_deadline_nanoseconds_ <= now &&
               next_presentation_deadline_nanoseconds_ <=
                   next_simulation_deadline_nanoseconds_) {
            saturating_increment(
                frame_pacing_snapshot_.missed_presentation_deadlines);
            advance_frame_deadline(
                next_presentation_deadline_nanoseconds_,
                presentation_deadline_remainder_,
                frame_pacing_config_.presentation_rate_hz);
        }
        if (next_presentation_deadline_nanoseconds_ >
            next_simulation_deadline_nanoseconds_)
            break;
        wait_until_monotonic_nanoseconds(
            next_presentation_deadline_nanoseconds_);
        present_and_record(true);
        advance_frame_deadline(
            next_presentation_deadline_nanoseconds_,
            presentation_deadline_remainder_,
            frame_pacing_config_.presentation_rate_hz);
    }

    if (monotonic_time_nanoseconds() <
        next_simulation_deadline_nanoseconds_) {
        wait_until_monotonic_nanoseconds(
            next_simulation_deadline_nanoseconds_);
    }
    advance_frame_deadline(
        next_simulation_deadline_nanoseconds_,
        simulation_deadline_remainder_,
        frame_pacing_config_.simulation_rate_hz);
    now = monotonic_time_nanoseconds();
    if (next_simulation_deadline_nanoseconds_ <= now) {
        // Presentation itself consumed a complete additional simulation
        // interval. Resynchronise once; never run multiple updates to catch up.
        saturating_increment(
            frame_pacing_snapshot_.late_simulation_frames);
        next_simulation_deadline_nanoseconds_ = now;
        simulation_deadline_remainder_ = 0u;
        advance_frame_deadline(
            next_simulation_deadline_nanoseconds_,
            simulation_deadline_remainder_,
            frame_pacing_config_.simulation_rate_hz);
    }
}

NativePortGraphicsDevice& NativePortDesktopHost::graphics() noexcept {
    return graphics_;
}

const NativePortGraphicsDevice&
NativePortDesktopHost::graphics() const noexcept {
    return graphics_;
}

NativePortGraphicsError::NativePortGraphicsError(
    const NativePortGraphicsFailure failure,
    const std::uint32_t platform_error_code,
    const std::string_view operation)
    : std::runtime_error(
          "native-port-graphics-" + std::string(operation) + ":" +
          std::to_string(platform_error_code)),
      failure_(failure), platform_error_code_(platform_error_code),
      operation_id_(native_port_graphics_operation_id(operation)) {}

NativePortGraphicsError::NativePortGraphicsError(
    const NativePortGraphicsFailure failure,
    const std::uint32_t platform_error_code,
    const std::uint64_t operation_id)
    : std::runtime_error(
          "native-port-graphics-operation-" + std::to_string(operation_id) +
          ":" + std::to_string(platform_error_code)),
      failure_(failure), platform_error_code_(platform_error_code),
      operation_id_(operation_id) {}

NativePortGraphicsFailure NativePortGraphicsError::failure() const noexcept {
    return failure_;
}

std::uint32_t NativePortGraphicsError::platform_error_code() const noexcept {
    return platform_error_code_;
}

std::uint64_t NativePortGraphicsError::operation_id() const noexcept {
    return operation_id_;
}

#ifdef _WIN32

namespace {

const wchar_t native_graphics_window_class[] =
    L"KatanaRecompNativeGraphicsV1";

constexpr char native_graphics_shader_source[] = R"(
cbuffer DrawConstants : register(b0) {
    row_major float4x4 draw_transform;
    row_major float4x4 draw_normal_transform;
    float4 material_diffuse;
    float4 material_ambient;
    float4 material_specular;
    float4 material_emission;
    float4 scene_ambient;
    float4 fog_color;
    float4 color_clamp_minimum;
    float4 color_clamp_maximum;
    float4 light_directions[4];
    float4 light_colors[4];
    float4 fog_parameters;
    float4 depth_parameters;
    float4 material_parameters;
    uint4 pipeline_flags;
    uint4 type_two_parameters;
    float4 logical_clip_transform;
    float4 logical_clip_bounds;
    uint4 logical_clip_parameters;
};

cbuffer FogTableConstants : register(b1) {
    float4 fog_lookup_table[32];
};

struct DrawVertexInput {
    float3 position : POSITION;
    float position_w : POSITION1;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    float3 normal : NORMAL0;
    float4 secondary_color : COLOR1;
    float fog_coordinate : TEXCOORD1;
    float depth_coordinate : TEXCOORD2;
};

struct DrawVertexOutput {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    float3 normal : NORMAL0;
    float4 secondary_color : COLOR1;
    float fog_coordinate : TEXCOORD1;
    noperspective float depth_coordinate : TEXCOORD2;
    noperspective float2 reciprocal_texcoord : TEXCOORD3;
    noperspective float4 pvr_screen_color : TEXCOORD4;
    noperspective float4 pvr_screen_secondary_color : TEXCOORD5;
    noperspective float pvr_screen_fog_coordinate : TEXCOORD6;
    float homogeneous_clip_w : TEXCOORD7;
    nointerpolation float flat_fog_coordinate : TEXCOORD8;
};

DrawVertexOutput draw_vertex_main(DrawVertexInput input) {
    DrawVertexOutput output;
    const bool clip_homogeneous =
        (pipeline_flags.x & 0x80000u) != 0u;
    const float4 source_position = float4(
        input.position, clip_homogeneous ? input.position_w : 1.0);
    output.position = clip_homogeneous
        ? source_position
        : mul(source_position, draw_transform);
    output.homogeneous_clip_w = output.position.w;
    if ((pipeline_flags.x & 0x20u) != 0u &&
        (pipeline_flags.x & 0x10000u) == 0u)
        output.position.z = 0.0;
    output.normal = normalize(
        mul(float4(input.normal, 0.0), draw_normal_transform).xyz);
    output.texcoord = (pipeline_flags.x & 0x10u) != 0u
        ? float2(-0.5 * output.normal.x + 0.5,
                  0.5 * output.normal.y + 0.5)
        : input.texcoord;
    output.color = input.color;
    output.secondary_color = input.secondary_color;
    output.fog_coordinate = input.fog_coordinate;
    output.flat_fog_coordinate = input.fog_coordinate;
    // Pixel-stage SV_Position.w is not a portable source for the original
    // clip-space W.  Screen-space geometry therefore keeps its explicit
    // reciprocal coordinate; homogeneous geometry carries clip W separately
    // and derives the reciprocal only after host clipping in the pixel stage.
    output.depth_coordinate = input.depth_coordinate;
    output.reciprocal_texcoord =
        output.texcoord * input.depth_coordinate;
    const float pvr_screen_weight = input.depth_coordinate;
    output.pvr_screen_color = input.color * pvr_screen_weight;
    output.pvr_screen_secondary_color =
        input.secondary_color * pvr_screen_weight;
    output.pvr_screen_fog_coordinate =
        input.fog_coordinate * pvr_screen_weight;
    return output;
}

Texture2D draw_texture : register(t0);
SamplerState draw_sampler : register(s0);

bool alpha_test_passes(float alpha, uint operation, float reference) {
    if (operation == 0u) return false;
    if (operation == 1u) return alpha < reference;
    if (operation == 2u) return alpha == reference;
    if (operation == 3u) return alpha <= reference;
    if (operation == 4u) return alpha > reference;
    if (operation == 5u) return alpha != reference;
    if (operation == 6u) return alpha >= reference;
    return true;
}

float fog_lookup_value(uint index) {
    const uint bounded_index = min(index, 127u);
    const uint vector_index = bounded_index >> 2u;
    const uint component = bounded_index & 3u;
    const float4 coefficients = fog_lookup_table[vector_index];
    if (component == 0u) return coefficients.x;
    if (component == 1u) return coefficients.y;
    if (component == 2u) return coefficients.z;
    return coefficients.w;
}

float lookup_table_fog(float coordinate, float density) {
    const float z = clamp(density * coordinate, 1.0, 255.9999);
    const float exponent = floor(log2(z));
    const float mantissa = z * 16.0 / exp2(exponent) - 16.0;
    const uint index = (uint)floor(mantissa + exponent * 16.0);
    const float fraction = mantissa - floor(mantissa);
    return saturate(lerp(fog_lookup_value(index),
                         fog_lookup_value(index + 1u),
                         fraction));
}

struct DrawPixelOutput {
    float4 color : SV_Target;
    float depth : SV_Depth;
};

bool native_logical_clip_pass(float2 pixel_position) {
    const uint mode = logical_clip_parameters.x;
    if (mode == 0u) return true;
    const float2 logical_position =
        (pixel_position - logical_clip_transform.zw) /
        max(logical_clip_transform.xy, float2(0.000001, 0.000001));
    const bool inside = logical_position.x >= logical_clip_bounds.x &&
                        logical_position.y >= logical_clip_bounds.y &&
                        logical_position.x < logical_clip_bounds.z &&
                        logical_position.y < logical_clip_bounds.w;
    return mode == 1u ? !inside : inside;
}

DrawPixelOutput draw_pixel_main(DrawVertexOutput input) {
    if (!native_logical_clip_pass(input.position.xy)) discard;
    const uint flags = pipeline_flags.x;
    const bool homogeneous_reciprocal_clip = (flags & 0x10000u) != 0u;
    const bool screen_space_reciprocal =
        (flags & 0x20u) != 0u && !homogeneous_reciprocal_clip;
    // homogeneous_clip_w uses ordinary perspective interpolation.  Because
    // each vertex publishes its own clip W, clipping preserves the equality
    // and perspective interpolation reconstructs fragment W exactly.  Taking
    // the reciprocal here avoids evaluating 1/W on rejected W<=0 vertices.
    const float reciprocal_coordinate = homogeneous_reciprocal_clip
        ? rcp(input.homogeneous_clip_w)
        : input.depth_coordinate;
    const bool pvr_screen_gouraud = (flags & 0x40000u) != 0u;
    const float pvr_screen_weight = input.depth_coordinate;
    const float4 interpolated_color = pvr_screen_gouraud
        ? input.pvr_screen_color / pvr_screen_weight
        : input.color;
    const float4 interpolated_secondary_color = pvr_screen_gouraud
        ? input.pvr_screen_secondary_color / pvr_screen_weight
        : input.secondary_color;
    const float interpolated_fog_coordinate = pvr_screen_gouraud
        ? input.pvr_screen_fog_coordinate / pvr_screen_weight
        : input.fog_coordinate;
    const bool vertex_color_enabled = (flags & 0x01u) != 0u;
    const bool secondary_color_enabled = (flags & 0x02u) != 0u;
    const bool lighting_enabled = (flags & 0x04u) != 0u;
    const bool specular_enabled = (flags & 0x08u) != 0u;
    const bool primary_alpha_enabled = (flags & 0x40u) != 0u;
    const bool texture_alpha_enabled = (flags & 0x80u) != 0u;
    const bool texture_present = (flags & 0x20000u) != 0u;
    float4 primary = material_diffuse;
    float3 post_color = material_emission.rgb;
    if (lighting_enabled) {
        const float3 normal = normalize(input.normal);
        float3 diffuse_light = scene_ambient.rgb * material_ambient.rgb;
        float3 specular_light = 0.0;
        [unroll]
        for (uint index = 0u; index < 4u; ++index) {
            if (index >= pipeline_flags.y) break;
            const float3 direction = normalize(light_directions[index].xyz);
            const float diffuse_amount = saturate(dot(normal, direction));
            diffuse_light += light_colors[index].rgb * diffuse_amount;
            if (specular_enabled && diffuse_amount > 0.0) {
                const float3 half_vector =
                    normalize(direction + float3(0.0, 0.0, 1.0));
                const float specular_amount = pow(
                    saturate(dot(normal, half_vector)),
                    max(material_parameters.x, 0.0001));
                specular_light += light_colors[index].rgb *
                                  material_specular.rgb * specular_amount;
            }
        }
        primary.rgb = material_diffuse.rgb * diffuse_light;
        post_color += specular_light;
    }
    if (vertex_color_enabled) primary *= interpolated_color;
    if (!primary_alpha_enabled) primary.a = 1.0;

    if (pipeline_flags.z == 6u) {
        const float primary_fog = lookup_table_fog(
            homogeneous_reciprocal_clip
                ? reciprocal_coordinate
                : interpolated_fog_coordinate,
            fog_parameters.z);
        primary = float4(fog_color.rgb, primary_fog);
    }

    float4 result = primary;
    if (texture_present) {
        const float2 texture_coordinate = screen_space_reciprocal
            ? input.reciprocal_texcoord / input.depth_coordinate
            : input.texcoord;
        float4 texture_color =
            draw_texture.Sample(draw_sampler, texture_coordinate);
        if ((flags & 0x800000u) != 0u) {
            // Two eight-bit angles are packed into ARGB4444. This is a
            // material response, not RGB normal-map modulation.
            const float altitude = (texture_color.a * 240.0 +
                                    texture_color.r * 15.0) *
                                   (1.5707963267948966 / 255.0);
            const float azimuth = (texture_color.g * 240.0 +
                                   texture_color.b * 15.0) *
                                  (6.283185307179586 / 255.0);
            texture_color.a = saturate(
                interpolated_secondary_color.a +
                interpolated_secondary_color.r * sin(altitude) +
                interpolated_secondary_color.g * cos(altitude) *
                cos(azimuth - 6.283185307179586 *
                              interpolated_secondary_color.b));
            texture_color.rgb = 1.0;
        } else if (!texture_alpha_enabled) {
            texture_color.a = 1.0;
        }
        const uint texture_combine = (flags >> 8u) & 0xffu;
        result = primary * texture_color;
        if (texture_combine == 1u) {
            result = texture_color;
        } else if (texture_combine == 2u) {
            result.rgb = lerp(primary.rgb, texture_color.rgb, texture_color.a);
            result.a = primary.a;
        } else if (texture_combine == 3u) {
            result = primary + texture_color;
        } else if (texture_combine == 4u) {
            result.rgb = primary.rgb * texture_color.rgb;
            result.a = texture_color.a;
        }
    }
    result.rgb += post_color;
    if (texture_present && secondary_color_enabled &&
        (flags & 0x800000u) == 0u)
        result.rgb += interpolated_secondary_color.rgb;
    if ((flags & 0x100000u) != 0u)
        result = clamp(result, color_clamp_minimum, color_clamp_maximum);

    float fog_amount = 0.0;
    if (pipeline_flags.z == 1u && (flags & 0x800000u) == 0u) {
        fog_amount = saturate((flags & 0x400000u) != 0u
            ? input.flat_fog_coordinate : interpolated_fog_coordinate);
    } else if (pipeline_flags.z == 2u) {
        fog_amount = saturate(
            (interpolated_fog_coordinate - fog_parameters.x) /
            (fog_parameters.y - fog_parameters.x));
    } else if (pipeline_flags.z == 3u) {
        fog_amount = saturate(
            1.0 - exp(-fog_parameters.z * interpolated_fog_coordinate));
    } else if (pipeline_flags.z == 4u) {
        const float fog_distance =
            fog_parameters.z * interpolated_fog_coordinate;
        fog_amount = saturate(1.0 - exp(-(fog_distance * fog_distance)));
    } else if (pipeline_flags.z == 5u) {
        fog_amount = lookup_table_fog(
            homogeneous_reciprocal_clip
                ? reciprocal_coordinate
                : interpolated_fog_coordinate,
            fog_parameters.z);
    }
    result.rgb = lerp(result.rgb, fog_color.rgb, fog_amount);

    result *= material_parameters.z;
    const uint alpha_test = pipeline_flags.w;
    if ((alpha_test & 0x100u) != 0u) {
        if ((alpha_test & 0x200u) != 0u) {
            const float alpha_8bit = round(result.a * 255.0);
            const float reference_8bit =
                (float)((alpha_test >> 16u) & 0xffu);
            if (alpha_8bit < reference_8bit) discard;
            result.a = 1.0;
        } else if (!alpha_test_passes(
                       result.a, alpha_test & 0xffu,
                       material_parameters.y)) {
            discard;
        }
    }
    DrawPixelOutput output;
    output.color = result;
    output.depth = input.position.z;
    if ((flags & 0x20u) != 0u) {
        output.depth =
            log2(1.0 + depth_parameters.x * reciprocal_coordinate) /
            depth_parameters.y;
    }
    return output;
}

struct CompositeVertexOutput {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

CompositeVertexOutput composite_vertex_main(uint vertex_id : SV_VertexID) {
    static const float2 positions[3] = {
        float2(-1.0,  1.0),
        float2( 3.0,  1.0),
        float2(-1.0, -3.0)
    };
    static const float2 texcoords[3] = {
        float2(0.0, 0.0),
        float2(2.0, 0.0),
        float2(0.0, 2.0)
    };
    CompositeVertexOutput output;
    output.position = float4(positions[vertex_id], 0.0, 1.0);
    output.texcoord = texcoords[vertex_id];
    return output;
}

Texture2D composite_texture : register(t0);
SamplerState composite_sampler : register(s0);

float4 composite_pixel_main(CompositeVertexOutput input) : SV_Target {
    return composite_texture.Sample(composite_sampler, input.texcoord);
}

cbuffer PerformanceOverlayConstants : register(b2) {
    uint4 performance_overlay_rows[32];
};

uint performance_overlay_word(uint4 row, uint index) {
    if (index == 0u) return row.x;
    if (index == 1u) return row.y;
    if (index == 2u) return row.z;
    return row.w;
}

float4 performance_overlay_pixel_main(
    CompositeVertexOutput input) : SV_Target {
    const int2 pixel = int2(floor(input.position.xy)) - int2(12, 12);
    if (pixel.x < 0 || pixel.y < 0 || pixel.x >= 128 || pixel.y >= 32)
        discard;
    const uint x = (uint)pixel.x;
    const uint y = (uint)pixel.y;
    const uint word = performance_overlay_word(
        performance_overlay_rows[y], x >> 5u);
    const bool glyph = (word & (1u << (x & 31u))) != 0u;
    return glyph
        ? float4(0.85, 1.0, 0.25, 0.96)
        : float4(0.0, 0.0, 0.0, 0.62);
}
)";

// This is a separate shader source so the ordinary ps_4_0 draw path remains
// byte-for-byte independent of UAV support.  Type-2 autosort is enabled only
// on feature-level 11 devices and therefore never changes the Opaque,
// PunchThrough, UI, or authored translucent shader contract.
constexpr char native_graphics_type_two_capture_shader_source[] = R"(
cbuffer DrawConstants : register(b0) {
    row_major float4x4 draw_transform;
    row_major float4x4 draw_normal_transform;
    float4 material_diffuse;
    float4 material_ambient;
    float4 material_specular;
    float4 material_emission;
    float4 scene_ambient;
    float4 fog_color;
    float4 color_clamp_minimum;
    float4 color_clamp_maximum;
    float4 light_directions[4];
    float4 light_colors[4];
    float4 fog_parameters;
    float4 depth_parameters;
    float4 material_parameters;
    uint4 pipeline_flags;
    uint4 type_two_parameters;
    float4 logical_clip_transform;
    float4 logical_clip_bounds;
    uint4 logical_clip_parameters;
};

cbuffer FogTableConstants : register(b1) {
    float4 fog_lookup_table[32];
};

struct DrawVertexOutput {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
    float3 normal : NORMAL0;
    float4 secondary_color : COLOR1;
    float fog_coordinate : TEXCOORD1;
    noperspective float depth_coordinate : TEXCOORD2;
    noperspective float2 reciprocal_texcoord : TEXCOORD3;
    noperspective float4 pvr_screen_color : TEXCOORD4;
    noperspective float4 pvr_screen_secondary_color : TEXCOORD5;
    noperspective float pvr_screen_fog_coordinate : TEXCOORD6;
    float homogeneous_clip_w : TEXCOORD7;
    nointerpolation float flat_fog_coordinate : TEXCOORD8;
};

struct DrawPixelOutput {
    float4 color : SV_Target;
    float depth : SV_Depth;
};

bool native_logical_clip_pass(float2 pixel_position) {
    const uint mode = logical_clip_parameters.x;
    if (mode == 0u) return true;
    const float2 logical_position =
        (pixel_position - logical_clip_transform.zw) /
        max(logical_clip_transform.xy, float2(0.000001, 0.000001));
    const bool inside = logical_position.x >= logical_clip_bounds.x &&
                        logical_position.y >= logical_clip_bounds.y &&
                        logical_position.x < logical_clip_bounds.z &&
                        logical_position.y < logical_clip_bounds.w;
    return mode == 1u ? !inside : inside;
}

struct TypeTwoFragment {
    uint color;
    float depth;
    uint sequence;
    uint primitive;
    uint next;
    uint blend_factors;
};

Texture2D draw_texture : register(t0);
SamplerState draw_sampler : register(s0);
Texture2D<float> type_two_depth_texture : register(t5);
RWTexture2D<uint> type_two_heads : register(u1);
RWStructuredBuffer<TypeTwoFragment> type_two_fragments : register(u2);
RWTexture2D<uint> type_two_counts : register(u3);
RWStructuredBuffer<uint> type_two_status : register(u4);

uint type_two_pack_color(float4 color) {
    const uint4 bytes = uint4(round(saturate(color) * 255.0));
    return (bytes.r << 24u) | (bytes.g << 16u) |
           (bytes.b << 8u) | bytes.a;
}

bool alpha_test_passes(float alpha, uint operation, float reference) {
    if (operation == 0u) return false;
    if (operation == 1u) return alpha < reference;
    if (operation == 2u) return alpha == reference;
    if (operation == 3u) return alpha <= reference;
    if (operation == 4u) return alpha > reference;
    if (operation == 5u) return alpha != reference;
    if (operation == 6u) return alpha >= reference;
    return true;
}

float fog_lookup_value(uint index) {
    const uint bounded_index = min(index, 127u);
    const uint vector_index = bounded_index >> 2u;
    const uint component = bounded_index & 3u;
    const float4 coefficients = fog_lookup_table[vector_index];
    if (component == 0u) return coefficients.x;
    if (component == 1u) return coefficients.y;
    if (component == 2u) return coefficients.z;
    return coefficients.w;
}

float lookup_table_fog(float coordinate, float density) {
    const float z = clamp(density * coordinate, 1.0, 255.9999);
    const float exponent = floor(log2(z));
    const float mantissa = z * 16.0 / exp2(exponent) - 16.0;
    const uint index = (uint)floor(mantissa + exponent * 16.0);
    const float fraction = mantissa - floor(mantissa);
    return saturate(lerp(fog_lookup_value(index),
                         fog_lookup_value(index + 1u),
                         fraction));
}

DrawPixelOutput draw_type_two_capture_main(
    DrawVertexOutput input,
    uint primitive_id : SV_PrimitiveID) {
    if (!native_logical_clip_pass(input.position.xy)) discard;
    const uint flags = pipeline_flags.x;
    const bool homogeneous_reciprocal_clip = (flags & 0x10000u) != 0u;
    const bool screen_space_reciprocal =
        (flags & 0x20u) != 0u && !homogeneous_reciprocal_clip;
    const float reciprocal_coordinate = homogeneous_reciprocal_clip
        ? rcp(input.homogeneous_clip_w)
        : input.depth_coordinate;
    const bool pvr_screen_gouraud = (flags & 0x40000u) != 0u;
    const float pvr_screen_weight = input.depth_coordinate;
    const float4 interpolated_color = pvr_screen_gouraud
        ? input.pvr_screen_color / pvr_screen_weight
        : input.color;
    const float4 interpolated_secondary_color = pvr_screen_gouraud
        ? input.pvr_screen_secondary_color / pvr_screen_weight
        : input.secondary_color;
    const float interpolated_fog_coordinate = pvr_screen_gouraud
        ? input.pvr_screen_fog_coordinate / pvr_screen_weight
        : input.fog_coordinate;
    const bool vertex_color_enabled = (flags & 0x01u) != 0u;
    const bool secondary_color_enabled = (flags & 0x02u) != 0u;
    const bool lighting_enabled = (flags & 0x04u) != 0u;
    const bool specular_enabled = (flags & 0x08u) != 0u;
    const bool primary_alpha_enabled = (flags & 0x40u) != 0u;
    const bool texture_alpha_enabled = (flags & 0x80u) != 0u;
    const bool texture_present = (flags & 0x20000u) != 0u;
    float4 primary = material_diffuse;
    float3 post_color = material_emission.rgb;
    if (lighting_enabled) {
        const float3 normal = normalize(input.normal);
        float3 diffuse_light = scene_ambient.rgb * material_ambient.rgb;
        float3 specular_light = 0.0;
        [unroll]
        for (uint index = 0u; index < 4u; ++index) {
            if (index >= pipeline_flags.y) break;
            const float3 direction = normalize(light_directions[index].xyz);
            const float diffuse_amount = saturate(dot(normal, direction));
            diffuse_light += light_colors[index].rgb * diffuse_amount;
            if (specular_enabled && diffuse_amount > 0.0) {
                const float3 half_vector =
                    normalize(direction + float3(0.0, 0.0, 1.0));
                const float specular_amount = pow(
                    saturate(dot(normal, half_vector)),
                    max(material_parameters.x, 0.0001));
                specular_light += light_colors[index].rgb *
                                  material_specular.rgb * specular_amount;
            }
        }
        primary.rgb = material_diffuse.rgb * diffuse_light;
        post_color += specular_light;
    }
    if (vertex_color_enabled) primary *= interpolated_color;
    if (!primary_alpha_enabled) primary.a = 1.0;

    if (pipeline_flags.z == 6u) {
        const float primary_fog = lookup_table_fog(
            homogeneous_reciprocal_clip
                ? reciprocal_coordinate
                : interpolated_fog_coordinate,
            fog_parameters.z);
        primary = float4(fog_color.rgb, primary_fog);
    }

    float4 result = primary;
    if (texture_present) {
        const float2 texture_coordinate = screen_space_reciprocal
            ? input.reciprocal_texcoord / input.depth_coordinate
            : input.texcoord;
        float4 texture_color =
            draw_texture.Sample(draw_sampler, texture_coordinate);
        if ((flags & 0x800000u) != 0u) {
            // Two eight-bit angles are packed into ARGB4444. This is a
            // material response, not RGB normal-map modulation.
            const float altitude = (texture_color.a * 240.0 +
                                    texture_color.r * 15.0) *
                                   (1.5707963267948966 / 255.0);
            const float azimuth = (texture_color.g * 240.0 +
                                   texture_color.b * 15.0) *
                                  (6.283185307179586 / 255.0);
            texture_color.a = saturate(
                interpolated_secondary_color.a +
                interpolated_secondary_color.r * sin(altitude) +
                interpolated_secondary_color.g * cos(altitude) *
                cos(azimuth - 6.283185307179586 *
                              interpolated_secondary_color.b));
            texture_color.rgb = 1.0;
        } else if (!texture_alpha_enabled) {
            texture_color.a = 1.0;
        }
        const uint texture_combine = (flags >> 8u) & 0xffu;
        result = primary * texture_color;
        if (texture_combine == 1u) {
            result = texture_color;
        } else if (texture_combine == 2u) {
            result.rgb = lerp(primary.rgb, texture_color.rgb, texture_color.a);
            result.a = primary.a;
        } else if (texture_combine == 3u) {
            result = primary + texture_color;
        } else if (texture_combine == 4u) {
            result.rgb = primary.rgb * texture_color.rgb;
            result.a = texture_color.a;
        }
    }
    result.rgb += post_color;
    if (texture_present && secondary_color_enabled &&
        (flags & 0x800000u) == 0u)
        result.rgb += interpolated_secondary_color.rgb;
    if ((flags & 0x100000u) != 0u)
        result = clamp(result, color_clamp_minimum, color_clamp_maximum);

    float fog_amount = 0.0;
    if (pipeline_flags.z == 1u && (flags & 0x800000u) == 0u) {
        fog_amount = saturate((flags & 0x400000u) != 0u
            ? input.flat_fog_coordinate : interpolated_fog_coordinate);
    } else if (pipeline_flags.z == 2u) {
        fog_amount = saturate(
            (interpolated_fog_coordinate - fog_parameters.x) /
            (fog_parameters.y - fog_parameters.x));
    } else if (pipeline_flags.z == 3u) {
        fog_amount = saturate(
            1.0 - exp(-fog_parameters.z * interpolated_fog_coordinate));
    } else if (pipeline_flags.z == 4u) {
        const float fog_distance =
            fog_parameters.z * interpolated_fog_coordinate;
        fog_amount = saturate(1.0 - exp(-(fog_distance * fog_distance)));
    } else if (pipeline_flags.z == 5u) {
        fog_amount = lookup_table_fog(
            homogeneous_reciprocal_clip
                ? reciprocal_coordinate
                : interpolated_fog_coordinate,
            fog_parameters.z);
    }
    result.rgb = lerp(result.rgb, fog_color.rgb, fog_amount);

    result *= material_parameters.z;
    const uint alpha_test = pipeline_flags.w;
    if ((alpha_test & 0x100u) != 0u) {
        if ((alpha_test & 0x200u) != 0u) {
            const float alpha_8bit = round(result.a * 255.0);
            const float reference_8bit =
                (float)((alpha_test >> 16u) & 0xffu);
            if (alpha_8bit < reference_8bit) discard;
            result.a = 1.0;
        } else if (!alpha_test_passes(
                       result.a, alpha_test & 0xffu,
                       material_parameters.y)) {
            discard;
        }
    }

    float final_depth = input.position.z;
    if ((flags & 0x20u) != 0u) {
        final_depth =
            log2(1.0 + depth_parameters.x * reciprocal_coordinate) /
            depth_parameters.y;
    }
    const uint2 pixel = uint2(input.position.xy);
    // UAV writes are pixel-shader side effects and therefore cannot rely on
    // the later DSV test to suppress hidden nodes.  Match the PVR OIT
    // front-depth contract explicitly before touching the per-pixel list.
    const float opaque_depth =
        type_two_depth_texture.Load(int3(pixel, 0));
    if (final_depth < opaque_depth) discard;
    uint pixel_count;
    InterlockedAdd(type_two_counts[pixel], 1u, pixel_count);
    uint ignored;
    InterlockedMax(type_two_status[2], pixel_count + 1u, ignored);
    uint fragment_index;
    InterlockedAdd(type_two_status[0], 1u, fragment_index);
    if (fragment_index >= type_two_parameters.w) {
        InterlockedExchange(type_two_status[1], 1u, ignored);
        discard;
    }
    TypeTwoFragment fragment;
    fragment.color = type_two_pack_color(result);
    fragment.depth = final_depth;
    fragment.sequence = type_two_parameters.z;
    fragment.primitive = primitive_id;
    fragment.next = 0xFFFFFFFFu;
    fragment.blend_factors = type_two_parameters.x |
                             ((type_two_parameters.y & 0x3u) << 16u);
    uint previous;
    InterlockedExchange(type_two_heads[pixel], fragment_index, previous);
    fragment.next = previous;
    type_two_fragments[fragment_index] = fragment;

    DrawPixelOutput output;
    // The capture blend state masks the color target and its depth state is
    // deliberately read-only.  Return the exact native depth for the GEQUAL
    // test while the PPLL keeps the complete translucent list for sorting.
    output.color = float4(0.0, 0.0, 0.0, 0.0);
    output.depth = final_depth;
    return output;
}
)";

constexpr char native_graphics_type_two_resolve_shader_source[] = R"(
cbuffer TypeTwoResolveConstants : register(b2) {
    uint4 type_two_parameters;
};

Texture2D type_two_base_texture : register(t1);
Texture2D<uint> type_two_head_texture : register(t2);
Texture2D<uint> type_two_count_texture : register(t4);
StructuredBuffer<uint> type_two_status : register(t5);

struct TypeTwoFragment {
    uint color;
    float depth;
    uint sequence;
    uint primitive;
    uint next;
    uint blend_factors;
};
StructuredBuffer<TypeTwoFragment> type_two_fragments : register(t3);
static const uint type_two_sort_capacity = NATIVE_TYPE_TWO_SORT_CAPACITY;

float4 type_two_unpack_color(uint packed) {
    return float4(float((packed >> 24u) & 0xffu),
                  float((packed >> 16u) & 0xffu),
                  float((packed >> 8u) & 0xffu),
                  float(packed & 0xffu)) / 255.0;
}

struct CompositeVertexOutput {
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

bool type_two_key_before(TypeTwoFragment candidate,
                         TypeTwoFragment selected) {
    return candidate.depth < selected.depth ||
           (candidate.depth == selected.depth &&
            (candidate.sequence < selected.sequence ||
             (candidate.sequence == selected.sequence &&
              candidate.primitive < selected.primitive)));
}

float4 type_two_blend_factor(uint factor,
                             float4 source,
                             float4 destination) {
    if (factor == 0u) return 0.0;
    if (factor == 1u) return 1.0;
    if (factor == 2u) return source;
    if (factor == 3u) return 1.0 - source;
    if (factor == 4u) return destination;
    if (factor == 5u) return 1.0 - destination;
    if (factor == 6u) return source.aaaa;
    if (factor == 7u) return 1.0 - source.aaaa;
    if (factor == 8u) return destination.aaaa;
    return 1.0 - destination.aaaa;
}

float4 type_two_blend(TypeTwoFragment fragment,
                      float4 framebuffer,
                      float4 secondary,
                      out bool destination_is_secondary) {
    const uint packed = fragment.blend_factors;
    const bool source_is_secondary = ((packed >> 16u) & 1u) != 0u;
    destination_is_secondary = ((packed >> 17u) & 1u) != 0u;
    const float4 source = source_is_secondary
        ? secondary
        : type_two_unpack_color(fragment.color);
    const float4 destination = destination_is_secondary
        ? secondary
        : framebuffer;
    const float4 source_color_factor = type_two_blend_factor(
        packed & 0xFu, source, destination);
    const float4 destination_color_factor = type_two_blend_factor(
        (packed >> 4u) & 0xFu, source, destination);
    const float4 source_alpha_factor = type_two_blend_factor(
        (packed >> 8u) & 0xFu, source, destination);
    const float4 destination_alpha_factor = type_two_blend_factor(
        (packed >> 12u) & 0xFu, source, destination);
    return float4(
        saturate(source.rgb * source_color_factor.rgb +
                 destination.rgb * destination_color_factor.rgb),
        saturate(source.a * source_alpha_factor.a +
                 destination.a * destination_alpha_factor.a));
}

float4 type_two_resolve_pixel_main(CompositeVertexOutput input) : SV_Target {
    // The allocator counts attempted nodes, including overflow. Only nodes
    // below the same configured arena bound can have been published. Keep
    // both linked-list checks below; no CPU synchronization is needed.
    const uint live_count = min(type_two_status[0], type_two_parameters.w);
    // Previously the CPU omitted resolve for an empty gather. Discard keeps
    // the render target byte-for-byte untouched for that same empty case.
    if (live_count == 0u) discard;
    const uint2 pixel = uint2(input.position.xy);
    const float4 base = type_two_base_texture.Load(int3(pixel, 0));
    float4 result = base;
    float4 secondary = 0.0;
    const uint pixel_count = type_two_count_texture.Load(int3(pixel, 0));
    const uint retain_limit = min(
        pixel_count, min(type_two_parameters.x, type_two_sort_capacity));
    TypeTwoFragment sorted_fragments[NATIVE_TYPE_TWO_SORT_CAPACITY];
    uint retained_pixel_count = 0u;
    uint retained_node = type_two_head_texture.Load(int3(pixel, 0));
    // The per-pixel counter includes globally discarded fragments. Load the
    // first configured nodes from the head once; an exhausted arena cannot
    // make an otherwise valid pixel fall back to its opaque base.
    [loop]
    while (retained_pixel_count < retain_limit &&
           retained_node != 0xFFFFFFFFu) {
        if (retained_node >= live_count ||
            retained_node >= type_two_parameters.w) return base;
        sorted_fragments[retained_pixel_count] =
            type_two_fragments[retained_node];
        retained_node = sorted_fragments[retained_pixel_count].next;
        ++retained_pixel_count;
    }
    // Insertion sort is stable because equal depth/sequence/primitive keys do
    // not move past one another. The input order is the authenticated linked
    // list order from the head, matching the previous resolver's tie choice.
    [loop]
    for (uint index = 1u; index < retained_pixel_count; ++index) {
        const TypeTwoFragment candidate = sorted_fragments[index];
        uint insertion = index;
        [loop]
        while (insertion > 0u) {
            // SM5 boolean operators do not guarantee short-circuit loads.
            // Establish the lower bound before indexing the predecessor.
            if (!type_two_key_before(
                    candidate, sorted_fragments[insertion - 1u])) break;
            sorted_fragments[insertion] =
                sorted_fragments[insertion - 1u];
            --insertion;
        }
        sorted_fragments[insertion] = candidate;
    }

    [loop]
    for (uint processed = 0u;
         processed < retained_pixel_count;
         ++processed) {
        const TypeTwoFragment selected = sorted_fragments[processed];
        bool destination_is_secondary = false;
        const float4 blended = type_two_blend(
            selected, result, secondary, destination_is_secondary);
        if (destination_is_secondary)
            secondary = blended;
        else
            result = blended;
    }
    return result;
}
)";

struct ShaderCompilerApi final {
    HMODULE module = nullptr;
    decltype(&D3DCompile) compile = nullptr;
    decltype(&D3DCreateBlob) create_blob = nullptr;
};

[[nodiscard]] const ShaderCompilerApi& shader_compiler_api() noexcept {
    static const auto api = []() noexcept {
        constexpr std::array names{
            L"d3dcompiler_47.dll",
            L"d3dcompiler_46.dll",
            L"d3dcompiler_43.dll",
        };
        for (const auto* name : names) {
            const auto module = LoadLibraryW(name);
            if (module == nullptr) continue;
            const auto compile = reinterpret_cast<decltype(&D3DCompile)>(
                GetProcAddress(module, "D3DCompile"));
            if (compile != nullptr) return ShaderCompilerApi{module, compile,
                reinterpret_cast<decltype(&D3DCreateBlob)>(GetProcAddress(module,"D3DCreateBlob"))};
            FreeLibrary(module);
        }
        return ShaderCompilerApi{};
    }();
    return api;
}

[[nodiscard]] ComPtr<ID3DBlob> compile_shader(
    const char* entry,
    const char* target) {
    return compile_shader_source(
        native_graphics_shader_source,
        sizeof(native_graphics_shader_source) - 1u,
        entry,
        target,
        "katana-native-port-graphics");
}

[[nodiscard]] ComPtr<ID3DBlob> compile_type_two_shader(
    const char* const entry,
    const char* const target,
    const std::uint32_t maximum_type2_fragments_per_pixel) {
    const bool resolve = std::strcmp(entry, "type_two_resolve_pixel_main") == 0;
    const auto* const source = resolve
        ? native_graphics_type_two_resolve_shader_source
        : native_graphics_type_two_capture_shader_source;
    const auto source_size = resolve
        ? sizeof(native_graphics_type_two_resolve_shader_source) - 1u
        : sizeof(native_graphics_type_two_capture_shader_source) - 1u;
    if (resolve) {
        // Both the declared constant and the array bound use this macro.
        // Replacing only the first token would leave an unbound HLSL symbol.
        std::string specialized_source =
            "#define NATIVE_TYPE_TWO_SORT_CAPACITY " +
            std::to_string(maximum_type2_fragments_per_pixel) + "\n";
        specialized_source.append(source, source_size);
        return compile_shader_source(
            specialized_source.data(),
            specialized_source.size(),
            entry,
            target,
            "katana-native-port-graphics-type-two");
    }
    return compile_shader_source(
        source,
        source_size,
        entry,
        target,
        "katana-native-port-graphics-type-two");
}

[[nodiscard]] ComPtr<ID3DBlob> compile_shader_source(
    const char* const source,
    const std::size_t source_size,
    const char* const entry,
    const char* const target,
    const char* const source_name) {
    const auto started=std::chrono::steady_clock::now();
    const auto& api=shader_compiler_api();
    static const auto compiler_identity=[] {
        std::array<wchar_t,32768> path{};const auto count=GetModuleFileNameW(shader_compiler_api().module,path.data(),DWORD(path.size()));
        return count && count<path.size()?sonic::startup::file_digest(std::filesystem::path(std::wstring(path.data(),count))):std::string{};
    }();
    std::string cache_key;
    if(!compiler_identity.empty()) {
        std::string contract="sarecomp-dxbc-v1:strict:O3:"+compiler_identity+":"+entry+":"+target+":"+source_name+":";
        contract.append(source,source_size);cache_key=sonic::startup::digest(contract);
    }
    const auto report=[&](bool hit) {sonic::startup::advance();std::fprintf(stderr,"SONIC_SHADER_CACHE backend=d3d11 entry=%s hit=%u elapsed_ms=%.3f\n",entry,unsigned(hit),std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());};
    if(api.create_blob && !cache_key.empty()) {
        const auto cached=sonic::startup::cache_load("d3d-shaders",cache_key,4u*1024*1024);
        if(cached.size()>=4 && std::memcmp(cached.data(),"DXBC",4)==0) {
            ComPtr<ID3DBlob> blob;
            if(SUCCEEDED(api.create_blob(cached.size(),blob.GetAddressOf()))) {std::memcpy(blob->GetBufferPointer(),cached.data(),cached.size());report(true);return blob;}
        }
    }
    const auto compile = shader_compiler_api().compile;
    if (compile == nullptr)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::ShaderCompilation,
            static_cast<std::uint32_t>(HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND)),
            "shader-compiler");
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> diagnostics;
    const auto result = compile(source,
                                source_size,
                                source_name,
                                nullptr,
                                nullptr,
                                entry,
                                target,
                                D3DCOMPILE_ENABLE_STRICTNESS |
                                    D3DCOMPILE_OPTIMIZATION_LEVEL3,
                                0u,
                                bytecode.GetAddressOf(),
                                diagnostics.GetAddressOf());
    if (FAILED(result))
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::ShaderCompilation,
            static_cast<std::uint32_t>(result),
            entry);
    if(!cache_key.empty()) sonic::startup::cache_save("d3d-shaders",cache_key,
        std::span(static_cast<const std::byte*>(bytecode->GetBufferPointer()),bytecode->GetBufferSize()));
    report(false);
    return bytecode;
}

[[nodiscard]] std::wstring utf8_to_wide(const std::string_view value) {
    if (value.empty() ||
        value.size() > static_cast<std::size_t>(
                           std::numeric_limits<int>::max()))
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidConfig, 0u, "title");
    const auto count = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (count <= 0)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidConfig,
            GetLastError(),
            "title-utf8");
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8,
                            MB_ERR_INVALID_CHARS,
                            value.data(),
                            static_cast<int>(value.size()),
                            result.data(),
                            count) != count)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidConfig,
            GetLastError(),
            "title-convert");
    return result;
}

[[nodiscard]] std::string wide_to_utf8(const std::wstring_view value) {
    if (value.empty() ||
        value.size() > static_cast<std::size_t>(
                           std::numeric_limits<int>::max()))
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidConfig,
            0u,
            "development-state-path");
    const auto count = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (count <= 0)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidConfig,
            GetLastError(),
            "development-state-path-utf8");
    std::string result(static_cast<std::size_t>(count), '\0');
    if (WideCharToMultiByte(CP_UTF8,
                            WC_ERR_INVALID_CHARS,
                            value.data(),
                            static_cast<int>(value.size()),
                            result.data(),
                            count,
                            nullptr,
                            nullptr) != count)
        throw NativePortGraphicsError(
            NativePortGraphicsFailure::InvalidConfig,
            GetLastError(),
            "development-state-path-convert");
    return result;
}

[[nodiscard]] DXGI_FORMAT texture_format(
    const NativePortTextureFormat format) noexcept {
    return format == NativePortTextureFormat::Bgra8Unorm
               ? DXGI_FORMAT_B8G8R8A8_UNORM
               : DXGI_FORMAT_R8G8B8A8_UNORM;
}

[[nodiscard]] D3D11_PRIMITIVE_TOPOLOGY primitive_topology(
    const NativePortPrimitiveTopology topology) noexcept {
    switch (topology) {
    case NativePortPrimitiveTopology::PointList:
        return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
    case NativePortPrimitiveTopology::LineList:
        return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    case NativePortPrimitiveTopology::LineStrip:
        return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case NativePortPrimitiveTopology::TriangleList:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case NativePortPrimitiveTopology::TriangleStrip:
        return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    }
    return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

[[nodiscard]] D3D11_BLEND blend_factor(
    const NativePortBlendFactor factor) noexcept {
    switch (factor) {
    case NativePortBlendFactor::Zero: return D3D11_BLEND_ZERO;
    case NativePortBlendFactor::One: return D3D11_BLEND_ONE;
    case NativePortBlendFactor::SourceColor: return D3D11_BLEND_SRC_COLOR;
    case NativePortBlendFactor::InverseSourceColor:
        return D3D11_BLEND_INV_SRC_COLOR;
    case NativePortBlendFactor::DestinationColor:
        return D3D11_BLEND_DEST_COLOR;
    case NativePortBlendFactor::InverseDestinationColor:
        return D3D11_BLEND_INV_DEST_COLOR;
    case NativePortBlendFactor::SourceAlpha: return D3D11_BLEND_SRC_ALPHA;
    case NativePortBlendFactor::InverseSourceAlpha:
        return D3D11_BLEND_INV_SRC_ALPHA;
    case NativePortBlendFactor::DestinationAlpha:
        return D3D11_BLEND_DEST_ALPHA;
    case NativePortBlendFactor::InverseDestinationAlpha:
        return D3D11_BLEND_INV_DEST_ALPHA;
    }
    return D3D11_BLEND_ZERO;
}

[[nodiscard]] D3D11_BLEND_OP blend_operation(
    const NativePortBlendOperation operation) noexcept {
    switch (operation) {
    case NativePortBlendOperation::Add: return D3D11_BLEND_OP_ADD;
    case NativePortBlendOperation::Subtract: return D3D11_BLEND_OP_SUBTRACT;
    case NativePortBlendOperation::ReverseSubtract:
        return D3D11_BLEND_OP_REV_SUBTRACT;
    case NativePortBlendOperation::Minimum: return D3D11_BLEND_OP_MIN;
    case NativePortBlendOperation::Maximum: return D3D11_BLEND_OP_MAX;
    }
    return D3D11_BLEND_OP_ADD;
}

[[nodiscard]] D3D11_COMPARISON_FUNC comparison_function(
    const NativePortCompareOperation compare) noexcept {
    switch (compare) {
    case NativePortCompareOperation::Never: return D3D11_COMPARISON_NEVER;
    case NativePortCompareOperation::Less: return D3D11_COMPARISON_LESS;
    case NativePortCompareOperation::Equal: return D3D11_COMPARISON_EQUAL;
    case NativePortCompareOperation::LessEqual:
        return D3D11_COMPARISON_LESS_EQUAL;
    case NativePortCompareOperation::Greater: return D3D11_COMPARISON_GREATER;
    case NativePortCompareOperation::NotEqual:
        return D3D11_COMPARISON_NOT_EQUAL;
    case NativePortCompareOperation::GreaterEqual:
        return D3D11_COMPARISON_GREATER_EQUAL;
    case NativePortCompareOperation::Always: return D3D11_COMPARISON_ALWAYS;
    }
    return D3D11_COMPARISON_NEVER;
}

[[nodiscard]] D3D11_TEXTURE_ADDRESS_MODE texture_address_mode(
    const NativePortTextureAddress address) noexcept {
    switch (address) {
    case NativePortTextureAddress::Clamp: return D3D11_TEXTURE_ADDRESS_CLAMP;
    case NativePortTextureAddress::Wrap: return D3D11_TEXTURE_ADDRESS_WRAP;
    case NativePortTextureAddress::Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
    }
    return D3D11_TEXTURE_ADDRESS_CLAMP;
}

[[nodiscard]] D3D11_FILTER texture_filter(
    const NativePortTextureFilter filter) noexcept {
    switch (filter) {
    case NativePortTextureFilter::Point:
        return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case NativePortTextureFilter::Bilinear:
        return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    case NativePortTextureFilter::Trilinear:
        return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    case NativePortTextureFilter::Anisotropic:
        return D3D11_FILTER_ANISOTROPIC;
    }
    return D3D11_FILTER_MIN_MAG_MIP_POINT;
}

[[nodiscard]] UINT next_buffer_capacity(const UINT required) noexcept {
    if (required == 0u) return 0u;
    constexpr auto highest_power_of_two = UINT{1u} << 31u;
    if (required > highest_power_of_two) return required;
    const auto rounded = std::bit_ceil(required);
    return rounded >= required ? rounded : required;
}

} // namespace

#endif

} // namespace katana::runtime
