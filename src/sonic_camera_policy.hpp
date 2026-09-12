#pragma once
#include <cstdint>

namespace sonic::camera {
// Reviewed P1 follow/area callbacks from PAL's camera table at 8C195250.
// The active control level remains an independent gate: an event can borrow
// one of these callbacks at a higher priority. Fixed/external-target, path,
// timed and module-owned cameras retain original ownership. See camera-style.md.
inline bool manual_camera_type(std::uint16_t type,std::uint32_t callback) noexcept {
    std::uint32_t expected=0;
    switch(type) {
    case 0:case 1: expected=0x8C01E6A6u;break;
    case 4:case 5:case 8:case 9: expected=0x8C023360u;break;
    case 12:case 13: expected=0x8C01EB02u;break;
    case 16:case 17: expected=0x8C023C5Eu;break;
    case 20:case 21:case 23:case 24: expected=0x8C020FE2u;break;
    case 31:case 32: expected=0x8C020D60u;break;
    case 34:case 35: expected=0x8C020C44u;break;
    case 36:case 37: expected=0x8C024586u;break;
    case 39:case 40: expected=0x8C01FF20u;break;
    case 42:case 43: expected=0x8C023C20u;break;
    case 58:case 59: expected=0x8C024768u;break;
    default:return false;
    }
    return (callback&0x1FFFFFFFu)==(expected&0x1FFFFFFFu);
}
}
