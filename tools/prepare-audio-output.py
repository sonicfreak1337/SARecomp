"""Port-local endpoint recovery; retain the exact SDK facade/audio-domain ABI."""
import hashlib
import sys
import zipfile
from pathlib import Path

archive, destination, extension = map(Path, sys.argv[1:4])
expected = {
    "native_port_audio.cpp": "0153b9d08fcecf5aba5ddf8872ae2e6342d1f6330c737684c0487bdebac71499",
    "native_port_audio_execution_domain.hpp": "8b7b7f540098f2cf7afd3a1333ade9f01b751f683860259d2cd42874fe401a44",
}
with zipfile.ZipFile(archive) as sdk:
    sources = {name: sdk.read("src/runtime/"+name) for name in expected}
for name, data in sources.items():
    if hashlib.sha256(data).hexdigest() != expected[name]:
        raise RuntimeError("Pinned audio source changed: "+name)
source = sources["native_port_audio.cpp"].decode()
def replace(before, after):
    global source
    if source.count(before) != 1:
        raise RuntimeError("Pinned audio boundary changed: "+before[:90])
    source = source.replace(before, after)

replace('#include "native_port_audio_execution_domain.hpp"',
        '#include "native_port_audio_execution_domain.hpp"\n#include "sonic_audio_device.hpp"\n#include <cstdio>')
# Extract the original pool/format initialization as one reusable function.
start = source.index("        completion_wake_ = std::make_unique<CompletionWakeState>();")
end = source.index('\n#else\n        fail(1u, "unsupported-host");', start)
initialize = source[start:end]
initialize = initialize.replace("&execution_domain, std::memory_order_release", "execution_domain_, std::memory_order_release")
initialize = initialize.replace('        if (result != MMSYSERR_NOERROR) fail(result, "open");',
    '        if (result != MMSYSERR_NOERROR) { device_=nullptr; report_device(result,"open"); }\n'
    '        device_base_=played_frames_; device_position_epoch_=0; last_device_position_raw_=0;\n'
    '        device_position_initialized_=false; silent_time_=::sonic::audio_device::now(); silent_fraction_=0;\n'
    '        retry_at_=silent_time_+1\'000\'000\'000; seen_change_=::sonic::audio_device::changes.load();\n'
    '        output_status_.set(device_?::sonic::recovery::OutputState::Connected : ::sonic::recovery::OutputState::Silent);')
source = source[:start]+'        execution_domain_=&execution_domain;\n        initialize_device();'+source[end:]
replace('    [[nodiscard]] bool submit(const std::span<const std::int16_t> samples) {',
        '    [[nodiscard]] bool submit(const std::span<const std::int16_t> samples) {')
replace('        PendingBlock pending;', '        PendingBlock pending;')
replace('        if (pending.block->prepared &&\n', '        if (device_ && pending.block->prepared &&\n')
replace('                fail(unprepare, "resize-unprepare");',
        '                recover_endpoint(unprepare,"resize-unprepare",false);\n                return submit(samples);')
replace('        auto result = submitted.prepared\n',
        '        if (device_) {\n        auto result = submitted.prepared\n')
replace('            fail(result, "prepare");',
        '            recover_endpoint(result,"prepare",false);\n            return submit(samples);')
replace('            fail(result, "submit");',
        '            recover_endpoint(result,"submit",false);\n            return submit(samples);')
replace('        saturating_add(submitted_buffers_, 1u);',
        '        saturating_add(submitted_buffers_, 1u);')
replace('        }\n#endif\n        saturating_add(submitted_buffers_, 1u);',
        '        }\n        } // connected endpoint; silent mode retains the same PCM queue\n#endif\n        saturating_add(submitted_buffers_, 1u);')
replace('        for (auto& pending : pending_) {\n            auto& block = *pending.block;',
        '        service_device();\n        if(!device_) update_silent_position();\n'
        '        for (auto& pending : pending_) {\n            auto& block = *pending.block;\n'
        '            if(!device_ && completed_frames_+block.frames<=played_frames_)block.header.dwFlags|=WHDR_DONE;')
