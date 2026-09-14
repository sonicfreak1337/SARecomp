#pragma once
#include "../ui/sonic_raster.hpp"
#include "supported_disc.hpp"
#include <string>
namespace sonic::setup {
enum class Page { Legal,Welcome,Files,Installing,Ready,Error };
struct View {
    Page page=Page::Legal;
    bool steam_deck=false,docked=false,source_selected=false;
    unsigned progress=0,selected=0;
    std::wstring filename,detail,error;
};
struct Resources {
    ui::Image hero;
    ui::Font regular,bold;
    explicit Resources(const std::filesystem::path& root):hero(ui::Image::load(root/"hero.png")),regular(root/"NotoSans-Regular.ttf"),bold(root/"NotoSans-Bold.ttf"){}
};
// The installer and capture tool use this same renderer and hit-test layout.
ui::Image render(const Resources&,const View&,unsigned width=1280,unsigned height=800);
ui::Rect primary_button(unsigned width,unsigned height);
ui::Rect back_button(unsigned width,unsigned height);
inline constexpr std::wstring_view legal_notice=L"This is an independent, non-profit fan project. It is not affiliated with, sponsored by, or endorsed by SEGA. Sonic the Hedgehog, Sonic Adventure, their characters, logos and original game content belong to SEGA Corporation and their respective rights holders. You must provide your own copy of Sonic Adventure. This software does not grant rights to the original game. Third-party software remains subject to its own licenses.";
}
