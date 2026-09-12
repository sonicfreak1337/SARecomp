#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <span>
#include <thread>

namespace sonic::rumble {
// PAL pdVib's eight-byte record, then the Flycast PuruPuru host translation.
// Timing is in real milliseconds, independent of guest and output frame rates.
struct Effect { float power=0, inclination=0; unsigned duration_ms=0; };
inline Effect decode(std::span<const std::uint8_t,8> record,unsigned timeout) noexcept {
    const unsigned flags=record[1];
    int strength=static_cast<std::int8_t>(record[2]);
    if(!(flags&0x88) && (strength==1 || strength==-1))strength*=2;
    unsigned amplitude=flags&0x88;
    if(strength>0)amplitude|=(unsigned(strength)<<4)&0x70;
    else if(strength<0)amplitude|=unsigned(-strength)&7;
    const unsigned p=amplitude&7,n=(amplitude>>4)&7,f=record[3],maximum=std::max(p,n);
    int inclination=(flags&0x88)?record[4]:0;
    if(amplitude&0x80)inclination=-inclination;
    else if(!(amplitude&8))inclination=0;
    Effect result;result.power=std::min((p+n)/7.f,1.f);
    result.duration_ms=((timeout&255)+1)*250;
    if(f && (!(flags&1) || inclination))
        result.duration_ms=std::min(result.duration_ms,1000u*(inclination?unsigned(std::abs(inclination))*maximum:1u)/f);
    if(inclination && maximum)result.inclination=f/(1000.f*inclination*maximum);
    return result;
}

// Only the second accessory slot is offered. The VMU in the first slot is
// untouched, and the original title's player->port lookup is still executed.
inline int controller(unsigned accessory) noexcept {
    return accessory<24 && accessory%6==2 ? int(accessory/6) : -1;
}

class Engine final {
public:
    using Send=bool(*)(void*,unsigned,std::uint16_t,std::uint16_t) noexcept;
    ~Engine(){detach(nullptr);}
    Engine()=default;
    Engine(const Engine&)=delete;
    Engine& operator=(const Engine&)=delete;
    void attach(void* owner,Send send) {
        detach(nullptr);
        if(!send)return;
        std::lock_guard lock(mutex_);owner_=owner;send_=send;stopping_=false;
        worker_=std::thread([this]{work();});
    }
    void detach(void* owner) noexcept {
        {
            std::lock_guard lock(mutex_);
            if(owner && owner!=owner_)return;
            stop_all();stopping_=true;changed_.notify_all();
        }
        if(worker_.joinable())worker_.join();
        std::lock_guard lock(mutex_);owner_=nullptr;send_=nullptr;slots_={};blocked_=true;
    }
    void bind(unsigned index,std::uint64_t identity,int endpoint) noexcept {
        if(index>=slots_.size())return;
        std::lock_guard lock(mutex_);auto& slot=slots_[index];
        if(slot.identity==identity && slot.endpoint==endpoint)return;
        stop(slot);slot={};slot.identity=identity;slot.endpoint=endpoint;
        changed_.notify_all();
    }
    void policy(bool blocked,unsigned strength) noexcept {
        std::lock_guard lock(mutex_);
        const auto gain=std::min(strength,100u)*.01f;
        if(blocked_==blocked && gain_==gain)return;
        blocked_=blocked;gain_=gain;
        if(blocked_ || !strength)stop_all();
        changed_.notify_all();
    }
    bool capable(unsigned accessory) noexcept {
        std::lock_guard lock(mutex_);return find(accessory)!=nullptr;
    }
    int configure(unsigned accessory,unsigned timeout) noexcept {
        std::lock_guard lock(mutex_);auto* slot=find(accessory);if(!slot)return -2;
        slot->timeout=timeout&255;return 0;
    }
    int request(unsigned accessory,std::span<const std::uint8_t,8> bytes) noexcept {
        std::lock_guard lock(mutex_);auto* slot=find(accessory);if(!slot)return -2;
        const auto effect=decode(bytes,slot->timeout);
        if(blocked_ || gain_==0 || effect.power==0 || effect.duration_ms==0){stop(*slot);return 0;}
        slot->effect=effect;slot->end=Clock::now()+std::chrono::milliseconds(effect.duration_ms);
        slot->next=effect.inclination>0?std::min(slot->end,Clock::now()+std::chrono::milliseconds(8)):slot->end;
        slot->active=true;const bool success=emit(*slot,effect.power);
        changed_.notify_all();return success?0:-1;
    }
    int cancel(unsigned accessory) noexcept {
        std::lock_guard lock(mutex_);auto* slot=find(accessory);if(!slot)return -2;
        stop(*slot);changed_.notify_all();return 0;
    }
private:
    using Clock=std::chrono::steady_clock;
    struct Slot {
        std::uint64_t identity=0;int endpoint=-1;unsigned timeout=19;
        bool active=false;std::uint16_t last=0;Effect effect;Clock::time_point end{},next{};
    };
    Slot* find(unsigned accessory) noexcept {
        const auto index=controller(accessory);
        if(index<0 || !send_)return nullptr;
        auto& slot=slots_[unsigned(index)];return slot.identity && slot.endpoint>=0?&slot:nullptr;
    }
    bool emit(Slot& slot,float power) noexcept {
        const auto value=std::uint16_t(std::lround(std::clamp(power*gain_,0.f,1.f)*65535.f));
        if(value==slot.last)return true;
        const bool ok=send_ && slot.endpoint>=0 && send_(owner_,unsigned(slot.endpoint),value,value);
        slot.last=ok?value:0;
        if(!ok){slot.active=false;slot.endpoint=-1;}
        return ok;
    }
    void stop(Slot& slot) noexcept {emit(slot,0);slot.active=false;}
    void stop_all() noexcept {for(auto& slot:slots_)stop(slot);}
    void work() noexcept {
        std::unique_lock lock(mutex_);
        while(!stopping_) {
            const auto now=Clock::now();auto wake=Clock::time_point::max();
            for(auto& slot:slots_)if(slot.active) {
                if(now>=slot.end){stop(slot);continue;}
                if(now>=slot.next) {
                    // Match Flycast's SDL translation: only positive slope
                    // has a declining envelope. Negative slope remains level.
                    const auto remaining=std::chrono::duration<float,std::milli>(slot.end-now).count();
                    emit(slot,slot.effect.power*slot.effect.inclination*remaining);
                    slot.next=std::min(slot.end,now+std::chrono::milliseconds(8));
                }
                if(slot.active)wake=std::min(wake,slot.next);
            }
            if(wake==Clock::time_point::max())changed_.wait(lock);
            else changed_.wait_until(lock,wake);
        }
    }
    std::mutex mutex_;std::condition_variable changed_;std::thread worker_;
    std::array<Slot,4> slots_{};void* owner_=nullptr;Send send_=nullptr;
    bool stopping_=false,blocked_=true;float gain_=1;
};
inline Engine& engine(){static Engine value;return value;}
}
