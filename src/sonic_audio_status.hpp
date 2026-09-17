#pragma once

#include "katana/runtime/native_port_audio_engine.hpp"
#include <memory>

namespace sonic::audio {

// Title-thread reads never wait for an audio command. Membership changes are
// ordered with voice creation/destruction on the existing audio consumer.
class StatusPublisher final {
public:
    struct Status {
        katana::runtime::NativePortAudioVoiceState state{};
        std::uint64_t mixed_output_frames{};
        std::uint64_t played_output_frames{};
    };
    StatusPublisher(katana::runtime::NativePortAudioEngine& engine,
                    const katana::runtime::NativePortAudioEngineConfig& config);
    ~StatusPublisher();
    StatusPublisher(const StatusPublisher&) = delete;
    StatusPublisher& operator=(const StatusPublisher&) = delete;

    void watch(katana::runtime::NativePortAudioVoiceHandle voice);
    void unwatch(katana::runtime::NativePortAudioVoiceHandle voice);
    [[nodiscard]] Status read(katana::runtime::NativePortAudioVoiceHandle voice);
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] bool async_status_enabled() noexcept;

}
