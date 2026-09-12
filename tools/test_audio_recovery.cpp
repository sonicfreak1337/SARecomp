#define NOMINMAX
#include <windows.h>
#include "sonic_audio_device.hpp"
#include "sonic_host_resume.hpp"
#include "katana/runtime/native_port_audio.hpp"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>
using namespace katana::runtime;
namespace a=sonic::audio_device;
namespace {
using WAVEOUTPROC=void(CALLBACK*)(HWAVEOUT,UINT,DWORD_PTR,DWORD_PTR,DWORD_PTR);
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
std::atomic<std::uint64_t> clock_ns{1};
std::atomic<bool> available{true},fail_write{false},fail_close{false};
std::atomic<unsigned> opens{0},closes{0},writes{0},paused_writes{0};
std::mutex lock;
struct Device {
    std::uint64_t time=0,position=0,submitted=0;
    bool paused=false;unsigned channels=1,rate=8000;
    DWORD_PTR callback=0,user=0;
    struct Pending {WAVEHDR* header;std::uint64_t end;};
    std::vector<Pending> pending;
};
std::vector<Device*> quarantined;
std::uint64_t now() noexcept {return clock_ns.load();}
void tick(Device& d){
    const auto n=now();if(!d.paused&&n>=d.time)d.position=std::min(d.submitted,d.position+(n-d.time)*d.rate/1'000'000'000);
    d.time=n;
    for(auto& p:d.pending)if(p.end<=d.position && !(p.header->dwFlags&WHDR_DONE)){
        p.header->dwFlags|=WHDR_DONE;
        if(d.callback)reinterpret_cast<WAVEOUTPROC>(d.callback)(reinterpret_cast<HWAVEOUT>(&d),WOM_DONE,d.user,0,0);
    }
    std::erase_if(d.pending,[&](const auto& p){return p.end<=d.position;});
}
MMRESULT WINAPI open(LPHWAVEOUT output,UINT,LPCWAVEFORMATEX f,DWORD_PTR cb,DWORD_PTR user,DWORD){
    ++opens;if(!available)return MMSYSERR_NODRIVER;
    auto* d=new Device;d->time=now();d->channels=f->nChannels;d->rate=f->nSamplesPerSec;d->callback=cb;d->user=user;
    *output=reinterpret_cast<HWAVEOUT>(d);return 0;
}
MMRESULT WINAPI close(HWAVEOUT h){std::lock_guard g(lock);++closes;auto* d=reinterpret_cast<Device*>(h);
    if(fail_close.exchange(false)){quarantined.push_back(d);return WAVERR_STILLPLAYING;}
    delete d;return 0;}
MMRESULT WINAPI reset(HWAVEOUT h){std::lock_guard g(lock);auto& d=*reinterpret_cast<Device*>(h);
    for(auto& p:d.pending)p.header->dwFlags|=WHDR_DONE;d.pending.clear();d.position=d.submitted=0;d.time=now();return 0;}
MMRESULT WINAPI prepare(HWAVEOUT,WAVEHDR* p,UINT){p->dwFlags|=WHDR_PREPARED;return 0;}
MMRESULT WINAPI unprepare(HWAVEOUT h,WAVEHDR* p,UINT){std::lock_guard g(lock);p->dwFlags&=~WHDR_PREPARED;
    auto& d=*reinterpret_cast<Device*>(h);std::erase_if(d.pending,[&](const auto& e){return e.header==p;});return 0;}
MMRESULT WINAPI write(HWAVEOUT h,WAVEHDR* p,UINT){std::lock_guard g(lock);
    if(!available||fail_write.exchange(false))return MMSYSERR_NODRIVER;
    auto& d=*reinterpret_cast<Device*>(h);tick(d);++writes;if(d.paused)++paused_writes;
    std::erase_if(d.pending,[&](const auto& e){return e.header==p;});
    d.submitted+=p->dwBufferLength/(d.channels*2);d.pending.push_back({p,d.submitted});return 0;}
MMRESULT WINAPI position(HWAVEOUT h,LPMMTIME out,UINT){std::lock_guard g(lock);
    if(!available)return MMSYSERR_NODRIVER;auto& d=*reinterpret_cast<Device*>(h);tick(d);out->wType=TIME_SAMPLES;out->u.sample=DWORD(d.position);return 0;}
MMRESULT WINAPI pause(HWAVEOUT h){std::lock_guard g(lock);auto& d=*reinterpret_cast<Device*>(h);tick(d);d.paused=true;return 0;}
MMRESULT WINAPI restart(HWAVEOUT h){std::lock_guard g(lock);auto& d=*reinterpret_cast<Device*>(h);d.time=now();d.paused=false;return 0;}
}
int main(){
    try{
        check(a::working_time()>0,"Windows working-state timer unavailable");
        sonic::lifecycle::SleepClock sleep;
        check(sleep.observe(1'000'000'000,1'000'000'000,0)==0,"sleep clock initialization");
        check(sleep.observe(3'000'000'000,3'000'000'000,0)==0,"ordinary slow frame was discarded");
        check(sleep.observe(20'000'000'000,5'000'000'000,1)==15'000'000'000,"sleep duration did not exclude active work");
        check(sleep.observe(20'010'000'000,5'010'000'000,2)==0,"duplicate resume counted twice");
        sleep.reset(30'000'000'000,6'000'000'000,3); // modal already excluded all paused time
        check(sleep.observe(30'033'000'000,6'033'000'000,3)==0,"modal pause double-excluded sleep");
        check(sleep.observe(100'033'000'000,6'133'000'000,3)==69'900'000'000,"missed power broadcast left timer debt");
        _putenv_s("KATANA_PORT_BACKGROUND_TEST","1");
        a::Api api;api.open=open;api.close=close;api.reset=reset;api.prepare=prepare;api.unprepare=unprepare;
        api.write=write;api.position=position;api.pause=pause;api.restart=restart;api.now=now;
        a::set_test_api(&api);
        struct ResetApi {~ResetApi(){a::set_test_api(nullptr);}} reset_api;
        NativePortAudioConfig cfg;cfg.format.sample_rate=8000;cfg.format.channels=1;cfg.maximum_queued_frames=8000;
        {
            NativePortAudioStream stream(cfg);std::vector<std::int16_t> pcm(800,1000);
            check(sonic::recovery::audio().state==sonic::recovery::AudioState::Connected,"initial endpoint status");
            const auto advance=[&](unsigned ms){clock_ns+=std::uint64_t(ms)*1'000'000;stream.refresh_playback_position();stream.poll();};
            check(stream.submit_pcm_s16(pcm),"initial submit");
            advance(25);check(stream.snapshot().playback_position_frames==200,"initial sample clock");
            a::changed();stream.refresh_playback_position();
            auto s=stream.snapshot();check(s.submitted_frames==800&&s.completed_frames==200&&s.queued_frames==600,"migration must retain unplayed samples");
            advance(25);check(stream.snapshot().playback_position_frames==400,"new device clock regressed or skipped");
            stream.pause();const auto paused=paused_writes.load();a::changed();advance(1000);
            check(stream.snapshot().state==NativePortAudioState::Paused&&stream.snapshot().playback_position_frames==400,"paused migration advanced clock");
            check(paused_writes>paused,"new device was not paused before requeue");
            stream.resume();advance(25);check(stream.snapshot().playback_position_frames==600,"resume lost source cursor");
            available=false;a::changed();stream.refresh_playback_position();advance(25);
            check(stream.snapshot().playback_position_frames==800&&stream.snapshot().queued_frames==0,"offline movie clock stalled");
            check(sonic::recovery::audio().state==sonic::recovery::AudioState::Silent,"missing endpoint failure status");
            check(stream.submit_pcm_s16(pcm),"offline submit");advance(80);
            check(stream.snapshot().playback_position_frames==1440,"silent clock is not real time");
            const auto attempts=opens.load();for(unsigned i=0;i<50;++i)stream.poll();
            check(opens==attempts,"unavailable endpoint caused an open retry storm");
            available=true;a::changed();stream.refresh_playback_position();advance(20);
            check(stream.snapshot().playback_position_frames==1600&&stream.snapshot().queued_frames==0,"reconnection changed media timeline");
            check(sonic::recovery::audio().state==sonic::recovery::AudioState::Connected&&sonic::recovery::audio().reconnections>0,"reconnected endpoint status");
            fail_write=true;check(stream.submit_pcm_s16(pcm),"failed hardware write should retain PCM offline");
            auto before=stream.snapshot().playback_position_frames;
            available=false;clock_ns+=8ull*60*60*1'000'000'000;a::resumed();stream.refresh_playback_position();
            check(stream.snapshot().playback_position_frames==before,"standby consumed the queued soundtrack");
            available=true;a::changed();stream.refresh_playback_position();advance(100);
            s=stream.snapshot();check(s.submitted_frames==2400&&s.playback_position_frames==2400&&s.queued_frames==0,"write failure recovery corrupted accounting");
            check(stream.submit_pcm_s16(pcm),"final submit");fail_write=true;a::changed();stream.refresh_playback_position();advance(100);
            check(!fail_write,"replacement failure did not reach a real requeue");
            check(stream.snapshot().playback_position_frames==3200,"replacement write failure lost samples");
            stream.stop();check(stream.snapshot().state==NativePortAudioState::Stopped,"hardware removal made normal stop fatal");
            check(sonic::recovery::audio().state==sonic::recovery::AudioState::Idle,"stopped endpoint still shown as failed");
        }
        {
            available=true;NativePortAudioStream stream(cfg);std::vector<std::int16_t> pcm(800);
            check(stream.submit_pcm_s16(pcm),"close-error submit");
            fail_close=true;a::changed();stream.refresh_playback_position();
            check(!fail_close,"failed-close scenario did not actually close its device");
            check(sonic::recovery::audio().state==sonic::recovery::AudioState::RestartRequired,"quarantined endpoint lacks restart guidance");
            clock_ns+=100'000'000;stream.refresh_playback_position();stream.poll();
            check(stream.snapshot().state==NativePortAudioState::Running&&stream.snapshot().playback_position_frames==800,"failed close did not safely degrade");
            stream.stop();
        }
        {
            available=true;NativePortAudioStream music(cfg);
            available=false;NativePortAudioStream movie(cfg);
            const auto partial=sonic::recovery::audio();
            check(partial.state==sonic::recovery::AudioState::Partial&&partial.connected==1&&partial.silent==1,"one connected stream hid another stream's failure");
            movie.stop();check(sonic::recovery::audio().state==sonic::recovery::AudioState::Connected,"closed silent stream left a stale failure");
            music.stop();
        }
        {
            available=false;NativePortAudioStream stream(cfg);std::vector<std::int16_t> pcm(800);
            check(stream.submit_pcm_s16(pcm),"startup with no audio device failed");
            clock_ns+=50'000'000;stream.refresh_playback_position();
            check(stream.snapshot().playback_position_frames==400,"silent startup did not advance");
            available=true;a::changed();stream.refresh_playback_position();
            clock_ns+=50'000'000;stream.refresh_playback_position();stream.poll();
            check(stream.snapshot().playback_position_frames==800&&stream.snapshot().queued_frames==0,"initially absent device did not reconnect");
            stream.stop();
        }
        // The retained callback must be inert after its stream/domain target
        // lifetime ends, even if a faulty OS driver refused waveOutClose.
        for(auto* d:quarantined){if(d->callback)reinterpret_cast<WAVEOUTPROC>(d->callback)(reinterpret_cast<HWAVEOUT>(d),WOM_DONE,d->user,0,0);delete d;}
        a::set_test_api(nullptr);
        check(sonic::recovery::audio().state==sonic::recovery::AudioState::Idle&&sonic::recovery::audio().faults>0,"endpoint status lifetime or error counters");
        std::cout<<"SONIC_AUDIO_RECOVERY_OK default_change=retains_pcm removal=silent_clock reconnect=monotone pause=retained standby=no_catchup retry=bounded replacement_failure=bounded callbacks=fenced status=multiple_streams_and_lifetime\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"SONIC_AUDIO_RECOVERY_FAIL "<<e.what()<<'\n';return 1;}
}