replace('                if (result != MMSYSERR_NOERROR) fail(result, "unprepare");',
        '                if (result != MMSYSERR_NOERROR) {recover_endpoint(result,"unprepare",false);return;}')
replace('        update_playback_position();',
        '        service_device();\n        if(device_)update_playback_position();else update_silent_position();')
replace('        const auto result = waveOutPause(device_);\n        if (result != MMSYSERR_NOERROR) fail(result, "pause");',
        '        service_device();\n        if(device_){const auto result=waveOutPause(device_);if(result!=MMSYSERR_NOERROR)recover_endpoint(result,"pause",false);}\n'
        '        else update_silent_position();')
replace('        const auto result = waveOutRestart(device_);\n        if (result != MMSYSERR_NOERROR) fail(result, "resume");',
        '        service_device();\n        if(device_){const auto result=waveOutRestart(device_);if(result!=MMSYSERR_NOERROR)recover_endpoint(result,"resume",false);}\n'
        '        silent_time_=::sonic::audio_device::now();')
replace('        if (result != MMSYSERR_NOERROR) fail(result, "position");',
        '        if (result != MMSYSERR_NOERROR) {recover_endpoint(result,"position",false);return;}')
replace('        const auto device_frames = device_position_epoch_ + raw;',
        '        const auto device_frames = device_base_ + device_position_epoch_ + raw;')
replace('                if (!pending.owned || !pending.block->prepared) continue;',
        '                if (!pending.owned || !pending.block || !pending.block->prepared) continue;')
replace('            const auto close_result = waveOutClose(device_);',
        '            const auto close_result = waveOutClose(device_);\n            close_failed_=close_result!=MMSYSERR_NOERROR;')
replace('        if (error != 0u)\n            throw std::runtime_error("native-port-audio-stop:" + std::to_string(error));',
        '        if(error)report_device(error,"stop-device-error");\n'
        '        state_=NativePortAudioState::Stopped;error_code_=0;\n'
        '        output_status_.set(::sonic::recovery::OutputState::Idle);')
replace('    void require_owner_thread() const {',
        '#ifdef _WIN32\n    void initialize_device() {\n'+initialize+'\n    }\n'+extension.read_text()+'\n#endif\n\n    void require_owner_thread() const {')
replace('    HWAVEOUT device_ = nullptr;',
        '    ::sonic::audio_device::Watch device_watch_;\n'
        '    ::sonic::recovery::OutputStatus output_status_;\n'
        '    NativePortAudioExecutionDomain* execution_domain_=nullptr;\n'
        '    std::uint64_t device_base_=0, silent_time_=0, retry_at_=0, seen_change_=0, seen_resume_=0;\n'
        '    double silent_fraction_=0;\n'
        '    bool recovering_=false, permanently_silent_=false, close_failed_=false;\n'
        '    HWAVEOUT device_ = nullptr;')
# All WinMM calls stay at the original audio-domain owner. The substitution
# seam also lets tests inject removal/error/reset without touching real devices.
for name, method in (("waveOutOpen","open"),("waveOutClose","close"),("waveOutReset","reset"),
                     ("waveOutPrepareHeader","prepare"),("waveOutUnprepareHeader","unprepare"),
                     ("waveOutWrite","write"),("waveOutGetPosition","position"),
                     ("waveOutPause","pause"),("waveOutRestart","restart")):
    source = source.replace(name+"(", "::sonic::audio_device::api()."+method+"(")
sources["native_port_audio.cpp"] = source.encode()
destination.mkdir(parents=True, exist_ok=True)
for name, data in sources.items():
    target=destination/name
    if not target.exists() or target.read_bytes()!=data:
        target.write_bytes(data)
print("SONIC_AUDIO_OUTPUT_READY sdk_source_verified=1 facade_and_domain=retained")
