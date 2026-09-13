#pragma once
#include <cstdlib>
namespace sonic::input {
inline bool legacy_capabilities_first() noexcept {
    // Private same-binary experiment. The original order remains the default:
    // current measurements do not establish a useful CPU improvement.
    static const bool control=[] {const auto* p=std::getenv("SARECOMP_WINMM_POSITION_FIRST");return !(p&&*p&&*p!='0');}();
    return control;
}
template<class Position, class Capabilities>
bool query_connected_joystick(bool capabilities_first,Position&& position,Capabilities&& capabilities) {
    return capabilities_first ? capabilities()&&position() : position()&&capabilities();
}
}
