"""Bind one port-local DualSense correction to the exact retained SDK source."""
import hashlib
import sys
import zipfile
from pathlib import Path

archive, destination = Path(sys.argv[1]), Path(sys.argv[2])
expected = {
    "native_port_platform.cpp": "ec90e85fe3bb38625295f9f1fa79ebfab31bab7ca7bcc09faef5668f1b4eea22",
    "native_port_input_policy.hpp": "d2ac1e94a926449ddbb17366e69d10734d808a0b1c4fc567b8e1f99382c64137",
}
with zipfile.ZipFile(archive) as sdk:
    sources = {name: sdk.read("src/runtime/"+name) for name in expected}
for name, value in sources.items():
    if hashlib.sha256(value).hexdigest() != expected[name]:
        raise RuntimeError("Pinned camera platform source changed: "+name)
source = sources["native_port_platform.cpp"].decode("utf-8")
anchor = "#include \"native_port_input_policy.hpp\""
if source.count(anchor) != 1:
    raise RuntimeError("Camera platform include layout changed")
source = source.replace(anchor, anchor+'\n#include "sonic_camera_input.hpp"\n#include "sonic_presentation.hpp"\n#include "sonic_input.hpp"\n#include "sonic_rumble.hpp"')
anchor = "            if ((capabilities.wCaps & JOYCAPS_HASZ) != 0u &&\n"
if source.count(anchor) != 1:
    raise RuntimeError("Camera platform Sony axis layout changed")
correction = """            // Port-local opt-in correction, after identity-bound Sony admission.
            // XInput, Original camera, movement and buttons retain the pinned
            // SDK paths. Sony Z must never also become a phantom trigger.
            if (identity->kind == NativeGamepadSourceKind::DualSense &&
                ::sonic::presentation::settings().camera_style == ::sonic::camera::Style::Recompiled) {
                const auto axes = (capabilities.wCaps & (JOYCAPS_HASZ | JOYCAPS_HASR)) == (JOYCAPS_HASZ | JOYCAPS_HASR)
                    ? ::sonic::camera::dualsense_right_stick(info.dwZpos, info.dwRpos,
                        capabilities.wZmin, capabilities.wZmax, capabilities.wRmin, capabilities.wRmax)
                    : ::sonic::camera::RawStick{};
                destination.right_stick_x_raw = axes.x;
                destination.right_stick_y_raw = axes.y;
                static bool reported = false;
                if (!reported) {
                    std::fprintf(stderr, "SONIC_CAMERA_CONTROLLER source=dualsense axes=Z/R raw=%d,%d\\n", int(axes.x), int(axes.y));
                    reported = true;
                }
            }
"""
# DualSense Z is the proven right-stick X axis, not a combined trigger.
# Do not guess U/V semantics from capability flags. Dedicated analog trigger
# admission needs identity-bound endpoint evidence; existing digital shoulder
# bindings and all XInput/DualShock paths remain untouched.
trigger_guard = ("            if (identity->kind != NativeGamepadSourceKind::DualSense &&\n"
                 "                (capabilities.wCaps & JOYCAPS_HASZ) != 0u &&\n")
source = source.replace(anchor, correction+trigger_guard)
def replace_once(before, after):
    global source
    if source.count(before) != 1:
        raise RuntimeError("Pinned input source boundary changed: "+before[:72])
    source = source.replace(before, after)

# Physical discovery and slot retention stay in the SDK. Port-owned keyboard
# and remapping run once above this boundary, avoiding a second injected pad.
replace_once('        const auto gameplay_keyboard_enabled = keyboard_controls_ != nullptr &&\n            keyboard_controls_->enabled.load(std::memory_order_acquire);\n        const auto keyboard = keyboard_gamepad_state(gameplay_keyboard_enabled);\n        if (candidates.empty()) {',
    '        const bool gameplay_keyboard_enabled = false;\n        const detail::NativeKeyboardGamepadState keyboard{};\n        if (false && candidates.empty()) {')
replace_once('            const auto& previous = input_snapshot_.gamepads[slot];\n            if (placement[slot].has_value()) {',
    '            const auto& previous = input_snapshot_.gamepads[slot];\n'
    '            const bool sony = placement[slot].has_value() &&\n'
    '                (candidates[*placement[slot]].kind == NativeGamepadSourceKind::DualSense || candidates[*placement[slot]].kind == NativeGamepadSourceKind::DualShock);\n'
    '            ::sonic::input::note_controller(static_cast<unsigned>(slot),sony,new_device_id != 0u);\n'
    '            if (placement[slot].has_value()) {')
for field in ('low_frequency','high_frequency'):
    replace_once('vibration.'+field+" * 65'535.0f",'vibration.'+field+" * (::sonic::presentation::settings().vibration / 100.0f) * 65'535.0f")


