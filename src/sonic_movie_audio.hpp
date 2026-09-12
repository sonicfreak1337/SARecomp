#pragma once
#include "sonic_audio_settings.hpp"
#include "katana/runtime/native_port_ffmpeg_codec.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <vector>

namespace sonic::movie_audio {
using namespace katana::runtime;

// Sofdec audio is a mixed soundtrack. Apply Master only, keeping its decoder
// timestamps, stream ends and sample count intact. Never modify decoder-owned
// memory; at unity the original sample and pointer pass through unchanged.
inline void apply_gain(NativePortCodecReadResult& result,std::vector<std::int16_t>& output,float gain) {
    if(result.status!=NativePortCodecReadStatus::Sample || result.sample.kind!=NativePortCodecSampleKind::Audio || gain==1.0f)return;
    const auto& sample=result.sample;
    if(!std::isfinite(gain)||gain<0||gain>1 || (sample.audio_sample_count&&!sample.audio_samples) ||
       sample.audio_sample_count>output.max_size())throw std::invalid_argument("movie-audio-gain-layout");
    output.resize(std::size_t(sample.audio_sample_count));
    for(std::size_t i=0;i<output.size();++i)
        output[i]=std::int16_t(std::clamp(std::lround(sample.audio_samples[i]*gain),-32768L,32767L));
    result.sample.audio_samples=output.data();
}

namespace detail {
struct Decoder {
    const NativePortCodecProvider* upstream=nullptr;
    void* decoder=nullptr;
    std::vector<std::int16_t> adjusted;
    std::uint64_t samples=0;
    float minimum_gain=1,maximum_gain=0;
    ~Decoder(){
        if(decoder)upstream->close(decoder);
        const auto trace=std::getenv("SARECOMP_MOVIE_AUDIO_TRACE");
        if(samples&&trace&&std::string_view(trace)=="1")
            std::fprintf(stderr,"SONIC_MOVIE_AUDIO samples=%llu gain_min=%.3f gain_max=%.3f\n",
                         static_cast<unsigned long long>(samples),minimum_gain,maximum_gain);
    }
};
inline void open(void* user,const NativePortCodecOpenRequest* request,NativePortCodecOpenResult* result) noexcept {
    if(!result)return;
    *result={};
    if(!request||!user){result->failure=NativePortCodecFailure::InvalidContract;return;}
    try {
        auto state=std::make_unique<Decoder>();state->upstream=static_cast<const NativePortCodecProvider*>(user);
        state->upstream->open(state->upstream->user,request,result);
        state->decoder=result->decoder;
        if(result->failure!=NativePortCodecFailure::None || !state->decoder){
            result->decoder=nullptr;
            if(result->failure==NativePortCodecFailure::None)result->failure=NativePortCodecFailure::InvalidData;
            return;
        }
        result->decoder=state.release();
    }catch(const std::bad_alloc&){*result={};result->failure=NativePortCodecFailure::ResourceExhausted;}
    catch(...){*result={};result->failure=NativePortCodecFailure::Internal;}
}
inline void read(void* decoder,NativePortCodecReadResult* result) noexcept {
    if(!result)return;
    *result={};
    if(!decoder){result->failure=NativePortCodecFailure::InvalidContract;return;}
    auto& state=*static_cast<Decoder*>(decoder);
    state.upstream->read_next(state.decoder,result);
    try {
        const auto gain=audio::factor(audio::Bus::Master);
        if(result->status==NativePortCodecReadStatus::Sample&&result->sample.kind==NativePortCodecSampleKind::Audio){
            state.samples+=result->sample.audio_sample_count;
            state.minimum_gain=std::min(state.minimum_gain,gain);state.maximum_gain=std::max(state.maximum_gain,gain);
        }
        apply_gain(*result,state.adjusted,gain);
    }
    catch(const std::bad_alloc&){*result={};result->failure=NativePortCodecFailure::ResourceExhausted;}
    catch(...){*result={};result->failure=NativePortCodecFailure::InvalidData;}
}
inline void close(void* decoder) noexcept {delete static_cast<Decoder*>(decoder);}
}

// The upstream table must outlive its wrapper and all open decoders. Product
// code uses the pinned FFmpeg provider exclusively; there is no codec fallback.
inline NativePortCodecProvider wrap(const NativePortCodecProvider& upstream) noexcept {
    return {.structure_size=native_port_codec_provider_structure_size,
            .provider_name="sonic-ffmpeg-movie-master",
            .user=const_cast<NativePortCodecProvider*>(&upstream),
            .open=detail::open,.read_next=detail::read,.close=detail::close,
            .deterministic_audio_replay=0u}; // Live volume is intentionally not decoder-replay state.
}
inline const NativePortCodecProvider& provider() noexcept {
    static const auto value=wrap(native_port_ffmpeg_codec_provider());return value;
}
}
