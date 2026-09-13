#include "sonic_menu_runtime.hpp"
#include "sonic_menu.hpp"
#include "sonic_menu_text.hpp"
#include "sonic_menu_test_input.hpp"
#include "sonic_menu_policy.hpp"
#include "sonic_diagnostics.hpp"
#include "sonic_profiles.hpp"
#include "sonic_language.hpp"
#include "sonic_quit_prompt.hpp"
#include "katana/runtime/runtime.hpp"
#include "katana/runtime/native_port_content.hpp"
#include "katana/runtime/native_port_audio_engine.hpp"
#include "katana/runtime/native_port_ffmpeg_codec.hpp"
#include <atomic>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include <map>
#include <cwctype>
namespace sonic::menu {
namespace {
using namespace katana::runtime;
std::atomic<bool> external_request{false};
std::atomic<bool> open_available{false},modal_open{false};
std::atomic<int> observed_language{1};
bool requested=false,was_focused=false,was_connected=false,previous_open=false;
InputReleaseGate release_input;
std::string pause_reason;
std::filesystem::path config_path;
std::optional<presentation::Settings> restart;
int restart_lang=1;
std::uint32_t word(CpuState& c,std::uint32_t p){return c.memory.read_u32(canonical_physical_address(p));}
bool pointer(std::uint32_t p){const auto n=canonical_physical_address(p);return !(p&3)&&n>=0x0C010000&&n<=0x0CFFFF80;}
bool gameplay(NativePortContext& c) noexcept {
    try {return c.cpu && word(*c.cpu,0x8C7608B0)==15u;}catch(...){return false;}
}
std::wstring widen(std::string_view s){return std::wstring(s.begin(),s.end());}
std::wstring preview(const profiles::Preview& p,int language){
    std::wstring result=std::to_wstring(p.bytes/1024)+L" KiB · #"+std::to_wstring(p.generation);
    result+=L"\n";
    for(const auto& f:p.files){if(result.back()!=L'\n')result+=L" + ";result+=f=="SONICADV_ALF"?L"Chao":f=="SONICADV_INT"?std::wstring(text("story_data",language)):widen(f);}
    return result;
}
struct FileBrowser {
    std::map<std::string,std::filesystem::path> choices;
    void show(Model& model,const std::filesystem::path& directory){
        namespace fs=std::filesystem;choices.clear();std::vector<Choice> rows;
        const auto add=[&](const fs::path& p,std::wstring label){const auto id="file-"+std::to_string(choices.size());choices[id]=p;rows.push_back({id,std::move(label),p.wstring()});};
        if(directory.empty()){
            // Enumerate names of drive roots only; no recursive scan.
            for(wchar_t drive=L'A';drive<=L'Z';++drive){const auto p=fs::path(std::wstring(1,drive)+L":/");std::error_code e;if(fs::exists(p,e))add(p,p.wstring());}
        }else {
            add(directory==directory.root_path()?fs::path{}:directory.parent_path(),std::wstring(text("parent_folder",model.language())));
            std::vector<std::pair<bool,fs::path>> entries;std::error_code e;
            for(const auto& item:fs::directory_iterator(directory,fs::directory_options::skip_permission_denied,e)){
                if(entries.size()>=500)break;
                if(item.is_symlink(e))continue;
                if(item.is_directory(e))entries.emplace_back(true,item.path());
                else {auto ext=item.path().extension().wstring();std::transform(ext.begin(),ext.end(),ext.begin(),::towlower);
                    if(item.is_regular_file(e)&&(ext==L".sasave"||ext==L".ksave"))entries.emplace_back(false,item.path());}
            }
            if(e)throw std::filesystem::filesystem_error("save-import-directory",directory,e);
            std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.first!=b.first?a.first>b.first:a.second.filename()<b.second.filename();});
            for(const auto& [folder,p]:entries)add(p,p.filename().wstring()+(folder?L" /":L""));
        }
        model.choose("import_browser",std::move(rows));
    }
};
std::wstring visual_key(const Model& m,unsigned width,unsigned height){
    auto key=m.heading()+m.dialog()+m.footer()+m.description()+std::to_wstring(m.selected())+std::to_wstring(m.confirm_selected())+
        std::to_wstring(width)+L"x"+std::to_wstring(height);
    for(const auto& r:m.rows())key+=r.label+r.value;return key;
}
struct Music {
    std::unique_ptr<NativePortAudioEngine> engine;
    NativePortAudioVoiceHandle voice;
    float previous_gain=-1;bool previous_paused=false;
    explicit Music(NativePortContext& c) {
        NativePortAudioEngineConfig config;config.maximum_voices=1;
        engine=std::make_unique<NativePortAudioEngine>(*c.platform,native_port_ffmpeg_codec_provider(),config);
        const NativePortContentFileBinding binding{"OPTION.ADX",std::filesystem::path("SONICAD/OPTION.ADX"),
            "sha256:dfa32df863f1b64ab5c99fd173ee5c8bb627ada5a971bf59056d66a403f640d1",0,2709504};
        const auto metadata=inspect_native_port_adx_content(*c.platform,binding);
        voice=engine->create_voice(binding,native_port_adx_voice_config(metadata,config.output_format.sample_rate,0));
        engine->play(voice);
    }
    void tick(bool focused,bool paused){
        const auto& s=presentation::settings();const auto gain=(s.mute_background&&!focused)?0.0f:s.master_volume*.01f*s.music_volume*.01f;
        if(gain!=previous_gain){engine->set_gain_pan(voice,gain,0);previous_gain=gain;}
        if(paused!=previous_paused){engine->set_output_paused(paused);previous_paused=paused;}
        if(!paused)engine->pump();
    }
};
}
void initialize(const std::filesystem::path& executable){
    config_path=presentation::configuration_path(executable);requested=previous_open=false;release_input.reset();restart.reset();pause_reason.clear();
    load_background(executable.parent_path()/"assets/ui/options-background.png");
}
void request_open() noexcept {external_request=true;}
bool available() noexcept {return open_available.load()&&!modal_open.load();}
int effective_text_language() noexcept {const auto preference=presentation::settings().text_language;return preference<0?observed_language.load():preference;}
std::optional<presentation::Settings> take_restart(){return std::exchange(restart,std::nullopt);}
int restart_language() noexcept {return restart_lang;}
bool original_options_ready(NativePortContext& c) noexcept {
    try {
        if(!c.cpu || !c.loaded_aot || word(*c.cpu,0x8C7608B0)!=11)return false;
        if(!c.loaded_aot->validate_bound_entry(0x8C9001A0))return false;
        const auto entry=c.loaded_aot->active_entry_for_address(0x8C9001A0);
        if(!entry || !entry->active || !entry->lifecycle_generation || entry->module_sha256!=
            "sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5" ||
            canonical_physical_address(entry->runtime_start)!=0x0C900000 || entry->module_size!=0xE4648 || entry->source_offset!=0x1A0)return false;
        auto& cpu=*c.cpu;const auto controller=word(cpu,0x8C960AE8);if(!pointer(controller))return false;
        const auto w=word(cpu,controller+0x2C);if(!pointer(w)||word(cpu,w+4)!=8||word(cpu,w+12)!=8||word(cpu,w+20)!=0 ||
            cpu.memory.read_u16(canonical_physical_address(w+28))||cpu.memory.read_u8(canonical_physical_address(w+30)))return false;
        const auto task=word(cpu,0x8C9645D0);if(!pointer(task)||word(cpu,task+0x10)!=0x8C9078E0)return false;
        const auto work=word(cpu,task+0x2C);return pointer(work)&&word(cpu,work)==2&&word(cpu,work+24)==0&&word(cpu,work+20)==0xFFFFFFFF;
    }catch(...){return false;}
}
bool observe(NativePortContext& c,const input::Snapshot& s,bool suppressed){
    diagnostics::frame=c.frame_index;
    const auto& config=presentation::settings();const bool open=input::held(s,input::Action::Settings,config.bindings);
    const bool safe=gameplay(c)&&!suppressed&&!input::replay();
    if(c.cpu)observed_language=quit_prompt::effective_language(*c.cpu,config.text_language);
    open_available=safe||(!suppressed&&original_options_ready(c));
    if(safe && s.focused && open&&!previous_open)requested=true;
    if(safe && config.pause_focus_loss && was_focused&&!s.focused){requested=true;pause_reason="focus_pause";}
    if(safe && config.pause_controller_loss && was_connected&&!s.connected){requested=true;pause_reason="controller_pause";}
    if(external_request.exchange(false) && (safe||original_options_ready(c)))requested=true;
    was_focused=s.focused;was_connected=s.connected;previous_open=open;
    const bool awaiting_release=release_input.consume(s);
    return requested||awaiting_release||(!suppressed&&original_options_ready(c));
}
bool pending(NativePortContext& c) noexcept {return requested||original_options_ready(c);}
bool run(NativePortContext& c,const Services& services){
    const bool title=original_options_ready(c);if(!title&&!requested)return false;
    requested=false;if(!c.graphics || !c.platform || !c.host || !c.cpu)return false;
    if(!title&&!gameplay(c)){pause_reason.clear();return false;}
    modal_open=true;
    struct ModalClosed {~ModalClosed(){modal_open=false;}} modal_closed;
    const auto original=presentation::settings();const int lang=quit_prompt::effective_language(*c.cpu,original.text_language);
    const std::array<unsigned,3> game_language{word(*c.cpu,language::text_global),word(*c.cpu,language::voice_global),
        c.cpu->memory.read_u8(canonical_physical_address(language::subtitles_global))};
    Model model(original,lang,title);if(!pause_reason.empty())model.message(std::wstring(text(pause_reason,lang)));pause_reason.clear();
    const auto started=c.host->monotonic_time_nanoseconds(),instructions=c.cpu->retired_guest_instructions,frame=c.frame_index;
    input::set_modal(true);
    struct Restore {NativePortContext& c;const Services& service;std::uint64_t start;
        ~Restore(){input::set_modal(false);release_input.arm();try{if(service.resume)service.resume(c.host->monotonic_time_nanoseconds()-start);}catch(...){}}
    } restore{c,services,started};
    if(services.suspend_audio)services.suspend_audio();
    std::unique_ptr<Music> music;
    try {music=std::make_unique<Music>(c);}catch(const std::exception& e){std::cerr<<"SONIC_OPTIONS music_error="<<e.what()<<'\n';model.message(std::wstring(text("music_error",lang)));}
    std::wstring previous_image;bool leave=false,sound_test=false;TestInput test_input;FileBrowser browser;
    std::map<std::string,std::string> preview_digests;std::string export_digest;
    std::cerr<<"SONIC_OPTIONS opened title="<<title<<" language="<<lang<<" frame="<<frame<<'\n';
    while(!leave && c.stop_reason==NativePortStopReason::None){
        if(try_accept_native_port_host_stop(c))break;
        const auto now=c.host->monotonic_time_nanoseconds();
        if(c.host_deadline_nanoseconds && now>=c.host_deadline_nanoseconds){(void)request_native_port_host_stop(c,NativePortStopReason::HostDeadline);break;}
        const auto life=c.host->poll_lifecycle();if(life==NativePortLifecycleState::Shutdown){(void)request_native_port_host_stop(c,NativePortStopReason::HostRequested);break;}
        const auto extent=c.graphics->layout().output_extent;
        // Modal test input owns only this host menu, so it must not advance
        // the guest replay's poll stream while the guest is frozen.
        const auto test_sample=test_input.sample((now-started)*1e-9,extent.width,extent.height);
        const auto sampled=test_sample?*test_sample:input::sample(input::poll_host(*c.platform),true);
        const auto result=model.update(sampled,(now-started)*1e-9);
        if(test_sample && (result.command!=Command::None||result.changed))
            std::cerr<<"SONIC_OPTIONS_TEST action="<<unsigned(result.command)<<" changed="<<result.changed<<" selected="<<model.selected()<<'\n';
        if(result.changed){
            presentation::apply_live(model.value());const auto& v=model.value();
            guest_write_u32(*c.cpu,language::text_global,v.text_language<0?game_language[0]:unsigned(v.text_language),CodeWriteSource::Copy);
            guest_write_u32(*c.cpu,language::voice_global,v.voice_language<0?game_language[1]:unsigned(v.voice_language),CodeWriteSource::Copy);
            guest_write_u8(*c.cpu,language::subtitles_global,std::uint8_t(v.subtitles<0?game_language[2]:unsigned(v.subtitles)),CodeWriteSource::Copy);
        }
        try {
            switch(result.command){
            case Command::None:break;
            case Command::Close:leave=true;break;
            case Command::Save:presentation::save_settings(config_path,model.value());model.saved();model.message(std::wstring(text("saved",model.language())));break;
            case Command::SoundTest:
                if(model.dirty())model.message(std::wstring(text("save_before_sound",model.language())));
                else {sound_test=true;leave=true;}break;
            case Command::Restart:restart=model.value();leave=true;break;
            case Command::Backup:model.message(profiles::backup().wstring());break;
            case Command::PreviewExport:{const auto info=profiles::active_preview();export_digest=info.digest;model.ask("export_preview",Command::Export,preview(info,model.language()));break;}
            case Command::Export:model.message(profiles::export_save(export_digest).wstring());break;
            case Command::Diagnostics:model.message(profiles::export_diagnostics().wstring());break;
            case Command::NewProfile:{const auto id=profiles::create();model.message(std::wstring(text("created",model.language()))+L"\n"+widen(id));break;}
            case Command::SwitchProfile:
                if(result.argument.empty()){
                    model.choose("switch_profile",profile_choices(model.language(),original.active_profile));
                }else {
                    (void)profiles::profile_preview(result.argument); // Revalidate after preview, without writing it.
                    restart=model.value();restart->active_profile=result.argument;leave=true;
                }break;
            case Command::BrowseImport:
                if(result.argument.empty())browser.show(model,profiles::library_root()/"imports");
                else {
                    const auto file=browser.choices.at(result.argument);
                    if(file.empty()||std::filesystem::is_directory(file))browser.show(model,file);
                    else {const auto info=profiles::import_candidate(file);preview_digests.clear();preview_digests[info.id]=info.digest;
                        model.choose("import_save",{{info.id,file.filename().wstring(),preview(info,model.language())}});}
                }break;
            case Command::Import:case Command::Restore:
                if(result.argument.empty()){
                    preview_digests.clear();std::vector<Choice> choices;
                    if(result.command==Command::Import)choices.push_back({"browse_files",std::wstring(text("browse_files",model.language())),L".sasave / .ksave"});
                    for(const auto& p:profiles::snapshots(result.command==Command::Import)){
                        preview_digests[p.id]=p.digest;choices.push_back({p.id,widen(p.id),preview(p,model.language())});
                    }
                    if(choices.empty())model.message(std::wstring(text("no_backups",model.language()))+L"\n"+(profiles::library_root()/(result.command==Command::Import?"imports":"backups")).wstring());
                    else model.choose(result.command==Command::Import?"import_save":"restore_backup",std::move(choices));
                }else {profiles::stage_restore(result.argument,result.command==Command::Import,preview_digests.at(result.argument));restart=model.value();leave=true;}break;
            }
        }catch(const std::exception& e){std::cerr<<"SONIC_OPTIONS operation_error="<<e.what()<<'\n';model.message(std::wstring(text("operation_error",model.language())));}
        if(music)music->tick(sampled.focused,life==NativePortLifecycleState::Paused);
        if(services.apply_audio)services.apply_audio(sampled.focused);
        profiles::poll(now);
        model.status(profiles::backup_status().state,recovery::audio().state);
        if(leave)break;
        const auto key=visual_key(model,extent.width,extent.height);
        if(key!=previous_image && life!=NativePortLifecycleState::Paused){
            auto pixels=rasterize(model,extent.width,extent.height);NativePortImageView view;
            view.extent={pixels.width,pixels.height};view.format=NativePortTextureFormat::Rgba8Unorm;view.stride_bytes=pixels.width*4;view.pixels=pixels.pixels;
            c.graphics->present_image(view,NativePortViewportTarget::Ui,NativePortImageFit::Stretch);previous_image=key;
        }else if(life!=NativePortLifecycleState::Paused)c.host->present_frame(c.frame_index);
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    music.reset();
    std::cerr<<"SONIC_OPTIONS closed guest_instructions_delta="<<(c.cpu->retired_guest_instructions-instructions)<<" guest_frame_delta="<<(c.frame_index-frame)<<'\n';
    if(restart){restart_lang=model.language();(void)request_native_port_host_stop(c,NativePortStopReason::HostRequested);}
    else if(title && c.stop_reason==NativePortStopReason::None && services.original_transition){
        if(!services.original_transition(sound_test?5:7))throw std::runtime_error("sonic-options-original-transition");
        std::cerr<<"SONIC_OPTIONS original_destination="<<(sound_test?5:7)<<'\n';
    }
    return true;
}
}
