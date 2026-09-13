#pragma once
#include "sonic_presentation.hpp"
#include "sonic_input.hpp"
#include "sonic_recovery_status.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>
namespace sonic::menu {
enum class Command {None,Close,Save,Restart,SoundTest,Backup,Restore,Import,Export,NewProfile,SwitchProfile,Diagnostics,PreviewExport,BrowseImport};
struct Row {std::string id;std::wstring label,value;bool restart=false,enabled=true,read_only=false;};
struct Choice {std::string id;std::wstring title,details;bool enabled=true;};
struct Result {Command command=Command::None;std::string argument;bool changed=false;};
struct Rect {int left,top,right,bottom;bool contains(int x,int y)const noexcept{return x>=left&&x<right&&y>=top&&y<bottom;}};
class Model {
public:
    Model(presentation::Settings value,int language,bool title);
    Result update(const input::Snapshot&,double seconds);
    std::vector<Row> rows() const;
    std::wstring heading() const;
    std::wstring_view body() const noexcept;
    std::wstring description() const;
    std::wstring dialog() const;
    std::wstring footer() const;
    void choose(std::string page,std::vector<Choice> values);
    void message(std::wstring value);
    void ask(std::string key,Command command){confirm(std::move(key),command);}
    void ask(std::string key,Command command,std::wstring details){confirm(std::move(key),command);question_details_=std::move(details);}
    void countdown(unsigned seconds){countdown_=seconds;}
    void status(recovery::BackupState backup,recovery::AudioState audio) noexcept {backup_status_=backup;audio_status_=audio;}
    int language() const noexcept;
    bool modal() const noexcept{return capture_.has_value() || !question_.empty() || !message_.empty();}
    bool capturing() const noexcept{return capture_.has_value()&&question_.empty();}
    bool informational() const noexcept{return !message_.empty();}
    bool confirm_selected()const noexcept{return confirm_;}
    unsigned selected() const noexcept{return selected_;}
    unsigned visible_count()const noexcept{return page_=="title"?11:visible_rows;}
    unsigned first()const noexcept{return (selected_/visible_count())*visible_count();}
    const presentation::Settings& value()const noexcept{return draft_;}
    void saved(){initial_=draft_;}
    bool dirty()const noexcept{return draft_!=initial_;}
    static constexpr unsigned visible_rows=10;
    Rect list_bounds(unsigned width,unsigned height) const noexcept;
    Rect row_rect(unsigned visible,unsigned width,unsigned height) const noexcept;
    static Rect confirm_rect(bool yes,unsigned width,unsigned height) noexcept;
private:
    Result activate(const Row&,int direction=0);
    void page(std::string name);
    void confirm(std::string key,Command command,std::string argument={});
    presentation::Settings initial_,draft_;
    int initial_language_=1;bool title_=false;
    std::string page_="title",question_,argument_;
    Command pending_=Command::None;
    std::vector<Choice> choices_;
    unsigned selected_=0;
    input::Snapshot previous_{};
    bool armed_=false,confirm_=false;
    int pressed_row_=-1,pressed_confirmation_=-1,previous_direction_=0;
    double next_repeat_=0;
    input::GlyphStyle glyphs_=input::GlyphStyle::Keyboard;
    std::optional<input::Action> capture_;
    std::optional<input::Binding> candidate_;
    std::wstring message_,question_details_;
    unsigned countdown_=0;
    recovery::BackupState backup_status_=recovery::BackupState::Waiting;
    recovery::AudioState audio_status_=recovery::AudioState::Idle;
};
struct Image {unsigned width=0,height=0;std::vector<std::byte> pixels;};
Image rasterize(const Model&,unsigned width,unsigned height);
void load_background(const std::filesystem::path& image);
}
