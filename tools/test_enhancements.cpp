#define NOMINMAX
#include <windows.h>
#include "sonic_menu.hpp"
#include "sonic_menu_text.hpp"
#include "sonic_menu_policy.hpp"
#include "sonic_profiles.hpp"
#include "sonic_audio_settings.hpp"
#include "sonic_subtitles.hpp"
#include "sonic_startup.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs=std::filesystem;
using namespace sonic;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>void rejects(F&& f,const char* why){bool failed=false;try{f();}catch(...){failed=true;}check(failed,why);}
std::vector<std::byte> read(const fs::path& path){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f),"read");std::vector<std::byte> bytes(std::size_t(f.tellg()));f.seekg(0);f.read(reinterpret_cast<char*>(bytes.data()),bytes.size());check(bool(f),"read all");return bytes;}
void write(const fs::path& path,std::span<const std::byte> bytes){fs::create_directories(path.parent_path());std::ofstream f(path,std::ios::binary|std::ios::trunc);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(bool(f),"write");}
template<class T>void append(std::vector<std::byte>& b,T v){for(unsigned i=0;i<sizeof(T);++i)b.push_back(std::byte(v>>(i*8)));}
void string32(std::vector<std::byte>& b,std::string_view s){append(b,std::uint32_t(s.size()));for(char c:s)b.push_back(std::byte(c));}
void resign(std::vector<std::byte>& bytes,unsigned schema){
    for(unsigned i=0;i<4;++i)bytes[24+i]=std::byte(schema>>(8*i));
    std::vector<std::byte> digest;string32(digest,"katana-native-save-v2");string32(digest,"sonic-adventure-pal-v1003");string32(digest,"sonic-adventure-pal-v1003-vmu.save-c0-s0");append(digest,schema);
    digest.insert(digest.end(),bytes.begin()+32,bytes.begin()+48);digest.insert(digest.end(),bytes.begin()+80,bytes.end());const auto hex=startup::digest(digest);
    for(unsigned i=0;i<32;++i)bytes[48+i]=std::byte(std::stoul(hex.substr(i*2,2),nullptr,16));
}
struct Driver {
    input::Snapshot s{};double time=0;
    Driver(){s.focused=true;s.client_width=1920;s.client_height=1080;}
    menu::Result tick(menu::Model& m){time+=.1;return m.update(s,time);}
    menu::Result key(menu::Model& m,unsigned k){s.keys[k]=true;const auto r=tick(m);s.keys[k]=false;tick(m);return r;}
};
int main(int argc,char** argv){
    try{
        check(argc==3,"test root and authentic read-only save required");const auto root=fs::absolute(argv[1]);check(!fs::exists(root),"test root must be fresh");fs::create_directories(root);
        SetEnvironmentVariableW(L"KATANA_PORT_BACKGROUND_TEST",L"1");SetEnvironmentVariableW(L"SARECOMP_DISPLAY_CONFIG",nullptr);
        presentation::Settings settings;settings.setup_complete=true;settings.text_language=4;settings.renderer=rendering::Renderer::Vulkan;
        presentation::save_settings(root/"sonic-display.ini",settings);check(presentation::read_settings(root/"sonic-display.ini")==settings,"settings roundtrip");
        check(settings.gameplay_timing==1,"Recompiled timing must remain the default");
        auto original_timing=settings;original_timing.gameplay_timing=0;
        presentation::save_settings(root/"original-timing.ini",original_timing);
        check(presentation::read_settings(root/"original-timing.ini")==original_timing,"original cadence roundtrip");
        check(presentation::needs_restart(settings,original_timing),"cadence switch must require restart");
        {std::ofstream legacy(root/"sonic-display.ini",std::ios::app);legacy<<"hud_scale=150\nhud_margin_x=48\nhud_margin_y=36\n";}
        check(presentation::read_settings(root/"sonic-display.ini")==settings,"retired HUD settings affected configuration");
        {std::ofstream legacy(root/"sonic-display.ini",std::ios::app);legacy<<"interpolation=1\n";}
        check(presentation::read_settings(root/"sonic-display.ini")==settings,"retired interpolation affected configuration");
        {std::ofstream legacy(root/"sonic-display.ini",std::ios::app);legacy<<"anisotropy=16\n";}
        check(presentation::read_settings(root/"sonic-display.ini")==settings,"retired anisotropy remained enabled");
        static_assert(presentation::Settings::interpolation==0);
        {std::ofstream legacy(root/"sonic-display.ini",std::ios::app);legacy<<"presentation_fps=144\n";}
        check(presentation::read_settings(root/"sonic-display.ini")==settings,"retired FPS limit remained active");
        check(settings.presentation_fps==60,"default output must be 60 FPS without VSync");
        {std::ofstream legacy(root/"sonic-display.ini",std::ios::app);legacy<<"render_percent=200\n";}
        check(presentation::read_settings(root/"sonic-display.ini")==settings,"retired supersampling did not migrate to 100 percent");
        auto unsupported_scale=settings;unsupported_scale.render_percent=125;
        rejects([&]{presentation::validate_settings(unsupported_scale);},"supersampling above 100 percent remains enabled");
        presentation::save_settings(root/"sonic-display.ini",settings);
        const auto serialized=read(root/"sonic-display.ini");
        check(std::string_view(reinterpret_cast<const char*>(serialized.data()),serialized.size()).find("hud_")==std::string_view::npos,"retired HUD settings saved again");
        check(std::string_view(reinterpret_cast<const char*>(serialized.data()),serialized.size()).find("interpolation")==std::string_view::npos,"retired interpolation saved again");
        check(std::string_view(reinterpret_cast<const char*>(serialized.data()),serialized.size()).find("presentation_fps")==std::string_view::npos,"retired FPS selection saved again");
        presentation::initialize(root/"game.exe");auto changed=settings;changed.master_volume=50;changed.music_volume=20;changed.voice_volume=80;changed.effects_volume=40;changed.vsync=1;changed.width=2560;changed.mouse_camera=1;
        changed.gameplay_timing=0;
        presentation::apply_live(changed);check(presentation::settings().width==settings.width&&presentation::settings().vsync==settings.vsync&&presentation::settings().gameplay_timing==settings.gameplay_timing,"restart fields leaked live");
        check(std::abs(audio::factor(audio::Bus::Music)-.1f)<1e-6&&std::abs(audio::factor(audio::Bus::Voice)-.4f)<1e-6&&std::abs(audio::factor(audio::Bus::Effects)-.2f)<1e-6,"audio buses compounded");
        check(audio::adx_bus("EVENT_ADX_US.AFS")==audio::Bus::Voice&&audio::program_bus("sa-pal-v1003-mlt-119",6,0)==audio::Bus::Voice,"audio identity routing");
        check(audio::program_bus("sa-pal-v1003-mlt-001",3,0)==audio::Bus::Effects&&audio::program_bus("sa-pal-v1003-mlt-001",6,0)==audio::Bus::Voice,"mixed Chao collection routed as one bus");
        check(audio::program_bus("sa-pal-v1003-mlt-013",4,0)==audio::Bus::Effects&&audio::program_bus("sa-pal-v1003-mlt-013",1,0)==audio::Bus::Voice,"ALT relocated voice bank lost");
        check(audio::program_bus("sa-pal-v1003-mlt-107",1,29)==audio::Bus::Voice&&audio::program_bus("sa-pal-v1003-mlt-108",1,30)==audio::Bus::Voice&&audio::program_bus("sa-pal-v1003-mlt-107",1,31)==audio::Bus::Effects,"Sky Deck localized announcements not separated");
        presentation::apply_live(settings);
        auto readable=settings;readable.subtitle_background=1;
        const auto authored=subtitles::layout(42,390,340,40,readable);
        check(authored.target_center==authored.center&&authored.target_bottom==authored.bottom&&authored.scale==1,"subtitle backplate moved authored text");
        readable.subtitle_scale=200;
        const auto tall=subtitles::layout(8,20,630,460,readable);
        check(tall.target_bottom<=464.001f&&tall.target_bottom-460*tall.scale>=15.999f,"subtitle safe vertical fit");
        check(tall.target_center-315*tall.scale>=15.999f&&tall.target_center+315*tall.scale<=624.001f,"subtitle safe horizontal fit");
        Driver d;menu::Model model(settings,4,true);d.tick(model);
        const auto rect=model.row_rect(0,1920,1080);d.s.cursor_x=rect.left+20;d.s.cursor_y=rect.top+20;d.s.mouse[1]=true;d.tick(model);
        check(model.rows()[0].id=="display","mouse-down activated menu");d.s.cursor_x=0;d.s.mouse[1]=false;d.tick(model);check(model.rows()[0].id=="display","drag-out did not cancel");
        d.s.cursor_x=rect.left+20;d.s.mouse[1]=true;d.tick(model);d.s.mouse[1]=false;d.tick(model);check(model.rows()[0].id=="renderer","release-inside did not activate");
        menu::Model confirm(settings,4,false);confirm.ask("keep_display",menu::Command::Save);d.s={};d.s.focused=true;d.s.client_width=1920;d.s.client_height=1080;d.tick(confirm);
        const auto yes=menu::Model::confirm_rect(true,1920,1080);d.s.cursor_x=yes.left+20;d.s.cursor_y=yes.top+20;d.s.mouse[1]=true;
        check(d.tick(confirm).command==menu::Command::None&&confirm.modal(),"destructive mouse press accepted");d.s.mouse[1]=false;check(d.tick(confirm).command==menu::Command::Save,"explicit confirm release failed");
        menu::Model reconnect(settings,4,false);reconnect.ask("keep_display",menu::Command::Save);d.s={};d.s.focused=true;d.tick(reconnect);d.key(reconnect,37);
        d.s.connected=true;d.s.connection_changed=true;d.s.pad=1u<<10;check(d.tick(reconnect).command==menu::Command::None,"reconnect activated selection");d.s.connection_changed=false;d.s.pad=0;d.tick(reconnect);
        menu::Model binds(settings,4,false);binds.choose("bindings",{});d.s={};d.s.focused=true;d.tick(binds);for(unsigned i=0;i<4;++i)d.key(binds,40);d.key(binds,13);d.key(binds,'Z');check(binds.value().bindings[unsigned(input::Action::A)].key=='Z',"binding capture");
        for(int language=0;language<5;++language){
            auto v=settings;v.text_language=language;menu::Model timing(v,language,true);timing.choose("display",{});Driver controls;controls.tick(timing);
            for(unsigned i=0;i<6;++i)controls.key(timing,40);
            check(timing.rows()[6].id=="gameplay_timing"&&timing.rows()[6].restart&&timing.description()!=L"?","cadence row missing restart/localization");
            controls.key(timing,39);
            check(timing.value().gameplay_timing==0,"original cadence option did not activate");
        }
        for(int language=0;language<5;++language){
            auto v=settings;v.vsync=1;v.text_language=language;
            menu::Model sync(v,language,true);sync.choose("display",{});Driver controls;controls.tick(sync);
            for(unsigned i=0;i<5;++i)controls.key(sync,40);
            check(sync.rows()[5].id=="vsync"&&sync.rows()[5].value!=L"?"&&sync.description()!=L"?","VSync control missing/localization");
            controls.key(sync,39);
            check(sync.value().vsync==2&&sync.value().gameplay_timing==1,"VSync off changed gameplay cadence");
            controls.key(sync,39);
            check(sync.value().vsync==1,"VSync on did not wrap without Automatic");
            presentation::save_settings(root/"vsync-roundtrip.ini",v);
            check(presentation::read_settings(root/"vsync-roundtrip.ini")==v,"VSync roundtrip failed");
        }
        for(int language=0;language<5;++language){
            auto v=settings;v.text_language=language;menu::Model keys(v,language,false);keys.choose("bindings",{});Driver controls;controls.tick(keys);
            for(unsigned i=0;i<unsigned(input::Action::A);++i)controls.key(keys,40);
            controls.key(keys,13);controls.key(keys,27);
            check(keys.modal()&&!keys.capturing()&&keys.dialog()!=L"?","Escape remains unbindable or has no localized choice");
            controls.key(keys,13); // Default No: leave capture, keep the mapping.
            check(!keys.modal()&&!keys.dirty(),"Escape cancel path changed the binding");
            controls.key(keys,13);controls.key(keys,27);controls.key(keys,37);controls.key(keys,13);
            check(!keys.modal()&&keys.value().bindings[unsigned(input::Action::A)].key==27,"Escape assignment failed");
        }
        for(unsigned button=2;button<=5;++button){
            auto v=settings;v.master_volume=50;v.bindings[unsigned(input::Action::Confirm)].mouse=button;v.bindings[unsigned(input::Action::Cancel)].mouse=0;
            menu::Model pointer(v,4,false);pointer.choose("audio",{});Driver clicks;clicks.tick(pointer);
            clicks.s.mouse[button]=true;check(!clicks.tick(pointer).changed&&pointer.value().master_volume==50,"remapped mouse confirm fired on press");
            clicks.s.mouse[button]=false;check(clicks.tick(pointer).changed&&pointer.value().master_volume==55,"remapped mouse confirm missed release");
        }
        for(unsigned button=1;button<=5;++button){
            auto v=settings;v.bindings[unsigned(input::Action::Cancel)].mouse=button;v.bindings[unsigned(input::Action::Confirm)].mouse=0;
            menu::Model pointer(v,4,false);pointer.choose("audio",{});Driver clicks;clicks.tick(pointer);
            clicks.s.mouse[button]=true;clicks.tick(pointer);check(pointer.rows()[0].id=="master_volume","remapped mouse cancel fired on press");
            clicks.s.mouse[button]=false;clicks.tick(pointer);check(pointer.rows()[0].id=="display","remapped mouse cancel missed release");
            pointer.ask("overwrite",menu::Command::Restore);clicks.tick(pointer);clicks.key(pointer,37);
            clicks.s.mouse[button]=true;check(clicks.tick(pointer).command==menu::Command::None&&pointer.modal(),"mouse press dismissed confirmation");
            clicks.s.mouse[button]=false;check(clicks.tick(pointer).command==menu::Command::None&&!pointer.modal(),"mouse cancel confirmed destructive action");
        }
        for(int language=0;language<5;++language)for(auto page:{"title","display","audio","camera","controls","bindings","interface","profiles","system"}){
            auto v=settings;v.text_language=language;menu::Model localized(v,language,true);localized.choose(page,{});check(localized.heading()!=L"?","missing heading");
            for(const auto& row:localized.rows()){
                check(row.label!=L"?"&&row.value!=L"?","missing translation");
                check(!row.id.starts_with("hud_"),"retired HUD control visible");
                check(row.id!="interpolation","retired interpolation control visible");
                check(row.id!="anisotropy","retired anisotropy control visible");
                check(row.id!="presentation_fps","retired FPS selection visible");
            }check(localized.description()!=L"?","missing help");
        }
        for(int language=0;language<5;++language){
            auto v=settings;v.text_language=language;menu::Model status(v,language,true);
            for(auto state:{recovery::BackupState::Waiting,recovery::BackupState::Saved,recovery::BackupState::Failed,recovery::BackupState::CleanupPending}){
                status.status(state,recovery::AudioState::Idle);status.choose("profiles",{});
                const auto rows=status.rows();const auto& row=rows[rows.size()-2];
                check(row.id=="backup_status"&&row.read_only&&row.value!=L"?"&&status.description()!=L"?","localized backup status missing");
            }
            for(auto state:{recovery::AudioState::Idle,recovery::AudioState::Connected,recovery::AudioState::Silent,recovery::AudioState::Partial,recovery::AudioState::RestartRequired}){
                status.status(recovery::BackupState::Saved,state);status.choose("audio",{});
                const auto rows=status.rows();const auto& row=rows[rows.size()-2];
                check(row.id=="audio_status"&&row.read_only&&row.value!=L"?"&&status.description()!=L"?","localized audio status missing");
            }
            Driver status_input;status_input.tick(status);
            for(unsigned i=0;i<5;++i)status_input.key(status,40);
            check(status_input.key(status,13).command==menu::Command::None&&status.rows()[0].id=="master_volume"&&!status.dirty(),"status row changed settings or navigation");
        }
        input::Snapshot raw;raw.focused=true;raw.keys[38]=true;katana::runtime::NativePortInputSnapshot native{};input::transform(native,raw,false);check(native.gamepads[0].left_stick_y==1,"keyboard analog");
        input::set_replay(true);native.gamepads[0].left_stick_x_raw=1234;input::transform(native,raw,false);check(native.gamepads[0].left_stick_x_raw==1234,"replay modified");input::set_replay(false);
        for(unsigned button=1;button<=5;++button){
            auto v=settings;v.keyboard_enabled=1;v.bindings[unsigned(input::Action::A)]={0,0,button,0};presentation::apply_live(v);
            menu::InputReleaseGate gate;gate.arm();input::Snapshot held;held.focused=true;held.mouse[button]=true;held.keys[VK_ESCAPE]=true;
            input::transform(native,held,gate.consume(held));check(native.gamepads[0].buttons==0,"closing key leaked from modal");
            held.keys[VK_ESCAPE]=false;input::transform(native,held,gate.consume(held));check(native.gamepads[0].buttons==0,"held mouse button leaked after Options closed");
            held.mouse[button]=false;check(!gate.consume(held),"mouse release did not unlock gameplay");
            input::transform(native,held,gate.consume(held));check(native.gamepads[0].buttons==0,"mouse release generated an action");
            held.mouse[button]=true;input::transform(native,held,gate.consume(held));check(native.gamepads[0].buttons==(1u<<10),"fresh mouse press stayed suppressed");
        }
        presentation::apply_live(settings);
        const auto source=read(argv[2]);const auto source_hash=startup::digest(source);const auto preview=profiles::inspect(source);check(preview.files.size()==2&&preview.files[0]=="SONICADV_ALF"&&preview.files[1]=="SONICADV_INT","story/chao VMU envelope");
        const auto data=root/"data";profiles::initialize(data);const auto save=data/"sonic-adventure-pal-v1003/saves/sonic-adventure-pal-v1003-vmu.save-c0-s0.ksave";write(save,source);write(fs::path(save.wstring()+L".bak"),source);
        const auto exported=profiles::export_save();check(startup::file_digest(exported)==source_hash,"export changed VMU");const auto backed=profiles::backup();check(startup::file_digest(backed)==source_hash,"backup changed VMU");
        const auto imported=profiles::import_candidate(fs::absolute(argv[2]));check(imported.digest==source_hash,"external import changed VMU");
        rejects([&]{profiles::stage_restore(imported.id,true,std::string(64,'0'));},"changed preview accepted for restore");
        rejects([&]{profiles::export_save(std::string(64,'0'));},"changed preview accepted for export");
        auto corrupt=source;corrupt.back()^=std::byte{1};rejects([&]{profiles::inspect(corrupt);},"corrupt import accepted");write(save,corrupt);check(profiles::active_preview().digest==source_hash,"corrupt primary recovery");
        auto incompatible=source;resign(incompatible,2);write(save,incompatible);rejects([&]{profiles::active_preview();},"incompatible primary revived stale backup");
        auto invalid_volume=source;invalid_volume[80]=std::byte{0};resign(invalid_volume,1);write(save,invalid_volume);rejects([&]{profiles::active_preview();},"invalid volume revived stale backup");write(save,source);
        // Exercise the same profile projection and activation path as Options.
        // Unreadable inactive profiles must not block healthy or empty choices.
        const auto healthy_id=profiles::create(),empty_id=profiles::create(),corrupt_id=profiles::create(),incompatible_id=profiles::create();
        const auto profile_save=[&](const std::string& id){return profiles::data_root(id)/fs::relative(save,data);};
        write(profile_save(healthy_id),source);write(profile_save(corrupt_id),corrupt);write(profile_save(incompatible_id),incompatible);
        write(fs::path(profile_save(incompatible_id).wstring()+L".bak"),source);
        for(int language=0;language<5;++language){
            const auto choices=menu::profile_choices(language,"default");check(choices.size()==5,"bad profile blocked the catalog");
            for(unsigned i=0;i<choices.size();++i){
                const auto& choice=choices[i];const bool available=choice.id!=corrupt_id&&choice.id!=incompatible_id;
                check(choice.enabled==available,"profile availability ignored validation failure");
                if(!available)check(choice.details==menu::text("profile_unavailable",language)&&choice.details!=L"?","unreadable profile not localized");
                if(choice.id==empty_id)check(choice.details==menu::text("empty_profile",language),"empty profile not distinguished from unreadable data");
                menu::Model picker(settings,language,true);picker.choose("switch_profile",choices);Driver controls;controls.tick(picker);
                for(unsigned n=0;n<i;++n)controls.key(picker,VK_DOWN);
                check(controls.key(picker,VK_RETURN).command==menu::Command::None,"profile switched before confirmation");
                if(available){
                    check(picker.modal(),"healthy or empty profile cannot be selected");controls.key(picker,VK_LEFT);
                    const auto result=controls.key(picker,VK_RETURN);check(result.command==menu::Command::SwitchProfile&&result.argument==choice.id,"profile confirmation lost selection");
                }else{
                    check(!picker.modal()&&!picker.rows()[i].enabled,"unreadable profile accepted through keyboard");
                    controls.s.connected=true;controls.s.connection_changed=true;controls.tick(picker);controls.s.connection_changed=false;
                    controls.s.pad=1u<<10;check(controls.tick(picker).command==menu::Command::None&&!picker.modal(),"unreadable profile accepted through controller");controls.s.pad=0;controls.tick(picker);
                    const auto rect=picker.row_rect(i,1920,1080);controls.s.cursor_x=rect.left+20;controls.s.cursor_y=rect.top+20;
                    controls.s.mouse[1]=true;controls.tick(picker);controls.s.mouse[1]=false;
                    check(controls.tick(picker).command==menu::Command::None&&!picker.modal(),"unreadable profile accepted through mouse");
                }
            }
        }
        check(startup::file_digest(profile_save(healthy_id))==source_hash&&startup::file_digest(profile_save(corrupt_id))==startup::digest(corrupt)&&
            startup::file_digest(profile_save(incompatible_id))==startup::digest(incompatible)&&!fs::exists(profile_save(empty_id)),"catalog repaired or modified profile data");
        profiles::stage_restore(backed.filename().string(),false);const auto before=profiles::snapshots().size();profiles::apply_pending_restore();check(profiles::snapshots().size()==before+1,"restore safety backup missing");check(startup::file_digest(save)==source_hash,"restore split or changed VMU");
        // Fail only isolated backup publication, never the authoritative VMU.
        const auto fault_data=root/"backup-faults";profiles::initialize(fault_data);
        const auto fault_save=fault_data/"sonic-adventure-pal-v1003/saves/sonic-adventure-pal-v1003-vmu.save-c0-s0.ksave";
        write(fault_save,source);
        const auto backup_dir=profiles::library_root()/"backups/default";write(backup_dir,source);
        profiles::poll(1);profiles::poll(2'000'000'001);
        check(profiles::backup_status().state==recovery::BackupState::Failed&&profiles::backup_status().failures==1,"automatic backup failure was hidden or double-counted");
        profiles::poll(2'500'000'001);check(profiles::backup_status().failures==1,"backup failure caused tight retries");
        check(startup::file_digest(fault_save)==source_hash,"failed backup changed the live VMU");
        check(fs::remove(backup_dir),"test backup obstruction not removed");
        profiles::poll(4'000'000'001);
        check(profiles::backup_status().state==recovery::BackupState::Saved&&profiles::snapshots().size()==1,"automatic backup did not recover");
        auto limited=settings;limited.backup_limit=5;presentation::apply_live(limited);
        for(unsigned i=0;i<5;++i)write(backup_dir/("auto-000"+std::to_string(i)+".sasave"),source);
        struct LockedFile {HANDLE value=INVALID_HANDLE_VALUE;~LockedFile(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
        {
            LockedFile locked{CreateFileW((backup_dir/"auto-0000.sasave").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr)};
            check(locked.value!=INVALID_HANDLE_VALUE,"backup retention lock fixture");
            const auto published=profiles::backup(true);
            check(startup::file_digest(published)==source_hash&&profiles::backup_status().state==recovery::BackupState::CleanupPending,"cleanup failure hid a safely published backup");
            check(profiles::backup_status().saved==2&&profiles::backup_status().failures==1&&profiles::backup_status().cleanup_failures==1,"backup outcome accounting");
        }
        profiles::poll(6'000'000'001);
        check(profiles::snapshots().size()==5&&profiles::backup_status().state==recovery::BackupState::Saved&&profiles::backup_status().saved==2,"cleanup retry duplicated backups or failed to retire old copies");
        check(startup::file_digest(fault_save)==source_hash,"retention changed the live VMU");
        profiles::initialize(fault_data);write(fault_save,corrupt);
        profiles::poll(1);profiles::poll(2'000'000'001);
        check(profiles::backup_status().state==recovery::BackupState::Failed,"invalid source failure not shown");
        const auto report=read(profiles::export_diagnostics());const std::string report_text(reinterpret_cast<const char*>(report.data()),report.size());
        check(report_text.find("Backup state: failed")!=std::string::npos&&report_text.find("Audio output: idle")!=std::string::npos,"recovery diagnostics missing");
        check(report_text.find(source_hash)==std::string::npos&&report_text.find("SONICADV_ALF")==std::string::npos&&report_text.find(fault_data.string())==std::string::npos,"diagnostics included personal save identity");
        check(startup::file_digest(fault_save)==startup::digest(corrupt),"backup repair overwrote the primary");
        presentation::apply_live(settings);
        check(startup::file_digest(argv[2])==source_hash,"source save modified");
        std::cout<<"SONIC_ENHANCEMENTS_TEST_OK settings live_fields audio mouse_release post_modal_release=all5 reconnect remap languages=5 replay saves=story+chao corrupt incompatible profile_catalog=isolated restore backup_failures=visible retention=retry_safe diagnostics=private status=localized_readonly\n";return 0;
    }catch(const std::exception& e){std::cerr<<"SONIC_ENHANCEMENTS_TEST_FAIL "<<e.what()<<'\n';return 1;}
}
