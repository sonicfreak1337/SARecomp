#pragma once
// Private CPU UI drawing adapter. The retained menus use the same rectangles,
// strings, selections and compositing on both hosts; no Windows APIs are loaded.
#include "../ui/sonic_raster.hpp"
#include "../sonic_user_paths.hpp"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

using BYTE=unsigned char;using UINT=unsigned;using LONG=int;using COLORREF=unsigned;
struct RECT{int left,top,right,bottom;};struct POINT{int x,y;};struct SIZE{int cx,cy;};
struct BITMAPINFOHEADER{unsigned biSize=0;int biWidth=0,biHeight=0;unsigned short biPlanes=0,biBitCount=0;unsigned biCompression=0;};
struct BITMAPINFO{BITMAPINFOHEADER bmiHeader;};
constexpr unsigned RGB(unsigned r,unsigned g,unsigned b){return r|(g<<8)|(b<<16);}
constexpr BYTE GetRValue(COLORREF c){return BYTE(c);}constexpr BYTE GetGValue(COLORREF c){return BYTE(c>>8);}constexpr BYTE GetBValue(COLORREF c){return BYTE(c>>16);}
constexpr int DT_LEFT=0,DT_CENTER=1,DT_RIGHT=2,DT_VCENTER=4,DT_WORDBREAK=8,DT_SINGLELINE=16,DT_NOPREFIX=32,DT_END_ELLIPSIS=64;
constexpr int DIB_RGB_COLORS=0,BI_RGB=0,FW_NORMAL=400,FW_SEMIBOLD=600,FALSE=0,DEFAULT_CHARSET=0,OUT_DEFAULT_PRECIS=0,CLIP_DEFAULT_PRECIS=0,ANTIALIASED_QUALITY=0,DEFAULT_PITCH=0,TRANSPARENT=0,PS_SOLID=0;
constexpr int SYSTEM_FONT=1,NULL_BRUSH=2;
namespace sonic::linux_ui {
inline const ui::Font& font(bool bold,bool japanese){
    const auto root=paths::executable().parent_path()/"resources";
    static const ui::Font normal(root/"NotoSans-Regular.ttf"),strong(root/"NotoSans-Bold.ttf");
    if(japanese){static const ui::Font cjk(root/"NotoSansCJKjp-Regular.otf");return cjk;}
    return bold?strong:normal;
}
struct Object{enum Kind{Bitmap,Font,Brush,Pen}kind;bool stock=false;std::unique_ptr<ui::Canvas> image;int size=18,width=1;bool bold=false,japanese=false,empty=false;COLORREF color=0;explicit Object(Kind k):kind(k){}};
struct Context{Object *bitmap=nullptr,*font=nullptr,*pen=nullptr,*brush=nullptr;COLORREF color=RGB(255,255,255);POINT cursor{};};
inline Object stockfont{Object::Font},stockbrush{Object::Brush};
inline ui::Color color(COLORREF value){return {GetBValue(value),GetGValue(value),GetRValue(value),255};}
inline ui::Rect rectangle(RECT r){return {r.left,r.top,r.right-r.left,r.bottom-r.top};}
inline ui::Canvas& canvas(Context* dc){if(!dc||!dc->bitmap||!dc->bitmap->image)throw std::runtime_error("linux-ui-surface");return *dc->bitmap->image;}
}
using HDC=sonic::linux_ui::Context*;using HGDIOBJ=sonic::linux_ui::Object*;using HBITMAP=HGDIOBJ;using HFONT=HGDIOBJ;
inline HDC CreateCompatibleDC(void*){return new sonic::linux_ui::Context;}
inline void DeleteDC(HDC dc){delete dc;}
inline HGDIOBJ GetStockObject(int id){auto& value=id==NULL_BRUSH?sonic::linux_ui::stockbrush:sonic::linux_ui::stockfont;value.stock=true;value.empty=id==NULL_BRUSH;return &value;}
inline HGDIOBJ SelectObject(HDC dc,HGDIOBJ object){if(!object)return nullptr;HGDIOBJ* slot=nullptr;switch(object->kind){case sonic::linux_ui::Object::Bitmap:slot=&dc->bitmap;break;case sonic::linux_ui::Object::Font:slot=&dc->font;break;case sonic::linux_ui::Object::Brush:slot=&dc->brush;break;case sonic::linux_ui::Object::Pen:slot=&dc->pen;break;}return std::exchange(*slot,object);}
inline void DeleteObject(HGDIOBJ object){if(object&&!object->stock)delete object;}
inline HBITMAP CreateDIBSection(HDC,const BITMAPINFO* info,unsigned,void** bits,void*,unsigned){
    auto object=std::make_unique<sonic::linux_ui::Object>(sonic::linux_ui::Object::Bitmap);
    if(info->bmiHeader.biWidth<=0||info->bmiHeader.biHeight>=0||info->bmiHeader.biBitCount!=32)throw std::runtime_error("linux-ui-bitmap-layout");
    object->image=std::make_unique<sonic::ui::Canvas>(unsigned(info->bmiHeader.biWidth),unsigned(-info->bmiHeader.biHeight));
    *bits=object->image->image.pixels.data();return object.release();
}
inline HFONT CreateFontW(int height,int,int,int,int weight,int,int,int,int,int,int,int,int,const wchar_t* family){auto value=new sonic::linux_ui::Object(sonic::linux_ui::Object::Font);value->size=std::abs(height);value->bold=weight>=600;value->japanese=std::wstring_view(family).starts_with(L"Yu");return value;}
inline HGDIOBJ CreateSolidBrush(COLORREF c){auto value=new sonic::linux_ui::Object(sonic::linux_ui::Object::Brush);value->color=c;return value;}
inline HGDIOBJ CreatePen(int,int width,COLORREF c){auto value=new sonic::linux_ui::Object(sonic::linux_ui::Object::Pen);value->color=c;value->width=width;return value;}
inline void FillRect(HDC dc,const RECT* rect,HGDIOBJ brush){sonic::linux_ui::canvas(dc).fill(sonic::linux_ui::rectangle(*rect),sonic::linux_ui::color(brush->color));}
inline void SetBkMode(HDC,int){}
inline void SetTextColor(HDC dc,COLORREF c){dc->color=c;}
inline void GdiFlush(){}
inline bool GetTextExtentPoint32W(HDC dc,const wchar_t* text,int count,SIZE* size){const auto* f=dc->font?dc->font:GetStockObject(SYSTEM_FONT);size->cx=int(std::ceil(sonic::linux_ui::font(f->bold,f->japanese).measure({text,std::size_t(count)},float(f->size))));size->cy=f->size;return true;}
inline int DrawTextW(HDC dc,const wchar_t* text,int count,RECT* rect,unsigned flags){
    const auto* f=dc->font?dc->font:GetStockObject(SYSTEM_FONT);const auto& face=sonic::linux_ui::font(f->bold,f->japanese);
    std::wstring value(text,std::size_t(count));auto r=sonic::linux_ui::rectangle(*rect);
    const bool wrap=bool(flags&DT_WORDBREAK);const float size=float(f->size);
    if(!wrap&&(flags&DT_END_ELLIPSIS)&&face.measure(value,size)>r.w){while(!value.empty()&&face.measure(value+L"…",size)>r.w)value.pop_back();value+=L"…";}
    if(!wrap&&(flags&DT_VCENTER)){r.y+=(r.h-f->size)/2;r.h=f->size+4;}
    if(!wrap&&(flags&DT_RIGHT)){const int w=int(std::ceil(face.measure(value,size)));r.x+=r.w-w;r.w=w+1;}
    return sonic::linux_ui::canvas(dc).text(face,value,r,size,sonic::linux_ui::color(dc->color),wrap,bool(flags&DT_CENTER));
}
inline void MoveToEx(HDC dc,int x,int y,void*){dc->cursor={x,y};}
inline void LineTo(HDC dc,int x,int y){const auto* p=dc->pen;sonic::linux_ui::canvas(dc).line(dc->cursor.x,dc->cursor.y,x,y,sonic::linux_ui::color(p?p->color:0),p?p->width:1);dc->cursor={x,y};}
inline void Polyline(HDC dc,const POINT* points,int n){if(n<1)return;MoveToEx(dc,points[0].x,points[0].y,nullptr);for(int i=1;i<n;++i)LineTo(dc,points[i].x,points[i].y);}
inline void Rectangle(HDC dc,int l,int t,int r,int b){POINT points[]={{l,t},{r-1,t},{r-1,b-1},{l,b-1},{l,t}};Polyline(dc,points,5);}
inline void Ellipse(HDC dc,int l,int t,int r,int b){
    const double cx=(l+r)*.5,cy=(t+b)*.5,rx=(r-l)*.5,ry=(b-t)*.5;if(rx<=0||ry<=0)return;
    const int steps=std::max(24,int(7*std::max(rx,ry)));MoveToEx(dc,int(cx+rx),int(cy),nullptr);
    for(int i=1;i<=steps;++i){const double angle=i*6.283185307179586/steps;LineTo(dc,int(std::lround(cx+rx*std::cos(angle))),int(std::lround(cy+ry*std::sin(angle))));}
}
inline void RoundRect(HDC dc,int l,int t,int r,int b,int ew,int eh){
    const int radius=std::min(ew,eh)/2,thick=dc->pen?dc->pen->width:1;auto& c=sonic::linux_ui::canvas(dc);
    const auto inside=[](int x,int y,int l,int t,int r,int b,int radius){if(x<l||x>=r||y<t||y>=b)return false;const int dx=x-std::clamp(x,l+radius,r-radius-1),dy=y-std::clamp(y,t+radius,b-radius-1);return dx*dx+dy*dy<=radius*radius;};
    for(int y=t;y<b;++y)for(int x=l;x<r;++x)if(inside(x,y,l,t,r,b,radius)&&!inside(x,y,l+thick,t+thick,r-thick,b-thick,std::max(0,radius-thick)))c.pixel(x,y,sonic::linux_ui::color(dc->pen?dc->pen->color:0));
}
