#pragma once
#include <atomic>
#include <cstdint>
#include <string_view>

namespace sonic::recovery {
enum class BackupState {Waiting,Saved,Failed,CleanupPending};
inline std::string_view name(BackupState state) noexcept {
    switch(state){
    case BackupState::Saved:return "saved";
    case BackupState::Failed:return "failed";
    case BackupState::CleanupPending:return "cleanup_pending";
    default:return "waiting";
    }
}
enum class OutputState {Idle,Connected,Silent,RestartRequired};
enum class AudioState {Idle,Connected,Silent,Partial,RestartRequired};
inline std::string_view name(AudioState state) noexcept {
    switch(state){
    case AudioState::Connected:return "connected";
    case AudioState::Silent:return "silent_retrying";
    case AudioState::Partial:return "partial_retrying";
    case AudioState::RestartRequired:return "restart_required";
    default:return "idle";
    }
}
// Three 16-bit counters share one atomic publication. An endpoint migration
// must not briefly hide a second stream's failure from the menu reader.
inline std::atomic<std::uint64_t> output_counts{0},faults{0},reconnections{0};
inline std::atomic<std::uint32_t> last_error{0};
class OutputStatus {
    OutputState state_=OutputState::Idle;
    static constexpr std::uint64_t unit(OutputState state) noexcept {
        return state==OutputState::Idle?0:std::uint64_t{1}<<((unsigned(state)-1)*16);
    }
public:
    OutputStatus()=default;
    OutputStatus(const OutputStatus&)=delete;
    OutputStatus& operator=(const OutputStatus&)=delete;
    ~OutputStatus(){set(OutputState::Idle);}
    void set(OutputState state) noexcept {
        if(state==state_)return;
        output_counts.fetch_add(unit(state)-unit(state_),std::memory_order_release);
        state_=state;
    }
};
inline void audio_fault(std::uint32_t error) noexcept {
    if(error){last_error.store(error,std::memory_order_relaxed);faults.fetch_add(1,std::memory_order_relaxed);}
}
struct AudioSnapshot {
    AudioState state=AudioState::Idle;
    unsigned connected=0,silent=0,restart_required=0;
    std::uint64_t faults=0,reconnections=0;
    std::uint32_t last_error=0;
};
inline AudioSnapshot audio() noexcept {
    const auto counts=output_counts.load(std::memory_order_acquire);
    AudioSnapshot result;
    result.connected=unsigned(counts&0xffff);
    result.silent=unsigned((counts>>16)&0xffff);
    result.restart_required=unsigned((counts>>32)&0xffff);
    result.state=result.restart_required?AudioState::RestartRequired:
        result.silent?(result.connected?AudioState::Partial:AudioState::Silent):
        result.connected?AudioState::Connected:AudioState::Idle;
    result.faults=faults.load(std::memory_order_relaxed);
    result.reconnections=reconnections.load(std::memory_order_relaxed);
    result.last_error=last_error.load(std::memory_order_relaxed);
    return result;
}
}
