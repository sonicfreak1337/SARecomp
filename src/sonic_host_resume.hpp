#pragma once
#include <algorithm>
#include <cstdint>

namespace sonic::lifecycle {
// Compare elapsed host time with Windows working-state time. Ordinary slow
// frames count as working time; only actual sleep/hibernation is excluded.
// No fixed simulation frequency or catch-up clamp is introduced.
class SleepClock {
    std::uint64_t host_=0,awake_=0,epoch_=0;
    bool initialized_=false;
public:
    bool needs_sample(std::uint64_t host,std::uint64_t epoch)const noexcept {
        return !initialized_ || epoch!=epoch_ || host<host_ || host-host_>=100'000'000;
    }
    void reset(std::uint64_t host,std::uint64_t awake,std::uint64_t epoch)noexcept {
        host_=host;awake_=awake;epoch_=epoch;initialized_=true;
    }
    std::uint64_t observe(std::uint64_t host,std::uint64_t awake,std::uint64_t epoch)noexcept {
        std::uint64_t result=0;
        if(initialized_&&host>=host_&&awake>=awake_){
            const auto wall=host-host_,active=awake-awake_;
            // Exclude measurement noise and duplicate power broadcasts.
            if(wall>active && wall-active>50'000'000)result=wall-active;
        }
        reset(host,awake,epoch);return result;
    }
};
}
