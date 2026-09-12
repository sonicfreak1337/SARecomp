#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include "sonic_menu.hpp"
#include "sonic_menu_text.hpp"
#include "sonic_startup.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <fstream>
namespace sonic::menu {
namespace {Image background;}
void load_background(const std::filesystem::path& path){
    if(!background.pixels.empty())return;
    if(startup::file_digest(path)!="87b1a09d2a384825c0a82d05b857136fec64ac282f56cfc441d8ae57bc5d9330")throw std::runtime_error("menu-background-identity");
    Gdiplus::GdiplusStartupInput startup;ULONG_PTR token=0;
    if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok)throw std::runtime_error("menu-background-decoder");
    struct Stop {ULONG_PTR token;~Stop(){Gdiplus::GdiplusShutdown(token);}} stop{token};
    Gdiplus::Bitmap image(path.c_str());
    if(image.GetLastStatus()!=Gdiplus::Ok||image.GetWidth()!=2560||image.GetHeight()!=1920)throw std::runtime_error("menu-background-dimensions");
    Image next{image.GetWidth(),image.GetHeight(),{}};next.pixels.resize(std::size_t(next.width)*next.height*4);
    Gdiplus::Rect rect(0,0,next.width,next.height);Gdiplus::BitmapData data{};
    if(image.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data)!=Gdiplus::Ok)throw std::runtime_error("menu-background-pixels");
    for(unsigned y=0;y<next.height;++y)for(unsigned x=0;x<next.width;++x){
        const auto* p=static_cast<const std::byte*>(data.Scan0)+std::ptrdiff_t(y)*data.Stride+x*4;
        auto* d=next.pixels.data()+(std::size_t(y)*next.width+x)*4;d[0]=p[2];d[1]=p[1];d[2]=p[0];d[3]=std::byte{255};
    }
    image.UnlockBits(&data);
    background=std::move(next);
}
Image rasterize(const Model& model,unsigned width,unsigned height){
    if(width<320||height<240||width>7680||height>4320)throw std::invalid_argument("menu-extent");
    Image result{width,height,{}};result.pixels.resize(std::size_t(width)*height*4);
    struct Canvas {
        HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=nullptr;HGDIOBJ old=nullptr;
        std::array<HFONT,4> fonts{};
        ~Canvas(){if(dc){SelectObject(dc,GetStockObject(SYSTEM_FONT));if(old)SelectObject(dc,old);for(auto f:fonts)if(f)DeleteObject(f);if(bitmap)DeleteObject(bitmap);DeleteDC(dc);}}
    } c;
    if(!c.dc)throw std::runtime_error("menu-canvas");
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=LONG(width);
    info.bmiHeader.biHeight=-LONG(height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* bits=nullptr;c.bitmap=CreateDIBSection(c.dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!c.bitmap||!bits)throw std::runtime_error("menu-bitmap");c.old=SelectObject(c.dc,c.bitmap);
    const float scale=std::min(width/1280.0f,height/900.0f);
    constexpr int sizes[]{44,25,21,18};
    for(unsigned i=0;i<4;++i)c.fonts[i]=CreateFontW(-std::max(12,int(sizes[i]*scale)),0,0,0,i<2?FW_SEMIBOLD:FW_NORMAL,FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,model.language()==0?L"Yu Gothic UI":L"Segoe UI");
    const auto fill=[&](Rect r,COLORREF color){RECT rect{r.left,r.top,r.right,r.bottom};auto brush=CreateSolidBrush(color);FillRect(c.dc,&rect,brush);DeleteObject(brush);};
    const auto gradient=[&](Rect r,COLORREF a,COLORREF b){
        for(int y=r.top;y<r.bottom;++y){const auto t=float(y-r.top)/std::max(1,r.bottom-r.top-1);
            const auto mix=[&](int x,int z){return BYTE(x+(z-x)*t);};
            fill({r.left,y,r.right,y+1},RGB(mix(GetRValue(a),GetRValue(b)),mix(GetGValue(a),GetGValue(b)),mix(GetBValue(a),GetBValue(b))));}
    };
    const auto draw=[&](std::wstring_view value,Rect r,unsigned font,COLORREF color,UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE){
        RECT rect{r.left,r.top,r.right,r.bottom};SelectObject(c.dc,c.fonts[font]);SetTextColor(c.dc,color);
        DrawTextW(c.dc,value.data(),int(value.size()),&rect,flags|DT_NOPREFIX|DT_END_ELLIPSIS);
    };
    SetBkMode(c.dc,TRANSPARENT);
    gradient({0,0,int(width),int(height)},RGB(10,36,69),RGB(26,125,173));
    // Quiet circular motifs echo the retail menu without stretching its atlas.
    auto pen=CreatePen(PS_SOLID,std::max(1,int(scale)),RGB(33,107,147));auto oldpen=SelectObject(c.dc,pen);auto oldbrush=SelectObject(c.dc,GetStockObject(NULL_BRUSH));
    for(int i=0;i<5;++i){const int r=int((380+i*90)*scale);Ellipse(c.dc,int(width)-r,-r,int(width)+r,r);}
    SelectObject(c.dc,oldbrush);SelectObject(c.dc,oldpen);DeleteObject(pen);
    if(!background.pixels.empty()){
        GdiFlush();auto* destination=static_cast<std::byte*>(bits);
        // Aspect-preserving cover: widescreen crops only the artwork's excess
        // height; the supplied clouds are never stretched or tiled.
        const float factor=std::max(float(width)/background.width,float(height)/background.height);
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
            const auto sx=unsigned(std::clamp((x-width/2.0f)/factor+background.width/2.0f,0.0f,float(background.width-1)));
            const auto sy=unsigned(std::clamp((y-height/2.0f)/factor+background.height/2.0f,0.0f,float(background.height-1)));
            const auto* source=background.pixels.data()+(std::size_t(sy)*background.width+sx)*4;
            auto* pixel=destination+(std::size_t(y)*width+x)*4;pixel[0]=source[2];pixel[1]=source[1];pixel[2]=source[0];pixel[3]=std::byte{255};
        }
    }
    const auto first=model.list_bounds(width,height);
    const int pad=std::max(2,int(18*scale));
    const auto rounded=[&](Rect r,int radius,COLORREF color,BYTE alpha){
        GdiFlush();auto* output=static_cast<unsigned char*>(bits);
        for(int y=std::max(0,r.top);y<std::min(int(height),r.bottom);++y)for(int x=std::max(0,r.left);x<std::min(int(width),r.right);++x){
            const int cx=std::clamp(x,r.left+radius,r.right-radius-1),cy=std::clamp(y,r.top+radius,r.bottom-radius-1);
            if((x-cx)*(x-cx)+(y-cy)*(y-cy)>radius*radius)continue;
            auto* p=output+(std::size_t(y)*width+x)*4;
            p[0]=BYTE((p[0]*(255-alpha)+GetBValue(color)*alpha)/255);p[1]=BYTE((p[1]*(255-alpha)+GetGValue(color)*alpha)/255);p[2]=BYTE((p[2]*(255-alpha)+GetRValue(color)*alpha)/255);
        }
    };
    const Rect panel{first.left-pad,int(first.top-122*scale),first.right+pad,first.bottom+pad};
    for(int i=12;i>=2;--i)rounded({panel.left+i,panel.top+i,panel.right+i,panel.bottom+i},int(30*scale),RGB(0,10,16),12);
    rounded(panel,int(30*scale),RGB(18,123,170),210);
    gradient({first.left-pad,int(first.top-118*scale),first.right+pad,int(first.top-29*scale)},RGB(18,87,133),RGB(10,43,75));
    draw(model.heading(),{first.left+pad,int(first.top-109*scale),first.right-pad,int(first.top-42*scale)},0,RGB(250,250,255));
    draw(L"SONIC ADVENTURE  ·  RECOMPILED",{first.left,int(first.top-157*scale),first.right,int(first.top-126*scale)},3,RGB(18,71,98),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    const auto rows=model.rows();
    if(!model.body().empty())
        draw(model.body(),{first.left+pad,int(first.top+100*scale),first.right-pad,int(first.bottom-100*scale)},1,RGB(250,252,255),DT_CENTER|DT_WORDBREAK);
    for(unsigned n=0;n<model.visible_count() && model.first()+n<rows.size();++n){
        const auto index=model.first()+n;const auto& row=rows[index];auto r=model.row_rect(n,width,height);
        const bool selected=index==model.selected();
        if(selected){fill(r,RGB(245,178,0));const int border=std::max(2,int(4*scale));r={r.left+border,r.top+border,r.right-border,r.bottom-border};gradient(r,RGB(75,0,4),RGB(207,9,20));}
        else gradient(r,RGB(12,39,64),RGB(13,119,166));
        const auto color=row.enabled?RGB(247,250,255):RGB(134,170,186);
        const int middle=first.left+int(685*scale);
        draw(row.label,{r.left+pad,r.top,row.value.empty()?r.right-pad:middle-pad,r.bottom},1,color,row.value.empty()?DT_CENTER|DT_VCENTER|DT_SINGLELINE:DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        if(row.restart)draw(L"*",{middle-int(15*scale),r.top,middle,r.bottom},2,RGB(255,212,95));
        if(!row.value.empty()){
            if(!row.read_only)draw(L"‹",{middle,r.top,middle+int(30*scale),r.bottom},1,color);
            draw(row.value,{middle+int(35*scale),r.top,r.right-int(42*scale),r.bottom},2,color,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
            if(!row.read_only)draw(L"›",{r.right-int(30*scale),r.top,r.right-pad/2,r.bottom},1,color);
        }
    }
    const int bottom=first.bottom+int(32*scale);
    rounded({first.left-pad,bottom-int(8*scale),first.right+pad,bottom+int(132*scale)},int(18*scale),RGB(15,67,93),225);
    draw(model.description(),{first.left,bottom,first.right,bottom+int(66*scale)},2,RGB(228,242,248),DT_LEFT|DT_WORDBREAK);
    const auto footer=model.footer();
    draw(footer,{first.left,bottom+int(82*scale),first.right,bottom+int(123*scale)},3,RGB(249,252,255));
    const auto page=std::to_wstring(model.first()/model.visible_count()+1)+L" / "+std::to_wstring((rows.size()+model.visible_count()-1)/model.visible_count());
    draw(page,{first.right-int(160*scale),bottom+int(82*scale),first.right,bottom+int(123*scale)},3,RGB(188,225,241),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    if(std::any_of(rows.begin(),rows.end(),[](const auto& r){return r.restart;}))
        draw(L"* "+std::wstring(text("restart",model.language())),{first.left,int(first.top-30*scale),first.right,first.top-int(7*scale)},3,RGB(255,211,101),DT_RIGHT|DT_SINGLELINE);
    if(model.modal()){
        const int x=int(width/2.0f-485*scale),y=int(height/2.0f-150*scale);
        fill({x-int(6*scale),y-int(6*scale),int(width)-x+int(6*scale),int(height/2.0f+165*scale)},RGB(240,177,13));
        gradient({x,y,int(width)-x,int(height/2.0f+160*scale)},RGB(14,41,64),RGB(15,74,107));
        draw(model.dialog(),{x+pad,y+pad,int(width)-x-pad,int(height/2.0f+50*scale)},1,RGB(252,252,255),DT_CENTER|DT_WORDBREAK);
        if(!model.capturing())for(int yes=0;yes<2;++yes){
            if(model.informational()&&!yes)continue;
            auto r=Model::confirm_rect(yes,width,height);
            if(model.informational()){const auto delta=int(width/2)-(r.left+r.right)/2;r.left+=delta;r.right+=delta;}
            const bool selected=model.informational()||model.confirm_selected()==(yes!=0);
            if(selected)fill(r,RGB(245,176,0));
            Rect inner{r.left+3,r.top+3,r.right-3,r.bottom-3};gradient(inner,selected?RGB(97,0,5):RGB(12,45,69),selected?RGB(205,10,20):RGB(9,117,167));
            draw(text(yes?"yes":"no",model.language()),inner,1,RGB(253,253,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
    }
    GdiFlush();auto source=static_cast<const std::byte*>(bits);
    for(std::size_t i=0;i<result.pixels.size();i+=4){result.pixels[i]=source[i+2];result.pixels[i+1]=source[i+1];result.pixels[i+2]=source[i];result.pixels[i+3]=std::byte{255};}
    return result;
}
}
