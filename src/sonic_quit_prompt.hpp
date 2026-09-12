#pragma once
#include "katana/runtime/native_port.hpp"
#include "katana/runtime/native_port_platform.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

namespace sonic::quit_prompt {
enum class Screen { None, PressStart, MainMenu };
enum class Decision { None, Opened, Cancelled, Confirmed };
struct Buttons { bool back=false, accept=false; };

// A new title screen and every modal decision require a neutral release.
// Holding the opening B/Escape can neither cancel nor confirm the prompt.
class Prompt final {
public:
    Decision sample(Screen screen, Buttons buttons, bool focused=true) noexcept {
        if(screen!=screen_) {screen_=screen;pending_=visible_=armed_=consume_release_=false;}
        if(screen==Screen::None) {previous_=buttons;return Decision::None;}
        if(!focused) {armed_=false;previous_=buttons;return Decision::None;}
        const bool neutral=!buttons.back && !buttons.accept;
        const bool back=buttons.back && !previous_.back, accept=buttons.accept && !previous_.accept;
        previous_=buttons;
        if(pending_)return Decision::None;
        if(!armed_) {if(neutral){armed_=true;consume_release_=false;}return Decision::None;}
        if(visible_) {
            if(back || accept) {
                visible_=armed_=false;consume_release_=true;
                return back?Decision::Cancelled:Decision::Confirmed;
            }
        }else if(back) {pending_=true;armed_=false;return Decision::Opened;}
        return Decision::None;
    }
    void presented() noexcept {if(pending_){pending_=false;visible_=true;armed_=false;}}
    void disarm() noexcept {armed_=false;}
    void cancel() noexcept {pending_=visible_=armed_=false;consume_release_=true;}
    bool pending() const noexcept {return pending_;}
    bool visible() const noexcept {return visible_;}
    bool consumes_input() const noexcept {return pending_||visible_||consume_release_;}
    Screen screen() const noexcept {return screen_;}
private:
    Screen screen_=Screen::None;
    Buttons previous_{};
    bool pending_=false,visible_=false,armed_=false,consume_release_=false;
};

struct Labels {std::wstring_view question,confirm,cancel,keyboard;};
struct Image {unsigned width=640,height=256;std::vector<std::byte> pixels;};
const Labels& labels(int language) noexcept;
Image rasterize(int language);
Screen read_screen(katana::runtime::CpuState&,bool advertise_bound) noexcept;
int effective_language(katana::runtime::CpuState&,int preference) noexcept;
void sample_input(katana::runtime::NativePortContext&,katana::runtime::NativePortInputSnapshot&,bool suppressed) noexcept;
bool pending() noexcept;
bool visible() noexcept;
// Called only after all original draws have been flushed, before presenting.
bool draw(katana::runtime::NativePortContext&);
Decision poll_modal(katana::runtime::NativePortContext&,bool focused);
void release(katana::runtime::NativePortContext&) noexcept;
}
