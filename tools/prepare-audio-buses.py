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
        '#include "katana/runtime/native_port_content.hpp"\n#include "native_port_audio_execution_domain.hpp"\n#include "sonic_audio_settings.hpp"\n#include "sonic_sound_commands.hpp"')
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
# Keep every existing wire opcode stable. Only the port's tightly scoped void
# facade may request this command; normal SDK callers still receive a handle.
replace('    RestoreDevelopmentState,\n};',
        '    RestoreDevelopmentState,\n    SonicMidiNoteOnDeferred = 0x7001u,\n};')
replace('    void midi_note_off(const NativePortSoundMidiPortHandle port,\n',
        '''    void sonic_midi_note_on_deferred(const NativePortSoundMidiPortHandle port,
        const std::uint8_t note, const std::uint8_t velocity) {
        require_owner_thread();
        if (audio_.command_queue_snapshot().mode == NativePortAudioCommandQueueMode::SerialReference) {
            static_cast<void>(midi_note_on(port, note, velocity));
            return;
        }
        SoundBankCommand command;
        command.port = port; command.value8_0 = note; command.value8_1 = velocity;
        call_async(SoundBankOpcode::SonicMidiNoteOnDeferred, command);
        ++sonic::audio::sound_command_counts.deferred_notes;
    }
    void midi_note_off(const NativePortSoundMidiPortHandle port,
''')
replace('        case SoundBankOpcode::MidiNoteOn:\n',
        '''        case SoundBankOpcode::SonicMidiNoteOnDeferred:
            static_cast<void>(core_->midi_note_on(c.port, c.value8_0, c.value8_1)); return;
        case SoundBankOpcode::MidiNoteOn:
''')
replace('    return impl_->midi_note_on(port, note, velocity);',
        '''    if (sonic::audio::deferred_note_requested(this)) {
        impl_->sonic_midi_note_on_deferred(port, note, velocity);
        return {};
    }
    return impl_->midi_note_on(port, note, velocity);''')

# Metadata belongs to the parsed collection, not playback. Retain both positive
# and negative answers, with complete handle identity, only on the producer.
replace('        return call<std::uint8_t>(SoundBankOpcode::HasProgram, command) != 0u;',
        '''        return sonic_metadata_query(collection, 0u, bank, program, [&] {
            return call<std::uint8_t>(SoundBankOpcode::HasProgram, command) != 0u;
        });''')
replace('        return call<std::uint8_t>(SoundBankOpcode::HasSequence, command) != 0u;',
        '''        return sonic_metadata_query(collection, 1u, bank, sequence, [&] {
            return call<std::uint8_t>(SoundBankOpcode::HasSequence, command) != 0u;
        });''')
replace('        const auto content =\n            materialize_sound_bank_content(platform_, binding, config_);',
        '        sonic_invalidate_metadata();\n        const auto content =\n            materialize_sound_bank_content(platform_, binding, config_);')
replace('    void unload_collection(const NativePortSoundCollectionHandle value) {\n',
        '    void unload_collection(const NativePortSoundCollectionHandle value) {\n        sonic_invalidate_metadata();\n')
replace('        call_void(SoundBankOpcode::UnloadAllCollections);',
        '        sonic_invalidate_metadata();\n        call_void(SoundBankOpcode::UnloadAllCollections);')
replace('    void reset() { call_void(SoundBankOpcode::Reset); }',
        '    void reset() { sonic_invalidate_metadata(); call_void(SoundBankOpcode::Reset); }')
replace('        stage_development_state(state);\n        try {\n            call_void(SoundBankOpcode::RestoreDevelopmentState);',
        '        sonic_invalidate_metadata();\n        stage_development_state(state);\n        try {\n            call_void(SoundBankOpcode::RestoreDevelopmentState);')
replace('    std::unique_ptr<Core> core_;',
        '''    std::unique_ptr<Core> core_;
    mutable sonic::audio::SoundMetadataCache sonic_metadata_;
    std::atomic<bool> sonic_metadata_worker_mutation_{false};
    mutable std::shared_ptr<NativePortAudioExecutionDomain> sonic_metadata_domain_;
    mutable bool sonic_metadata_domain_unavailable_ = false;
    void sonic_invalidate_metadata() {
        require_owner_thread();
        // The title mutates collections on the producer. If a different user
        // invokes the public API inline on the worker, retire this optimization
        // permanently rather than sharing a producer-owned cache across threads.
        if (audio_.on_audio_thread()) sonic_metadata_worker_mutation_.store(true);
        else sonic_metadata_.clear();
    }
    template<class Query> bool sonic_metadata_query(NativePortSoundCollectionHandle collection,
            std::uint8_t kind, std::uint8_t bank, std::uint16_t item, Query&& query) const {
        require_owner_thread();
        if (!sonic::audio::metadata_cache_enabled() || audio_.on_audio_thread() ||
                sonic_metadata_worker_mutation_.load() || sonic_metadata_domain_unavailable_) return query();
        if (!sonic_metadata_domain_) {
            auto config = NativePortAudioExecutionDomainConfig{};
            config.command_queue.mode = audio_.command_queue_snapshot().mode;
            if (config.command_queue.mode == NativePortAudioCommandQueueMode::SerialReference) return query();
            // AudioEngine already owns the process domain. The title uses the
            // standard queue configuration; other API users may customize it.
            // A mismatch merely retires this optimization, never their engine.
            try { sonic_metadata_domain_ = NativePortAudioExecutionDomain::acquire(config); }
            catch (...) { sonic_metadata_domain_unavailable_ = true; return query(); }
        }
        const auto domain = sonic_metadata_domain_->snapshot();
        const auto& queue = domain.queue;
        // Domain errors (including stamp overflow) can precede queue failure.
        // Follow the original query/error boundary for both kinds of failure.
        if (domain.first_error != NativePortAudioExecutionDomainFailure::None ||
                domain.shutdown_requested) return query();
        if (queue.lifecycle != NativePortAudioCommandQueueLifecycle::Running ||
                queue.mode == NativePortAudioCommandQueueMode::SerialReference ||
                queue.first_error != NativePortAudioCommandQueueFailure::None) return query();
        return sonic_metadata_.read(collection, kind, bank, item, query);
    }''')

destination.mkdir(parents=True, exist_ok=True)
path = destination / "native_port_sound_bank.cpp"
if not path.exists() or path.read_text(encoding="utf-8") != source:
    path.write_text(source, encoding="utf-8")
