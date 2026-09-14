#include "sonic_raster.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace sonic::ui {
namespace {
std::vector<unsigned char> bytes(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("Cannot open UI resource: "+path.string());
    return {std::istreambuf_iterator<char>(file),{}};
}
}
Image Image::load(const std::filesystem::path& path){
    const auto data=bytes(path);int w=0,h=0,channels=0;
    const auto* raw=stbi_load_from_memory(data.data(),int(data.size()),&w,&h,&channels,4);
    if(!raw || w<1 || h<1 || w>16384 || h>16384){stbi_image_free(const_cast<unsigned char*>(raw));throw std::runtime_error("Invalid UI image");}
    Image result{unsigned(w),unsigned(h),{raw,raw+std::size_t(w)*h*4}};
    stbi_image_free(const_cast<unsigned char*>(raw));return result;
}
void Image::save(const std::filesystem::path& path)const{
    std::ofstream file(path,std::ios::binary);
    if(!file)throw std::runtime_error("Cannot write UI capture");
    const auto write=[](void* context,void* data,int size){static_cast<std::ofstream*>(context)->write(static_cast<char*>(data),size);};
    if(!stbi_write_png_to_func(write,&file,int(width),int(height),4,pixels.data(),int(width)*4)||!file)
        throw std::runtime_error("Cannot encode UI capture");
}
struct Font::Impl {std::vector<unsigned char> bytes;stbtt_fontinfo info{};};
Font::Font(const std::filesystem::path& path):impl_(std::make_shared<Impl>()){
    impl_->bytes=bytes(path);
    if(!stbtt_InitFont(&impl_->info,impl_->bytes.data(),stbtt_GetFontOffsetForIndex(impl_->bytes.data(),0)))
        throw std::runtime_error("Invalid UI font");
}
float Font::measure(std::wstring_view text,float size,float letter_spacing)const{
    const auto scale=stbtt_ScaleForPixelHeight(&impl_->info,size);float width=0;
    for(unsigned i=0;i<text.size();++i){int advance=0,bearing=0;stbtt_GetCodepointHMetrics(&impl_->info,text[i],&advance,&bearing);
        width+=advance*scale;if(i+1<text.size())width+=stbtt_GetCodepointKernAdvance(&impl_->info,text[i],text[i+1])*scale+letter_spacing;}
    return width;
}
Canvas::Canvas(unsigned w,unsigned h):image{w,h,std::vector<std::uint8_t>(std::size_t(w)*h*4,0)}{}
void Canvas::pixel(int x,int y,Color c){
    if(x<0||y<0||unsigned(x)>=image.width||unsigned(y)>=image.height)return;
    auto* d=image.pixels.data()+(std::size_t(y)*image.width+x)*4;
    const unsigned a=c.a,back=d[3]*(255-a)/255,out=a+back;
    if(out){d[0]=std::uint8_t((c.r*a+d[0]*back)/out);d[1]=std::uint8_t((c.g*a+d[1]*back)/out);d[2]=std::uint8_t((c.b*a+d[2]*back)/out);}
    d[3]=std::uint8_t(out);
}
void Canvas::fill(Rect r,Color c){for(int y=std::max(0,r.y);y<std::min(int(image.height),r.y+r.h);++y)
    for(int x=std::max(0,r.x);x<std::min(int(image.width),r.x+r.w);++x)pixel(x,y,c);}
void Canvas::rounded(Rect r,int radius,Color c){radius=std::max(0,std::min({radius,r.w/2,r.h/2}));
    for(int y=std::max(0,r.y);y<std::min(int(image.height),r.y+r.h);++y)for(int x=std::max(0,r.x);x<std::min(int(image.width),r.x+r.w);++x){
        const int cx=std::clamp(x,r.x+radius,r.x+r.w-radius-1),cy=std::clamp(y,r.y+radius,r.y+r.h-radius-1);
        const float distance=std::sqrt(float((x-cx)*(x-cx)+(y-cy)*(y-cy)));
        if(distance>radius+0.5f)continue;auto shaded=c;shaded.a=std::uint8_t(c.a*std::clamp(radius+0.5f-distance,0.0f,1.0f));pixel(x,y,shaded);}}
void Canvas::gradient(Rect r,Color a,Color b){for(int y=0;y<r.h;++y){const auto t=float(y)/std::max(1,r.h-1);
    const auto mix=[&](int x,int z){return std::uint8_t(x+(z-x)*t);};fill({r.x,r.y+y,r.w,1},{mix(a.r,b.r),mix(a.g,b.g),mix(a.b,b.b),mix(a.a,b.a)});}}
