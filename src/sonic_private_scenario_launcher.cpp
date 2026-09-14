#include "sonic_private_scenario_launcher.hpp"

#include <algorithm>
#include <limits>
#include <cstdlib>
#include <cstring>
#include "sonic_input.hpp"

#if defined(_WIN32)
#include <windows.h>
#else
constexpr unsigned VK_UP=0x26,VK_DOWN=0x28,VK_LEFT=0x25,VK_RIGHT=0x27,VK_RETURN=0x0d,VK_ESCAPE=0x1b;
#endif

namespace sonic_native_private {
namespace {

// The generated 73-stage x 6-character cross-product is mechanical selector
// evidence only.  Keep its type private to this translation unit so it cannot
// be returned through the launchable ScenarioDescriptor API by accident.
struct StageSelectorInventoryEntry final {
    std::string_view id;
    std::string_view label;
    std::uint32_t stage_table_index = 0u;
    std::uint32_t stage_major = 0u;
    std::uint32_t stage_minor = 0u;
    std::uint32_t context_selector = 0u;
    std::uint32_t context_value = 0u;
};

struct ActionStageCharacterRange final {
    std::string_view id;
    std::string_view label;
    std::size_t first = 0u;
    std::size_t count = 0u;
};

#include "sonic_private_stage_scenarios.inc"

// Mechanical selector evidence remains separate from launchable rows.
// Event previews use their regular resident loader and do not assert a boss
// victory or synthesize story/save flags. Add reviewed rows in character order:
// Sonic, Tails, Knuckles, Amy, Gamma, Big.
static_assert(kGeneratedStageSelectorInventory.size() == 438u);
static_assert(kGeneratedActionStageScenarios.size() == 32u);
static_assert(kGeneratedActionStageCharacters.size() == 6u);
static_assert(kGeneratedDiagnosticScenarios.size() == 1u);
constexpr auto& kActionStageCharacters = kGeneratedActionStageCharacters;
constexpr std::array<ScenarioDescriptor, 1u> kEventPreviewScenarios{{
    {.id = "sonic-event-chaos-preview",
     .label = "Sonic: EV0002 (experimental)",
     .guest_path = "SONICAD/EV0002.PRS",
     .encoded_identity = "sha256:33246bfd2810174a99340d545decdfc8644164df8a2c108919c4a9ca78eb73db",
     .decoded_identity = "sha256:da5515a09542079beca148a3efe46edc9bc7b435f1ec950c557745d4d2ef65a8",
     .encoded_size = 70827u,
     .decoded_size = 157824u,
     .runtime_base = 0x0CB80000u,
     .entry_offset = 0x424u,
     .event_id = 2u,
     .context_value = 0u,
     .prerequisites = ScenarioPrerequisiteAdventureContext,
     .provider_kind = ScenarioProviderKind::EventLoader,
     .static_identity_proven = true,
     .entry_shape_proven = true,
     // 0903E8 only queues a request for an already-owned story manager.
     // The ADVERTISE launch does not establish that owner; r280 captures
     // show its ordinary attract demo taking over instead of isolated EV2.
     .provider_abi_proven = false},
}};
constexpr std::array<ScenarioDescriptor, 2u> kStaffrollScenarios{{
    {.id = "credits-current-character",
     .label = "Credits: Staff Roll (current character)",
     .guest_path = "SONICAD/SUMMARY.PRS",
     .encoded_identity =
         "sha256:7d2ddc3e5441ef4ce18331401cbc06afa10c2f0d791142358af2ad4425e15777",
     .decoded_identity =
         "sha256:93969e279339fd1a9687e2cee7d842e544a41eb777283f2073a04adf1d531949",
     .encoded_size = 92380u,
     .decoded_size = 695821u,
     .runtime_base = 0x0C900000u,
     .entry_offset = 0x1BE0u,
     .prerequisites = ScenarioPrerequisiteNone,
     .provider_kind = ScenarioProviderKind::Staffroll,
     .static_identity_proven = true,
     .entry_shape_proven = true,
     .provider_abi_proven = true},
    {.id = "tutorial-sonic",
     .label = "Sonic: How to Play",
     .guest_path = "SONICAD/SUMMARY.PRS",
     .encoded_identity =
         "sha256:7d2ddc3e5441ef4ce18331401cbc06afa10c2f0d791142358af2ad4425e15777",
     .decoded_identity =
         "sha256:93969e279339fd1a9687e2cee7d842e544a41eb777283f2073a04adf1d531949",
     .encoded_size = 92380u,
     .decoded_size = 695821u,
     .runtime_base = 0x0C900000u,
     .entry_offset = 0xB60u,
     .prerequisites = ScenarioPrerequisiteNone,
     .provider_kind = ScenarioProviderKind::Tutorial,
     .static_identity_proven = true,
     .entry_shape_proven = true,
     .provider_abi_proven = true},
}};
constexpr auto kScenarioDescriptors = [] {
    std::array<ScenarioDescriptor, kGeneratedActionStageScenarios.size() +
                                   kGeneratedDiagnosticScenarios.size() +
                                   kStaffrollScenarios.size() +
                                   kEventPreviewScenarios.size()> rows{};
    std::size_t index = 0u;
    for (const auto& row : kGeneratedActionStageScenarios) rows[index++] = row;
    for (const auto& row : kGeneratedDiagnosticScenarios) rows[index++] = row;
    for (const auto& row : kStaffrollScenarios) rows[index++] = row;
    for (const auto& row : kEventPreviewScenarios) rows[index++] = row;
    return rows;
}();
constexpr auto kScenarioGroups = [] {
    std::array<ActionStageCharacterRange, kActionStageCharacters.size() + 3u> groups{};
    std::copy(kActionStageCharacters.begin(), kActionStageCharacters.end(), groups.begin());
    groups[kActionStageCharacters.size()] = {
        "diagnostics-sonic", "Sonic / Diagnostics", kGeneratedActionStageScenarios.size(),
        kGeneratedDiagnosticScenarios.size()};
    groups[kActionStageCharacters.size() + 1u] = {
        "credits", "Tutorial / Credits",
        kGeneratedActionStageScenarios.size() + kGeneratedDiagnosticScenarios.size(),
        kStaffrollScenarios.size()};
    groups.back() = {"events", "Events", kGeneratedActionStageScenarios.size() +
                     kGeneratedDiagnosticScenarios.size() + kStaffrollScenarios.size(),
                     kEventPreviewScenarios.size()};
    return groups;
}();

[[nodiscard]] constexpr bool action_stage_catalog_is_launch_authorized()
    noexcept {
    std::size_t expected_first = 0u;
    std::size_t tuple_override_count = 0u;
    for (const auto& character : kActionStageCharacters) {
        if (character.first != expected_first || character.count == 0u ||
            character.first + character.count > kScenarioDescriptors.size())
            return false;
        for (std::size_t index = 0u; index < character.count; ++index) {
            const auto& descriptor =
                kScenarioDescriptors[character.first + index];
            if (!descriptor.static_identity_proven ||
                !descriptor.entry_shape_proven ||
                !descriptor.provider_abi_proven ||
                !descriptor.stage_minor_proven ||
                descriptor.provider_kind != ScenarioProviderKind::StageLoader ||
                descriptor.route_order != index ||
                descriptor.stage_major == 0u ||
                descriptor.stage_major == 11u ||
                descriptor.stage_major > 12u ||
                descriptor.id.find("candidate") != std::string_view::npos)
                return false;
            if (descriptor.stage_tuple_override) {
                if (descriptor.stage_major != 12u) return false;
                ++tuple_override_count;
            }
        }
        expected_first += character.count;
    }
    return expected_first == kGeneratedActionStageScenarios.size() &&
           tuple_override_count == 3u;
}

static_assert(action_stage_catalog_is_launch_authorized());

struct LauncherState final {
    const void* title_state = nullptr;
    std::uint64_t last_frame = 0u;
    std::array<std::uint32_t, 4u> previous_buttons{};
    std::array<std::size_t, kScenarioGroups.size()> selected_by_character{};
    std::size_t character = 0u;
    std::uint32_t failed_launches = 0u;
    bool open = false;
    bool previous_ctrl_f10 = false;
    bool previous_key_up = false;
    bool previous_key_down = false;
    bool previous_key_left = false;
    bool previous_key_right = false;
    bool previous_key_accept = false;
    bool previous_key_cancel = false;
    std::size_t automatic_scenario = 0u;
    bool automatic_scenario_pending = false;
    std::string_view status =
        "Ctrl+F10: open evidence-bound scenario select";
};

thread_local LauncherState launcher_state;
thread_local bool automatic_scenario_consumed = false;

constexpr std::uint32_t kTitleButtonB = 1u << 1u;
constexpr std::uint32_t kTitleButtonA = 1u << 2u;
constexpr std::uint32_t kTitleButtonUp = 1u << 4u;
constexpr std::uint32_t kTitleButtonDown = 1u << 5u;
constexpr std::uint32_t kTitleButtonLeft = 1u << 6u;
constexpr std::uint32_t kTitleButtonRight = 1u << 7u;
// The private menu is keyboard-only to open. No Share/Create/View shortcut.

[[nodiscard]] bool ctrl_f10_down() noexcept {
#if defined(_WIN32)
    return (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
           (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
#else
    return sonic::input::key_down(0x11) && sonic::input::key_down(0x79);
#endif
}

[[nodiscard]] bool key_down(const int key) noexcept {
#if defined(_WIN32)
    return (GetAsyncKeyState(key) & 0x8000) != 0;
#else
    return sonic::input::key_down(static_cast<unsigned>(key));
#endif
}

[[nodiscard]] bool edge(
    const std::uint32_t current,
    const std::uint32_t previous,
    const std::uint32_t mask) noexcept {
    return (current & mask) != 0u && (previous & mask) == 0u;
}

[[nodiscard]] const ScenarioDescriptor* environment_scenario() noexcept {
    if (automatic_scenario_consumed) return nullptr;
    const auto* requested = std::getenv("KATANA_SONIC_PRIVATE_SCENARIO");
    if (!requested) return nullptr;
    const auto length = std::strlen(requested);
    if (length == 0u || length >= 64u) return nullptr;
    const std::string_view id(requested, length);
    const auto descriptors = scenario_descriptors();
    const auto found =
        std::ranges::find(descriptors, id, &ScenarioDescriptor::id);
    return found == descriptors.end() ? nullptr : &*found;
}

void reset_launcher(const void* const title_state) noexcept {
    launcher_state = {};
    launcher_state.title_state = title_state;
    launcher_state.status =
        "Ctrl+F10: open evidence-bound scenario select";
    if (const auto* const requested = environment_scenario();
        requested != nullptr) {
        const auto descriptors = scenario_descriptors();
        launcher_state.automatic_scenario = static_cast<std::size_t>(
            requested - descriptors.data());
        launcher_state.automatic_scenario_pending = true;
    }
}

} // namespace

std::span<const ScenarioDescriptor> scenario_descriptors() noexcept {
    return kScenarioDescriptors;
}

bool scenario_menu_open(
    katana::runtime::NativePortContext& context) noexcept {
    if (context.title_state == nullptr) return false;
    if (launcher_state.title_state != context.title_state ||
        context.frame_index < launcher_state.last_frame)
        reset_launcher(context.title_state);
    return launcher_state.open;
}

void scenario_tick(
    katana::runtime::NativePortContext& context,
    const std::span<const std::uint32_t> title_buttons,
    const ScenarioProvider& provider) noexcept {
    if (context.title_state == nullptr || title_buttons.empty()) return;
    if (launcher_state.title_state != context.title_state ||
        context.frame_index < launcher_state.last_frame)
        reset_launcher(context.title_state);
    launcher_state.last_frame = context.frame_index;

    const auto current = title_buttons.front();
    const auto previous = launcher_state.previous_buttons.front();
    const auto hotkey = ctrl_f10_down();
    const auto hotkey_edge = hotkey && !launcher_state.previous_ctrl_f10;
    launcher_state.previous_ctrl_f10 = hotkey;
    launcher_state.previous_buttons.front() = current;

    if (hotkey_edge) {
        launcher_state.open = !launcher_state.open;
        launcher_state.status = launcher_state.open
                                    ? "Left/Right: group  Up/Down: scenario  A: launch"
                                    : "Ctrl+F10: open evidence-bound scenario select";
    }
    const auto descriptors = scenario_descriptors();
    if (descriptors.empty()) return;
    if (launcher_state.automatic_scenario_pending &&
        launcher_state.automatic_scenario < descriptors.size() &&
        provider.launch != nullptr) {
        const auto& descriptor =
            descriptors[launcher_state.automatic_scenario];
        const auto automatic_ready = provider.automatic_ready != nullptr
                                         ? provider.automatic_ready
                                         : provider.ready;
        const bool ready = automatic_ready == nullptr ||
                           automatic_ready(context, descriptor);
        if (ready) {
            launcher_state.automatic_scenario_pending = false;
            if (provider.launch(context, descriptor)) {
                automatic_scenario_consumed = true;
                launcher_state.open = false;
                launcher_state.status = descriptor.label;
                return;
            }
            launcher_state.status = "Automatic scenario launch failed";
            if (launcher_state.failed_launches !=
                (std::numeric_limits<std::uint32_t>::max)())
                ++launcher_state.failed_launches;
        }
    }
    if (!launcher_state.open) return;
    const bool keyboard_up = key_down(VK_UP);
    const bool keyboard_down = key_down(VK_DOWN);
    const bool keyboard_left = key_down(VK_LEFT);
    const bool keyboard_right = key_down(VK_RIGHT);
    const bool keyboard_accept = key_down(VK_RETURN);
    const bool keyboard_cancel = key_down(VK_ESCAPE);
    const bool up_edge = keyboard_up && !launcher_state.previous_key_up;
    const bool down_edge =
        keyboard_down && !launcher_state.previous_key_down;
    const bool left_edge =
        keyboard_left && !launcher_state.previous_key_left;
    const bool right_edge =
        keyboard_right && !launcher_state.previous_key_right;
    const bool accept_edge =
        keyboard_accept && !launcher_state.previous_key_accept;
    const bool cancel_edge =
        keyboard_cancel && !launcher_state.previous_key_cancel;
    launcher_state.previous_key_up = keyboard_up;
    launcher_state.previous_key_down = keyboard_down;
    launcher_state.previous_key_left = keyboard_left;
    launcher_state.previous_key_right = keyboard_right;
    launcher_state.previous_key_accept = keyboard_accept;
    launcher_state.previous_key_cancel = keyboard_cancel;

    const bool controller_down = edge(current, previous, kTitleButtonDown);
    const bool controller_up = edge(current, previous, kTitleButtonUp);
    const bool controller_left = edge(current, previous, kTitleButtonLeft);
    const bool controller_right = edge(current, previous, kTitleButtonRight);
    const bool character_left = left_edge || controller_left;
    const bool character_right = right_edge || controller_right;
    const bool navigated = down_edge || up_edge || controller_down ||
                           controller_up || character_left || character_right;
    if (character_right)
        launcher_state.character =
            (launcher_state.character + 1u) % kScenarioGroups.size();
    if (character_left)
        launcher_state.character = launcher_state.character == 0u
                                       ? kScenarioGroups.size() - 1u
                                       : launcher_state.character - 1u;
    const auto& character = kScenarioGroups[launcher_state.character];
    auto& selected =
        launcher_state.selected_by_character[launcher_state.character];
    if (selected >= character.count) selected = 0u;
    if (down_edge || controller_down)
        selected = (selected + 1u) % character.count;
    if (up_edge || controller_up)
        selected = selected == 0u ? character.count - 1u : selected - 1u;
    if (cancel_edge || edge(current, previous, kTitleButtonB)) {
        launcher_state.open = false;
        launcher_state.status = "Scenario menu closed";
    }
    // A direction and confirm can arrive in one host sample when the player
    // rolls the D-pad into Cross/A.  Never launch the newly selected row
    // before it has been presented for a full modal frame.
    if (!navigated &&
        (accept_edge || edge(current, previous, kTitleButtonA)) &&
        provider.launch != nullptr) {
        const auto& descriptor =
            descriptors[character.first + selected];
        const bool ready = provider.ready == nullptr ||
                           provider.ready(context, descriptor);
        if (ready && provider.launch(context, descriptor)) {
            launcher_state.open = false;
            launcher_state.status = descriptor.label;
        } else {
            launcher_state.status = "Unavailable in the current game state";
            if (launcher_state.failed_launches !=
                (std::numeric_limits<std::uint32_t>::max)())
                ++launcher_state.failed_launches;
        }
    }

    if (!launcher_state.open || provider.draw == nullptr) return;
    ScenarioOverlayView view;
    view.open = true;
    view.character = character.label;
    view.selected = selected;
    view.failed_launches = launcher_state.failed_launches;
    view.status = launcher_state.status;
    const auto first_visible = selected < view.lines.size()
                                   ? 0u : selected - view.lines.size() + 1u;
    view.line_count = (std::min)(view.lines.size(), character.count - first_visible);
    for (std::size_t index = 0u; index < view.line_count; ++index) {
        const auto descriptor_index = character.first + first_visible + index;
        const auto& descriptor = descriptors[descriptor_index];
        view.lines[index] = {
            descriptor.label,
            first_visible + index == selected,
            provider.ready != nullptr && provider.ready(context, descriptor)};
    }
    provider.draw(context, view);
}

} // namespace sonic_native_private
