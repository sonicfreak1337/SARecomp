#include "setup_ui.hpp"
#include <algorithm>
#include <cmath>

namespace sonic::setup {
namespace {
constexpr ui::Color white{245,250,255},muted{177,207,226},gold{255,198,58},ink{8,35,59},cyan{90,225,244};
ui::Rect mapped(ui::Rect r,unsigned w,unsigned h){const float scale=std::min(w/1280.0f,h/800.0f);return {int((w-1280*scale)/2+r.x*scale),int((h-800*scale)/2+r.y*scale),int(r.w*scale),int(r.h*scale)};}
}
ui::Rect primary_button(unsigned w,unsigned h){return mapped({899,628,293,57},w,h);}
ui::Rect back_button(unsigned w,unsigned h){return mapped({699,628,176,57},w,h);}
ui::Image render(const Resources& resources,const View& state,unsigned width,unsigned height){
    ui::Canvas c(width,height);const float s=std::min(width/1280.0f,height/800.0f);
    const auto r=[&](ui::Rect rect){return mapped(rect,width,height);};
    const auto text=[&](std::wstring_view value,ui::Rect box,float size,ui::Color color=white,bool bold=false,bool wrap=false,bool center=false){return c.text(bold?resources.bold:resources.regular,value,r(box),size*s,color,wrap,center);};
    const auto fill=[&](ui::Rect box,ui::Color color){c.fill(r(box),color);};
    const auto round=[&](ui::Rect box,int radius,ui::Color color){c.rounded(r(box),int(radius*s),color);};
    c.gradient({0,0,int(width),int(height)},{7,42,76},{13,97,132});
    // Artwork stays intact; the ocean/cloud colours continue into the frame.
    c.blit(resources.hero,r({0,0,668,800}),true);
    fill({668,0,612,800},{6,35,58,255});
    fill({665,0,3,800},{109,227,244,170});
    round({699,36,179,30},15,{30,74,92});
#ifdef _WIN32
    text(L"WINDOWS EDITION",{712,44,154,19},12,cyan,true);
#else
    text(state.steam_deck?L"STEAM DECK EDITION":L"LINUX EDITION",{712,44,154,19},12,cyan,true);
#endif
    text(L"SETUP",{1082,40,110,24},17,muted,true,false,false);
    const unsigned step=state.page==Page::Welcome?0:state.page==Page::Files?1:state.page==Page::Installing?2:3;
    constexpr std::array labels{L"WELCOME",L"GAME FILES",L"INSTALL",L"READY"};
    for(unsigned i=0;i<4&&state.page!=Page::Legal;++i){const int x=699+int(i)*129;const bool active=i<=step;
        fill({x,101,108,3},active?gold:ui::Color{46,79,101});
        text(labels[i],{x,116,115,20},12,active?white:muted,i==step);
    }
    if(state.page==Page::Welcome){
        text(L"Install\nSonic Adventure.",{699,182,490,119},45,white,true,true);
        text(L"Select your original game files to install the game.",{699,323,481,65},23,muted,false,true);
        round({699,429,493,121},12,{18,58,80});
#ifdef _WIN32
        text(L"WINDOWS VERSION",{720,448,450,25},16,cyan,true);
        text(L"Native Windows  ·  Controller ready",{720,481,450,24},19,white);
#else
        text(state.steam_deck?L"STEAM DECK DEFAULTS":L"LINUX VERSION",{720,448,450,25},16,cyan,true);
        text(state.steam_deck?L"Native 1280 × 800  ·  Fullscreen  ·  Vulkan":L"Native Linux  ·  Widescreen  ·  Controller ready",{720,481,450,24},19,white);
#endif
        text(L"You will need your original Dreamcast GDI and its track files.",{699,570,490,50},18,muted,false,true);
    }else if(state.page==Page::Files){
        text(L"Select game files",{699,173,493,59},38,white,true);
        text(L"Choose your .gdi file. Keep all track files together in the same folder.",{699,246,486,60},20,muted,false,true);
        round({699,323,493,170},12,{18,58,80});
        text(L"SUPPORTED RELEASE",{720,339,450,23},14,cyan,true);
        text(required_release,{720,369,450,29},22,white,true);
        text(L"track03.bin  /  SHA-256",{720,410,450,22},15,muted);
        std::wstring required_hash(required_tracks.back().sha256.begin(),required_tracks.back().sha256.end());
        required_hash.insert(32,1,L'\n');
        text(required_hash,{720,437,450,44},16,white,false,true);
        round({699,509,493,77},10,state.source_selected?ui::Color{21,83,91}:ui::Color{20,60,84});
        text(state.source_selected?L"GDI SELECTED":L"ORIGINAL GAME FILES",{720,521,450,19},12,state.source_selected?cyan:gold,true);
        text(state.source_selected?state.filename:L"Select your Sonic Adventure .gdi",{720,548,450,27},19,white,true);
        text(L"All tracks are verified. Other releases are not supported.",{699,598,490,23},16,muted);
    }else if(state.page==Page::Installing){
        text(L"Installing\ngame files",{699,178,492,114},46,white,true,true);
        text(state.detail.empty()?L"Installing your original game files…":state.detail,{699,323,485,61},22,muted,false,true);
        text(std::to_wstring(state.progress)+L"%",{699,426,450,68},57,white,true);
        round({699,518,493,12},6,{31,72,96});round({699,518,std::max(12,int(493*std::min(100u,state.progress)/100)),12},6,gold);
        text(L"Please wait until installation is complete.",{699,562,490,63},18,muted,false,true);
    }else if(state.page==Page::Ready){
        text(L"Installation complete",{699,185,500,65},35,white,true);
        text(L"Sonic Adventure is ready to play.",{699,276,490,47},25,muted);
        round({699,379,493,151},12,{18,65,78});
        text(L"INSTALLATION COMPLETE",{720,402,450,25},16,cyan,true);
        text(state.steam_deck?L"Steam Deck settings are ready.":L"Your game is ready to play.",{720,443,449,27},22,white,true);
        text(L"You can change game settings from Options.",{720,484,449,25},17,muted);
    }else if(state.page==Page::Error){
        text(L"Installation stopped",{699,185,500,65},36,white,true);
        text(state.error,{699,289,490,239},22,white,false,true);
        text(L"Your original disc files and saves are unchanged.",{699,549,490,64},18,muted,false,true);
    }else{
        text(L"PLEASE READ BEFORE CONTINUING",{699,121,493,22},14,cyan,true);
        text(L"Before we begin.",{699,155,493,61},42,white,true);
        fill({699,226,493,1},{44,76,94});
        // One reading column. Paragraphs follow their measured text height;
        // the notice no longer consists of separately positioned fragments.
        int y=250;
        const auto paragraph=[&](std::wstring_view value,float size,ui::Color color,bool bold,int gap){
            const int used=text(value,{699,y,493,600-y},size,color,bold,true);
            y+=int(std::ceil(used/s))+gap;
        };
        paragraph(L"This is an independent,\nnon-profit fan project.",24,white,true,20);
        paragraph(L"SONIC ADVENTURE & ORIGINAL\nGAME CONTENT BELONG TO SEGA.",22,gold,true,12);
        paragraph(L"Characters, logos and other original material belong to SEGA Corporation and their respective rights holders.",20,muted,false,12);
        paragraph(L"Not affiliated with, sponsored by, or endorsed by SEGA.",20,muted,false,0);
        fill({699,534,493,1},{44,76,94});
        text(L"You must provide your own copy\nof Sonic Adventure.",{699,551,493,60},21,white,true,true);
    }
    if(state.page==Page::Installing){
        const auto back=back_button(width,height);c.rounded(back,int(7*s),{25,65,87});
        c.text(resources.regular,L"Cancel",{back.x,back.y+int(17*s),back.w,back.h-int(20*s)},19*s,white,false,true);
    }else{
        const auto primary=primary_button(width,height);c.rounded(primary,int(7*s),gold);
        const auto label=state.page==Page::Legal?L"I understand":state.page==Page::Welcome?L"Continue":state.page==Page::Files?(state.source_selected?L"Verify & install":L"Choose GDI"):state.page==Page::Ready?L"Play Sonic Adventure":L"Back to setup";
        c.text(resources.bold,label,{primary.x+12,primary.y+int(16*s),primary.w-24,primary.h-int(20*s)},20*s,ink,false,true);
        if(state.page!=Page::Welcome && state.page!=Page::Ready){const auto back=back_button(width,height);c.rounded(back,int(7*s),{25,65,87});
            c.text(resources.regular,state.page==Page::Legal?L"Exit":L"Back",{back.x,back.y+int(17*s),back.w,back.h-int(20*s)},19*s,white,false,true);}
        if(state.selected==1&&state.page!=Page::Welcome&&state.page!=Page::Ready){const auto box=back_button(width,height);
            c.line(box.x,box.y,box.x+box.w-1,box.y,cyan,int(2*s));c.line(box.x,box.y+box.h-2,box.x+box.w-1,box.y+box.h-2,cyan,int(2*s));}
        if(state.selected==2&&state.page==Page::Files){const auto box=r({699,509,493,77});
            c.line(box.x,box.y,box.x+box.w-1,box.y,cyan,int(2*s));c.line(box.x,box.y+box.h-2,box.x+box.w-1,box.y+box.h-2,cyan,int(2*s));}
    }
    fill({699,717,493,1},{44,76,94});
    text(L"Original game © SEGA.  |  Legal & third-party notices",{699,730,500,20},13,muted);
    c.text(resources.bold,L"Port by SoNiCFReaK",r({699,752,500,21}),15*s,white,false,false,-0.25f*s);
    text(L"Powered by KatanaRecomp",{699,774,500,19},13,cyan);
    return std::move(c.image);
}
}
