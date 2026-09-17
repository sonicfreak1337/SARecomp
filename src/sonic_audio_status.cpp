#include "sonic_audio_status.hpp"
#include "sonic_native_cpu_policy.hpp"
#include "native_port_audio_execution_domain.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>

namespace sonic::audio {
using namespace katana::runtime;
namespace {
constexpr std::size_t capacity = 8;
constexpr std::uint16_t update_opcode = 1, close_opcode = 2;
std::uint64_t identity(NativePortAudioVoiceHandle v) noexcept {
    return v ? (std::uint64_t(v.generation) << 32) | v.slot : 0;
}
NativePortAudioExecutionDomainConfig domain_config(const NativePortAudioEngineConfig& config) {
    auto queue = config.command_queue;
    if (native_port_audio_serial_reference_requested())
        queue.mode = NativePortAudioCommandQueueMode::SerialReference;
    return {queue};
}
StatusPublisher::Status status_of(const NativePortAudioVoiceSnapshot& s) noexcept {
    return {s.state, s.mixed_output_frames, s.played_output_frames};
}
std::int64_t clock_ns() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

bool async_status_enabled() noexcept {
    static const bool enabled = native_cpu::enabled("SARECOMP_ASYNC_AUDIO_STATUS");
    return enabled;
}

class StatusPublisher::Impl final {
    struct Entry {
        NativePortAudioVoiceHandle voice{};
        std::uint64_t revision{};
        Status last{}; // Each thread has its own entries, never borrowed.
    };
    struct Update {
        std::uint32_t index{};
        NativePortAudioVoiceHandle voice{};
        std::uint64_t revision{};
    };
    struct Cell {
        // All payload fields are atomic: this is not a data-racing seqlock.
        // Sequential consistency makes the version check cover the payload's
        // complete order, including x86 and non-x86 builds.
        std::atomic<std::uint64_t> sequence{}, revision{}, voice{}, mixed{}, played{};
        std::atomic<std::uint32_t> state{};
        void publish(const Entry& entry) noexcept {
            sequence.fetch_add(1);
            revision.store(entry.revision);
            voice.store(identity(entry.voice));
            state.store(static_cast<std::uint32_t>(entry.last.state));
            mixed.store(entry.last.mixed_output_frames);
            played.store(entry.last.played_output_frames);
            sequence.fetch_add(1);
        }
        bool read(Entry& expected) const noexcept {
            for (unsigned attempt = 0; attempt != 3; ++attempt) {
                const auto before = sequence.load();
                if (before & 1) continue;
                const auto observed_revision = revision.load();
                const auto observed_voice = voice.load();
                Status result{static_cast<NativePortAudioVoiceState>(state.load()),
                              mixed.load(), played.load()};
                if (before == sequence.load() &&
                    observed_revision == expected.revision &&
                    observed_voice == identity(expected.voice)) {
                    expected.last = result;
                    return true;
                }
            }
            // Bounded reader: use its last coherent sample of this exact
            // registration. The lifecycle ACK seeds it before any read.
            return false;
        }
    };

public:
    Impl(NativePortAudioEngine& engine, const NativePortAudioEngineConfig& config)
        : engine_(engine), domain_(NativePortAudioExecutionDomain::acquire(domain_config(config))) {
        const auto registered = domain_->register_target(
            NativePortAudioExecutionDomainTarget::AudioEngine, this,
            &execute, &cleanup, &service);
        if (!registered) throw std::runtime_error("audio-status-register");
        handle_ = *registered;
    }
    ~Impl() {
        // A terminal domain runs cleanup itself; unregister fences any active
        // service. Cleanup intentionally never dereferences the audio engine.
        const auto result = domain_->dispatch_sync(handle_, close_opcode, {}, frame());
        if (!result.completed()) domain_->shutdown();
        if (!domain_->unregister_target(handle_, this)) domain_->shutdown();
    }
    void watch(NativePortAudioVoiceHandle voice) {
        require_owner();
        if (!voice) throw std::runtime_error("audio-status-invalid-voice");
        for (std::size_t i = 0; i != capacity; ++i)
            if (identity(producer_[i].voice) == identity(voice)) {
                update(i, voice); return;
            }
        for (std::size_t i = 0; i != capacity; ++i)
            if (!producer_[i].voice) { update(i, voice); return; }
        throw std::runtime_error("audio-status-capacity");
    }
    void unwatch(NativePortAudioVoiceHandle voice) {
        require_owner();
        if (!voice) return;
        for (std::size_t i = 0; i != capacity; ++i)
            if (identity(producer_[i].voice) == identity(voice)) {
                update(i, {}); return;
            }
    }
    Status read(NativePortAudioVoiceHandle voice) {
        require_owner();
        if (!voice) throw std::runtime_error("audio-status-invalid-voice");
        // Serial reference has no autonomous worker. Preserve its synchronous
        // semantics without depending on device callbacks to publish a sample.
        if (domain_->mode() == NativePortAudioCommandQueueMode::SerialReference)
            return status_of(engine_.voice_snapshot(voice));
        // A missing/disconnected device may have no completion callbacks.
        // A bounded atomic wake lets the worker advance its silent clock even
        // inside a title wait loop. It neither queues a command nor waits.
        const auto now = clock_ns();
        constexpr std::int64_t refresh_ns = 10'000'000;
        if (now - published_at_.load(std::memory_order_relaxed) >= refresh_ns &&
            now - last_kick_ >= refresh_ns) {
            last_kick_ = now;
            domain_->request_consumer_service();
        }
        const auto domain_state = domain_->snapshot();
        if (failed_.load() || domain_state.first_error != NativePortAudioExecutionDomainFailure::None ||
            domain_state.shutdown_requested)
            throw std::runtime_error("audio-status-worker-failed");
        for (std::size_t i = 0; i != capacity; ++i)
            if (identity(producer_[i].voice) == identity(voice)) {
                static_cast<void>(cells_[i].read(producer_[i]));
                return producer_[i].last;
            }
        throw std::runtime_error("audio-status-unwatched-voice");
    }
private:
    void require_owner() const {
        if (std::this_thread::get_id() != owner_)
            throw std::runtime_error("audio-status-producer-thread");
    }
    std::uint64_t frame() const noexcept {
        return domain_->last_frame_index_nonblocking().value_or(0);
    }
    void update(std::size_t index, NativePortAudioVoiceHandle voice) {
        auto& entry = producer_[index];
        if (entry.revision == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("audio-status-revision-overflow");
        const Update command{static_cast<std::uint32_t>(index), voice, entry.revision + 1};
        const auto result = domain_->dispatch_sync(handle_, update_opcode,
            std::as_bytes(std::span(&command, 1)), frame());
        if (!result.completed() || result.ack.result_size != sizeof(Status))
            throw std::runtime_error("audio-status-update-failed");
        entry.voice = voice;
        entry.revision = command.revision;
        std::memcpy(&entry.last, result.ack.bytes.data(), sizeof(Status));
    }
    static void execute(void* object, std::uint16_t opcode, std::span<const std::byte> payload,
                        NativePortAudioCommandAckResult& result) noexcept {
        auto& self = *static_cast<Impl*>(object);
        try {
            if (opcode == close_opcode && payload.empty()) {
                self.closed_ = true;
                self.consumer_ = {};
                return;
            }
            if (opcode != update_opcode || payload.size() != sizeof(Update) || self.closed_)
                throw std::runtime_error("audio-status-command");
            Update update;
            std::memcpy(&update, payload.data(), sizeof(update));
            if (update.index >= capacity || !update.revision)
                throw std::runtime_error("audio-status-command-index");
            auto& entry = self.consumer_[update.index];
            entry = {update.voice, update.revision, {}};
            if (entry.voice) entry.last = status_of(self.engine_.voice_snapshot(entry.voice));
            self.cells_[update.index].publish(entry);
            static_assert(sizeof(Status) <= native_port_audio_command_queue_max_ack_result_bytes);
            std::memcpy(result.bytes.data(), &entry.last, sizeof(Status));
            result.result_size = sizeof(Status);
        } catch (...) {
            self.failed_.store(true);
            result.status = NativePortAudioCommandAckStatus::Failed;
            result.error_code = 1;
        }
    }
    static std::uint32_t service(void* object) noexcept {
        auto& self = *static_cast<Impl*>(object);
        if (self.closed_) return 0;
        try {
            for (std::size_t i = 0; i != capacity; ++i) {
                auto& entry = self.consumer_[i];
                if (!entry.voice) continue;
                // Already on the sole consumer: the SDK executes this query
                // inline against Core, without queue publication or a pump.
                entry.last = status_of(self.engine_.voice_snapshot(entry.voice));
                self.cells_[i].publish(entry);
            }
            self.published_at_.store(clock_ns(), std::memory_order_relaxed);
            return 0;
        } catch (...) {
            self.failed_.store(true);
            return 1;
        }
    }
    static void cleanup(void* object) noexcept {
        auto& self = *static_cast<Impl*>(object);
        self.closed_ = true;
        self.consumer_ = {};
        self.failed_.store(true);
    }
    NativePortAudioEngine& engine_;
    const std::thread::id owner_ = std::this_thread::get_id();
    std::shared_ptr<NativePortAudioExecutionDomain> domain_;
    NativePortAudioExecutionDomainTargetHandle handle_{};
    std::array<Entry, capacity> producer_{}, consumer_{};
    std::array<Cell, capacity> cells_{};
    std::atomic<bool> failed_{};
    std::atomic<std::int64_t> published_at_{};
    std::int64_t last_kick_{}; // Producer only.
    bool closed_{}; // Audio consumer only.
};

StatusPublisher::StatusPublisher(NativePortAudioEngine& engine, const NativePortAudioEngineConfig& config)
    : impl_(std::make_unique<Impl>(engine, config)) {}
StatusPublisher::~StatusPublisher() = default;
void StatusPublisher::watch(NativePortAudioVoiceHandle voice) { impl_->watch(voice); }
void StatusPublisher::unwatch(NativePortAudioVoiceHandle voice) { impl_->unwatch(voice); }
StatusPublisher::Status StatusPublisher::read(NativePortAudioVoiceHandle voice) { return impl_->read(voice); }
}
