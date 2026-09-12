#define NOMINMAX
#include <windows.h>
#include "sonic_quit_prompt.hpp"
#include <array>
#include <stdexcept>

namespace sonic::quit_prompt {
const Labels& labels(int language) noexcept {
    // Same order as the PAL game's text-language setting.
    static constexpr std::array<Labels,5> text{{
        {L"ゲームを終了しますか？",L"終了",L"キャンセル",L"Enter: 終了     Esc: キャンセル"},
        {L"Quit the game?",L"Quit",L"Cancel",L"Enter: Quit     Esc: Cancel"},
        {L"Voulez-vous quitter le jeu ?",L"Quitter",L"Annuler",L"Entrée : Quitter     Échap : Annuler"},
        {L"¿Quieres salir del juego?",L"Salir",L"Cancelar",L"Intro: Salir     Esc: Cancelar"},
        {L"Spiel wirklich beenden?",L"Beenden",L"Abbrechen",L"Eingabe: Beenden     Esc: Abbrechen"},
    }};
    return text[language>=0 && language<5?language:1];
}
Image rasterize(int language,const input::Bindings& bindings,input::GlyphStyle glyphs) {
    Image result;result.pixels.resize(std::size_t(result.width)*result.height*4);
    struct Canvas {
        HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=nullptr;HGDIOBJ previous=nullptr;
        std::array<HFONT,4> fonts{};
        ~Canvas(){if(previous)SelectObject(dc,previous);for(auto font:fonts)if(font)DeleteObject(font);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
    } canvas;
    if(!canvas.dc)throw std::runtime_error("quit-prompt-canvas");
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=LONG(result.width);
    info.bmiHeader.biHeight=-LONG(result.height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* pixels=nullptr;canvas.bitmap=CreateDIBSection(canvas.dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!canvas.bitmap || !pixels)throw std::runtime_error("quit-prompt-bitmap");
    canvas.previous=SelectObject(canvas.dc,canvas.bitmap);
    const wchar_t* family=language==0?L"Yu Gothic UI":L"Segoe UI";
    constexpr std::array sizes{18,30,22,17};
    for(unsigned i=0;i<sizes.size();++i) {
        canvas.fonts[i]=CreateFontW(-sizes[i],0,0,0,i==1?FW_SEMIBOLD:FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,family);
        if(!canvas.fonts[i])throw std::runtime_error("quit-prompt-font");
    }
    const auto fill=[&](RECT rect,COLORREF color){auto brush=CreateSolidBrush(color);FillRect(canvas.dc,&rect,brush);DeleteObject(brush);};
    fill({0,0,640,256},RGB(16,28,48));fill({0,0,640,4},RGB(25,160,237));
    SetBkMode(canvas.dc,TRANSPARENT);
    const auto text=[&](std::wstring_view value,RECT rect,unsigned font,COLORREF color,UINT alignment=DT_CENTER){
        SelectObject(canvas.dc,canvas.fonts[font]);SetTextColor(canvas.dc,color);
        DrawTextW(canvas.dc,value.data(),int(value.size()),&rect,alignment|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
    };
    const auto& copy=labels(language);
    text(L"Sonic Adventure: Recompiled",{24,19,616,46},0,RGB(174,202,228));
    text(copy.question,{20,55,620,110},1,RGB(246,249,255));
    for(unsigned i=0;i<2;++i) {
        const int x=i?334:26;fill({x,131,x+280,191},RGB(29,51,76));
        const auto action=i?input::Action::Cancel:input::Action::Confirm;
        const auto hint=input::binding_name(bindings[unsigned(action)],glyphs);
        text(hint,{x+8,135,x+272,158},glyphs==input::GlyphStyle::Keyboard?3:2,i?RGB(255,139,145):RGB(131,194,255));
        text(i?copy.cancel:copy.confirm,{x+8,158,x+272,188},2,RGB(245,248,255));
    }
    text(copy.keyboard,{16,207,624,242},3,RGB(180,200,225));
    GdiFlush();const auto* bgra=static_cast<const std::byte*>(pixels);
    for(std::size_t i=0;i<result.pixels.size();i+=4){result.pixels[i]=bgra[i+2];result.pixels[i+1]=bgra[i+1];result.pixels[i+2]=bgra[i];result.pixels[i+3]=std::byte{255};}
    // The bitmap must not retain a selected font during GDI cleanup.
    SelectObject(canvas.dc,GetStockObject(SYSTEM_FONT));
    return result;
}
}
