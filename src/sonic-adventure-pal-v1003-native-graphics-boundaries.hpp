#pragma once

#include "katana/runtime/game_project.hpp"

#include <array>

// Private, identity-bound promotion candidates for the Sonic Adventure PAL
// native graphics adapter.  The byte identities live in the native-port
// manifest; this table deliberately contains only addresses, exact extents
// and stable symbolic identities.  It contains no title bytes.
namespace sonic_adventure::private_data {

inline constexpr std::array native_graphics_exact_boundaries{
    // These two post-PAL runtime owners have exact checkpoint identities in
    // the private manifest. They must be materialized as whole functions so
    // their Required replacements cannot degrade into instruction-site hooks.
    katana::runtime::GameProjectFunctionBoundary{
        0x8C604FF0u, 0xB6u, "sa_native_frame_turnover"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C641E40u, 0x2D6u, "sa_native_scene_begin"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C605B50u, 0x18u, "sa_nj_border_color"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C608C0Cu, 0x12u, "sa_nj_texture_list_bind"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C63CD64u, 0x5E6u, "sa_nj_draw_polygon"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C63D354u, 0x246u, "sa_nj_draw_texture"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C63D5ACu, 0x2FCu, "sa_nj_draw_textureh"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C63E114u, 0x66u, "sa_nj_draw_pretransformed_line_dispatch"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C63EB08u, 0x64u, "sa_nj_draw_pretransformed_dispatch"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C63EB98u, 0xA0u, "sa_nj_draw_pretransformed_colored"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C63EC48u, 0xC4u, "sa_nj_draw_pretransformed_textured"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C63EDF4u, 0x9Cu, "sa_nj_draw_sprite_2d"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C63EEB4u, 0x9Cu, "sa_nj_draw_sprite_3d"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C09E6B8u, 0xE8u, "sa_route_draw_sprite_3d"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C640862u, 0x104u, "sa_nj_glyph_text"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C6409C0u, 0x23Eu, "sa_nj_glyph_decimal"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C640D22u, 0x110u, "sa_nj_glyph_nibbles"},
    katana::runtime::GameProjectFunctionBoundary{
        0x8C642380u, 0x5C0u, "sa_kamui_present_drain"},
};

} // namespace sonic_adventure::private_data
