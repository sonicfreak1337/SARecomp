#define NOMINMAX
#include <windows.h>
#include "sonic_audio_device.hpp"
#include "sonic_audio_settings.hpp"
#include "sonic_qsound_reverb_medium.hpp"
#include "katana/runtime/native_port_audio_engine.hpp"
#include "katana/runtime/native_port_ffmpeg_codec.hpp"
#include "katana/runtime/native_port_sound_bank.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>
using namespace katana::runtime;
namespace fs=std::filesystem;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::atomic<std::uint64_t> clock_frames{0};
std::mutex output_lock;
std::vector<std::int16_t> output;
struct Device{std::uint64_t origin=0,submitted=0;bool paused=false;std::vector<std::pair<WAVEHDR*,std::uint64_t>> pending;};
std::uint64_t now()noexcept{return 1+clock_frames.load()*1'000'000'000/44100;}
std::uint64_t cursor(Device& d){return std::min(d.submitted,clock_frames.load()-d.origin);}
void tick(Device& d){if(d.paused)return;const auto p=cursor(d);for(auto& [h,end]:d.pending)if(end<=p)h->dwFlags|=WHDR_DONE;}
MMRESULT WINAPI open(LPHWAVEOUT out,UINT,LPCWAVEFORMATEX format,DWORD_PTR,DWORD_PTR,DWORD){
    if(format->nChannels!=2||format->nSamplesPerSec!=44100)return WAVERR_BADFORMAT;
    auto* d=new Device;d->origin=clock_frames.load();*out=reinterpret_cast<HWAVEOUT>(d);return 0;
}
MMRESULT WINAPI close(HWAVEOUT h){delete reinterpret_cast<Device*>(h);return 0;}
MMRESULT WINAPI reset(HWAVEOUT h){auto& d=*reinterpret_cast<Device*>(h);for(auto& p:d.pending)p.first->dwFlags|=WHDR_DONE;d.pending.clear();d.submitted=0;d.origin=clock_frames.load();return 0;}
MMRESULT WINAPI prepare(HWAVEOUT,WAVEHDR* h,UINT){h->dwFlags|=WHDR_PREPARED;return 0;}
MMRESULT WINAPI unprepare(HWAVEOUT h,WAVEHDR* p,UINT){auto& d=*reinterpret_cast<Device*>(h);p->dwFlags&=~WHDR_PREPARED;std::erase_if(d.pending,[&](const auto& v){return v.first==p;});return 0;}
MMRESULT WINAPI write(HWAVEOUT h,WAVEHDR* p,UINT){auto& d=*reinterpret_cast<Device*>(h);tick(d);d.submitted+=p->dwBufferLength/4;d.pending.emplace_back(p,d.submitted);return 0;}
void observe(std::span<const std::int16_t> pcm)noexcept{std::lock_guard guard(output_lock);output.insert(output.end(),pcm.begin(),pcm.end());}
MMRESULT WINAPI position(HWAVEOUT h,LPMMTIME out,UINT){auto& d=*reinterpret_cast<Device*>(h);tick(d);out->wType=TIME_SAMPLES;out->u.sample=DWORD(cursor(d));return 0;}
MMRESULT WINAPI pause(HWAVEOUT h){reinterpret_cast<Device*>(h)->paused=true;return 0;}
MMRESULT WINAPI restart(HWAVEOUT h){reinterpret_cast<Device*>(h)->paused=false;return 0;}

struct Result{std::vector<std::int16_t> pcm;double energy=0;};
std::uint64_t fingerprint(const Result& result){
    std::uint64_t hash=14695981039346656037ull;
    for(auto sample:result.pcm){const auto bits=std::uint16_t(sample);hash=(hash^(bits&255))*1099511628211ull;hash=(hash^(bits>>8))*1099511628211ull;}
    return hash;
}
std::uint64_t cpu_time(){
    FILETIME created,exited,kernel,user;check(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user)!=0,"process clock");
    return ((std::uint64_t(kernel.dwHighDateTime)<<32)|kernel.dwLowDateTime)+((std::uint64_t(user.dwHighDateTime)<<32)|user.dwLowDateTime);
}
Result render(NativePortPlatformServices& platform,const NativePortContentFileBinding& binding,
              unsigned bank,unsigned program,unsigned voice,unsigned effects,unsigned master,bool restore,bool play=true){
    auto settings=sonic::presentation::settings();settings.voice_volume=voice;settings.effects_volume=effects;settings.master_volume=master;
    sonic::presentation::apply_live(settings);clock_frames=0;
    {std::lock_guard guard(output_lock);output.clear();}
    {
        NativePortAudioEngine audio(platform,native_port_ffmpeg_codec_provider());
        NativePortSoundBankConfig cfg;cfg.effect_kernel_provider=sonic::native_qsound_reverb_medium::provider();
        NativePortSoundBankEngine sound(platform,audio,cfg);
        const auto collection=sound.load_collection(binding);
        for(unsigned i=0;i<512;++i){clock_frames+=256;audio.pump();(void)sound.snapshot();}
        {std::lock_guard guard(output_lock);output.clear();}
        NativePortSoundMidiPortConfig port_config;port_config.program_bank=std::uint8_t(bank);port_config.program=std::uint8_t(program);
        const auto port=sound.open_midi_port(collection,port_config);
        if(play)(void)sound.midi_note_on(port,60,127);
        if(restore){
            audio.set_output_paused(true);
            const auto state=sound.capture_development_state();
            sound.restore_development_state(state);
            audio.set_output_paused(false);
        }
        // The endpoint is entirely synthetic: no Windows sound is played.
        for(unsigned i=0;i<48;++i){clock_frames+=256;audio.pump();(void)sound.snapshot();}
    }
    std::lock_guard guard(output_lock);Result result;result.pcm=output;for(auto sample:output)result.energy+=std::abs(int(sample));return result;
}
}
int main(int argc,char** argv){try{
    check(argc==9,"content root, fresh data root, logical id, file, SHA, size, bank, program required");
    const auto root=fs::absolute(argv[2]);check(!fs::exists(root),"test directory must be new");fs::create_directories(root);
    _putenv_s("KATANA_PORT_BACKGROUND_TEST","1");
    sonic::audio_device::Api api;api.open=open;api.close=close;api.reset=reset;api.prepare=prepare;api.unprepare=unprepare;
    api.write=write;api.position=position;api.pause=pause;api.restart=restart;api.now=now;api.observe_pcm=observe;
    sonic::audio_device::set_test_api(&api);
    struct Clear{~Clear(){sonic::audio_device::set_test_api(nullptr);}} clear;
    sonic::presentation::initialize(root/"game.exe");
    NativePortPlatformConfig platform_config;platform_config.content_root=fs::absolute(argv[1]);platform_config.user_data_root=root/"data";
    platform_config.project_id="sonic-audio-bus-test";platform_config.require_gamepad_backend=false;
    NativePortPlatformServices platform(platform_config);
    const NativePortContentFileBinding binding{argv[3],argv[4],argv[5],0,std::stoull(argv[6])};
    const auto bank=unsigned(std::stoul(argv[7])),program=unsigned(std::stoul(argv[8]));
    const auto cpu_started=cpu_time();
    const auto full=render(platform,binding,bank,program,100,100,100,false);
    const auto voices=render(platform,binding,bank,program,100,0,100,false);
    const auto effects=render(platform,binding,bank,program,0,100,100,false);
    const auto half=render(platform,binding,bank,program,100,100,50,false);
    const auto restored=render(platform,binding,bank,program,0,0,100,true);
    const auto quiet=render(platform,binding,bank,program,100,100,100,false,false);
    const auto quiet_restored=render(platform,binding,bank,program,100,100,100,true,false);
    const auto master_muted=render(platform,binding,bank,program,100,100,0,false);
    const auto render_cpu_ms=(cpu_time()-cpu_started)*.0001;
    std::cout<<"PCM_FINGERPRINTS";
    for(const auto* result:{&full,&voices,&effects,&half,&restored,&quiet,&quiet_restored,&master_muted})std::cout<<' '<<fingerprint(*result);
    std::cout<<" render_cpu_ms="<<render_cpu_ms<<'\n';
    std::cout<<binding.logical_id<<" bank="<<bank<<" program="<<program<<" full="<<full.energy<<" voices="<<voices.energy<<" effects="<<effects.energy<<" half="<<half.energy<<" quiet="<<quiet.energy<<'\n';
    check(full.energy>quiet.energy+10000,"fixture did not produce audible PCM");
    // Compare to the same loaded, silent bank: its authored DSP delay line
    // has a startup tail even without notes. Do not mistake that for a voice.
    check((voices.pcm==quiet.pcm&&effects.pcm==full.pcm)||(effects.pcm==quiet.pcm&&voices.pcm==full.pcm),"program not assigned exclusively to one bus");
    check(std::abs(half.energy/full.energy-.5)<.035,"master gain missing or applied more than once");
    check(restored.pcm==quiet_restored.pcm,"restored active note lost its bus");
    check(master_muted.energy==0,"master mute left DSP output audible");
    std::cout<<"PASS actual SoundBank PCM routing and state restore\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
