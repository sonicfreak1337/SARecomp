#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace sonic::input {
enum class Action : unsigned {Up,Down,Left,Right,A,B,X,Y,Start,LeftTrigger,RightTrigger,
    LookUp,LookDown,LookLeft,LookRight,Settings,Confirm,Cancel,Count};
inline constexpr unsigned action_count=unsigned(Action::Count);
inline constexpr std::array<std::string_view,action_count> action_names{
    "up","down","left","right","a","b","x","y","start","left_trigger","right_trigger",
    "look_up","look_down","look_left","look_right","settings","confirm","cancel"};
// Physical host buttons, plus two synthetic trigger bits. Key codes are
// Win32 virtual keys; mouse 1..5 means left/right/middle/X1/X2, 0 unbound.
struct Binding {unsigned key=0,alternate=0,mouse=0,pad=0;bool operator==(const Binding&) const=default;};
using Bindings=std::array<Binding,action_count>;
inline constexpr Bindings default_bindings{{
    {'W',0x26,0,1},{'S',0x28,0,2},{'A',0x25,0,4},{'D',0x27,0,8},
    {0x20,0,0,1u<<10},{'B',0,0,1u<<11},{'X',0,0,1u<<12},{'Y',0,0,1u<<13},
    {0x0d,0,0,1u<<4},{'Q',0,0,(1u<<8)|(1u<<14)},{'E',0,0,(1u<<9)|(1u<<15)},
    {'I',0,0,0},{'K',0,0,0},{'J',0,0,0},{'L',0,0,0},
    {0x79,0,0,1u<<7},{0x0d,0x20,1,1u<<10},{0x1b,0,2,1u<<11}
}};
enum class GlyphStyle : unsigned {Automatic,Xbox,PlayStation,Keyboard};
}
