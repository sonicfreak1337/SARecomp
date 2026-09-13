#pragma once
#include <cstdint>
#include <optional>

namespace sonic::big_hud {
// Original MINICART/fishing popups use some of the same artwork. Bind the
// two main fishing-HUD owners and the formatter's actual caller, not position
// or the FISHING/CON_REGULAR texture list alone.
inline bool formatted_digits(std::uint32_t pr,std::uint32_t carrier,
                             std::uint32_t texlist,std::uint32_t frames) noexcept {
    return pr==0x8C08F028u && carrier==0x8C1BF420u &&
           texlist==0x8C1BF0E4u && frames==0x8C1BF1C8u;
}
inline bool left_anchored(std::uint32_t pr,std::uint32_t sp,std::uint32_t carrier,
                          std::uint32_t texlist,std::uint32_t frames,
                          std::optional<std::uint32_t> formatter_parent={}) noexcept {
    if(carrier==sp && texlist==0x8C54CA8Cu && frames==0x8C565F84u &&
       (pr==0x8C0E6FA0u || pr==0x8C0E6FB8u || pr==0x8C0E7134u || pr==0x8C0E714Cu))
        return true;
    if(carrier==sp && texlist==0x8C566038u && frames==0x8C566040u &&
       (pr==0x8C0E6FE2u || pr==0x8C0E7176u))
        return true;
    return formatted_digits(pr,carrier,texlist,frames) && formatter_parent &&
        (*formatter_parent==0x8C0E7494u || *formatter_parent==0x8C0E74C2u ||
         *formatter_parent==0x8C0E75D2u);
}
}
