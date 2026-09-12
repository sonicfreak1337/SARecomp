"""Port-local per-program bus gain; retain the pinned SoundBank public/state ABI."""
import hashlib
import sys
import zipfile
from pathlib import Path

archive, destination = map(Path, sys.argv[1:3])
with zipfile.ZipFile(archive) as sdk:
    data = sdk.read("src/runtime/native_port_sound_bank.cpp")
if hashlib.sha256(data).hexdigest() != "645484cf8e751a9a85a73607e21817f62ff6107211ac4dcaf8ebeb79011e0655":
    raise RuntimeError("Pinned SoundBank source changed; review required")
source = data.decode("utf-8")

def replace(before, after):
    global source
    if source.count(before) != 1:
        raise RuntimeError("Pinned SoundBank layout changed: " + before[:90])
    source = source.replace(before, after)

replace('#include "katana/runtime/native_port_content.hpp"',
        '#include "katana/runtime/native_port_content.hpp"\n#include "sonic_audio_settings.hpp"')
replace('    struct SynthVoice final {',
        '    struct SynthVoice final {\n        sonic::audio::Bus sonic_bus = sonic::audio::Bus::Master;')
replace('                    voice.split = &split;',
        '                    voice.split = &split;\n'
        '                    voice.sonic_bus = sonic::audio::program_bus(\n'
        '                        collection.logical_id, bank_index, program_index);')
# Derived host policy is deliberately not serialized. Recompute from the same
# validated coordinates already used to restore sample/program ownership.
replace('            value.sample = sample->second; value.split = split;',
        '            value.sample = sample->second; value.split = split;\n'
        '            value.sonic_bus = sonic::audio::program_bus(\n'
        '                collection.logical_id, location[0], location[1]);')
replace('    void render_block(const std::uint32_t frames) {',
        '    void render_block(const std::uint32_t frames) {\n'
        '        const auto& sonic_settings = sonic::presentation::settings();\n'
        '        const std::array<float, 4> sonic_bus_gain{\n'
        '            1.0f, sonic_settings.music_volume * .01f,\n'
        '            sonic_settings.voice_volume * .01f,\n'
        '            sonic_settings.effects_volume * .01f};')
replace('                                               external_gain,\n                                               external_pitch_bend);',
        '                                               external_gain * sonic_bus_gain[static_cast<unsigned>(voice.sonic_bus)],\n'
        '                                               external_pitch_bend);')
# Master/background muting also covers accumulated DSP/reverb tails, after
# their authored arithmetic and before AudioEngine's one final saturation.
replace('            const auto destination = static_cast<std::size_t>(rendered) * 2u;\n',
        '            const auto destination = static_cast<std::size_t>(rendered) * 2u;\n'
        '            const auto sonic_master = sonic::audio::factor(sonic::audio::Bus::Master);\n')
replace('                    mix_[sample];', '                    mix_[sample] * sonic_master;')

# Every collection still advances its authored DSP, including silent banks
# and reverb tails. Only redundant input conversion and immutable routing
# work are removed. Preserve separate factors and multiplication order.
replace('            const auto value = collection.effect_sends[index];\n',
        '            const auto value = collection.effect_sends[index];\n'
        '            if (value == 0.0) {\n'
        '                collection.effect_kernel_inputs[index] = 0;\n'
        '                continue;\n            }\n')
replace('        for (std::uint32_t frame = 0u; frame < frames; ++frame) {\n'
        '            const auto bus_base =\n',
        '        struct SonicRoute { double left, right, level; };\n'
        '        std::array<SonicRoute, maximum_effect_buses> sonic_routes;\n'
        '        for (std::size_t output = 0; output < maximum_effect_buses; ++output) {\n'
        '            const auto& route = collection.effect_outputs[output];\n'
        '            const auto [left, right] = aica_pan_gains(route.pan);\n'
        '            sonic_routes[output] = {left, right, aica_send_gain(route.level)};\n'
        '        }\n'
        '        for (std::uint32_t frame = 0u; frame < frames; ++frame) {\n'
        '            const auto bus_base =\n')
replace('                const auto& route = collection.effect_outputs[output];\n'
        '                const auto [left, right] = aica_pan_gains(route.pan);\n'
        '                const auto level = aica_send_gain(route.level);\n',
        '                const auto [left, right, level] = sonic_routes[output];\n')
destination.mkdir(parents=True, exist_ok=True)
path = destination / "native_port_sound_bank.cpp"
if not path.exists() or path.read_text(encoding="utf-8") != source:
    path.write_text(source, encoding="utf-8")