# A paused host dialog must not advance the guest replay cursor or append
# physical menu navigation to its recording. Retain a separate physical
# snapshot so a live poll during replay cannot inherit recorded pad state.
initialization = '''            last_joystick_count_ = joyGetNumDevs();
            const auto identities = enumerate_native_joystick_identities();
            joystick_identities_ = identities.has_value()
                                       ? std::move(*identities)
                                       : std::vector<NativeJoystickIdentity>{};
            start_joystick_identity_worker(!identities.has_value());'''
replace_once('        if (!input_replay_mode_) {\n'+initialization+'\n        }',
             '        if (!input_replay_mode_) initialize_physical_input();')
replace_once('    [[nodiscard]] NativePortInputSnapshot poll_gamepads() {\n        require_owner_thread();\n        if (input_initial_state_pending_) {',
    '    void initialize_physical_input() {\n'
    '        if (physical_input_initialized_) return;\n'+initialization+'\n'
    '        physical_input_initialized_ = true;\n    }\n\n'
    '    [[nodiscard]] NativePortInputSnapshot poll_gamepads() {\n        require_owner_thread();\n'
    '        const bool host_poll = ::sonic::input::host_poll_active();\n'
    '        if (input_initial_state_pending_ && !host_poll) {')
replace_once('        if (input_replay_mode_) {\n            input_snapshot_ = input_trace_->next();',
             '        if (input_replay_mode_ && !host_poll) {\n            input_snapshot_ = input_trace_->next();')
replace_once('        std::vector<NativeGamepadCandidate> candidates;\n',
             '        initialize_physical_input();\n        std::vector<NativeGamepadCandidate> candidates;\n')
replace_once('        NativePortInputSnapshot result = input_snapshot_;\n        saturating_increment(result.poll_sequence);',
             '        NativePortInputSnapshot result = physical_input_snapshot_;\n        if (!host_poll) saturating_increment(result.poll_sequence);')
replace_once('            const auto& previous = input_snapshot_.gamepads[slot];',
             '            const auto& previous = physical_input_snapshot_.gamepads[slot];')
replace_once('        input_snapshot_ = result;\n        saturating_increment(telemetry_->snapshot.input_polls);',
             '        physical_input_snapshot_ = result;\n'
             '        if (host_poll) return result;\n'
             '        input_snapshot_ = result;\n        saturating_increment(telemetry_->snapshot.input_polls);')
replace_once('        input_snapshot_ = {};\n',
             '        input_snapshot_ = {};\n        physical_input_snapshot_ = {};\n')
replace_once('    NativePortInputSnapshot input_snapshot_;\n',
             '    NativePortInputSnapshot input_snapshot_;\n'
             '    NativePortInputSnapshot physical_input_snapshot_;\n'
             '    bool physical_input_initialized_ = false;\n')
# Native PuruPuru deadlines cannot depend on game/presentation progress. The
# worker only calls the already-loaded XInput transport, never guest state.
# It is joined before that DLL/owner is destroyed. Hidden tests emit nothing.
replace_once('        if (!input_replay_mode_) initialize_physical_input();\n    }',
    '        if (!input_replay_mode_) initialize_physical_input();\n'
    '        const auto* background = std::getenv("KATANA_PORT_BACKGROUND_TEST");\n'
    '        if (!input_replay_mode_ && !(background && *background && *background != \'0\') && xinput_.set_state)\n'
    '            ::sonic::rumble::engine().attach(&xinput_, [](void* opaque, unsigned endpoint, std::uint16_t low, std::uint16_t high) noexcept {\n'
    '                auto& api = *static_cast<XInputApi*>(opaque);\n'
    '                XINPUT_VIBRATION vibration{low, high};\n'
    '                return api.set_state(endpoint, &vibration) == ERROR_SUCCESS;\n'
    '            });\n    }')
replace_once('        stop_joystick_identity_worker();\n',
    '        ::sonic::rumble::engine().detach(&xinput_);\n        stop_joystick_identity_worker();\n')
replace_once('    void finalize_clean_shutdown() {\n        require_owner_thread();',
    '    void finalize_clean_shutdown() {\n        require_owner_thread();\n        ::sonic::rumble::engine().detach(&xinput_);')
replace_once('        physical_input_snapshot_ = result;\n',
    '        for (unsigned slot = 0; slot < native_port_gamepad_count; ++slot) {\n'
    '            const auto endpoint = vibration_xinput_slot(input_device_ids_[slot]);\n'
    '            ::sonic::rumble::engine().bind(slot, input_device_ids_[slot], endpoint ? int(*endpoint) : -1);\n'
    '        }\n        physical_input_snapshot_ = result;\n')
sources["native_port_platform.cpp"] = source.encode("utf-8")
destination.mkdir(parents=True, exist_ok=True)
for name, value in sources.items():
    target = destination/name
    if not target.exists() or target.read_bytes() != value:
        target.write_bytes(value)
print("SONIC_CAMERA_PLATFORM_READY sdk_source_verified=1 mapping=dualsense_Z_R original_and_xinput=unchanged")
