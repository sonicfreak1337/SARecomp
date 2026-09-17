#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include "sonic_audio_device.hpp"
#endif
#include "sonic_audio_status.hpp"
#include "native_port_audio_execution_domain.hpp"
#include "katana/runtime/native_port_ffmpeg_codec.hpp"
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace katana::runtime;
using Publisher = sonic::audio::StatusPublisher;
using Clock = std::chrono::steady_clock;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejected(F&& f, const char* message) {
    bool caught = false;
    try { f(); } catch (...) { caught = true; }
    check(caught, message);
}
void env(const char* key, const char* value) {
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
}
#ifdef _WIN32
MMRESULT WINAPI unavailable(LPHWAVEOUT, UINT, LPCWAVEFORMATEX, DWORD_PTR, DWORD_PTR, DWORD) {
    return MMSYSERR_NODRIVER;
}
#endif
int main(int argc, char** argv) {
    try {
        check(argc == 2, "fresh test data directory required");
        const auto root = std::filesystem::absolute(argv[1]);
        check(!std::filesystem::exists(root), "test directory must be new");
        std::filesystem::create_directories(root / "content");
        env("KATANA_PORT_BACKGROUND_TEST", "1");
        env("SDL_AUDIODRIVER", "dummy");
#ifdef _WIN32
        sonic::audio_device::Api api;
        api.open = unavailable;
        sonic::audio_device::set_test_api(&api);
        struct Clear { ~Clear() { sonic::audio_device::set_test_api(nullptr); } } clear;
#endif
        NativePortPlatformConfig cfg;
        cfg.content_root = root / "content";
        cfg.user_data_root = root / "data";
        cfg.project_id = "sonic-audio-status-test";
        cfg.require_gamepad_backend = false;
        NativePortPlatformServices platform(cfg);
        NativePortAudioEngineConfig audio_cfg;
        audio_cfg.maximum_voices = 16;
        NativePortAudioEngine audio(platform, native_port_ffmpeg_codec_provider(), audio_cfg);
        auto publisher = std::make_unique<Publisher>(audio, audio_cfg);
        auto domain = NativePortAudioExecutionDomain::acquire();
        const bool serial = domain->mode() == NativePortAudioCommandQueueMode::SerialReference;
        rejected([&] { (void)publisher->read({}); }, "invalid handle accepted");
        std::array<NativePortAudioVoiceHandle, 8> voices;
        for (auto& v : voices) {
            v = audio.create_pcm_feed();
            publisher->watch(v);
            check(publisher->read(v).state == NativePortAudioVoiceState::Ready, "ready publication");
        }
        const auto ninth = audio.create_pcm_feed();
        rejected([&] { publisher->watch(ninth); }, "watch capacity unbounded");
        audio.release(ninth);
        std::thread wrong_owner([&] {
            rejected([&] { (void)publisher->read(voices[0]); }, "foreign producer accepted");
        });
        wrong_owner.join();

        const auto v = voices[0];
        std::vector<std::int16_t> pcm(4410 * 2, 0); // 100 ms of silence.
        check(audio.submit_pcm_s16(v, pcm), "PCM rejected");
        audio.finish_pcm_feed(v);
        audio.play(v);
        publisher->watch(v);
        audio.pause(v);
        publisher->watch(v);
        check(publisher->read(v).state == NativePortAudioVoiceState::Paused, "pause publication");
        audio.resume(v);
        publisher->watch(v);
        check(publisher->read(v).state != NativePortAudioVoiceState::Paused, "resume publication");

        // Status polling itself must not submit a single command or ACK.
        const auto before = domain->snapshot().queue;
        const auto started = Clock::now();
        for (unsigned n = 0; n != 50000; ++n) {
            const auto status = publisher->read(v);
            check(status.played_output_frames <= status.mixed_output_frames, "torn frame counters");
        }
        const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        const auto after = domain->snapshot().queue;
        if (!serial) {
            check(before.submitted_commands == after.submitted_commands, "poll queued commands");
            check(before.ack_waits == after.ack_waits, "poll waited for ACK");
        }

        // No title frame, pump, or synchronous voice query: the worker must
        // still publish the final audible tail and reach PLAY_END semantics.
        auto status = publisher->read(v);
        const auto deadline = Clock::now() + std::chrono::seconds(6);
        while (Clock::now() < deadline &&
               (status.state != NativePortAudioVoiceState::Completed ||
                status.played_output_frames != status.mixed_output_frames)) {
            if (serial) audio.pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            const auto next = publisher->read(v);
            check(next.played_output_frames >= status.played_output_frames, "playback regressed");
            status = next;
        }
        check(status.state == NativePortAudioVoiceState::Completed &&
              status.played_output_frames == 4410 && status.mixed_output_frames == 4410,
              "worker-only completion/tail did not publish");
        for (auto old : voices) {
            publisher->unwatch(old);
            audio.stop(old);
            audio.release(old);
            if (!serial) rejected([&] { (void)publisher->read(old); }, "retired registration survived");
        }
        const auto replacement = audio.create_pcm_feed();
        check(replacement.slot == v.slot && replacement.generation != v.generation, "slot reuse fixture");
        publisher->watch(replacement);
        check(publisher->read(replacement).state == NativePortAudioVoiceState::Ready &&
              publisher->read(replacement).played_output_frames == 0, "stale completed generation reused");
        if (!serial) rejected([&] { (void)publisher->read(v); }, "old handle matched reused slot");
        audio.stop(replacement);
        publisher->watch(replacement);
        check(publisher->read(replacement).state == NativePortAudioVoiceState::Stopped, "stop publication");
        publisher->unwatch(replacement);
        audio.release(replacement);
        publisher.reset();

        // New epoch has no old registrations; terminal cleanup is safe even
        // though domain cleanup visits the engine before this observer.
        publisher = std::make_unique<Publisher>(audio, audio_cfg);
        const auto terminal = audio.create_pcm_feed();
        publisher->watch(terminal);
        domain->shutdown();
        rejected([&] { (void)publisher->read(terminal); }, "terminal failure stayed invisible");
        publisher.reset();
        std::cout << "SONIC_AUDIO_STATUS_OK mode=" << (serial ? "serial" : "dedicated")
                  << " polls=50000 poll_ms=" << elapsed
                  << " queue_delta=" << after.submitted_commands - before.submitted_commands
                  << " lifecycle=exact tail=drained reuse=isolated teardown=clean\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
