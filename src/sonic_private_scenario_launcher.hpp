#pragma once

#include "katana/runtime/native_port.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

// This header is deliberately private to SonicAdventureRecomp.  It describes
// title-bound scenario metadata without adding Sonic concepts to Katana's
// public runtime ABI.
namespace sonic_native_private {

enum class ScenarioProviderKind : std::uint8_t {
    EventLoader,
    StageLoader,
    Staffroll,
};

enum ScenarioPrerequisite : std::uint32_t {
    ScenarioPrerequisiteNone = 0u,
    ScenarioPrerequisiteLiveBossCompletion = 1u << 0u,
    ScenarioPrerequisiteStageClear = 1u << 1u,
    ScenarioPrerequisiteAdventureContext = 1u << 2u,
};

struct ScenarioDescriptor final {
    std::string_view id;
    std::string_view label;
    std::string_view guest_path;
    std::string_view encoded_identity;
    std::string_view decoded_identity;
    std::uint32_t encoded_size = 0u;
    std::uint32_t decoded_size = 0u;
    std::uint32_t runtime_base = 0u;
    std::uint32_t entry_offset = 0u;
    std::uint32_t event_id = 0u;
    std::uint32_t loader_case = 0u;
    std::uint32_t stage_major = 0u;
    std::uint32_t stage_minor = 0u;
    // Stage requests are authorized through the resident PAL request tables,
    // not by calling a loader case or a PRS entry.  The index selects the
    // immutable (major, minor) pair and the context selector resolves through
    // the immutable title-context table.  The expected value prevents either
    // table from becoming a mutable host-controlled dispatch surface.
    std::uint32_t stage_table_index = 0u;
    std::uint32_t context_selector = 0u;
    std::uint32_t context_value = 0u;
    std::uint32_t prerequisites = ScenarioPrerequisiteNone;
    std::uint8_t route_order = 0u;
    ScenarioProviderKind provider_kind = ScenarioProviderKind::EventLoader;
    // These flags are evidence gates, not hints.  A false flag keeps the
    // descriptor visible for diagnostics but prevents provider dispatch.
    bool static_identity_proven = false;
    bool entry_shape_proven = false;
    bool stage_minor_proven = false;
    // A valid PRS entry shape does not prove the caller ABI. In particular,
    // event entries may require a title-owned context pointer rather than an
    // event number in r4. This separate gate remains false until the exact
    // loader/task provider contract is bound.
    bool provider_abi_proven = false;
    // Optional index in the resident per-character, 43-byte story-progress
    // table.  It is used only when a descriptor carries StageClear and is
    // validated against the current title context before dispatch.
    std::uint32_t progress_flag_index = ~std::uint32_t{0u};
    // Most Action Stages are represented directly in the retail PAL
    // selector table. Hot Shelter is not: the three real story routes carry
    // a proven table tuple through state 12 and replace only the arguments of
    // the original SetLevelAndAct call. These fields bind that carrier and
    // make the exceptional route explicit instead of inventing a table row.
    std::uint32_t selector_stage_major = 0u;
    std::uint32_t selector_stage_minor = 0u;
    bool stage_tuple_override = false;
};

struct ScenarioOverlayLine final {
    std::string_view text;
    bool selected = false;
    bool ready = false;
};

struct ScenarioOverlayView final {
    bool open = false;
    std::string_view character;
    std::size_t selected = 0u;
    std::size_t line_count = 0u;
    std::uint32_t failed_launches = 0u;
    std::string_view status;
    std::array<ScenarioOverlayLine, 16u> lines{};
};

struct ScenarioProvider final {
    bool (*ready)(
        katana::runtime::NativePortContext&,
        const ScenarioDescriptor&) noexcept = nullptr;
    // Automatic launches requested at process start need a stricter
    // lifecycle gate than an interactive launch.  In particular, zeroed
    // resident selector state during the opening is not yet an active title
    // loader.  The game-specific adapter binds this callback to an exact
    // executable-image identity instead of guessing a frame number.
    bool (*automatic_ready)(
        katana::runtime::NativePortContext&,
        const ScenarioDescriptor&) noexcept = nullptr;
    bool (*launch)(
        katana::runtime::NativePortContext&,
        const ScenarioDescriptor&) noexcept = nullptr;
    void (*draw)(
        katana::runtime::NativePortContext&,
        const ScenarioOverlayView&) noexcept = nullptr;
};

[[nodiscard]] std::span<const ScenarioDescriptor> scenario_descriptors()
    noexcept;

// True only while this title instance owns the private modal menu.  The
// adapter uses the state before publishing its per-frame controller image so
// menu navigation can never also become a guest input edge.
[[nodiscard]] bool scenario_menu_open(
    katana::runtime::NativePortContext& context) noexcept;

// Called once from the already-bound native frame provider.  It polls only
// the private Ctrl+F10 edge and the title-facing button words supplied by the
// adapter; it never polls a platform device, allocates, or changes the normal
// title path while the menu is closed.
void scenario_tick(
    katana::runtime::NativePortContext& context,
    std::span<const std::uint32_t> title_buttons,
    const ScenarioProvider& provider) noexcept;

} // namespace sonic_native_private
