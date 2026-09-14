#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace sonic::setup {
struct RequiredTrack {
    unsigned number,lba,type,sector_size;
    std::uint64_t bytes;
    std::string_view sha256;
};
inline constexpr std::array<RequiredTrack,3> required_tracks{{
    {1,0,4,2352,26721072,"cb45a8de12af5b13abb559fc8acc714744a4e22965fe67158cdbd0037cace093"},
    {2,11511,0,2352,13994400,"57eff24181c0581e969a40ec4972336da0891dcd093f814d7dd577c64e0142a6"},
    {3,45000,4,2352,1185760800,"189f89cb695ff81b40878463f326ead759437794d733329ebf3943196ad59b45"}
}};
inline constexpr std::string_view required_gdi_sha256="d6e475056e725ae2887f613d4788c2376f86630227c19404d1436ac0672a2d1e";
inline constexpr std::wstring_view required_release=L"Sonic Adventure · PAL · v1.003 (1999)";
}
