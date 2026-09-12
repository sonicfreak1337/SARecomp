#pragma once
#include "sonic_input_bindings.hpp"
#include <cstddef>
#include <string>
#include <vector>
namespace sonic::tutorial {
struct Labels {
    int language=1;
    std::wstring next, back;
    bool operator==(const Labels&) const = default;
};
struct Image { unsigned width=0,height=0; std::vector<std::byte> pixels; };
Labels labels(int language,const input::Bindings&,input::GlyphStyle);
// A 512x32 retail bar, rasterized at 4x. Its right end is outside the guest
// viewport: all text must fit the explicitly measured visible source width.
Image rasterize(const Labels&,unsigned visible_width);
}