void Canvas::line(int x,int y,int x2,int y2,Color c,int thickness){
    const int count=std::max(std::abs(x2-x),std::abs(y2-y));if(!count){fill({x,y,thickness,thickness},c);return;}
    for(int i=0;i<=count;++i)fill({x+(x2-x)*i/count,y+(y2-y)*i/count,thickness,thickness},c);}
void Canvas::blit(const Image& source,Rect r,bool cover,std::uint8_t alpha){
    if(!source.width||!source.height||r.w<=0||r.h<=0)return;
    const auto factor=cover?std::max(float(r.w)/source.width,float(r.h)/source.height):std::min(float(r.w)/source.width,float(r.h)/source.height);
    const float left=r.x+(r.w-source.width*factor)/2,top=r.y+(r.h-source.height*factor)/2;
    for(int y=std::max(r.y,0);y<std::min(r.y+r.h,int(image.height));++y)for(int x=std::max(r.x,0);x<std::min(r.x+r.w,int(image.width));++x){
        const float sx=(x-left)/factor,sy=(y-top)/factor;if(sx<0||sy<0||sx>=source.width||sy>=source.height)continue;
        const auto ix=unsigned(sx),iy=unsigned(sy),jx=std::min(ix+1,source.width-1),jy=std::min(iy+1,source.height-1);
        const auto* a=&source.pixels[(std::size_t(iy)*source.width+ix)*4];const auto* b=&source.pixels[(std::size_t(iy)*source.width+jx)*4];
        const auto* c=&source.pixels[(std::size_t(jy)*source.width+ix)*4];const auto* d=&source.pixels[(std::size_t(jy)*source.width+jx)*4];
        const auto component=[&](unsigned n){return std::uint8_t(std::lerp(std::lerp(float(a[n]),float(b[n]),sx-ix),std::lerp(float(c[n]),float(d[n]),sx-ix),sy-iy));};
        pixel(x,y,{component(0),component(1),component(2),std::uint8_t(unsigned(component(3))*alpha/255)});}
}
int Canvas::text(const Font& font,std::wstring_view value,Rect r,float size,Color color,bool wrap,bool center,float letter_spacing){
    const auto* info=&font.impl_->info;const float scale=stbtt_ScaleForPixelHeight(info,size);
    int ascent,descent,gap;stbtt_GetFontVMetrics(info,&ascent,&descent,&gap);
    const int line_height=int(std::ceil(size*1.35f));int y=r.y;
    while(!value.empty() && y+size<=r.y+r.h){
        auto length=value.find(L'\n');if(length==std::wstring_view::npos)length=value.size();
        if(font.measure(value.substr(0,length),size,letter_spacing)>r.w){
            unsigned fit=0;while(fit<length&&font.measure(value.substr(0,fit+1),size,letter_spacing)<=r.w)++fit;
            if(wrap){auto space=value.substr(0,fit).find_last_of(L' ');length=space==std::wstring_view::npos?std::max(1u,fit):std::max(std::size_t(1),space);}
            else length=fit;
        }
        const auto line=value.substr(0,length);float x=float(r.x)+(center?(r.w-font.measure(line,size,letter_spacing))/2:0);
        for(unsigned i=0;i<line.size();++i){int w,h,ox,oy;
            const int origin=int(std::floor(x));
            auto* bitmap=stbtt_GetCodepointBitmapSubpixel(info,scale,scale,x-origin,0,line[i],&w,&h,&ox,&oy);
            for(int py=0;py<h;++py)for(int px=0;px<w;++px){auto c=color;c.a=std::uint8_t(unsigned(c.a)*bitmap[py*w+px]/255);
                const int dx=origin+ox+px,dy=y+int(ascent*scale)+oy+py;if(r.contains(dx,dy))pixel(dx,dy,c);}
            stbtt_FreeBitmap(bitmap,nullptr);int advance,bearing;stbtt_GetCodepointHMetrics(info,line[i],&advance,&bearing);x+=advance*scale;
            if(i+1<line.size())x+=stbtt_GetCodepointKernAdvance(info,line[i],line[i+1])*scale+letter_spacing;
        }
        y+=line_height;if(!wrap)break;value.remove_prefix(length);while(!value.empty()&&(value.front()==L' '||value.front()==L'\n'))value.remove_prefix(1);
    }
    return y-r.y;
}
}
