#include "sonic_movie_audio.hpp"
#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace katana::runtime;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {unsigned opens=0,closes=0,reads=0;bool fail_open=false;};
constexpr std::array<std::int16_t,8> pcm{-32768,-20000,-3,0,3,10000,20000,32767};
constexpr std::array<std::int16_t,8> half{-16384,-10000,-2,0,2,5000,10000,16384};
void open(void* user,const NativePortCodecOpenRequest*,NativePortCodecOpenResult* result) noexcept {
    auto& f=*static_cast<Fixture*>(user);++f.opens;
    *result={};result->decoder=&f;result->duration_nanoseconds=99000;result->audio_channels=2;
    result->audio_sample_rate=44100;result->has_audio=result->has_video=1;
    if(f.fail_open)result->failure=NativePortCodecFailure::InvalidData;
}
void read(void* decoder,NativePortCodecReadResult* result) noexcept {
    auto& f=*static_cast<Fixture*>(decoder);++f.reads;
    *result={};result->failure=NativePortCodecFailure::None;
    result->status=f.reads==8?NativePortCodecReadStatus::EndOfStream:NativePortCodecReadStatus::Sample;
    auto& s=result->sample;s.timestamp_nanoseconds=f.reads*1000;s.duration_nanoseconds=1000;
    s.kind=f.reads==5?NativePortCodecSampleKind::Video:f.reads==6?NativePortCodecSampleKind::AudioEnd:
           f.reads==7?NativePortCodecSampleKind::VideoEnd:NativePortCodecSampleKind::Audio;
    s.audio_sample_rate=44100;s.audio_channels=2;s.audio_samples=pcm.data();s.audio_sample_count=pcm.size();
    s.video_pixels=pcm.data();s.video_byte_count=sizeof(pcm);s.video_width=2;s.video_height=2;s.video_stride_bytes=8;
}
void close(void* decoder) noexcept {++static_cast<Fixture*>(decoder)->closes;}
}
int main(){
    try {
        sonic::presentation::Settings settings;settings.setup_complete=true;
        sonic::presentation::apply_live(settings);sonic::audio::focused=true;
        Fixture fixture;
        NativePortCodecProvider base{.structure_size=native_port_codec_provider_structure_size,
            .provider_name="movie-test",.user=&fixture,.open=open,.read_next=read,.close=close};
        const auto wrapper=sonic::movie_audio::wrap(base);NativePortCodecOpenRequest request;
        NativePortCodecOpenResult opened;wrapper.open(wrapper.user,&request,&opened);
        require(opened.decoder && opened.decoder!=&fixture && opened.duration_nanoseconds==99000 &&
                opened.audio_sample_rate==44100&&opened.audio_channels==2,"open metadata or ownership");
        NativePortCodecReadResult result;
        wrapper.read_next(opened.decoder,&result);
        require(result.sample.audio_samples==pcm.data(),"unity copied or altered decoder data");
        settings.master_volume=50;sonic::presentation::apply_live(settings);
        wrapper.read_next(opened.decoder,&result);
        require(result.sample.audio_samples!=pcm.data()&&std::equal(half.begin(),half.end(),result.sample.audio_samples),"half gain PCM");
        require(result.sample.timestamp_nanoseconds==2000&&result.sample.duration_nanoseconds==1000&&result.sample.audio_sample_count==8,"audio timing changed");
        settings.mute_background=1;sonic::presentation::apply_live(settings);sonic::audio::focused=false;
        wrapper.read_next(opened.decoder,&result);
        require(std::all_of(result.sample.audio_samples,result.sample.audio_samples+8,[](auto n){return n==0;}),"background mute");
        sonic::audio::focused=true;settings.master_volume=100;settings.music_volume=0;settings.voice_volume=0;settings.effects_volume=0;
        sonic::presentation::apply_live(settings);wrapper.read_next(opened.decoder,&result);
        require(result.sample.audio_samples==pcm.data()&&pcm[0]==-32768&&pcm[7]==32767,"mixed movie incorrectly routed or decoder modified");
        settings.master_volume=0;sonic::presentation::apply_live(settings);
        wrapper.read_next(opened.decoder,&result);
        require(result.sample.kind==NativePortCodecSampleKind::Video&&result.sample.video_pixels==pcm.data()&&result.sample.timestamp_nanoseconds==5000,"video changed");
        wrapper.read_next(opened.decoder,&result);require(result.sample.kind==NativePortCodecSampleKind::AudioEnd,"audio EOS changed");
        wrapper.read_next(opened.decoder,&result);require(result.sample.kind==NativePortCodecSampleKind::VideoEnd,"video EOS changed");
        wrapper.read_next(opened.decoder,&result);require(result.status==NativePortCodecReadStatus::EndOfStream,"terminal EOS changed");
        wrapper.close(opened.decoder);require(fixture.closes==1,"decoder not closed exactly once");
        fixture.fail_open=true;wrapper.open(wrapper.user,&request,&opened);
        require(opened.failure==NativePortCodecFailure::InvalidData&&!opened.decoder&&fixture.closes==2,"failed open leaked or swallowed error");
        std::cout<<"SONIC_MOVIE_AUDIO_TEST_OK unity=identity master=half background=mute timestamps=unchanged video_eos=unchanged close=owned\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"SONIC_MOVIE_AUDIO_TEST_FAIL "<<e.what()<<'\n';return 1;}
}
