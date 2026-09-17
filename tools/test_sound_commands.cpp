#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include "sonic_audio_device.hpp"
#endif
#include "sonic_sound_commands.hpp"
#include "sonic_qsound_reverb_medium.hpp"
#include "katana/runtime/native_port_ffmpeg_codec.hpp"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace katana::runtime;
namespace fs=std::filesystem;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>void rejected(F&& f,const char* message){bool caught=false;try{f();}catch(...){caught=true;}check(caught,message);}
void env(const char* name,const char* value){
#ifdef _WIN32
    _putenv_s(name,value);
#else
    setenv(name,value,1);
#endif
}
#ifdef _WIN32
MMRESULT WINAPI unavailable(LPHWAVEOUT,UINT,LPCWAVEFORMATEX,DWORD_PTR,DWORD_PTR,DWORD){return MMSYSERR_NODRIVER;}
#endif
int main(int argc,char** argv)try{
    check(argc==9,"content root, fresh data root, logical id, file, SHA, size, bank, program required");
    const auto root=fs::absolute(argv[2]);check(!fs::exists(root),"test directory must be new");fs::create_directories(root);
    env("KATANA_PORT_BACKGROUND_TEST","1");env("SDL_AUDIODRIVER","dummy");
    env("SARECOMP_SOUND_METADATA_CACHE","1");env("SARECOMP_SOUND_METADATA_VERIFY","0");env("SARECOMP_DEFERRED_MIDI_NOTES","1");
    // Same address/slot is not an identity. Negative results, generations,
    // distinct query kinds and an epoch reset must all remain distinguishable.
    sonic::audio::SoundMetadataCache cache;
    NativePortSoundCollectionHandle fake{3,7};unsigned queries=0;
    auto no=[&]{++queries;return false;};auto yes=[&]{++queries;return true;};
    check(!cache.read(fake,0,1,2,no)&&!cache.read(fake,0,1,2,yes)&&queries==1,"negative cache");
    check(cache.read({3,8},0,1,2,yes)&&queries==2,"generation reuse");
    check(!cache.read({3,8},1,1,2,no)&&queries==3,"query kind");
    cache.clear();check(cache.read({3,8},1,1,2,yes)&&queries==4,"restored epoch");
    cache.clear();rejected([&]{cache.read(fake,0,0,0,[]{throw std::runtime_error("query");return true;});},"exception accepted");
    check(!cache.read(fake,0,0,0,no)&&queries==5,"exception cached");
#ifdef _WIN32
    sonic::audio_device::Api api;api.open=unavailable;sonic::audio_device::set_test_api(&api);
    struct Clear{~Clear(){sonic::audio_device::set_test_api(nullptr);}}clear;
#endif
    NativePortPlatformConfig pc;pc.content_root=fs::absolute(argv[1]);pc.user_data_root=root/"data";
    pc.project_id="sonic-sound-commands-test";pc.require_gamepad_backend=false;
    NativePortPlatformServices platform(pc);
    NativePortAudioEngine audio(platform,native_port_ffmpeg_codec_provider());
    NativePortSoundBankConfig config;config.effect_kernel_provider=sonic::native_qsound_reverb_medium::provider();
    NativePortSoundBankEngine sound(platform,audio,config);
    const NativePortContentFileBinding binding{argv[3],argv[4],argv[5],0,std::stoull(argv[6])};
    const auto bank=std::uint8_t(std::stoul(argv[7])),program=std::uint8_t(std::stoul(argv[8]));
    const bool serial=audio.command_queue_snapshot().mode==NativePortAudioCommandQueueMode::SerialReference;
    auto collection=sound.load_collection(binding);
    check(sound.has_program(collection,bank,program),"fixture program missing");
    const bool sequence=sound.has_sequence(collection,15,65535);
    const auto before=audio.command_queue_snapshot();
    for(unsigned i=0;i<2000;++i){check(sound.has_program(collection,bank,program),"positive query changed");
        check(sound.has_sequence(collection,15,65535)==sequence,"negative query changed");}
    const auto after=audio.command_queue_snapshot();
    if(!serial)check(after.submitted_commands==before.submitted_commands&&after.ack_waits==before.ack_waits,"metadata queued commands");
    NativePortSoundMidiPortConfig mc;mc.program_bank=bank;mc.program=program;
    auto port=sound.open_midi_port(collection,mc);
    const auto ordinary=sound.midi_note_on(port,60,127);check(bool(ordinary),"ordinary API lost handle");
    sound.midi_stop(port);(void)sound.midi_port_snapshot(port);
    const auto note_before=audio.command_queue_snapshot();
    sonic::audio::start_note_without_handle(sound,port,60,127);
    const auto note_after=audio.command_queue_snapshot();
    if(!serial)check(note_after.submitted_commands==note_before.submitted_commands+1&&note_after.ack_waits==note_before.ack_waits,"deferred note waited for ACK");
    check(!sonic::audio::deferred_note_requested(&sound),"deferred scope leaked");
    check(sound.midi_port_snapshot(port).active_notes==1,"note ordering lost");
    sound.midi_note_off(port,60);sound.midi_stop(port);
    check(sound.midi_port_snapshot(port).active_notes==0,"note-off/stop ordering lost");
    bool foreign_rejected=false;
    std::thread foreign([&]{try{sonic::audio::start_note_without_handle(sound,port,60,127);}catch(...){foreign_rejected=true;}});foreign.join();
    check(foreign_rejected,"foreign producer accepted");
    sound.close_midi_port(port);sound.unload_collection(collection);
    rejected([&]{(void)sound.has_program(collection,bank,program);},"stale cached handle accepted");
    const auto previous=collection;collection=sound.load_collection(binding);
    check(collection.slot!=previous.slot||collection.generation!=previous.generation,"generation did not advance");
    check(sound.has_program(collection,bank,program),"reloaded program missing");
    audio.set_output_paused(true);
    const auto audio_state=audio.capture_development_state();const auto bank_state=sound.capture_development_state();
    sound.unload_all_collections();audio.restore_development_state(audio_state);sound.restore_development_state(bank_state);
    const auto misses=sonic::audio::sound_command_counts.metadata_misses;
    check(sound.has_program(collection,bank,program),"restored program missing");
    if(!serial)check(sonic::audio::sound_command_counts.metadata_misses==misses+1,"restore reused cached epoch");
    sound.reset();rejected([&]{(void)sound.has_program(collection,bank,program);},"reset reused cached handle");
    collection=sound.load_collection(binding);check(sound.has_program(collection,bank,program),"new epoch missing");
    if(serial)rejected([&]{sonic::audio::start_note_without_handle(sound,{},60,127);},"serial failure deferred");
    else{
        sonic::audio::start_note_without_handle(sound,{},60,127);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(audio.command_queue_snapshot().first_error==NativePortAudioCommandQueueFailure::None&&std::chrono::steady_clock::now()<deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        check(audio.command_queue_snapshot().first_error!=NativePortAudioCommandQueueFailure::None,"deferred failure disappeared");
        rejected([&]{(void)sound.has_program(collection,bank,program);},"cache hid terminal failure");
    }
    const auto c=sonic::audio::sound_command_counts;
    std::cout<<"SOUND_COMMAND_TEST_OK serial="<<serial<<" metadata_queries=4000 queued="<<after.submitted_commands-before.submitted_commands
        <<" metadata_hits="<<c.metadata_hits<<" misses="<<c.metadata_misses<<" deferred_notes="<<c.deferred_notes<<" lifecycle=ok terminal=ok\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"SOUND_COMMAND_TEST_FAILED "<<e.what()<<'\n';return 1;}
