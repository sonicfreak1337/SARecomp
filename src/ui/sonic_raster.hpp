#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace sonic::ui {
struct Color {std::uint8_t r,g,b,a=255;};
struct Rect {int x,y,w,h;bool contains(int px,int py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;}};
struct Image {
    unsigned width=0,height=0;
    std::vector<std::uint8_t> pixels;
    static Image load(const std::filesystem::path&);
    void save(const std::filesystem::path&)const;
};
class Font {
    struct Impl;std::shared_ptr<Impl> impl_;
    friend class Canvas;
public:
    explicit Font(const std::filesystem::path&);
    float measure(std::wstring_view,float size,float letter_spacing=0)const;
};
class Canvas {
public:
    Image image;
    explicit Canvas(unsigned w,unsigned h);
    void pixel(int x,int y,Color);
    void fill(Rect,Color);
    void rounded(Rect,int radius,Color);
    void gradient(Rect,Color top,Color bottom);
    void line(int x,int y,int x2,int y2,Color,int thickness=1);
    void blit(const Image&,Rect,bool cover=false,std::uint8_t alpha=255);
    int text(const Font&,std::wstring_view,Rect,float size,Color,bool wrap=false,bool center=false,float letter_spacing=0);
};
}
