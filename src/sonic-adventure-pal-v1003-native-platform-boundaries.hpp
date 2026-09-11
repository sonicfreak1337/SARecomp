// Exact private PAL v1.003 platform boundaries recovered from the checked
// post-PAL image. These entries are identity-bound game-project metadata;
// they do not add a public runtime or a hardware register model.
#pragma once

#include <array>

#include "katana/runtime/game_project.hpp"

namespace sonic_adventure::private_data {

inline constexpr std::array native_platform_exact_boundaries{
    katana::runtime::GameProjectFunctionBoundary{
        0x8C645ACEu,
        0xAAu,
        "sa_native_platform_interrupt_dispatch"},
};

} // namespace sonic_adventure::private_data
