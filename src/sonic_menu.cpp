#include "sonic_menu.hpp"
#include "sonic_menu_text.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
namespace sonic::menu {
namespace {
using presentation::Settings;
struct Field {std::string_view id;unsigned Settings::*member;unsigned min,max,step;};
constexpr Field fields[]{
#define SONIC_SETTING(name,initial,minimum,maximum) {#name,&Settings::name,minimum,maximum,maximum>40?5u:1u},
#include "sonic_settings_fields.inc"
#undef SONIC_SETTING
    {"presentation_fps",&Settings::presentation_fps,30,144,1},
    {"render_percent",&Settings::render_percent,25,100,25}
};
const Field* field(std::string_view id){for(const auto& f:fields)if(f.id==id)return &f;return nullptr;}
std::wstring copy(std::string_view id,int l){return std::wstring(text(id,l));}
constexpr std::array languages{L"日本語",L"English",L"Français",L"Español",L"Deutsch"};
constexpr std::pair<unsigned,unsigned> resolutions[]{{640,480},{1280,720},{1920,1080},{2560,1440},{3440,1440},{3840,1600},{3840,2160},{5120,2160}};
std::wstring value_text(std::string_view id,const Settings& s,int l,input::GlyphStyle style){
    if(id.starts_with("bind_")){
        const auto name=id.substr(5);const auto it=std::find(input::action_names.begin(),input::action_names.end(),name);
        return it==input::action_names.end()?L"":input::binding_name(s.bindings[it-input::action_names.begin()],style);
    }
    if(id=="renderer")return s.renderer==rendering::Renderer::Vulkan?L"Vulkan":L"Direct3D 11";
    if(id=="resolution")return std::to_wstring(s.width)+L" × "+std::to_wstring(s.height);
    if(id=="widescreen")return copy(s.widescreen?"on":"original",l);
    if(id=="window_mode")return copy(std::array{"windowed","borderless","fullscreen"}[unsigned(s.window_mode)],l);
    if(id=="camera_style")return copy(s.camera_style==camera::Style::Original?"original":"recompiled",l);
    if(id=="text_language")return s.text_language<0?copy("game",l):languages[s.text_language];
    if(id=="voice_language")return s.voice_language<0?copy("game",l):languages[s.voice_language];
    if(id=="subtitles")return copy(s.subtitles<0?"game":s.subtitles?"on":"off",l);
    if(id=="glyph_style")return s.glyph_style==0?copy("automatic",l):s.glyph_style==1?L"Xbox":s.glyph_style==2?L"PlayStation":copy("keyboard",l);
    if(id=="vsync")return copy(s.vsync==0?"automatic":s.vsync==1?"on":"off",l);
    if(id=="presentation_fps"&&s.vsync==1)return copy("display_controlled",l);
    if(id=="camera_return_seconds")return s.camera_return_seconds?std::to_wstring(s.camera_return_seconds)+L" "+copy("seconds",l):copy("never",l);
    if(const auto f=field(id)){
        const auto v=s.*(f->member);
        if(f->max==1)return copy(v?"on":"off",l);
        return std::to_wstring(v)+(id=="anisotropy"?L"×":id=="presentation_fps"?L" FPS":
            id=="backup_limit"?L"":L"%");
    }
    return L"";
}
bool is_menu_action(unsigned action){return action>=unsigned(input::Action::Settings);}
bool overlaps(const input::Binding& a,const input::Binding& b){return (a.pad&b.pad) || (a.mouse&&a.mouse==b.mouse) ||
    (a.key&&(a.key==b.key||a.key==b.alternate)) || (a.alternate&&(a.alternate==b.key||a.alternate==b.alternate));}
void remove_binding(input::Binding& b,const input::Binding& taken){
    b.pad&=~taken.pad;if(b.mouse && b.mouse==taken.mouse)b.mouse=0;
    if(b.key&&(b.key==taken.key||b.key==taken.alternate))b.key=0;
    if(b.alternate&&(b.alternate==taken.key||b.alternate==taken.alternate))b.alternate=0;
}
}
Model::Model(Settings value,int l,bool title):initial_(value),draft_(std::move(value)),initial_language_(l),title_(title){}
int Model::language()const noexcept{return draft_.text_language<0?initial_language_:draft_.text_language;}
void Model::page(std::string name){page_=std::move(name);selected_=0;pressed_row_=-1;armed_=false;}
void Model::choose(std::string name,std::vector<Choice> values){choices_=std::move(values);page(std::move(name));}
void Model::message(std::wstring value){message_=std::move(value);armed_=false;}
void Model::confirm(std::string key,Command command,std::string argument){question_=std::move(key);pending_=command;argument_=std::move(argument);confirm_=false;armed_=false;question_details_.clear();countdown_=0;}
Rect Model::list_bounds(unsigned w,unsigned h) const noexcept {
    const float scale=std::min(w/1280.0f,h/900.0f),span=page_=="title"?760.0f:1100.0f,x=(w-span*scale)/2;
    return {int(x),int(h/2.0f-235*scale),int(x+span*scale),int(h/2.0f+251*scale)};
}
Rect Model::row_rect(unsigned n,unsigned w,unsigned h) const noexcept {
    const auto bounds=list_bounds(w,h);const float scale=std::min(w/1280.0f,h/900.0f);
    const float step=page_=="title"?44.0f:49.0f;
    const float offset=float(page_=="about"?9:n)*step;
    return {bounds.left,int(h/2.0f+(offset-235)*scale),bounds.right,int(h/2.0f+(offset-235+step-4)*scale)};
}
Rect Model::confirm_rect(bool yes,unsigned w,unsigned h) noexcept {
    const float scale=std::min(w/1280.0f,h/900.0f);const float x=w/2.0f+(yes?-440:30)*scale;
    return {int(x),int(h/2.0f+65*scale),int(x+410*scale),int(h/2.0f+130*scale)};
}
std::vector<Row> Model::rows()const{
    std::vector<Row> result;
    const auto add=[&](std::string_view id,bool restart=false,bool enabled=true){auto key=id.starts_with("bind_")?id.substr(5):id;
        result.push_back({std::string(id),copy(key,language()),value_text(id,draft_,language(),glyphs_),restart,enabled});};
    const auto group=[&](std::initializer_list<std::string_view> ids){for(auto id:ids)add(id);};
    const auto status=[&](std::string_view id,std::string_view value){
        result.push_back({std::string(id),copy(id,language()),copy(value,language()),false,true,true});
    };
    if(page_=="title"){
        group({"display","audio","camera","controls","interface","profiles","system"});add("sound_test",false,title_);group({"about","save","back"});
    }else if(page_=="display"){
        for(auto id:{"renderer","resolution","window_mode","widescreen","render_percent"})add(id,true);
        add("presentation_fps",false,draft_.vsync!=1);add("vsync",true);group({"anisotropy","back"});
    }else if(page_=="audio"){
        group({"master_volume","music_volume","voice_volume","effects_volume","mute_background"});
        status("audio_status","audio_"+std::string(recovery::name(audio_status_)));add("back");
    }
    else if(page_=="camera")group({"camera_style","mouse_camera","camera_sensitivity_x","camera_sensitivity_y","mouse_sensitivity_x","mouse_sensitivity_y","camera_invert_x","camera_invert_y","camera_deadzone","camera_return_seconds","back"});
    else if(page_=="controls")group({"bindings","keyboard_enabled","movement_deadzone","swap_sticks","glyph_style","reset_bindings","back"});
    else if(page_=="interface")group({"text_language","voice_language","subtitles","subtitle_scale","subtitle_background","back"});
    else if(page_=="system")group({"pause_focus_loss","pause_controller_loss","mute_background","export_diagnostics","back"});
    else if(page_=="about")add("back");
    else if(page_=="profiles"){
        group({"switch_profile","new_profile","automatic_backups","backup_limit","backup_now","restore_backup","import_save","export_save"});
        status("backup_status","backup_"+std::string(recovery::name(backup_status_)));add("back");
    }
    else if(page_=="bindings"){
        for(auto name:input::action_names)add("bind_"+std::string(name));add("back");
    }else {
        for(const auto& c:choices_)result.push_back({c.id,c.title,L"",false,c.enabled});add("back");
    }
    return result;
}
std::wstring Model::heading()const{return copy(page_,language());}
std::wstring_view Model::body()const noexcept{
    if(page_!="about")return {};
    return L"Original Game created by SEGA, 1998, 1999\n\n"
           L"Port created by SoNiCFReaK, 2026\n\n"
           L"Powered by KatanaRecomp";
}
std::wstring Model::description()const{
    if(page_=="about")return {};
    const auto entries=rows();const auto& id=entries[std::min<std::size_t>(selected_,entries.size()-1)].id;
    for(const auto& c:choices_)if(c.id==id&&!c.details.empty())return c.details;
    // Interpolation was withdrawn; its prototype is no longer a menu option.
    if(id=="vsync")return copy("vsync_help",language());
    if(id=="presentation_fps")return copy(draft_.vsync==1?"fps_vsync_help":"fps_help",language());
    if(id=="export_diagnostics")return copy("diagnostics_help",language());
    if(id=="sound_test"&&!title_)return copy("title_only",language());
    if(page_=="display")return copy("display_help",language());
    if(page_=="audio"){
        if(audio_status_==recovery::AudioState::RestartRequired)return copy("audio_restart_help",language());
        if(audio_status_==recovery::AudioState::Silent)return copy("audio_silent_help",language());
        if(audio_status_==recovery::AudioState::Partial)return copy("audio_partial_help",language());
        return copy("audio_help",language());
    }
    if(page_=="camera")return copy("camera_help",language());
    if(page_=="controls" || page_=="bindings")return copy("controls_help",language());
    if(page_=="profiles"){
        if(backup_status_==recovery::BackupState::Failed)return copy("backup_failed_help",language());
        if(backup_status_==recovery::BackupState::CleanupPending)return copy("backup_cleanup_help",language());
    }
    if(page_=="profiles" || page_=="restore_backup" || page_=="import_save")return copy("profile_help",language());
    return copy("live_help",language());
}
std::wstring Model::dialog()const{
    if(!message_.empty())return message_;
    if(!question_.empty())return copy(question_,language())+(question_details_.empty()?L"":L"\n\n"+question_details_)+(countdown_?L"\n"+std::to_wstring(countdown_)+L" "+copy("seconds",language()):L"");
    if(capture_)return copy("capture",language());
    return {};
}
std::wstring Model::footer()const{
    return input::binding_name(draft_.bindings[unsigned(input::Action::Confirm)],glyphs_)+L"  "+copy("yes",language())+L"       "+
        input::binding_name(draft_.bindings[unsigned(input::Action::Cancel)],glyphs_)+L"  "+copy("back",language());
}
Result Model::activate(const Row& row,int direction){
    Result result;
    if(!row.enabled||row.read_only)return result;
    const auto& id=row.id;
    if(id=="back"){
        if(page_=="title"){
            if(draft_==initial_)result.command=Command::Close;
            else confirm("discard",Command::Close);
        }
        else page(page_=="bindings"?"controls":page_=="switch_profile"||page_=="restore_backup"||page_=="import_save"||page_=="import_browser"?"profiles":"title");
        return result;
    }
    if(id=="save"){
        if(presentation::needs_restart(initial_,draft_))confirm("restart_question",Command::Restart);
        else result.command=Command::Save;
        return result;
    }
    if(id=="sound_test"){result.command=Command::SoundTest;return result;}
    if(id=="reset_bindings"){confirm("reset_bindings",Command::None);return result;}
    if(id=="export_save"){result.command=Command::PreviewExport;return result;}
    if(id=="new_profile"){confirm("new_profile",Command::NewProfile);return result;}
    if(id=="export_diagnostics"){confirm("diagnostics_help",Command::Diagnostics);return result;}
    if(id=="backup_now"){result.command=Command::Backup;return result;}
    if(id=="import_save" || id=="restore_backup" || id=="switch_profile"){
        result.command=id=="import_save"?Command::Import:id=="restore_backup"?Command::Restore:Command::SwitchProfile;return result;
    }
    if(page_=="import_browser"){result.command=Command::BrowseImport;result.argument=id;return result;}
    if(page_=="import_save"&&id=="browse_files"){result.command=Command::BrowseImport;return result;}
    if(page_=="switch_profile" || page_=="restore_backup" || page_=="import_save"){
        confirm(page_=="switch_profile"?"restart_question":"overwrite",page_=="switch_profile"?Command::SwitchProfile:page_=="restore_backup"?Command::Restore:Command::Import,id);
        for(const auto& c:choices_)if(c.id==id)question_details_=c.details;
        return result;
    }
    if(id.starts_with("bind_")){
        const auto name=std::string_view(id).substr(5);capture_=input::Action(std::find(input::action_names.begin(),input::action_names.end(),name)-input::action_names.begin());
        armed_=false;return result;
    }
    const auto change=[&](int value,int min,int max){return direction<0?(value<=min?max:value-1):(value>=max?min:value+1);};
    if(id=="renderer")draft_.renderer=rendering::Renderer(change(int(draft_.renderer),0,1));
    else if(id=="widescreen"){
        draft_.widescreen=!draft_.widescreen;
        if(draft_.widescreen)message(copy("widescreen_warning",language()));
    }
    else if(id=="window_mode")draft_.window_mode=rendering::WindowMode(change(int(draft_.window_mode),0,2));
    else if(id=="camera_style")draft_.camera_style=camera::Style(change(int(draft_.camera_style),0,1));
    else if(id=="text_language")draft_.text_language=change(draft_.text_language,-1,4);
    else if(id=="voice_language")draft_.voice_language=change(draft_.voice_language,-1,1);
    else if(id=="subtitles")draft_.subtitles=change(draft_.subtitles,-1,1);
    else if(id=="resolution"){
        int index=-1;for(unsigned i=0;i<std::size(resolutions);++i)if(resolutions[i]==std::pair{draft_.width,draft_.height})index=int(i);
        const auto r=resolutions[change(index,0,int(std::size(resolutions)-1))];draft_.width=r.first;draft_.height=r.second;
    }else if(const auto f=field(id)){
        auto& value=draft_.*(f->member);
        if(f->max<=3)value=unsigned(change(int(value),int(f->min),int(f->max)));
        else if(id=="anisotropy")value=direction<0?(value<=1?16:std::max(1u,value/2)):(value>=16?1:std::min(16u,value*2));
        else value=direction<0?unsigned(std::max(int(f->min),int(value)-int(f->step))):std::min(f->max,value+f->step);
    }else {page(id);return result;}
    result.changed=true;return result;
}
Result Model::update(const input::Snapshot& s,double now){
    Result result;glyphs_=s.glyphs;
    const auto old=std::exchange(previous_,s);
    if(!s.focused || s.connection_changed){armed_=false;pressed_row_=pressed_confirmation_=-1;return result;}
    const auto held=[&](const input::Snapshot& v,input::Action a){return input::held(v,a,draft_.bindings);};
    auto digital=s,old_digital=old;digital.mouse={};old_digital.mouse={};
    const bool accept=held(digital,input::Action::Confirm)||s.keys[13],cancel=held(digital,input::Action::Cancel)||s.keys[27];
    const bool old_accept=held(old_digital,input::Action::Confirm)||old.keys[13],old_cancel=held(old_digital,input::Action::Cancel)||old.keys[27];
    const bool any=accept||cancel||s.pad||std::any_of(s.mouse.begin(),s.mouse.end(),[](bool v){return v;})||std::any_of(s.keys.begin(),s.keys.end(),[](bool v){return v;});
    if(!armed_){if(!any && std::abs(s.move_x)<0.3f && std::abs(s.move_y)<0.3f)armed_=true;return result;}
    const auto mouse_release=[&](input::Action action){
        const auto button=draft_.bindings[unsigned(action)].mouse;
        return button&&old.mouse[button]&&!s.mouse[button];
    };
    // Left confirm uses release-inside hit testing below. Other remapped
    // pointer buttons act on the selection, but never activate at mouse-down.
    bool press_accept=(accept&&!old_accept)||
        (draft_.bindings[unsigned(input::Action::Confirm)].mouse!=1&&mouse_release(input::Action::Confirm));
    bool press_cancel=(cancel&&!old_cancel)||mouse_release(input::Action::Cancel);
    if(capture_ && question_.empty()){
        input::Binding binding{};
        for(unsigned k=1;k<256;++k)if(s.keys[k]&&!old.keys[k]){binding.key=k;break;}
        for(unsigned k=1;k<6;++k)if(s.mouse[k]&&!old.mouse[k]){binding.mouse=k;break;}
        if(const auto pad=s.pad&~old.pad)binding.pad=pad&(~pad+1);
        if(binding.key||binding.mouse||binding.pad){
            // Rebind the active device, retaining the other input methods.
            auto candidate=draft_.bindings[unsigned(*capture_)];
            if(binding.pad)candidate.pad=binding.pad;
            else {candidate.key=binding.key;candidate.alternate=0;candidate.mouse=binding.mouse;}
            bool conflict=false;
            for(unsigned i=0;i<input::action_count;++i)if(i!=unsigned(*capture_)&&is_menu_action(i)==is_menu_action(unsigned(*capture_))&&overlaps(binding,draft_.bindings[i]))conflict=true;
            // Escape is a bindable key too. Its explicit choice also retains
            // a keyboard-only way to leave capture without changing anything.
            if(conflict||binding.key==27){candidate_=candidate;confirm(conflict?"duplicate":"bind_escape",Command::None);}
            else {draft_.bindings[unsigned(*capture_)]=candidate;capture_.reset();armed_=false;result.changed=true;}
        }
        return result;
    }
    const int vertical=(s.keys[40]||(s.pad&2)||s.move_y<-.5f)?1:(s.keys[38]||(s.pad&1)||s.move_y>.5f)?-1:0;
    const int horizontal=(s.keys[39]||(s.pad&8)||s.move_x>.5f)?1:(s.keys[37]||(s.pad&4)||s.move_x<-.5f)?-1:0;
    const int direction=vertical?vertical:horizontal?horizontal*2:0;
    const bool repeat=direction&&(direction!=previous_direction_||now>=next_repeat_);
    if(repeat)next_repeat_=now+(direction!=previous_direction_?.38:.085);previous_direction_=direction;
    if(modal()){
        if(!message_.empty()){if(press_accept||press_cancel||(old.mouse[1]&&!s.mouse[1])){message_.clear();armed_=false;}return result;}
        if(repeat&&horizontal)confirm_=horizontal<0;
        if(s.mouse[1]&&!old.mouse[1]){
            pressed_confirmation_=-1;for(int yes=0;yes<2;++yes)if(confirm_rect(yes,s.client_width,s.client_height).contains(s.cursor_x,s.cursor_y))pressed_confirmation_=yes;
        }
        if(!s.mouse[1]&&old.mouse[1]&&pressed_confirmation_>=0&&confirm_rect(pressed_confirmation_,s.client_width,s.client_height).contains(s.cursor_x,s.cursor_y)){
            confirm_=pressed_confirmation_!=0;press_accept=true;
        }
        if(press_accept||press_cancel){
            const bool commit=press_accept&&!press_cancel&&confirm_;
            if((question_=="duplicate"||question_=="bind_escape") && capture_ && candidate_){
                if(commit){
                    const auto target=unsigned(*capture_);const auto b=*candidate_;
                    // Only remove the changed device's assignment.
                    input::Binding taken{};const auto before=draft_.bindings[target];
                    if(before.pad!=b.pad)taken.pad=b.pad;else {taken.key=b.key;taken.alternate=b.alternate;taken.mouse=b.mouse;}
                    for(unsigned i=0;i<input::action_count;++i)if(i!=target&&is_menu_action(i)==is_menu_action(target))remove_binding(draft_.bindings[i],taken);
                    draft_.bindings[target]=b;result.changed=true;
                }
                candidate_.reset();capture_.reset();
            }else if(question_=="reset_bindings"&&commit){draft_.bindings=input::default_bindings;result.changed=true;}
            else if(question_=="discard"&&commit){draft_=initial_;result.changed=true;result.command=Command::Close;}
            else if(commit){result.command=pending_;result.argument=argument_;}
            question_.clear();pending_=Command::None;argument_.clear();armed_=false;
        }
        return result;
    }
    auto list=rows();selected_=std::min(selected_,unsigned(list.size()-1));
    if(press_cancel)return activate(Row{"back"});
    if(repeat&&vertical)selected_=unsigned((int(selected_)+vertical+int(list.size()))%int(list.size()));
    if(s.wheel)selected_=unsigned(std::clamp(int(selected_)-s.wheel,0,int(list.size()-1)));
    if(repeat&&horizontal)return activate(list[selected_],horizontal);
    const bool cursor_moved=s.cursor_x!=old.cursor_x || s.cursor_y!=old.cursor_y;
    int hit=-1;
    for(unsigned n=0;n<visible_count()&&first()+n<list.size();++n)if(row_rect(n,s.client_width,s.client_height).contains(s.cursor_x,s.cursor_y))hit=int(first()+n);
    if(cursor_moved&&hit>=0)selected_=unsigned(hit);
    if(s.mouse[1]&&!old.mouse[1])pressed_row_=hit;
    if(!s.mouse[1]&&old.mouse[1]){
        if(hit>=0&&hit==pressed_row_){selected_=unsigned(hit);return activate(list[selected_],s.cursor_x<int(s.client_width*.78)?-1:1);}
        pressed_row_=-1;
    }
    // Mouse bindings are release-activated, not activated at mouse-down.
    if(press_accept&&!s.mouse[1]&&!s.mouse[2])return activate(list[selected_]);
    return result;
}
}
