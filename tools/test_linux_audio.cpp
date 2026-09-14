#include "katana/runtime/native_port_audio.hpp"
#include "sonic_audio_device.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(int argc,char** argv){
    try{
        setenv("KATANA_PORT_BACKGROUND_TEST","1",1);
        const bool unavailable=argc>1&&std::string_view(argv[1])=="unavailable";
        setenv("SDL_AUDIODRIVER",unavailable?"sonic-intentionally-unavailable":"dummy",1);
        katana::runtime::NativePortAudioConfig config;
        config.format={48000,2};config.maximum_queued_frames=12000;
        katana::runtime::NativePortAudioStream stream(config);
        std::vector<std::int16_t> pcm(9600*2,20000);
        require(stream.submit_pcm_s16(pcm),"submit failed");
        std::fill(pcm.begin(),pcm.end(),-20000);
        stream.pause();
        stream.refresh_playback_position();const auto frozen=stream.playback_position_frames();
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        stream.refresh_playback_position();
        require(stream.playback_position_frames()==frozen,"pause advanced PCM clock");
        ::sonic::audio_device::changed();
        stream.poll();
        require(stream.snapshot().state==katana::runtime::NativePortAudioState::Paused,"device notification unpaused stream");
        stream.resume();
        std::uint64_t previous=frozen;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
        while(stream.playback_position_frames()<9600&&std::chrono::steady_clock::now()<deadline){
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            stream.refresh_playback_position();const auto now=stream.playback_position_frames();
            require(now>=previous&&now<=9600,"PCM cursor outside submission bounds");previous=now;
        }
        stream.poll();const auto complete=stream.snapshot();
        require(complete.submitted_buffers==1&&complete.submitted_frames==9600,"submission accounting");
        require(complete.playback_position_frames==9600&&complete.completed_frames==9600&&complete.queued_frames==0,"last PCM block did not drain");
        const auto status=::sonic::recovery::audio();
        require(unavailable?status.silent==1:status.connected==1,"endpoint recovery status");
        // A subsequent stream block must resume from the drained cursor.
        require(stream.submit_pcm_s16(std::span(pcm).first(960)) ,"second submit");
        stream.stop();stream.stop();
        require(stream.snapshot().state==katana::runtime::NativePortAudioState::Stopped&&stream.snapshot().queued_frames==0,"stop retained live queue");
        require(::sonic::recovery::audio().state==::sonic::recovery::AudioState::Idle,"endpoint status leaked");
        std::cout<<"SONIC_LINUX_AUDIO_OK backend="<<(unavailable?"silent-recovery":"SDL-dummy")
            <<" pause=exact clock=bounded tail=drained notification=paused shutdown=clean\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
