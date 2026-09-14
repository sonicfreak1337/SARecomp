#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#else
#include "linux/ui_raster_adapter.hpp"
#endif
#include "sonic_tutorial_art.hpp"
#include "sonic_input.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace sonic::tutorial {
namespace {
#include "sonic_tutorial_control_spans.inc"
constexpr unsigned scale=4;
struct Mask {
    HDC dc=CreateCompatibleDC(nullptr); HBITMAP bitmap=nullptr; HGDIOBJ old=nullptr;
    HFONT font=nullptr; HGDIOBJ old_font=nullptr; std::uint8_t* bits=nullptr;
    unsigned width,height;
    Mask(unsigned w,unsigned h):width(w),height(h){
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=LONG(w);info.bmiHeader.biHeight=-LONG(h);
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        void* address=nullptr;bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&address,nullptr,0);
        if(!dc || !bitmap || !address){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);
            throw std::runtime_error("tutorial-art-surface");}
        bits=static_cast<std::uint8_t*>(address);old=SelectObject(dc,bitmap);
        std::fill_n(bits,std::size_t(w)*h*4,0);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
    }
    ~Mask(){if(old_font)SelectObject(dc,old_font);if(font)DeleteObject(font);
        if(old)SelectObject(dc,old);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
    void font_size(unsigned pixels,int language){
        if(old_font){SelectObject(dc,old_font);old_font=nullptr;}if(font)DeleteObject(font);
        font=CreateFontW(-int(pixels),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,
            language==0?L"Yu Gothic UI":L"Segoe UI");
        if(!font)throw std::runtime_error("tutorial-art-font");old_font=SelectObject(dc,font);
    }
    unsigned measure(const std::wstring& value){SIZE size{};
        if(!GetTextExtentPoint32W(dc,value.data(),int(value.size()),&size))throw std::runtime_error("tutorial-art-measure");
        return unsigned(size.cx);}
};
void over(std::byte* p,const std::array<unsigned,3>& color,unsigned alpha){
    const auto previous=std::to_integer<unsigned>(p[3]);
    const auto out=alpha+(previous*(255-alpha)+127)/255;
    if(!out)return;
    for(unsigned c=0;c<3;++c)p[c]=std::byte((color[c]*alpha+
        (std::to_integer<unsigned>(p[c])*previous*(255-alpha)+127)/255+out/2)/out);
    p[3]=std::byte(out);
}
void paint_mask(Image& image,const Mask& mask,unsigned left,unsigned top){
    GdiFlush();
    // A small dark outline preserves contrast over the original panels.
    for(unsigned pass=0;pass<2;++pass)for(unsigned y=0;y<mask.height;++y)for(unsigned x=0;x<mask.width;++x){
        const unsigned alpha=mask.bits[(std::size_t(y)*mask.width+x)*4];if(!alpha)continue;
        if(pass==0){for(int dy=-2;dy<=2;dy+=2)for(int dx=-2;dx<=2;dx+=2){
            const int xx=int(left+x)+dx,yy=int(top+y)+dy;
            if(xx>=0 && yy>=0 && unsigned(xx)<image.width && unsigned(yy)<image.height)
                over(image.pixels.data()+(std::size_t(yy)*image.width+xx)*4,{0,22,50},alpha);
        }}else if(left+x<image.width && top+y<image.height)
            over(image.pixels.data()+(std::size_t(top+y)*image.width+left+x)*4,{255,255,255},alpha);
    }
}
Image diagram(const Controls& controls){
    Image image{128*scale,128*scale,{}};image.pixels.resize(std::size_t(image.width)*image.height*4);
    Mask mask(image.width,image.height);mask.font_size(12*scale,controls.language);
    auto pen=CreatePen(PS_SOLID,2*scale,RGB(255,255,255));auto old_pen=SelectObject(mask.dc,pen);
    auto old_brush=SelectObject(mask.dc,GetStockObject(NULL_BRUSH));
    if(controls.style==input::GlyphStyle::Keyboard){
        RoundRect(mask.dc,8*scale,28*scale,120*scale,85*scale,8*scale,8*scale);
        for(unsigned y=0;y<3;++y)for(unsigned x=0;x<8;++x)
            Rectangle(mask.dc,(16+x*12)*scale,(35+y*13)*scale,(24+x*12)*scale,(43+y*13)*scale);
        Rectangle(mask.dc,36*scale,74*scale,93*scale,80*scale);
        RoundRect(mask.dc,49*scale,94*scale,79*scale,124*scale,12*scale,12*scale);
        MoveToEx(mask.dc,64*scale,94*scale,nullptr);LineTo(mask.dc,64*scale,107*scale);
    }else{
        POINT points[]={{24*scale,24*scale},{100*scale,24*scale},{121*scale,86*scale},
            {106*scale,102*scale},{80*scale,76*scale},{46*scale,76*scale},{21*scale,102*scale},
            {7*scale,86*scale},{24*scale,24*scale}};
        Polyline(mask.dc,points,int(std::size(points)));
        for(unsigned x:std::array{44u,72u})Ellipse(mask.dc,(x-9)*scale,54*scale,(x+9)*scale,72*scale);
        const auto text=[&](std::wstring_view value,int x,int y,int w,int h){RECT r{x*int(scale),y*int(scale),(x+w)*int(scale),(y+h)*int(scale)};
            DrawTextW(mask.dc,value.data(),int(value.size()),&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);};
        text(L"+",21,32,18,24);
        const bool ps=controls.style==input::GlyphStyle::PlayStation;
        text(ps?L"△":L"Y",86,26,14,18);text(ps?L"□":L"X",73,40,14,18);
        text(ps?L"○":L"B",99,40,14,18);text(ps?L"×":L"A",86,54,14,18);
        mask.font_size(9*scale,controls.language);text(L"L",36,53,16,19);text(L"R",64,53,16,19);
    }
    SelectObject(mask.dc,old_brush);SelectObject(mask.dc,old_pen);DeleteObject(pen);
    paint_mask(image,mask,0,0);return image;
}
}
std::span<const Artwork> artwork_catalog() noexcept{return artwork_table;}
const Artwork* find_artwork(std::string_view archive,std::string_view archive_sha256,
    unsigned ordinal,std::string_view pvrt_sha256) noexcept{
    for(const auto& row:artwork_table)if(row.archive==archive && row.ordinal==ordinal &&
        row.archive_sha256==archive_sha256 && row.pvrt_sha256==pvrt_sha256)return &row;
    return nullptr;
}
std::wstring control_label(ControlToken token,const Controls& controls){
    using A=input::Action;
    const auto name=[&](A action){return input::binding_name(controls.bindings[unsigned(action)],controls.style);};
    const auto four=[&](A up,A down,A left,A right){return name(up)+L"/"+name(left)+L"/"+name(down)+L"/"+name(right);};
    switch(token){
        case ControlToken::A:return name(A::A);case ControlToken::B:return name(A::B);
        case ControlToken::X:return name(A::X);case ControlToken::Y:return name(A::Y);
        case ControlToken::Move:return controls.style==input::GlyphStyle::Keyboard?
            four(A::Up,A::Down,A::Left,A::Right):(controls.swap_sticks?L"RS":L"LS");
        case ControlToken::Camera:
            if(controls.recompiled){
                if(controls.style!=input::GlyphStyle::Keyboard)return controls.swap_sticks?L"LS":L"RS";
                if(controls.mouse_camera){constexpr std::array mouse{L"マウス",L"Mouse",L"Souris",L"Ratón",L"Maus"};
                    return mouse[std::clamp(controls.language,0,4)];}
                return four(A::LookUp,A::LookDown,A::LookLeft,A::LookRight);
            }
            [[fallthrough]];
        case ControlToken::ShoulderPair:return name(A::LeftTrigger)+L" / "+name(A::RightTrigger);
        case ControlToken::Diagram:return L"";
    }return L"";
}
Image rasterize_artwork(const Artwork& art,const Controls& controls,std::span<const std::uint8_t> source){
    if(art.width>512 || !art.width || !art.height || art.height>512 ||
        source.size()!=std::size_t(art.width)*art.height*4 || art.spans.empty())
        throw std::runtime_error("tutorial-art-shape");
    if(art.spans.size()==1 && art.spans[0].token==ControlToken::Diagram){
        if(art.width!=128 || art.height!=128)throw std::runtime_error("tutorial-diagram-shape");
        return diagram(controls);
    }
    if(art.height!=32)throw std::runtime_error("tutorial-art-strip-height");
    struct Run{ControlSpan span;std::wstring label;unsigned width;};std::vector<Run> runs;
    Mask measure(4,4);measure.font_size(18*scale,controls.language);
    unsigned previous=0,total=art.width,used=0;
    for(unsigned y=0;y<art.height;++y)for(unsigned x=0;x<art.width;++x)
        if(source[(std::size_t(y)*art.width+x)*4+3])used=std::max(used,x+1);
    for(const auto& span:art.spans){
        if(!span.w || !span.h || span.x<previous || span.x+span.w>art.width || span.y+span.h>art.height)
            throw std::runtime_error("tutorial-art-span");
        auto label=control_label(span.token,controls);const auto w=std::max(span.w,(measure.measure(label)+scale-1)/scale+4);
        if(w>512)throw std::runtime_error("tutorial-art-label-width");
        total+=w-span.w;used+=w-span.w;previous=span.x+span.w;runs.push_back({span,std::move(label),w});
    }
    if(total>2048)throw std::runtime_error("tutorial-art-row-budget");
    Image work{total*scale,art.height*scale,{}};work.pixels.resize(std::size_t(work.width)*work.height*4);
    const auto copy=[&](unsigned from,unsigned to,unsigned width){
        for(unsigned y=0;y<work.height;++y)for(unsigned x=0;x<width*scale;++x){
            const auto src=(std::size_t(y/scale)*art.width+from+x/scale)*4;
            auto* dst=work.pixels.data()+(std::size_t(y)*work.width+to*scale+x)*4;
            for(unsigned c=0;c<4;++c)dst[c]=std::byte(source[src+c]);
        }
    };
    unsigned from=0,to=0;
    for(const auto& run:runs){
        copy(from,to,run.span.x-from);to+=run.span.x-from;
        // Only the input glyph columns are removed. Some original labels sit
        // on solid blue instruction capsules: extend that background per row.
        for(unsigned y=0;y<work.height;++y){
            std::array<std::uint8_t,4> background{};
            for(int direction:std::array{-1,1})for(unsigned d=1;d<=3;++d){
                const int sx=direction<0?int(run.span.x)-int(d):int(run.span.x+run.span.w+d-1);
                if(sx<0 || unsigned(sx)>=art.width)continue;
                const auto* p=source.data()+(std::size_t(y/scale)*art.width+sx)*4;
                if(p[0]<40 && p[1]<60 && p[2]>180 && p[3]>220)std::copy_n(p,4,background.begin());
            }
            for(unsigned x=0;x<run.width*scale;++x){auto* dst=work.pixels.data()+(std::size_t(y)*work.width+to*scale+x)*4;
                if(y/scale<run.span.y || y/scale>=run.span.y+run.span.h){
                    const auto sx=run.span.x+std::min(run.span.w-1,(x/scale)*run.span.w/run.width);
                    for(unsigned c=0;c<4;++c)dst[c]=std::byte(source[(std::size_t(y/scale)*art.width+sx)*4+c]);
                }else for(unsigned c=0;c<4;++c)dst[c]=std::byte(background[c]);}
        }
        Mask label(run.width*scale,work.height);label.font_size(18*scale,controls.language);
        RECT rect{2*int(scale),0,int(label.width)-2*int(scale),int(label.height)};
        DrawTextW(label.dc,run.label.data(),int(run.label.size()),&rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        paint_mask(work,label,to*scale,0);from=run.span.x+run.span.w;to+=run.width;
    }
    copy(from,to,art.width-from);
    Image result{art.width,art.height,{}};result.pixels.resize(std::size_t(result.width)*result.height*4);
    // Extra space is taken from original transparent right padding first.
    // Only genuinely over-wide lines contract; page geometry never changes.
    const double squeeze=double(std::max(art.width,used))/art.width;
    // Resolve only the supersampled host glyphs here. Untouched source pixels
    // remain exact (not nearest-upscaled artwork sampled again by the GPU).
    for(unsigned y=0;y<result.height;++y)for(unsigned x=0;x<result.width;++x){
        std::array<unsigned,4> sum{};
        for(unsigned dy=0;dy<scale;++dy)for(unsigned dx=0;dx<scale;++dx){
            const auto sx=std::min(work.width-1,unsigned((x*scale+dx)*squeeze));
            const auto* p=work.pixels.data()+(std::size_t(y*scale+dy)*work.width+sx)*4;
            for(unsigned c=0;c<4;++c)sum[c]+=std::to_integer<unsigned>(p[c]);
        }
        for(unsigned c=0;c<4;++c)result.pixels[(std::size_t(y)*result.width+x)*4+c]=
            std::byte((sum[c]+scale*scale/2)/(scale*scale));
    }
    return result;
}
}
