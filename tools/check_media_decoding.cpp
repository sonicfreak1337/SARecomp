// Offline decoder comparison: exact game provider, silent, no game or saves.
#include "katana/runtime/native_port_ffmpeg_codec.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using namespace katana::runtime;
namespace fs=std::filesystem;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
std::uint32_t read_at(void* user,std::uint64_t offset,void* out,std::uint64_t size)noexcept {
    const auto data=*static_cast<const std::span<const std::uint8_t>*>(user);
    if(offset>data.size()||size>data.size()-offset)return 0;
    if(size)std::memcpy(out,data.data()+offset,std::size_t(size));
    return 1;
}
std::uint32_t le32(std::span<const std::uint8_t> data,std::size_t offset){
    require(offset<=data.size()&&data.size()-offset>=4,"AFS table bounds");
    return std::uint32_t(data[offset])|(std::uint32_t(data[offset+1])<<8)|
        (std::uint32_t(data[offset+2])<<16)|(std::uint32_t(data[offset+3])<<24);
}
struct Fingerprint {
    std::uint64_t value=14695981039346656037ull,bytes=0;
    void add(const void* data,std::size_t size){
        for(auto b:std::span(static_cast<const std::uint8_t*>(data),size))value=(value^b)*1099511628211ull;
        bytes+=size;
    }
    void word(std::uint64_t n){for(unsigned i=0;i<8;++i){const auto b=std::uint8_t(n>>(i*8));add(&b,1);}}
};
void decode(const std::string& name,std::span<const std::uint8_t> data,unsigned limit){
    auto& provider=native_port_ffmpeg_codec_provider();
    NativePortCodecOpenRequest request;request.source={&data,data.size(),read_at};
    request.require_audio=1;request.require_video=name.ends_with(".SFD")?1u:0u;
    NativePortCodecOpenResult opened;provider.open(provider.user,&request,&opened);
    if(opened.failure!=NativePortCodecFailure::None||!opened.decoder)
        throw std::runtime_error(name+" open failure "+std::to_string(unsigned(opened.failure))+"/"+std::to_string(opened.provider_error_code));
    struct Close{const NativePortCodecProvider& p;void* d;~Close(){p.close(d);}} close{provider,opened.decoder};
    Fingerprint hash;unsigned samples=0,audio=0,video=0;bool ended=false;
    hash.word(opened.duration_nanoseconds);hash.word(opened.audio_sample_rate);hash.word(opened.audio_channels);
    while(limit==0||samples<limit){
        NativePortCodecReadResult r;provider.read_next(opened.decoder,&r);
        if(r.status==NativePortCodecReadStatus::EndOfStream){ended=true;break;}
        if(r.status!=NativePortCodecReadStatus::Sample)
            throw std::runtime_error(name+" read failure "+std::to_string(unsigned(r.failure))+"/"+std::to_string(r.provider_error_code));
        const auto& s=r.sample;++samples;
        hash.word(unsigned(s.kind));hash.word(s.timestamp_nanoseconds);hash.word(s.duration_nanoseconds);
        if(s.kind==NativePortCodecSampleKind::Audio){
            ++audio;hash.word(s.audio_sample_rate);hash.word(s.audio_channels);hash.word(s.audio_sample_count);
            hash.add(s.audio_samples,std::size_t(s.audio_sample_count)*sizeof(std::int16_t));
        }else if(s.kind==NativePortCodecSampleKind::Video){
            ++video;hash.word(s.video_width);hash.word(s.video_height);hash.word(s.video_stride_bytes);hash.word(s.video_bottom_up);
            hash.word(s.video_display_aspect_numerator);hash.word(s.video_display_aspect_denominator);
            hash.add(s.video_pixels,std::size_t(s.video_byte_count));
        }
    }
    require(samples>0,"empty decoded stream");
    std::cout<<name<<'\t'<<samples<<'\t'<<audio<<'\t'<<video<<'\t'<<ended<<'\t'<<hash.bytes<<'\t'<<std::hex<<hash.value<<std::dec<<'\n';
}
}
int main(int argc,char** argv){try{
    require(argc>=3&&argc<=4,"usage: check_media_decoding directory samples [filename]; 0 drains complete files");
    const auto parsed=std::stoul(argv[2]);require(parsed<=1000000,"sample limit");const auto limit=unsigned(parsed);
    std::vector<fs::path> files;
    for(const auto& e:fs::directory_iterator(argv[1])){
        if(!e.is_regular_file())continue;
        auto extension=e.path().extension().string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return char(std::toupper(c));});
        if((extension==".ADX"||extension==".SFD"||extension==".AFS")&&(argc<4||e.path().filename()==argv[3]))files.push_back(e.path());
    }
    std::sort(files.begin(),files.end());require(!files.empty(),"no media files");
    for(const auto& file:files){
        const auto size=fs::file_size(file);require(size<512ull*1024*1024,"media size bound");
        std::vector<std::uint8_t> bytes((std::size_t(size)));std::ifstream stream(file,std::ios::binary);
        stream.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()));require(bool(stream),"media read");
        if(bytes.size()>=8&&std::memcmp(bytes.data(),"AFS\0",4)==0){
            const auto count=le32(bytes,4);require(count<65536,"AFS count");
            for(unsigned i=0;i<count;++i){const auto offset=le32(bytes,8+i*8),length=le32(bytes,12+i*8);
                require(offset<=bytes.size()&&length<=bytes.size()-offset,"AFS member bounds");
                if(length)decode(file.filename().string()+":"+std::to_string(i),std::span(bytes).subspan(offset,length),limit);
            }
        }else decode(file.filename().string(),bytes,limit);
    }
    return 0;
}catch(const std::exception& error){std::cerr<<"SONIC_MEDIA_CHECK_FAILED "<<error.what()<<'\n';return 1;}}
