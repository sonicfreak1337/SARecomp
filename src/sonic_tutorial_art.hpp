#pragma once
#include "sonic_tutorial_prompt.hpp"
#include <cstdint>
#include <span>
#include <string_view>
namespace sonic::tutorial {
enum class ControlToken { A,B,X,Y,Move,Camera,ShoulderPair,Diagram };
struct ControlSpan { ControlToken token; unsigned x,y,w,h; };
struct Artwork {
    std::string_view archive,archive_sha256,pvrt_sha256;
    unsigned ordinal,width,height;
    std::span<const ControlSpan> spans;
};
struct Controls {
    int language=1;
    input::Bindings bindings=input::default_bindings;
    input::GlyphStyle style=input::GlyphStyle::Keyboard;
    bool recompiled=false,swap_sticks=false,mouse_camera=false;
    bool operator==(const Controls&) const=default;
};
std::span<const Artwork> artwork_catalog() noexcept;
const Artwork* find_artwork(std::string_view archive,std::string_view archive_sha256,
    unsigned ordinal,std::string_view pvrt_sha256) noexcept;
std::wstring control_label(ControlToken,const Controls&);
// Change only measured control spans. Original prose comes from the installed
// identity-bound texture; shifted runs retain their pixels and alpha.
Image rasterize_artwork(const Artwork&,const Controls&,std::span<const std::uint8_t> rgba);
}
