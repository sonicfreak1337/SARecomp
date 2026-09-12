#pragma once
#include "sonic_presentation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <span>
namespace sonic::subtitles {
struct Layout {float center=320,target_center=320,bottom=464,target_bottom=464,scale=1;};
inline Layout layout(float x,float y,float width,float height,const presentation::Settings& s) noexcept {
    Layout result;result.center=x+width*.5f;result.bottom=y+height;
    // Enlarge uniformly, retaining the entire authored line in the safe area.
    // Long rasterized lines cap at the available width rather than clipping.
    result.scale=std::clamp(s.subtitle_scale*.01f,1.0f,2.0f);
    result.scale=std::min(result.scale,608.0f/std::max(1.0f,width));
    constexpr float safe_bottom=464.0f;
    result.scale=std::min(result.scale,(safe_bottom-16.0f)/std::max(1.0f,height));
    const auto half_width=std::max(0.0f,width)*result.scale*.5f;
    result.target_center=std::clamp(result.center,16.0f+half_width,624.0f-half_width);
    result.target_bottom=std::clamp(result.bottom,16.0f+height*result.scale,safe_bottom);return result;
}
inline void apply(std::span<katana::runtime::NativePortVertex> vertices,const Layout& l,bool background) noexcept {
    for(auto& v:vertices){if(!background)v.position[0]=l.target_center+(v.position[0]-l.center)*l.scale;v.position[1]=l.target_bottom+(v.position[1]-l.bottom)*l.scale;}
}
inline std::array<katana::runtime::NativePortVertex,4> backdrop(std::span<const katana::runtime::NativePortVertex> text) noexcept {
    std::array<katana::runtime::NativePortVertex,4> result{};float top=480,bottom=0,depth=1;
    for(const auto& v:text){top=std::min(top,v.position[1]);bottom=std::max(bottom,v.position[1]);depth=v.position[2];}
    for(unsigned i=0;i<4;++i){auto& v=result[i];v.position={i<2?0.0f:640.0f,(i&1)?std::min(480.0f,bottom+6):std::max(0.0f,top-6),depth};v.depth_coordinate=depth;v.color={0,0,0,.82f};}
    return result;
}
}
