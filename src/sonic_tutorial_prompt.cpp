#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "sonic_tutorial_prompt.hpp"
#include "sonic_input.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace sonic::tutorial {
Labels labels(int language,const input::Bindings& bindings,input::GlyphStyle style) {
    constexpr std::array next{L"次へ",L"Next",L"Suivant",L"Siguiente",L"Weiter"};
    constexpr std::array back{L"戻る",L"Back",L"Retour",L"Volver",L"Zurück"};
    language=std::clamp(language,0,4);
    const auto a=input::binding_name(bindings[unsigned(input::Action::A)],style);
    auto b=input::binding_name(bindings[unsigned(input::Action::B)],style);
    // SUMMARY accepts X or B for Back. Prefer the usual B/Cancel position;
    // if it is unbound, show the still-valid X alternative. A wins in guest code.
    if(b==L"—") b=input::binding_name(bindings[unsigned(input::Action::X)],style);
    return {language,a+L" : "+next[language],b+L" : "+back[language]};
}
Image rasterize(const Labels& labels,unsigned visible_width) {
    if(visible_width<128 || visible_width>512) throw std::runtime_error("tutorial-visible-width");
    constexpr int scale=4,width=512*scale,height=32*scale;
    struct Surface {
        HDC dc=CreateCompatibleDC(nullptr); HBITMAP bitmap=nullptr; HGDIOBJ old=nullptr;
        HFONT font=nullptr; HGDIOBJ old_font=nullptr;
        ~Surface(){if(old_font)SelectObject(dc,old_font);if(font)DeleteObject(font);
            if(old)SelectObject(dc,old);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
    } surface;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;
    info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;
    surface.bitmap=CreateDIBSection(surface.dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!surface.dc || !surface.bitmap || !pixels)throw std::runtime_error("tutorial-raster-surface");
    surface.old=SelectObject(surface.dc,surface.bitmap);SetBkMode(surface.dc,TRANSPARENT);
    for(int y=0;y<height;++y){auto brush=CreateSolidBrush(RGB(39+y*10/height,176+y*14/height,233));
        RECT row{0,y,width,y+1};FillRect(surface.dc,&row,brush);DeleteObject(brush);}
    const auto select_font=[&](int size){
        if(surface.old_font){SelectObject(surface.dc,surface.old_font);surface.old_font=nullptr;}
        if(surface.font)DeleteObject(surface.font);
        surface.font=CreateFontW(-size*scale,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,
            labels.language==0?L"Yu Gothic UI":L"Segoe UI");
        if(!surface.font)throw std::runtime_error("tutorial-raster-font");
        surface.old_font=SelectObject(surface.dc,surface.font);
    };
    const auto measure=[&](const std::wstring& text){SIZE size{};
        if(!GetTextExtentPoint32W(surface.dc,text.data(),int(text.size()),&size))throw std::runtime_error("tutorial-raster-measure");
        return size.cx;};
    const auto line=labels.next+L"     "+labels.back;
    const int available=int(visible_width-12)*scale;
    bool two_lines=false,fit=false;
    for(int size=19;size>=15;--size){select_font(size);if(measure(line)<=available){fit=true;break;}}
    if(!fit){two_lines=true;for(int size=13;size>=9;--size){select_font(size);
        if(std::max(measure(labels.next),measure(labels.back))<=available){fit=true;break;}}}
    if(!fit)throw std::runtime_error("tutorial-label-overflow");
    SetTextColor(surface.dc,RGB(0,16,36));
    const auto draw=[&](const std::wstring& text,int top,int bottom){
        RECT rect{6*scale,top,int(visible_width-6)*scale,bottom};
        DrawTextW(surface.dc,text.data(),int(text.size()),&rect,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    };
    if(two_lines){draw(labels.next,0,height/2);draw(labels.back,height/2,height);}
    else draw(line,0,height);
    GdiFlush();Image image{width,height,{}};image.pixels.resize(std::size_t(width)*height*4);
    const auto* source=static_cast<const std::byte*>(pixels);
    for(std::size_t i=0;i<image.pixels.size();i+=4){image.pixels[i]=source[i+2];image.pixels[i+1]=source[i+1];
        image.pixels[i+2]=source[i];image.pixels[i+3]=std::byte{255};}
    return image;
}
}
