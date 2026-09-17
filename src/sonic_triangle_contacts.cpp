#include "sonic_triangle_contacts.hpp"
#include "sonic_native_collision_memory.hpp"
#include "sonic_collision_math.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <bit>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
namespace sonic::triangle_contacts {
namespace {
using namespace katana::runtime;
constexpr std::array<std::uint16_t,890> words_0{
    0x2FE6,0x2FD6,0x2FC6,0x2FB6,0x2FA6,0x2F96,0x2F86,0xFFFB,0xFFEB,0xFFDB,0xFFCB,0x4F22,
    0x7FCC,0x6E43,0x64E3,0xEB00,0x740C,0x1FB9,0xE606,0x24B2,0x7420,0x24B2,0x76FD,0x7420,
    0x4615,0x24B2,0x8DF7,0x7420,0xC722,0xF3E8,0xF508,0xE01C,0xF350,0xFE37,0xC720,0xF3E8,
    0xF408,0xE03C,0xF340,0xFE37,0xE004,0xF3E6,0xE05C,0xF350,0xFE37,0xE004,0xF3E6,0xE07C,
    0xF340,0xFE37,0xE008,0xF3E6,0x902A,0xF350,0xFE37,0xE008,0xF3E6,0x9026,0xF340,0xFE37,
    0xD314,0x66B3,0x6432,0x2448,0x8D08,0xEC01,0x5342,0x3350,0x8B01,0xA003,0x66C3,0x6442,
    0x2448,0x8BF7,0x2668,0x8B01,0xA30A,0xE000,0xC70C,0x5244,0xFD9D,0x1F25,0x5343,0x1F34,
    0xFC08,0xC70A,0xFE08,0xA2F9,0xFF8D,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,
    0x009C,0x00BC,0x0000,0x435C,0x0000,0xC35C,0xD548,0x8C02,0xB716,0x38D1,0xF986,0x4622,
    0x0009,0x0009,0x0009,0x0009,0x54F5,0xE004,0xF0E6,0x7418,0xF3E8,0xF546,0xE008,0xF648,
    0xF501,0xF2E6,0xF631,0xF446,0xC752,0xF421,0xF208,0xF35C,0xF352,0xF06C,0xF36E,0xF04C,
    0xF34E,0xF325,0x8B01,0xA2C6,0x0009,0x5DF5,0xE004,0xF2E6,0xE01C,0x7D18,0x69D3,0x790C,
    0xF598,0x6AD3,0xF8D8,0x7A18,0xFF27,0xE004,0xF7D6,0xF35C,0xF381,0xF271,0xFB96,0xFAE8,
    0xE01C,0xF9A8,0xF1AC,0xF181,0xF322,0xF2BC,0xF271,0xF43C,0xF212,0xF39C,0xF351,0xF421,
    0xF2F6,0xF2B1,0xE004,0xF6A6,0xE01C,0xF1AC,0xF151,0xF891,0xF322,0xF26C,0xF2B1,0xFA91,
    0xF761,0xF53C,0xF3F6,0xF212,0xF361,0xF7A2,0xF521,0xF28D,0xF832,0xF34C,0xF352,0xF68C,
    0xF235,0x8F02,0xF671,0xA0D2,0x0009,0xF35C,0xF362,0xF235,0x8B01,0xA0CC,0x0009,0xF34C,
    0xF362,0xF28D,0xF235,0x8B01,0xA0C5,0x0009,0xD328,0x67F3,0x6593,0x7728,0x66A3,0x430B,
    0x64D3,0xE028,0xF3D8,0xF6F6,0xE004,0xF26C,0xF232,0xF3D6,0xE02C,0xF1F6,0xE030,0xF4F6,
    0xE008,0xF132,0xF24D,0xF3D6,0xF211,0xF14C,0xF132,0xF34C,0xF342,0xF52C,0xFC35,0x8F02,
    0xF511,0xA0A4,0x0009,0xF3E8,0xE004,0xF632,0xF3E6,0xE02C,0xF2F6,0xE030,0xF232,0xF64D,
    0xF3F6,0xE008,0xF621,0xF651,0xF633,0xFF67,0xE008,0xF3E6,0xF26C,0xF325,0x8F28,0x68B3,
    0x64E3,0x9315,0x740C,0x343C,0x6242,0x2228,0x8905,0xE010,0xF346,0xE008,0xF2F6,0xF235,
    0x8B2B,0x68E3,0x9308,0x780C,0xA027,0x383C,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,
    0x0080,0x0000,0x1000,0x473D,0x7360,0x8C02,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,
    0x0009,0x0009,0x0009,0x0009,0x64E3,0x932D,0x740C,0x343C,0x6242,0x2228,0x8905,0xE010,
    0xF346,0xE008,0xF2F6,0xF325,0x8B03,0x68E3,0x9320,0x780C,0x383C,0x2888,0x8953,0xD30F,
    0x64F3,0x430B,0x7428,0xF38D,0xF034,0x8B25,0x63F3,0x7328,0xE004,0x62F3,0x7228,0xF2FA,
    0xF3D7,0x63F3,0x7328,0xE008,0xF3F7,0xA01D,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,
    0x0009,0x0009,0x0009,0x0009,0x00A0,0x0000,0xA69C,0x8C63,0x0009,0x0009,0x0009,0x0009,
    0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,0xD26F,0x64F3,0x420B,0x7428,
    0xE008,0x1FC9,0x28C2,0x53F4,0x1831,0x63F3,0x7328,0xF3F6,0xE010,0xF837,0xE02C,0x6232,
    0x1825,0x5231,0x1826,0x5232,0xD366,0x1827,0xF5F6,0xE028,0x430B,0xF4F6,0xF0E2,0xD364,
    0xE030,0xF03D,0x025A,0x622B,0x1823,0x430B,0xF4F6,0xF0E2,0xF03D,0x025A,0x1822,0xE004,
    0xF6D6,0xF796,0xE008,0xF8E6,0xF5D6,0xF28C,0xF37C,0xF251,0xF361,0xF996,0xE004,0xFAE6,
    0xFBA6,0xE008,0xF322,0xF1AC,0xF29C,0xF161,0xF251,0xF43C,0xF3BC,0xF371,0xF212,0xF1A6,
    0xE020,0xFF17,0xF191,0xF421,0xF28C,0xF291,0xF322,0xF2AC,0xF271,0xE020,0xF73C,0xF3F6,
    0xFAB1,0xF6B1,0xF122,0xF24C,0xF531,0xF831,0xF711,0xF5A2,0xF682,0xF272,0xF35C,0xF56C,
    0xF531,0xF38D,0xF325,0x8B01,0xA0B9,0x0009,0xF27C,0xF252,0xF325,0x8B01,0xA0B3,0x0009,
    0xF34C,0xF352,0xF28D,0xF235,0x8B01,0xA0AC,0x0009,0xD33D,0x67F3,0x6593,0x7728,0x66A3,
    0x430B,0x64D3,0xE028,0xF3F6,0xE00C,0xFF37,0xE004,0xF2D8,0xF322,0xF2D6,0xE02C,0xF1F6,
    0xE008,0xF122,0xF34D,0xF2D6,0xE030,0xF311,0xF1F6,0xE00C,0xF122,0xF43C,0xF3F6,0xF23C,
    0xF232,0xFC25,0x8F02,0xF411,0xA089,0x0009,0xE004,0xF3E6,0xE02C,0xF2F6,0xE008,0xF232,
    0xF3E6,0xE030,0xF1F6,0xE00C,0xF132,0xF24D,0xF3F6,0xE004,0xF211,0xF241,0xF233,0xFF27,
    0xF3E8,0xF325,0x8F0E,0x68B3,0x64E3,0x740C,0x6242,0x2228,0x8905,0xE010,0xF346,0xE004,
    0xF2F6,0xF235,0x8B0F,0x68E3,0xA00D,0x780C,0x64E3,0x742C,0x6342,0x2338,0x8905,0xE010,
    0xF346,0xE004,0xF2F6,0xF325,0x8B01,0x68E3,0x782C,0x2888,0x8955,0xD213,0x64F3,0x420B,
    0x7428,0xF38D,0xF034,0x8B27,0x63F3,0x7328,0xE004,0x62F3,0x7228,0xF2FA,0xF3D7,0x63F3,
    0x7328,0xE008,0xF3F7,0xA01F,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,
    0x0009,0x0009,0x0009,0x0009,0xA88C,0x8C63,0xD038,0x8C10,0xCF98,0x8C10,0x7360,0x8C02,
    0xA69C,0x8C63,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,0xD287,0x64F3,0x420B,0x7428,
    0xE004,0x1FC9,0x28C2,0x53F4,0x1831,0x63F3,0x7328,0xF3F6,0xE010,0xF837,0xE02C,0x6232,
    0x1825,0x5231,0x1826,0x5232,0xD37E,0x1827,0xF5F6,0xE028,0x430B,0xF4F6,0xF0E2,0xD37C,
    0xE030,0xF03D,0x025A,0x622B,0x1823,0x430B,0xF4F6,0xF0E2,0xF03D,0x025A,0x1822,0xE008,
    0xF8E8,0xF596,0xF6D8,0xF7D6,0xF28C,0xF261,0xF35C,0xF371,0xFAE6,0xF998,0xFBA6,0xE018,
    0xF322,0xF2AC,0xF19C,0xF271,0xF161,0xF43C,0xF3BC,0xF351,0xF122,0xF28C,0xF291,0xF411,
    0xF1A8,0xF322,0xF2AC,0xFF17,0xF251,0xF191,0xF122,0xE018,0xF53C,0xF3F6,0xFAB1,0xF7B1,
    0xF24C,0xF631,0xF831,0xF511,0xF6A2,0xF782,0xF252,0xF36C,0xF67C,0xF631,0xF38D,0xF325,
    0x8B01,0xA094,0x0009,0xF25C,0xF262,0xF325,0x8B01,0xA08E,0x0009,0xF34C,0xF362,0xF28D,
    0xF235,0x8B01,0xA087,0x0009,0xD356,0x67F3,0x6593,0x7728,0x66A3,0x430B,0x64D3,0xE028,
    0xF3D8,0xF6F6,0xE02C,0xF4F6,0xE004,0xF26C,0xF232,0xF3D6,0xE008,0xF14C,0xF132,0xF3D6,
    0xF24D,0xE030,0xF211,0xF1F6,0xF132,0xF34C,0xF342,0xF52C,0xFC35,0x8D68,0xF511,0xE008,
    0xF3E6,0xE030,0xF2F6,0xE02C,0xF232,0xF3E8,0xF632,0xF3F6,0xE004,0xF24D,0xF261,0xF251,
    0xF233,0xFF2A,0xF3E6,0xF325,0x8F0D,0x68B3,0x64E3,0x744C,0x6242,0x2228,0x8904,0xE010,
    0xF2F8,0xF346,0xF235,0x8B0E,0x68E3,0xA00C,0x784C,0x64E3,0x746C,0x6342,0x2338,0x8904,
    0xE010,0xF2F8,0xF346,0xF325,0x8B01,0x68E3,0x786C,0x2888,0x8939,0xD231,0x64F3,0x420B,
    0x7428,0xF38D,0xF034,0x8B0C,0x63F3,0x7328,0xE004,0x62F3,0x7228,0xF2FA,0xF3D7,0x63F3,
    0x7328,0xE008,0xF3F7,0xA004,0x0009,0xD223,0x64F3,0x420B,0x7428,0xE010,0x1FC9,0x28C2,
    0x53F4,0x1831,0x63F3,0x7328,0xF3F8,0xF837,0xE02C,0x6232,0x1825,0x5231,0x1826,0x5232,
    0xD31A,0x1827,0xF5F6,0xE028,0x430B,0xF4F6,0xF0E2,0xD318,0xE030,0xF03D,0x025A,0x622B,
    0x1823,0x430B,0xF4F6,0xF0E2,0xF03D,0x025A,0x1822,0x53F5,0x6232,0x1F25,0x53F5,0x2338,
    0x8901,0xAD19,0x0009,0x50F9,0x7F34,0x4F26,0xFCF9,0xFDF9,0xFEF9,0xFFF9,0x68F6,0x69F6,
    0x6AF6,0x6BF6,0x6CF6,0x6DF6,0x000B,0x6EF6,0x0009,0x0009,0x0009,0x0009,0x0009,0x0009,
    0x0009,0x0009,0x0009,0x0009,0xA88C,0x8C63,0xD038,0x8C10,0xCF98,0x8C10,0x7360,0x8C02,
    0xA69C,0x8C63,
};
constexpr std::array<std::uint16_t,33> words_1{
    0xF959,0xE004,0xF449,0xF369,0xF941,0xF859,0xF431,0xF649,0xF369,0xF861,0xF548,0xF631,
    0xF758,0xF368,0xF751,0xF531,0xF38C,0xF842,0xF27C,0xF262,0xF352,0xF742,0xF231,0xF72A,
    0xF29C,0xF252,0xF962,0xF271,0xF891,0xF727,0xE008,0x000B,0xF787,
};
constexpr std::array<std::uint16_t,8> words_2{
    0xF78D,0xF449,0xF549,0xF649,0xF5ED,0xF76D,0x000B,0xF07C,
};
constexpr std::array<std::uint16_t,16> words_3{
    0xF049,0xF149,0xF249,0xF38D,0xF0ED,0xF40C,0xF03C,0xF37D,0xF232,0xF132,0xF432,0xF42B,
    0xF41B,0xF44B,0x000B,0xF032,
};
constexpr std::array<std::uint16_t,40> words_4{
    0x4F22,0x7FFC,0xD30E,0x430B,0x0009,0x63F3,0xD40D,0xFF0A,0x6232,0x2249,0x3240,0x8B0B,
    0x62F3,0x6122,0xD30A,0x2138,0x8903,0xD20A,0x9009,0xA003,0x2202,0xD108,0x9206,0x2122,
    0xF0F8,0x7F04,0x4F26,0x000B,0x0009,0x044D,0x044C,0x0000,0xE4EC,0x8C10,0x0000,0x7F80,
    0xFFFF,0x007F,0xC048,0x8C7A,
};
constexpr std::array<std::uint16_t,40> words_5{
    0x4F22,0x7FFC,0xD30E,0x430B,0x0009,0x63F3,0xD40D,0xFF0A,0x6232,0x2249,0x3240,0x8B0B,
    0x62F3,0x6122,0xD30A,0x2138,0x8903,0xD20A,0x9009,0xA003,0x2202,0xD108,0x9206,0x2122,
    0xF0F8,0x7F04,0x4F26,0x000B,0x0009,0x044D,0x044C,0x0000,0xE5E0,0x8C10,0x0000,0x7F80,
    0xFFFF,0x007F,0xC048,0x8C7A,
};
constexpr std::array<std::uint16_t,166> words_6{
    0xFFFB,0x4F22,0x7FF8,0xE004,0xF54C,0xFF57,0x53F1,0x4311,0x8D01,0xE400,0xE408,0xF38D,
    0xF534,0xE704,0x8F02,0xE601,0xA00C,0x247B,0x52F1,0xD32A,0xD12A,0x2239,0x3210,0x8B02,
    0xE002,0xA003,0x240B,0xF554,0x8900,0x246B,0xD526,0x2648,0x8904,0xF058,0x7F08,0x4F26,
    0x000B,0xFFF9,0x63F3,0xD223,0xFF4A,0x6132,0x2128,0x8902,0xF3F8,0xF34D,0xFF3A,0xD320,
    0xF3F8,0xF538,0xF355,0x8B01,0xA02D,0xF558,0xF354,0x8B06,0xE208,0xD11C,0x2428,0x8D26,
    0xF518,0xA024,0xF54D,0x2478,0x8901,0xA020,0xF54C,0xD218,0xF3F8,0xF228,0xF235,0x8B09,
    0xF14C,0xF142,0xF05C,0xFF4C,0xF011,0xF15C,0xF06D,0xF100,0xA009,0xFF13,0xF14C,0xF142,
    0xF05C,0xF011,0xF15C,0xF06D,0xF101,0xFF1C,0xFF43,0xD30D,0x430B,0xF4FC,0xD20C,0xF40C,
    0x420B,0xE401,0xF50C,0xF05C,0x7F08,0x4F26,0x000B,0xFFF9,0xFFFF,0x7FFF,0x0000,0x7F80,
    0x012C,0x8C16,0x0000,0x8000,0x0138,0x8C16,0x0154,0x8C16,0x013C,0x8C16,0xEEC4,0x8C10,
    0xE6F8,0x8C10,0x4F22,0x7FF8,0x63F3,0x7304,0xE004,0xF34A,0xD30E,0xFF5A,0xF4F6,0xF35C,
    0x430B,0xF433,0x62F3,0x6322,0xD40B,0x2348,0x8D0D,0xF40C,0x61F3,0x7104,0x6312,0x2438,
    0x8903,0xD208,0xF34C,0xA003,0xF428,0xD307,0xF34C,0xF438,0xF430,0x7F08,0x4F26,0x000B,
    0xF04C,0x0000,0xEEC4,0x8C10,0x0000,0x8000,0x017C,0x8C16,0x014C,0x8C16,
};
constexpr std::array<std::uint16_t,96> words_7{
    0x2FE6,0x7FFC,0x66F3,0xD228,0xD325,0xD128,0xD525,0xF64A,0x6762,0xF728,0x6E62,0x2739,
    0x3730,0x8F0C,0xF518,0xD024,0x20E8,0x8B03,0x25E8,0x8B20,0xA01B,0x0009,0xD321,0xF038,
    0x7F04,0x000B,0x6EF6,0xD220,0x4715,0xD320,0xF428,0x8D04,0xF638,0x25E8,0x8B1B,0xA016,
    0x0009,0xE3E9,0x473C,0x374C,0x4715,0x8B02,0x9323,0x3733,0x8B15,0x4411,0x8B09,0x25E8,
    0x8B03,0xF07C,0x7F04,0x000B,0x6EF6,0xF05C,0x7F04,0x000B,0x6EF6,0x25E8,0x8B03,0xF04C,
    0x7F04,0x000B,0x6EF6,0xF06C,0x7F04,0x000B,0x6EF6,0xD20A,0xE317,0x25E9,0x473C,0x22E9,
    0x257B,0x252B,0x2652,0xF068,0x7F04,0x000B,0x6EF6,0x00FF,0x0000,0x7F80,0x0000,0x8000,
    0x0134,0x8C16,0x0164,0x8C16,0xFFFF,0x007F,0x012C,0x8C16,0x0130,0x8C16,0x0160,0x8C16,
};
constexpr std::array<std::uint16_t,240> words_8{
    0x2FE6,0x2FD6,0xFFFB,0xFFEB,0xFFDB,0xFFCB,0x4F22,0x7FF4,0xE008,0xF54C,0xFF57,0x53F2,
    0x4311,0x8D01,0xE400,0xE408,0xF38D,0xF534,0xE704,0x8F02,0xE601,0xA00C,0x247B,0x52F2,
    0xD357,0xD158,0x2239,0x3210,0x8B02,0xE002,0xA003,0x240B,0xF554,0x8900,0x246B,0x63F3,
    0xD253,0xFF4A,0x6132,0x2128,0x8D03,0x6543,0xF3F8,0xF34D,0xFF3A,0x2648,0x8902,0xD24F,
    0xA08C,0xF528,0x62F3,0x6122,0xD34A,0xE2E9,0xE020,0x2139,0x412C,0x7181,0x3103,0xED08,
    0x8F08,0x2D49,0xD148,0x2DD8,0x8F02,0xF518,0xA07A,0x0009,0xA078,0xF54D,0x2478,0x8901,
    0xA074,0xF54C,0xD143,0xF3F8,0xF518,0xD343,0xF535,0x8F04,0xF438,0xD242,0xFE28,0xA00F,
    0xFF3C,0xF534,0x8B04,0x2DD8,0x8900,0xF44D,0xA063,0xF04C,0xD23D,0xF3F8,0xF228,0xF530,
    0xFE4C,0xF230,0xFF2C,0xFF53,0xD33A,0x64F3,0xD23A,0x7404,0xF538,0x420B,0xF4FC,0x50F1,
    0xD332,0xDE38,0x8800,0x8D10,0xFD38,0x8801,0x890F,0x8802,0x890D,0x8803,0x891A,0x8804,
    0x8918,0x8805,0x8925,0x8806,0x8923,0x8807,0x892E,0xA03A,0x0009,0xA034,0xF4FC,0xD12F,
    0xE4FE,0xD22D,0xF218,0xF328,0xF2F0,0xD32D,0xFE30,0xFC2C,0x430B,0xF4FC,0xFD00,0xFCD3,
    0xA025,0xF4CC,0xD12A,0xE4FF,0xD228,0xF218,0xF328,0xF2F0,0xD325,0xFE30,0xFC2C,0x430B,
    0xF4FC,0xFD00,0xFCD3,0xA016,0xF4CC,0xD125,0xD325,0xD223,0xF218,0xF038,0xF1FC,0xF120,
    0xF328,0xFDFE,0xFE30,0xF41C,0xA009,0xF4D3,0xD114,0xFDF0,0xD211,0xF218,0xF328,0xF2F0,
    0xFE30,0xF42C,0xF4D3,0x4E0B,0x0009,0xF5EC,0xF500,0x2DD8,0x8900,0xF54D,0xF05C,0x7F0C,
    0x4F26,0xFCF9,0xFDF9,0xFEF9,0xFFF9,0x6DF6,0x000B,0x6EF6,0xFFFF,0x7FFF,0x0000,0x7F80,
    0x0000,0x8000,0x012C,0x8C16,0x0154,0x8C16,0x0138,0x8C16,0x0158,0x8C16,0x0130,0x8C16,
    0x0168,0x8C16,0x02D8,0x8C16,0xFAF8,0x8C10,0xFAD4,0x8C10,0x02C4,0x8C16,0x0170,0x8C16,
    0xE6F8,0x8C10,0x02C8,0x8C16,0x016C,0x8C16,0x02CC,0x8C16,0x02D4,0x8C16,0x02D0,0x8C16,
};
constexpr std::array<std::uint16_t,148> words_9{
    0xF64C,0xF642,0xD506,0xE405,0xF559,0xF359,0x74FF,0xF06C,0x4415,0xF35E,0x8DF9,0xF53C,
    0xF04C,0xF65C,0x000B,0xF062,0x02DC,0x8C16,0x2FE6,0x7FE4,0x66F3,0x7618,0xD536,0x6EF3,
    0x7E14,0xF64A,0xFE5A,0x67F3,0x6362,0x2359,0x3350,0x8D0D,0x7710,0x53F5,0x2359,0x3350,
    0x8B03,0x52F5,0xD32F,0x2238,0x8B04,0xE014,0xF38D,0xF2F6,0xF234,0x8B05,0xD32C,0xE200,
    0x2422,0xF338,0xA04C,0xF73A,0x61E2,0x2159,0x3150,0x8B03,0xE200,0x2422,0xA044,0xF74A,
    0x6262,0xD326,0x2239,0x1F22,0x6162,0xD225,0x2129,0x2612,0x60E2,0x2029,0x2E02,0xE00C,
    0xF3E8,0xF468,0xF433,0xFF47,0x51F3,0x2F12,0x2139,0x1F11,0x61F2,0xD31E,0x2519,0x3533,
    0x8905,0xF43D,0x005A,0x405A,0xE00C,0xF32D,0xFF37,0xE00C,0x52F3,0x53F1,0x223B,0x1F23,
    0xF0F6,0xC717,0xF208,0xF025,0x8B04,0xC716,0xF30C,0xF108,0xA001,0xF310,0xF30C,0xF33D,
    0x025A,0x6323,0x435A,0x4311,0x2422,0x8D04,0xF32D,0xD10D,0x415A,0xF20D,0xF320,0xF2E8,
    0xF168,0xF322,0xF131,0xF71A,0x6272,0x53F2,0x223B,0x2722,0xF078,0x7F1C,0x000B,0x6EF6,
    0x0000,0x7F80,0xFFFF,0x007F,0x012C,0x8C16,0x0000,0x8000,0xFFFF,0x7FFF,0x0000,0x4F80,
    0x0000,0x4F00,0x0000,0xCF80,
};
constexpr std::array<std::uint16_t,228> words_10{
    0x0001,0x7F80,0x0000,0x0000,0x0000,0x7F80,0x0000,0x3F80,0x0000,0x3F00,0x0000,0x3E80,
    0x0000,0x4000,0x0000,0x4080,0x0FDB,0x4049,0x0FDB,0x40C9,0x0FDB,0x3FC9,0x0FDB,0x3F49,
    0x7218,0x3F31,0x0000,0x8000,0x0000,0xFF80,0x0000,0xBF80,0x0000,0xBF00,0x0000,0xBE80,
    0x0000,0xC000,0x0000,0xC080,0x0FDB,0xC049,0x0FDB,0xC0C9,0x0FDB,0xBFC9,0x0FDB,0xBF49,
    0x7218,0xBF31,0x0194,0x8C16,0x730A,0x4779,0x2032,0x6556,0x2072,0x2E31,0x3130,0x302E,
    0x2032,0x7542,0x6C69,0x3A64,0x614D,0x2072,0x3330,0x3120,0x3939,0x2039,0x3431,0x353A,
    0x3A38,0x3633,0x000A,0x0000,0x2020,0x2020,0x2020,0x2020,0x6020,0x6060,0x6060,0x2020,
    0x2020,0x2020,0x2020,0x2020,0x2020,0x2020,0x2020,0x2020,0x1048,0x1010,0x1010,0x1010,
    0x1010,0x1010,0x1010,0x1010,0x8484,0x8484,0x8484,0x8484,0x8484,0x1010,0x1010,0x1010,
    0x8110,0x8181,0x8181,0x0181,0x0101,0x0101,0x0101,0x0101,0x0101,0x0101,0x0101,0x0101,
    0x0101,0x1001,0x1010,0x1010,0x8210,0x8282,0x8282,0x0282,0x0202,0x0202,0x0202,0x0202,
    0x0202,0x0202,0x0202,0x0202,0x0202,0x1002,0x1010,0x2010,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
    0xDBB0,0x3E7A,0x6338,0x3EED,0xBC7D,0x3F24,0x0000,0x3F40,0x0000,0xBF40,0x0000,0x3E00,
    0x2DE9,0xBDBA,0x8E38,0x3DE3,0x4925,0xBE12,0xCCCD,0x3E4C,0xAAAB,0xBEAA,0x0000,0x3F80,
};
constexpr std::array<std::span<const std::uint16_t>,11> identities{words_0,words_1,words_2,words_3,words_4,words_5,words_6,words_7,words_8,words_9,words_10};
struct Range { std::uint32_t address,size; };
bool overlap(Range a,Range b) noexcept {
    const auto x=a.address&0x1FFFFFFFu,y=b.address&0x1FFFFFFFu;
    return x<std::uint64_t(y)+b.size && y<std::uint64_t(x)+a.size;
}
bool mode_ok(const CpuState& cpu) noexcept {
    const auto f=cpu.read_fpscr();
    return cpu.privileged_mode_inline() && !cpu.trap_pending && !cpu.sleeping &&
        (cpu.sr&sr_fd_mask)==0u && (f&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask))==0u &&
        (f&fpscr_dn_mask)!=0u && (f&fpscr_rounding_mode_mask)<=1u;
}
bool observers_ok(const Memory& m) noexcept {
    return !m.watchpoint_count() && !m.has_trace_handler() && !m.has_guest_memory_access_sink() &&
        !m.has_mmio_trace_handler() && m.guest_write_observer_allows_prevalidated_linear_writes();
}
bool p0_ok(const CpuState& cpu) noexcept {
    return (cpu.mmucr&1u)==0u && (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu);
}
bool admitted(const DirectLinearMemoryGuard& g,bool p0,Range r) noexcept {
    const auto a=r.address,p=a&0x1FFFFFFFu;
    return g && g.physical_base==0x0C000000u && g.physical_span>=0x1000000u &&
        g.backing_mask==0xFFFFFFu && !(a&3u) &&
        ((a&0xC0000000u)==0x80000000u || (p0 && a>=0x0C000000u && a<0x0D000000u)) &&
        p>=0x0C000000u && p<0x0D000000u && r.size<=0x0D000000u-p;
}
std::uint32_t peek(const DirectLinearMemoryGuard& g,std::uint32_t a) noexcept {
    std::uint32_t v; std::memcpy(&v,g.read_bytes+(a&0xFFFFFFu),4u); return v;
}
[[noreturn]] void broken() { throw std::runtime_error("triangle-contacts: interrupted after mutation; original fallback forbidden"); }
} // namespace
bool try_execute(katana::runtime::CpuState& cpu,
    const katana::runtime::NativePortImmutableWriteGuard* immutable,const RetainedCallBridge& bridge) {
    using namespace katana::runtime;
    static_assert(std::endian::native==std::endian::little);
    if (cpu.pc!=entry || !bridge.invoke || !immutable || immutable->write_detected() ||
        !mode_ok(cpu) || !observers_ok(cpu.memory)) return false;
    auto& memory=cpu.memory;
    auto g=memory.direct_linear_memory_guard(false);
    const bool p0=p0_ok(cpu);
    const auto initial_mmucr=cpu.mmucr;
    const auto initial_space=cpu.address_space.get();
    const auto initial_mode=initial_space?initial_space->mode():AddressTranslationMode::NoMmu;
    const auto initial_fpu_mode=cpu.read_fpscr()&(fpscr_fr_mask|fpscr_rounding_mode_mask|fpscr_dn_mask);
    const auto initial_sr=cpu.sr;
    const auto exception_generation=cpu.exception_generation;
    const auto stack_top=cpu.r[15],query=cpu.r[4];
    // Owner=100 stack bytes. Deepest angle adds wrapper8+E4EC16+EEC4/40+
    // FAF8/32=96. E6F8 is an 8-byte leaf, FAD4 stackless. Reserve 256 total.
    // FAF8's pointer output addresses EEC4's own stack, never caller data.
    const std::array writes{Range{query+12u,192u},Range{stack_top-0x100u,0x100u},Range{0x8C7AC048u,4u}};
    for (std::size_t i=0;i<writes.size();++i) {
        const auto w=writes[i];
        if (!admitted(g,p0,w) || immutable->tracks_address(w.address&0x1FFFFFFFu,w.size) ||
            !memory.is_writable_linear_range(w.address&0x1FFFFFFFu,w.size,false)) return false;
        for (std::size_t j=0;j<i;++j) if (overlap(w,writes[j])) return false;
    }
    const auto read_range=[&](Range r) {
        if (!admitted(g,p0,r)) return false;
        for (const auto w:writes) if (overlap(r,w)) return false;
        return true;
    };
    for (std::size_t i=0;i<source_spans.size();++i) {
        const auto s=source_spans[i];
        if (!read_range({s.address,s.size}) ||
            std::memcmp(g.read_bytes+(s.address&0xFFFFFFu),identities[i].data(),s.size)!=0) return false;
    }
    if (!read_range({query,12u}) || !read_range({0x8C02D548u,4u})) return false;
    // Validate every selected-list read before the first store. Read/read alias
    // is legal; writes against any node/read are declined. A cyclic/oversized
    // list declines after the bounded scan, without modifying guest state.
    auto object=peek(g,0x8C02D548u);
    unsigned budget=16384u;
    while (object) {
        if (!budget-- || !read_range({object,20u})) return false;
        if (peek(g,object+8u)==cpu.r[5]) break;
        object=peek(g,object);
    }
    auto node=object?peek(g,object+16u):0u;
    budget=16384u;
    while (node) {
        if (!budget-- || !read_range({node,60u})) return false;
        node=peek(g,node);
    }
    // No false returns below. Stable observers cannot change RAM or mappings.
    const auto read_backing=g.read_bytes;
    const auto generation=g.generation;
    const auto revalidate=[&]() {
        g=memory.direct_linear_memory_guard(false);
        if (!g || g.read_bytes!=read_backing || g.generation!=generation ||
            !memory.direct_linear_memory_guard_current(g,false) || !mode_ok(cpu) ||
            !observers_ok(memory) || immutable->write_detected() ||
            cpu.exception_generation!=exception_generation || cpu.mmucr!=initial_mmucr ||
            cpu.address_space.get()!=initial_space ||
            (initial_space && initial_space->mode()!=initial_mode) || cpu.sr!=initial_sr ||
            (cpu.read_fpscr()&(fpscr_fr_mask|fpscr_rounding_mode_mask|fpscr_dn_mask))!=initial_fpu_mode) broken();
        for (const auto w:writes)
            if (!admitted(g,p0,w) || immutable->tracks_address(w.address&0x1FFFFFFFu,w.size) ||
                !memory.is_writable_linear_range(w.address&0x1FFFFFFFu,w.size,false)) broken();
    };
    collision_memory::Access access;
    access.capture(cpu,*immutable,g);
    const auto load=[&](std::uint32_t a) {
        std::uint32_t v=0u;
        if(access.try_read(a,v))return v;
        if (!direct_linear_guard_read_u32(g,(a&0x1FFFFFFFu)|0x80000000u,v)) broken();
        return v;
    };
    const auto load16=[&](std::uint32_t a) {
        std::uint16_t v=0u;
        if (!access.try_read(a,v) && !direct_linear_guard_read_u16(g,(a&0x1FFFFFFFu)|0x80000000u,v)) broken();
        return std::uint32_t(std::int32_t(std::int16_t(v)));
    };
    const auto store=[&](std::uint32_t pc,std::uint32_t a,std::uint32_t v,CodeWriteSource source) {
        if(access.try_store(a,v))return;
        if (!memory.try_write_direct_linear_u32(a&0x1FFFFFFFu,v,source))
            guest_write_u32_at(cpu,GuestInstructionOrigin{pc,pc,true},a,v,source);
    };
    std::optional<HostFpuExecutionEpoch> epoch;
    epoch.emplace(cpu);
    const auto call=[&](std::uint32_t target) {
        const auto ret=cpu.pr,sp=cpu.r[15];
        access.reset(); epoch.reset(); g={}; // No RAM/FPU capability spans a call.
        cpu.pc=target;
        if (target==collision_math::cross_entry || target==collision_math::length_entry ||
            target==collision_math::normalize_entry) {
            if (!collision_math::try_execute(cpu,immutable)) broken();
        } else if (target==0x8C10D038u || target==0x8C10CF98u) {
            if (!bridge.invoke(bridge.context,cpu,target)) broken();
        } else broken();
        if (cpu.pc!=ret || cpu.pr!=ret || cpu.r[15]!=sp) broken();
        revalidate(); access.capture(cpu,*immutable,g); epoch.emplace(cpu);
    };
    bool branch=false;
    // Statically expanded original CFG: every operation is annotated by its
    // actual retail word. Literal islands never become executable instructions.
L8C029400: // 2FE6
    { const auto a=cpu.r[15]-4u; const auto v=cpu.r[14]; store(0x8C029400u,a,v,CodeWriteSource::Cpu);cpu.r[15]=a; } goto L8C029402;
L8C029402: // 2FD6
    { const auto a=cpu.r[15]-4u; const auto v=cpu.r[13]; store(0x8C029402u,a,v,CodeWriteSource::Cpu);cpu.r[15]=a; } goto L8C029404;
L8C029404: // 2FC6
    { const auto a=cpu.r[15]-4u; const auto v=cpu.r[12]; store(0x8C029404u,a,v,CodeWriteSource::Cpu);cpu.r[15]=a; } goto L8C029406;
L8C029406: // 2FB6
    { const auto a=cpu.r[15]-4u; const auto v=cpu.r[11]; store(0x8C029406u,a,v,CodeWriteSource::Cpu);cpu.r[15]=a; } goto L8C029408;
L8C029408: // 2FA6
    { const auto a=cpu.r[15]-4u; const auto v=cpu.r[10]; store(0x8C029408u,a,v,CodeWriteSource::Cpu);cpu.r[15]=a; } goto L8C02940A;
L8C02940A: // 2F96
    { const auto a=cpu.r[15]-4u; const auto v=cpu.r[9]; store(0x8C02940Au,a,v,CodeWriteSource::Cpu);cpu.r[15]=a; } goto L8C02940C;
L8C02940C: // 2F86
    { const auto a=cpu.r[15]-4u; const auto v=cpu.r[8]; store(0x8C02940Cu,a,v,CodeWriteSource::Cpu);cpu.r[15]=a; } goto L8C02940E;
L8C02940E: // FFFB
    { const auto a=cpu.r[15]-4u; store(0x8C02940Eu,a,cpu.fr[15],CodeWriteSource::Fpu);cpu.r[15]=a; } goto L8C029410;
L8C029410: // FFEB
    { const auto a=cpu.r[15]-4u; store(0x8C029410u,a,cpu.fr[14],CodeWriteSource::Fpu);cpu.r[15]=a; } goto L8C029412;
L8C029412: // FFDB
    { const auto a=cpu.r[15]-4u; store(0x8C029412u,a,cpu.fr[13],CodeWriteSource::Fpu);cpu.r[15]=a; } goto L8C029414;
L8C029414: // FFCB
    { const auto a=cpu.r[15]-4u; store(0x8C029414u,a,cpu.fr[12],CodeWriteSource::Fpu);cpu.r[15]=a; } goto L8C029416;
L8C029416: // 4F22
    { const auto a=cpu.r[15]-4u; store(0x8C029416u,a,cpu.pr,CodeWriteSource::Cpu);cpu.r[15]=a; } goto L8C029418;
L8C029418: // 7FCC
    cpu.r[15]+=0xFFFFFFCCu; goto L8C02941A;
L8C02941A: // 6E43
    cpu.r[14]=cpu.r[4]; goto L8C02941C;
L8C02941C: // 64E3
    cpu.r[4]=cpu.r[14]; goto L8C02941E;
L8C02941E: // EB00
    cpu.r[11]=0x00000000u; goto L8C029420;
L8C029420: // 740C
    cpu.r[4]+=0x0000000Cu; goto L8C029422;
L8C029422: // 1FB9
    store(0x8C029422u,cpu.r[15]+36u,cpu.r[11],CodeWriteSource::Cpu); goto L8C029424;
L8C029424: // E606
    cpu.r[6]=0x00000006u; goto L8C029426;
L8C029426: // 24B2
    store(0x8C029426u,cpu.r[4],cpu.r[11],CodeWriteSource::Cpu); goto L8C029428;
L8C029428: // 7420
    cpu.r[4]+=0x00000020u; goto L8C02942A;
L8C02942A: // 24B2
    store(0x8C02942Au,cpu.r[4],cpu.r[11],CodeWriteSource::Cpu); goto L8C02942C;
L8C02942C: // 76FD
    cpu.r[6]+=0xFFFFFFFDu; goto L8C02942E;
L8C02942E: // 7420
    cpu.r[4]+=0x00000020u; goto L8C029430;
L8C029430: // 4615
    cpu.t=std::int32_t(cpu.r[6])>0; goto L8C029432;
L8C029432: // 24B2
    store(0x8C029432u,cpu.r[4],cpu.r[11],CodeWriteSource::Cpu); goto L8C029434;
L8C029434: // 8DF7
    branch=cpu.t; cpu.r[4]+=0x00000020u; if (branch) goto L8C029426; goto L8C029438;
L8C029438: // C722
    cpu.r[0]=0x8C0294C4u; goto L8C02943A;
L8C02943A: // F3E8
    cpu.fr[3]=load(cpu.r[14]); goto L8C02943C;
L8C02943C: // F508
    cpu.fr[5]=load(cpu.r[0]); goto L8C02943E;
L8C02943E: // E01C
    cpu.r[0]=0x0000001Cu; goto L8C029440;
L8C029440: // F350
    fpu_binary(cpu,FpuBinaryOperation::Add,5u,3u); goto L8C029442;
L8C029442: // FE37
    store(0x8C029442u,cpu.r[0]+cpu.r[14],cpu.fr[3],CodeWriteSource::Fpu); goto L8C029444;
L8C029444: // C720
    cpu.r[0]=0x8C0294C8u; goto L8C029446;
L8C029446: // F3E8
    cpu.fr[3]=load(cpu.r[14]); goto L8C029448;
L8C029448: // F408
    cpu.fr[4]=load(cpu.r[0]); goto L8C02944A;
L8C02944A: // E03C
    cpu.r[0]=0x0000003Cu; goto L8C02944C;
L8C02944C: // F340
    fpu_binary(cpu,FpuBinaryOperation::Add,4u,3u); goto L8C02944E;
L8C02944E: // FE37
    store(0x8C02944Eu,cpu.r[0]+cpu.r[14],cpu.fr[3],CodeWriteSource::Fpu); goto L8C029450;
L8C029450: // E004
    cpu.r[0]=0x00000004u; goto L8C029452;
L8C029452: // F3E6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[14]); goto L8C029454;
L8C029454: // E05C
    cpu.r[0]=0x0000005Cu; goto L8C029456;
L8C029456: // F350
    fpu_binary(cpu,FpuBinaryOperation::Add,5u,3u); goto L8C029458;
L8C029458: // FE37
    store(0x8C029458u,cpu.r[0]+cpu.r[14],cpu.fr[3],CodeWriteSource::Fpu); goto L8C02945A;
L8C02945A: // E004
    cpu.r[0]=0x00000004u; goto L8C02945C;
L8C02945C: // F3E6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[14]); goto L8C02945E;
L8C02945E: // E07C
    cpu.r[0]=0x0000007Cu; goto L8C029460;
L8C029460: // F340
    fpu_binary(cpu,FpuBinaryOperation::Add,4u,3u); goto L8C029462;
L8C029462: // FE37
    store(0x8C029462u,cpu.r[0]+cpu.r[14],cpu.fr[3],CodeWriteSource::Fpu); goto L8C029464;
L8C029464: // E008
    cpu.r[0]=0x00000008u; goto L8C029466;
L8C029466: // F3E6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[14]); goto L8C029468;
L8C029468: // 902A
    cpu.r[0]=load16(0x8C0294C0u); goto L8C02946A;
L8C02946A: // F350
    fpu_binary(cpu,FpuBinaryOperation::Add,5u,3u); goto L8C02946C;
L8C02946C: // FE37
    store(0x8C02946Cu,cpu.r[0]+cpu.r[14],cpu.fr[3],CodeWriteSource::Fpu); goto L8C02946E;
L8C02946E: // E008
    cpu.r[0]=0x00000008u; goto L8C029470;
L8C029470: // F3E6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[14]); goto L8C029472;
L8C029472: // 9026
    cpu.r[0]=load16(0x8C0294C2u); goto L8C029474;
L8C029474: // F340
    fpu_binary(cpu,FpuBinaryOperation::Add,4u,3u); goto L8C029476;
L8C029476: // FE37
    store(0x8C029476u,cpu.r[0]+cpu.r[14],cpu.fr[3],CodeWriteSource::Fpu); goto L8C029478;
L8C029478: // D314
    cpu.r[3]=load(0x8C0294CCu); goto L8C02947A;
L8C02947A: // 66B3
    cpu.r[6]=cpu.r[11]; goto L8C02947C;
L8C02947C: // 6432
    cpu.r[4]=load(cpu.r[3]); goto L8C02947E;
L8C02947E: // 2448
    cpu.t=(cpu.r[4]&cpu.r[4])==0u; goto L8C029480;
L8C029480: // 8D08
    branch=cpu.t; cpu.r[12]=0x00000001u; if (branch) goto L8C029494; goto L8C029484;
L8C029484: // 5342
    cpu.r[3]=load(cpu.r[4]+8u); goto L8C029486;
L8C029486: // 3350
    cpu.t=cpu.r[3]==cpu.r[5]; goto L8C029488;
L8C029488: // 8B01
    if (!cpu.t) goto L8C02948E; goto L8C02948A;
L8C02948A: // A003
    cpu.r[6]=cpu.r[12]; goto L8C029494;
L8C02948E: // 6442
    cpu.r[4]=load(cpu.r[4]); goto L8C029490;
L8C029490: // 2448
    cpu.t=(cpu.r[4]&cpu.r[4])==0u; goto L8C029492;
L8C029492: // 8BF7
    if (!cpu.t) goto L8C029484; goto L8C029494;
L8C029494: // 2668
    cpu.t=(cpu.r[6]&cpu.r[6])==0u; goto L8C029496;
L8C029496: // 8B01
    if (!cpu.t) goto L8C02949C; goto L8C029498;
L8C029498: // A30A
    cpu.r[0]=0x00000000u; goto L8C029AB0;
L8C02949C: // C70C
    cpu.r[0]=0x8C0294D0u; goto L8C02949E;
L8C02949E: // 5244
    cpu.r[2]=load(cpu.r[4]+16u); goto L8C0294A0;
L8C0294A0: // FD9D
    cpu.fr[13]=0x3F800000u; goto L8C0294A2;
L8C0294A2: // 1F25
    store(0x8C0294A2u,cpu.r[15]+20u,cpu.r[2],CodeWriteSource::Cpu); goto L8C0294A4;
L8C0294A4: // 5343
    cpu.r[3]=load(cpu.r[4]+12u); goto L8C0294A6;
L8C0294A6: // 1F34
    store(0x8C0294A6u,cpu.r[15]+16u,cpu.r[3],CodeWriteSource::Cpu); goto L8C0294A8;
L8C0294A8: // FC08
    cpu.fr[12]=load(cpu.r[0]); goto L8C0294AA;
L8C0294AA: // C70A
    cpu.r[0]=0x8C0294D4u; goto L8C0294AC;
L8C0294AC: // FE08
    cpu.fr[14]=load(cpu.r[0]); goto L8C0294AE;
L8C0294AE: // A2F9
    cpu.fr[15]=0u; goto L8C029AA4;
L8C0294E0: // 54F5
    cpu.r[4]=load(cpu.r[15]+20u); goto L8C0294E2;
L8C0294E2: // E004
    cpu.r[0]=0x00000004u; goto L8C0294E4;
L8C0294E4: // F0E6
    cpu.fr[0]=load(cpu.r[0]+cpu.r[14]); goto L8C0294E6;
L8C0294E6: // 7418
    cpu.r[4]+=0x00000018u; goto L8C0294E8;
L8C0294E8: // F3E8
    cpu.fr[3]=load(cpu.r[14]); goto L8C0294EA;
L8C0294EA: // F546
    cpu.fr[5]=load(cpu.r[0]+cpu.r[4]); goto L8C0294EC;
L8C0294EC: // E008
    cpu.r[0]=0x00000008u; goto L8C0294EE;
L8C0294EE: // F648
    cpu.fr[6]=load(cpu.r[4]); goto L8C0294F0;
L8C0294F0: // F501
    fpu_binary(cpu,FpuBinaryOperation::Subtract,0u,5u); goto L8C0294F2;
L8C0294F2: // F2E6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[14]); goto L8C0294F4;
L8C0294F4: // F631
    fpu_binary(cpu,FpuBinaryOperation::Subtract,3u,6u); goto L8C0294F6;
L8C0294F6: // F446
    cpu.fr[4]=load(cpu.r[0]+cpu.r[4]); goto L8C0294F8;
L8C0294F8: // C752
    cpu.r[0]=0x8C029644u; goto L8C0294FA;
L8C0294FA: // F421
    fpu_binary(cpu,FpuBinaryOperation::Subtract,2u,4u); goto L8C0294FC;
L8C0294FC: // F208
    cpu.fr[2]=load(cpu.r[0]); goto L8C0294FE;
L8C0294FE: // F35C
    cpu.fr[3]=cpu.fr[5]; goto L8C029500;
L8C029500: // F352
    fpu_binary(cpu,FpuBinaryOperation::Multiply,5u,3u); goto L8C029502;
L8C029502: // F06C
    cpu.fr[0]=cpu.fr[6]; goto L8C029504;
L8C029504: // F36E
    fpu_multiply_accumulate(cpu,6u,3u); goto L8C029506;
L8C029506: // F04C
    cpu.fr[0]=cpu.fr[4]; goto L8C029508;
L8C029508: // F34E
    fpu_multiply_accumulate(cpu,4u,3u); goto L8C02950A;
L8C02950A: // F325
    fpu_compare_greater(cpu,2u,3u); goto L8C02950C;
L8C02950C: // 8B01
    if (!cpu.t) goto L8C029512; goto L8C02950E;
L8C02950E: // A2C6
    ; goto L8C029A9E;
L8C029512: // 5DF5
    cpu.r[13]=load(cpu.r[15]+20u); goto L8C029514;
L8C029514: // E004
    cpu.r[0]=0x00000004u; goto L8C029516;
L8C029516: // F2E6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[14]); goto L8C029518;
L8C029518: // E01C
    cpu.r[0]=0x0000001Cu; goto L8C02951A;
L8C02951A: // 7D18
    cpu.r[13]+=0x00000018u; goto L8C02951C;
L8C02951C: // 69D3
    cpu.r[9]=cpu.r[13]; goto L8C02951E;
L8C02951E: // 790C
    cpu.r[9]+=0x0000000Cu; goto L8C029520;
L8C029520: // F598
    cpu.fr[5]=load(cpu.r[9]); goto L8C029522;
L8C029522: // 6AD3
    cpu.r[10]=cpu.r[13]; goto L8C029524;
L8C029524: // F8D8
    cpu.fr[8]=load(cpu.r[13]); goto L8C029526;
L8C029526: // 7A18
    cpu.r[10]+=0x00000018u; goto L8C029528;
L8C029528: // FF27
    store(0x8C029528u,cpu.r[0]+cpu.r[15],cpu.fr[2],CodeWriteSource::Fpu); goto L8C02952A;
L8C02952A: // E004
    cpu.r[0]=0x00000004u; goto L8C02952C;
L8C02952C: // F7D6
    cpu.fr[7]=load(cpu.r[0]+cpu.r[13]); goto L8C02952E;
L8C02952E: // F35C
    cpu.fr[3]=cpu.fr[5]; goto L8C029530;
L8C029530: // F381
    fpu_binary(cpu,FpuBinaryOperation::Subtract,8u,3u); goto L8C029532;
L8C029532: // F271
    fpu_binary(cpu,FpuBinaryOperation::Subtract,7u,2u); goto L8C029534;
L8C029534: // FB96
    cpu.fr[11]=load(cpu.r[0]+cpu.r[9]); goto L8C029536;
L8C029536: // FAE8
    cpu.fr[10]=load(cpu.r[14]); goto L8C029538;
L8C029538: // E01C
    cpu.r[0]=0x0000001Cu; goto L8C02953A;
L8C02953A: // F9A8
    cpu.fr[9]=load(cpu.r[10]); goto L8C02953C;
L8C02953C: // F1AC
    cpu.fr[1]=cpu.fr[10]; goto L8C02953E;
L8C02953E: // F181
    fpu_binary(cpu,FpuBinaryOperation::Subtract,8u,1u); goto L8C029540;
L8C029540: // F322
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,3u); goto L8C029542;
L8C029542: // F2BC
    cpu.fr[2]=cpu.fr[11]; goto L8C029544;
L8C029544: // F271
    fpu_binary(cpu,FpuBinaryOperation::Subtract,7u,2u); goto L8C029546;
L8C029546: // F43C
    cpu.fr[4]=cpu.fr[3]; goto L8C029548;
L8C029548: // F212
    fpu_binary(cpu,FpuBinaryOperation::Multiply,1u,2u); goto L8C02954A;
L8C02954A: // F39C
    cpu.fr[3]=cpu.fr[9]; goto L8C02954C;
L8C02954C: // F351
    fpu_binary(cpu,FpuBinaryOperation::Subtract,5u,3u); goto L8C02954E;
L8C02954E: // F421
    fpu_binary(cpu,FpuBinaryOperation::Subtract,2u,4u); goto L8C029550;
L8C029550: // F2F6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[15]); goto L8C029552;
L8C029552: // F2B1
    fpu_binary(cpu,FpuBinaryOperation::Subtract,11u,2u); goto L8C029554;
L8C029554: // E004
    cpu.r[0]=0x00000004u; goto L8C029556;
L8C029556: // F6A6
    cpu.fr[6]=load(cpu.r[0]+cpu.r[10]); goto L8C029558;
L8C029558: // E01C
    cpu.r[0]=0x0000001Cu; goto L8C02955A;
L8C02955A: // F1AC
    cpu.fr[1]=cpu.fr[10]; goto L8C02955C;
L8C02955C: // F151
    fpu_binary(cpu,FpuBinaryOperation::Subtract,5u,1u); goto L8C02955E;
L8C02955E: // F891
    fpu_binary(cpu,FpuBinaryOperation::Subtract,9u,8u); goto L8C029560;
L8C029560: // F322
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,3u); goto L8C029562;
L8C029562: // F26C
    cpu.fr[2]=cpu.fr[6]; goto L8C029564;
L8C029564: // F2B1
    fpu_binary(cpu,FpuBinaryOperation::Subtract,11u,2u); goto L8C029566;
L8C029566: // FA91
    fpu_binary(cpu,FpuBinaryOperation::Subtract,9u,10u); goto L8C029568;
L8C029568: // F761
    fpu_binary(cpu,FpuBinaryOperation::Subtract,6u,7u); goto L8C02956A;
L8C02956A: // F53C
    cpu.fr[5]=cpu.fr[3]; goto L8C02956C;
L8C02956C: // F3F6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[15]); goto L8C02956E;
L8C02956E: // F212
    fpu_binary(cpu,FpuBinaryOperation::Multiply,1u,2u); goto L8C029570;
L8C029570: // F361
    fpu_binary(cpu,FpuBinaryOperation::Subtract,6u,3u); goto L8C029572;
L8C029572: // F7A2
    fpu_binary(cpu,FpuBinaryOperation::Multiply,10u,7u); goto L8C029574;
L8C029574: // F521
    fpu_binary(cpu,FpuBinaryOperation::Subtract,2u,5u); goto L8C029576;
L8C029576: // F28D
    cpu.fr[2]=0u; goto L8C029578;
L8C029578: // F832
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,8u); goto L8C02957A;
L8C02957A: // F34C
    cpu.fr[3]=cpu.fr[4]; goto L8C02957C;
L8C02957C: // F352
    fpu_binary(cpu,FpuBinaryOperation::Multiply,5u,3u); goto L8C02957E;
L8C02957E: // F68C
    cpu.fr[6]=cpu.fr[8]; goto L8C029580;
L8C029580: // F235
    fpu_compare_greater(cpu,3u,2u); goto L8C029582;
L8C029582: // 8F02
    branch=!cpu.t; fpu_binary(cpu,FpuBinaryOperation::Subtract,7u,6u); if (branch) goto L8C02958A; goto L8C029586;
L8C029586: // A0D2
    ; goto L8C02972E;
L8C02958A: // F35C
    cpu.fr[3]=cpu.fr[5]; goto L8C02958C;
L8C02958C: // F362
    fpu_binary(cpu,FpuBinaryOperation::Multiply,6u,3u); goto L8C02958E;
L8C02958E: // F235
    fpu_compare_greater(cpu,3u,2u); goto L8C029590;
L8C029590: // 8B01
    if (!cpu.t) goto L8C029596; goto L8C029592;
L8C029592: // A0CC
    ; goto L8C02972E;
L8C029596: // F34C
    cpu.fr[3]=cpu.fr[4]; goto L8C029598;
L8C029598: // F362
    fpu_binary(cpu,FpuBinaryOperation::Multiply,6u,3u); goto L8C02959A;
L8C02959A: // F28D
    cpu.fr[2]=0u; goto L8C02959C;
L8C02959C: // F235
    fpu_compare_greater(cpu,3u,2u); goto L8C02959E;
L8C02959E: // 8B01
    if (!cpu.t) goto L8C0295A4; goto L8C0295A0;
L8C0295A0: // A0C5
    ; goto L8C02972E;
L8C0295A4: // D328
    cpu.r[3]=load(0x8C029648u); goto L8C0295A6;
L8C0295A6: // 67F3
    cpu.r[7]=cpu.r[15]; goto L8C0295A8;
L8C0295A8: // 6593
    cpu.r[5]=cpu.r[9]; goto L8C0295AA;
L8C0295AA: // 7728
    cpu.r[7]+=0x00000028u; goto L8C0295AC;
L8C0295AC: // 66A3
    cpu.r[6]=cpu.r[10]; goto L8C0295AE;
L8C0295AE: // 430B
    { const auto target=cpu.r[3]; cpu.pr=0x8C0295B2u; cpu.r[4]=cpu.r[13]; call(target); } goto L8C0295B2;
L8C0295B2: // E028
    cpu.r[0]=0x00000028u; goto L8C0295B4;
L8C0295B4: // F3D8
    cpu.fr[3]=load(cpu.r[13]); goto L8C0295B6;
L8C0295B6: // F6F6
    cpu.fr[6]=load(cpu.r[0]+cpu.r[15]); goto L8C0295B8;
L8C0295B8: // E004
    cpu.r[0]=0x00000004u; goto L8C0295BA;
L8C0295BA: // F26C
    cpu.fr[2]=cpu.fr[6]; goto L8C0295BC;
L8C0295BC: // F232
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,2u); goto L8C0295BE;
L8C0295BE: // F3D6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[13]); goto L8C0295C0;
L8C0295C0: // E02C
    cpu.r[0]=0x0000002Cu; goto L8C0295C2;
L8C0295C2: // F1F6
    cpu.fr[1]=load(cpu.r[0]+cpu.r[15]); goto L8C0295C4;
L8C0295C4: // E030
    cpu.r[0]=0x00000030u; goto L8C0295C6;
L8C0295C6: // F4F6
    cpu.fr[4]=load(cpu.r[0]+cpu.r[15]); goto L8C0295C8;
L8C0295C8: // E008
    cpu.r[0]=0x00000008u; goto L8C0295CA;
L8C0295CA: // F132
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,1u); goto L8C0295CC;
L8C0295CC: // F24D
    fpu_negate(cpu,2u); goto L8C0295CE;
L8C0295CE: // F3D6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[13]); goto L8C0295D0;
L8C0295D0: // F211
    fpu_binary(cpu,FpuBinaryOperation::Subtract,1u,2u); goto L8C0295D2;
L8C0295D2: // F14C
    cpu.fr[1]=cpu.fr[4]; goto L8C0295D4;
L8C0295D4: // F132
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,1u); goto L8C0295D6;
L8C0295D6: // F34C
    cpu.fr[3]=cpu.fr[4]; goto L8C0295D8;
L8C0295D8: // F342
    fpu_binary(cpu,FpuBinaryOperation::Multiply,4u,3u); goto L8C0295DA;
L8C0295DA: // F52C
    cpu.fr[5]=cpu.fr[2]; goto L8C0295DC;
L8C0295DC: // FC35
    fpu_compare_greater(cpu,3u,12u); goto L8C0295DE;
L8C0295DE: // 8F02
    branch=!cpu.t; fpu_binary(cpu,FpuBinaryOperation::Subtract,1u,5u); if (branch) goto L8C0295E6; goto L8C0295E2;
L8C0295E2: // A0A4
    ; goto L8C02972E;
L8C0295E6: // F3E8
    cpu.fr[3]=load(cpu.r[14]); goto L8C0295E8;
L8C0295E8: // E004
    cpu.r[0]=0x00000004u; goto L8C0295EA;
L8C0295EA: // F632
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,6u); goto L8C0295EC;
L8C0295EC: // F3E6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[14]); goto L8C0295EE;
L8C0295EE: // E02C
    cpu.r[0]=0x0000002Cu; goto L8C0295F0;
L8C0295F0: // F2F6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[15]); goto L8C0295F2;
L8C0295F2: // E030
    cpu.r[0]=0x00000030u; goto L8C0295F4;
L8C0295F4: // F232
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,2u); goto L8C0295F6;
L8C0295F6: // F64D
    fpu_negate(cpu,6u); goto L8C0295F8;
L8C0295F8: // F3F6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[15]); goto L8C0295FA;
L8C0295FA: // E008
    cpu.r[0]=0x00000008u; goto L8C0295FC;
L8C0295FC: // F621
    fpu_binary(cpu,FpuBinaryOperation::Subtract,2u,6u); goto L8C0295FE;
L8C0295FE: // F651
    fpu_binary(cpu,FpuBinaryOperation::Subtract,5u,6u); goto L8C029600;
L8C029600: // F633
    fpu_binary(cpu,FpuBinaryOperation::Divide,3u,6u); goto L8C029602;
L8C029602: // FF67
    store(0x8C029602u,cpu.r[0]+cpu.r[15],cpu.fr[6],CodeWriteSource::Fpu); goto L8C029604;
L8C029604: // E008
    cpu.r[0]=0x00000008u; goto L8C029606;
L8C029606: // F3E6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[14]); goto L8C029608;
L8C029608: // F26C
    cpu.fr[2]=cpu.fr[6]; goto L8C02960A;
L8C02960A: // F325
    fpu_compare_greater(cpu,2u,3u); goto L8C02960C;
L8C02960C: // 8F28
    branch=!cpu.t; cpu.r[8]=cpu.r[11]; if (branch) goto L8C029660; goto L8C029610;
L8C029610: // 64E3
    cpu.r[4]=cpu.r[14]; goto L8C029612;
L8C029612: // 9315
    cpu.r[3]=load16(0x8C029640u); goto L8C029614;
L8C029614: // 740C
    cpu.r[4]+=0x0000000Cu; goto L8C029616;
L8C029616: // 343C
    cpu.r[4]+=cpu.r[3]; goto L8C029618;
L8C029618: // 6242
    cpu.r[2]=load(cpu.r[4]); goto L8C02961A;
L8C02961A: // 2228
    cpu.t=(cpu.r[2]&cpu.r[2])==0u; goto L8C02961C;
L8C02961C: // 8905
    if (cpu.t) goto L8C02962A; goto L8C02961E;
L8C02961E: // E010
    cpu.r[0]=0x00000010u; goto L8C029620;
L8C029620: // F346
    cpu.fr[3]=load(cpu.r[0]+cpu.r[4]); goto L8C029622;
L8C029622: // E008
    cpu.r[0]=0x00000008u; goto L8C029624;
L8C029624: // F2F6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[15]); goto L8C029626;
L8C029626: // F235
    fpu_compare_greater(cpu,3u,2u); goto L8C029628;
L8C029628: // 8B2B
    if (!cpu.t) goto L8C029682; goto L8C02962A;
L8C02962A: // 68E3
    cpu.r[8]=cpu.r[14]; goto L8C02962C;
L8C02962C: // 9308
    cpu.r[3]=load16(0x8C029640u); goto L8C02962E;
L8C02962E: // 780C
    cpu.r[8]+=0x0000000Cu; goto L8C029630;
L8C029630: // A027
    cpu.r[8]+=cpu.r[3]; goto L8C029682;
L8C029660: // 64E3
    cpu.r[4]=cpu.r[14]; goto L8C029662;
L8C029662: // 932D
    cpu.r[3]=load16(0x8C0296C0u); goto L8C029664;
L8C029664: // 740C
    cpu.r[4]+=0x0000000Cu; goto L8C029666;
L8C029666: // 343C
    cpu.r[4]+=cpu.r[3]; goto L8C029668;
L8C029668: // 6242
    cpu.r[2]=load(cpu.r[4]); goto L8C02966A;
L8C02966A: // 2228
    cpu.t=(cpu.r[2]&cpu.r[2])==0u; goto L8C02966C;
L8C02966C: // 8905
    if (cpu.t) goto L8C02967A; goto L8C02966E;
L8C02966E: // E010
    cpu.r[0]=0x00000010u; goto L8C029670;
L8C029670: // F346
    cpu.fr[3]=load(cpu.r[0]+cpu.r[4]); goto L8C029672;
L8C029672: // E008
    cpu.r[0]=0x00000008u; goto L8C029674;
L8C029674: // F2F6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[15]); goto L8C029676;
L8C029676: // F325
    fpu_compare_greater(cpu,2u,3u); goto L8C029678;
L8C029678: // 8B03
    if (!cpu.t) goto L8C029682; goto L8C02967A;
L8C02967A: // 68E3
    cpu.r[8]=cpu.r[14]; goto L8C02967C;
L8C02967C: // 9320
    cpu.r[3]=load16(0x8C0296C0u); goto L8C02967E;
L8C02967E: // 780C
    cpu.r[8]+=0x0000000Cu; goto L8C029680;
L8C029680: // 383C
    cpu.r[8]+=cpu.r[3]; goto L8C029682;
L8C029682: // 2888
    cpu.t=(cpu.r[8]&cpu.r[8])==0u; goto L8C029684;
L8C029684: // 8953
    if (cpu.t) goto L8C02972E; goto L8C029686;
L8C029686: // D30F
    cpu.r[3]=load(0x8C0296C4u); goto L8C029688;
L8C029688: // 64F3
    cpu.r[4]=cpu.r[15]; goto L8C02968A;
L8C02968A: // 430B
    { const auto target=cpu.r[3]; cpu.pr=0x8C02968Eu; cpu.r[4]+=0x00000028u; call(target); } goto L8C02968E;
L8C02968E: // F38D
    cpu.fr[3]=0u; goto L8C029690;
L8C029690: // F034
    fpu_compare_equal(cpu,3u,0u); goto L8C029692;
L8C029692: // 8B25
    if (!cpu.t) goto L8C0296E0; goto L8C029694;
L8C029694: // 63F3
    cpu.r[3]=cpu.r[15]; goto L8C029696;
L8C029696: // 7328
    cpu.r[3]+=0x00000028u; goto L8C029698;
L8C029698: // E004
    cpu.r[0]=0x00000004u; goto L8C02969A;
L8C02969A: // 62F3
    cpu.r[2]=cpu.r[15]; goto L8C02969C;
L8C02969C: // 7228
    cpu.r[2]+=0x00000028u; goto L8C02969E;
L8C02969E: // F2FA
    store(0x8C02969Eu,cpu.r[2],cpu.fr[15],CodeWriteSource::Fpu); goto L8C0296A0;
L8C0296A0: // F3D7
    store(0x8C0296A0u,cpu.r[0]+cpu.r[3],cpu.fr[13],CodeWriteSource::Fpu); goto L8C0296A2;
L8C0296A2: // 63F3
    cpu.r[3]=cpu.r[15]; goto L8C0296A4;
L8C0296A4: // 7328
    cpu.r[3]+=0x00000028u; goto L8C0296A6;
L8C0296A6: // E008
    cpu.r[0]=0x00000008u; goto L8C0296A8;
L8C0296A8: // F3F7
    store(0x8C0296A8u,cpu.r[0]+cpu.r[3],cpu.fr[15],CodeWriteSource::Fpu); goto L8C0296AA;
L8C0296AA: // A01D
    ; goto L8C0296E8;
L8C0296E0: // D26F
    cpu.r[2]=load(0x8C0298A0u); goto L8C0296E2;
L8C0296E2: // 64F3
    cpu.r[4]=cpu.r[15]; goto L8C0296E4;
L8C0296E4: // 420B
    { const auto target=cpu.r[2]; cpu.pr=0x8C0296E8u; cpu.r[4]+=0x00000028u; call(target); } goto L8C0296E8;
L8C0296E8: // E008
    cpu.r[0]=0x00000008u; goto L8C0296EA;
L8C0296EA: // 1FC9
    store(0x8C0296EAu,cpu.r[15]+36u,cpu.r[12],CodeWriteSource::Cpu); goto L8C0296EC;
L8C0296EC: // 28C2
    store(0x8C0296ECu,cpu.r[8],cpu.r[12],CodeWriteSource::Cpu); goto L8C0296EE;
L8C0296EE: // 53F4
    cpu.r[3]=load(cpu.r[15]+16u); goto L8C0296F0;
L8C0296F0: // 1831
    store(0x8C0296F0u,cpu.r[8]+4u,cpu.r[3],CodeWriteSource::Cpu); goto L8C0296F2;
L8C0296F2: // 63F3
    cpu.r[3]=cpu.r[15]; goto L8C0296F4;
L8C0296F4: // 7328
    cpu.r[3]+=0x00000028u; goto L8C0296F6;
L8C0296F6: // F3F6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[15]); goto L8C0296F8;
L8C0296F8: // E010
    cpu.r[0]=0x00000010u; goto L8C0296FA;
L8C0296FA: // F837
    store(0x8C0296FAu,cpu.r[0]+cpu.r[8],cpu.fr[3],CodeWriteSource::Fpu); goto L8C0296FC;
L8C0296FC: // E02C
    cpu.r[0]=0x0000002Cu; goto L8C0296FE;
L8C0296FE: // 6232
    cpu.r[2]=load(cpu.r[3]); goto L8C029700;
L8C029700: // 1825
    store(0x8C029700u,cpu.r[8]+20u,cpu.r[2],CodeWriteSource::Cpu); goto L8C029702;
L8C029702: // 5231
    cpu.r[2]=load(cpu.r[3]+4u); goto L8C029704;
L8C029704: // 1826
    store(0x8C029704u,cpu.r[8]+24u,cpu.r[2],CodeWriteSource::Cpu); goto L8C029706;
L8C029706: // 5232
    cpu.r[2]=load(cpu.r[3]+8u); goto L8C029708;
L8C029708: // D366
    cpu.r[3]=load(0x8C0298A4u); goto L8C02970A;
L8C02970A: // 1827
    store(0x8C02970Au,cpu.r[8]+28u,cpu.r[2],CodeWriteSource::Cpu); goto L8C02970C;
L8C02970C: // F5F6
    cpu.fr[5]=load(cpu.r[0]+cpu.r[15]); goto L8C02970E;
L8C02970E: // E028
    cpu.r[0]=0x00000028u; goto L8C029710;
L8C029710: // 430B
    { const auto target=cpu.r[3]; cpu.pr=0x8C029714u; cpu.fr[4]=load(cpu.r[0]+cpu.r[15]); call(target); } goto L8C029714;
L8C029714: // F0E2
    fpu_binary(cpu,FpuBinaryOperation::Multiply,14u,0u); goto L8C029716;
L8C029716: // D364
    cpu.r[3]=load(0x8C0298A8u); goto L8C029718;
L8C029718: // E030
    cpu.r[0]=0x00000030u; goto L8C02971A;
L8C02971A: // F03D
    fpu_truncate_to_fpul(cpu,0u); goto L8C02971C;
L8C02971C: // 025A
    cpu.r[2]=cpu.fpul; goto L8C02971E;
L8C02971E: // 622B
    cpu.r[2]=0u-cpu.r[2]; goto L8C029720;
L8C029720: // 1823
    store(0x8C029720u,cpu.r[8]+12u,cpu.r[2],CodeWriteSource::Cpu); goto L8C029722;
L8C029722: // 430B
    { const auto target=cpu.r[3]; cpu.pr=0x8C029726u; cpu.fr[4]=load(cpu.r[0]+cpu.r[15]); call(target); } goto L8C029726;
L8C029726: // F0E2
    fpu_binary(cpu,FpuBinaryOperation::Multiply,14u,0u); goto L8C029728;
L8C029728: // F03D
    fpu_truncate_to_fpul(cpu,0u); goto L8C02972A;
L8C02972A: // 025A
    cpu.r[2]=cpu.fpul; goto L8C02972C;
L8C02972C: // 1822
    store(0x8C02972Cu,cpu.r[8]+8u,cpu.r[2],CodeWriteSource::Cpu); goto L8C02972E;
L8C02972E: // E004
    cpu.r[0]=0x00000004u; goto L8C029730;
L8C029730: // F6D6
    cpu.fr[6]=load(cpu.r[0]+cpu.r[13]); goto L8C029732;
L8C029732: // F796
    cpu.fr[7]=load(cpu.r[0]+cpu.r[9]); goto L8C029734;
L8C029734: // E008
    cpu.r[0]=0x00000008u; goto L8C029736;
L8C029736: // F8E6
    cpu.fr[8]=load(cpu.r[0]+cpu.r[14]); goto L8C029738;
L8C029738: // F5D6
    cpu.fr[5]=load(cpu.r[0]+cpu.r[13]); goto L8C02973A;
L8C02973A: // F28C
    cpu.fr[2]=cpu.fr[8]; goto L8C02973C;
L8C02973C: // F37C
    cpu.fr[3]=cpu.fr[7]; goto L8C02973E;
L8C02973E: // F251
    fpu_binary(cpu,FpuBinaryOperation::Subtract,5u,2u); goto L8C029740;
L8C029740: // F361
    fpu_binary(cpu,FpuBinaryOperation::Subtract,6u,3u); goto L8C029742;
L8C029742: // F996
    cpu.fr[9]=load(cpu.r[0]+cpu.r[9]); goto L8C029744;
L8C029744: // E004
    cpu.r[0]=0x00000004u; goto L8C029746;
L8C029746: // FAE6
    cpu.fr[10]=load(cpu.r[0]+cpu.r[14]); goto L8C029748;
L8C029748: // FBA6
    cpu.fr[11]=load(cpu.r[0]+cpu.r[10]); goto L8C02974A;
L8C02974A: // E008
    cpu.r[0]=0x00000008u; goto L8C02974C;
L8C02974C: // F322
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,3u); goto L8C02974E;
L8C02974E: // F1AC
    cpu.fr[1]=cpu.fr[10]; goto L8C029750;
L8C029750: // F29C
    cpu.fr[2]=cpu.fr[9]; goto L8C029752;
L8C029752: // F161
    fpu_binary(cpu,FpuBinaryOperation::Subtract,6u,1u); goto L8C029754;
L8C029754: // F251
    fpu_binary(cpu,FpuBinaryOperation::Subtract,5u,2u); goto L8C029756;
L8C029756: // F43C
    cpu.fr[4]=cpu.fr[3]; goto L8C029758;
L8C029758: // F3BC
    cpu.fr[3]=cpu.fr[11]; goto L8C02975A;
L8C02975A: // F371
    fpu_binary(cpu,FpuBinaryOperation::Subtract,7u,3u); goto L8C02975C;
L8C02975C: // F212
    fpu_binary(cpu,FpuBinaryOperation::Multiply,1u,2u); goto L8C02975E;
L8C02975E: // F1A6
    cpu.fr[1]=load(cpu.r[0]+cpu.r[10]); goto L8C029760;
L8C029760: // E020
    cpu.r[0]=0x00000020u; goto L8C029762;
L8C029762: // FF17
    store(0x8C029762u,cpu.r[0]+cpu.r[15],cpu.fr[1],CodeWriteSource::Fpu); goto L8C029764;
L8C029764: // F191
    fpu_binary(cpu,FpuBinaryOperation::Subtract,9u,1u); goto L8C029766;
L8C029766: // F421
    fpu_binary(cpu,FpuBinaryOperation::Subtract,2u,4u); goto L8C029768;
L8C029768: // F28C
    cpu.fr[2]=cpu.fr[8]; goto L8C02976A;
L8C02976A: // F291
    fpu_binary(cpu,FpuBinaryOperation::Subtract,9u,2u); goto L8C02976C;
L8C02976C: // F322
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,3u); goto L8C02976E;
L8C02976E: // F2AC
    cpu.fr[2]=cpu.fr[10]; goto L8C029770;
L8C029770: // F271
    fpu_binary(cpu,FpuBinaryOperation::Subtract,7u,2u); goto L8C029772;
L8C029772: // E020
    cpu.r[0]=0x00000020u; goto L8C029774;
L8C029774: // F73C
    cpu.fr[7]=cpu.fr[3]; goto L8C029776;
L8C029776: // F3F6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[15]); goto L8C029778;
L8C029778: // FAB1
    fpu_binary(cpu,FpuBinaryOperation::Subtract,11u,10u); goto L8C02977A;
L8C02977A: // F6B1
    fpu_binary(cpu,FpuBinaryOperation::Subtract,11u,6u); goto L8C02977C;
L8C02977C: // F122
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,1u); goto L8C02977E;
L8C02977E: // F24C
    cpu.fr[2]=cpu.fr[4]; goto L8C029780;
L8C029780: // F531
    fpu_binary(cpu,FpuBinaryOperation::Subtract,3u,5u); goto L8C029782;
L8C029782: // F831
    fpu_binary(cpu,FpuBinaryOperation::Subtract,3u,8u); goto L8C029784;
L8C029784: // F711
    fpu_binary(cpu,FpuBinaryOperation::Subtract,1u,7u); goto L8C029786;
L8C029786: // F5A2
    fpu_binary(cpu,FpuBinaryOperation::Multiply,10u,5u); goto L8C029788;
L8C029788: // F682
    fpu_binary(cpu,FpuBinaryOperation::Multiply,8u,6u); goto L8C02978A;
L8C02978A: // F272
    fpu_binary(cpu,FpuBinaryOperation::Multiply,7u,2u); goto L8C02978C;
L8C02978C: // F35C
    cpu.fr[3]=cpu.fr[5]; goto L8C02978E;
L8C02978E: // F56C
    cpu.fr[5]=cpu.fr[6]; goto L8C029790;
L8C029790: // F531
    fpu_binary(cpu,FpuBinaryOperation::Subtract,3u,5u); goto L8C029792;
L8C029792: // F38D
    cpu.fr[3]=0u; goto L8C029794;
L8C029794: // F325
    fpu_compare_greater(cpu,2u,3u); goto L8C029796;
L8C029796: // 8B01
    if (!cpu.t) goto L8C02979C; goto L8C029798;
L8C029798: // A0B9
    ; goto L8C02990E;
L8C02979C: // F27C
    cpu.fr[2]=cpu.fr[7]; goto L8C02979E;
L8C02979E: // F252
    fpu_binary(cpu,FpuBinaryOperation::Multiply,5u,2u); goto L8C0297A0;
L8C0297A0: // F325
    fpu_compare_greater(cpu,2u,3u); goto L8C0297A2;
L8C0297A2: // 8B01
    if (!cpu.t) goto L8C0297A8; goto L8C0297A4;
L8C0297A4: // A0B3
    ; goto L8C02990E;
L8C0297A8: // F34C
    cpu.fr[3]=cpu.fr[4]; goto L8C0297AA;
L8C0297AA: // F352
    fpu_binary(cpu,FpuBinaryOperation::Multiply,5u,3u); goto L8C0297AC;
L8C0297AC: // F28D
    cpu.fr[2]=0u; goto L8C0297AE;
L8C0297AE: // F235
    fpu_compare_greater(cpu,3u,2u); goto L8C0297B0;
L8C0297B0: // 8B01
    if (!cpu.t) goto L8C0297B6; goto L8C0297B2;
L8C0297B2: // A0AC
    ; goto L8C02990E;
L8C0297B6: // D33D
    cpu.r[3]=load(0x8C0298ACu); goto L8C0297B8;
L8C0297B8: // 67F3
    cpu.r[7]=cpu.r[15]; goto L8C0297BA;
L8C0297BA: // 6593
    cpu.r[5]=cpu.r[9]; goto L8C0297BC;
L8C0297BC: // 7728
    cpu.r[7]+=0x00000028u; goto L8C0297BE;
L8C0297BE: // 66A3
    cpu.r[6]=cpu.r[10]; goto L8C0297C0;
L8C0297C0: // 430B
    { const auto target=cpu.r[3]; cpu.pr=0x8C0297C4u; cpu.r[4]=cpu.r[13]; call(target); } goto L8C0297C4;
L8C0297C4: // E028
    cpu.r[0]=0x00000028u; goto L8C0297C6;
L8C0297C6: // F3F6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[15]); goto L8C0297C8;
L8C0297C8: // E00C
    cpu.r[0]=0x0000000Cu; goto L8C0297CA;
L8C0297CA: // FF37
    store(0x8C0297CAu,cpu.r[0]+cpu.r[15],cpu.fr[3],CodeWriteSource::Fpu); goto L8C0297CC;
L8C0297CC: // E004
    cpu.r[0]=0x00000004u; goto L8C0297CE;
L8C0297CE: // F2D8
    cpu.fr[2]=load(cpu.r[13]); goto L8C0297D0;
L8C0297D0: // F322
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,3u); goto L8C0297D2;
L8C0297D2: // F2D6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[13]); goto L8C0297D4;
L8C0297D4: // E02C
    cpu.r[0]=0x0000002Cu; goto L8C0297D6;
L8C0297D6: // F1F6
    cpu.fr[1]=load(cpu.r[0]+cpu.r[15]); goto L8C0297D8;
L8C0297D8: // E008
    cpu.r[0]=0x00000008u; goto L8C0297DA;
L8C0297DA: // F122
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,1u); goto L8C0297DC;
L8C0297DC: // F34D
    fpu_negate(cpu,3u); goto L8C0297DE;
L8C0297DE: // F2D6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[13]); goto L8C0297E0;
L8C0297E0: // E030
    cpu.r[0]=0x00000030u; goto L8C0297E2;
L8C0297E2: // F311
    fpu_binary(cpu,FpuBinaryOperation::Subtract,1u,3u); goto L8C0297E4;
L8C0297E4: // F1F6
    cpu.fr[1]=load(cpu.r[0]+cpu.r[15]); goto L8C0297E6;
L8C0297E6: // E00C
    cpu.r[0]=0x0000000Cu; goto L8C0297E8;
L8C0297E8: // F122
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,1u); goto L8C0297EA;
L8C0297EA: // F43C
    cpu.fr[4]=cpu.fr[3]; goto L8C0297EC;
L8C0297EC: // F3F6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[15]); goto L8C0297EE;
L8C0297EE: // F23C
    cpu.fr[2]=cpu.fr[3]; goto L8C0297F0;
L8C0297F0: // F232
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,2u); goto L8C0297F2;
L8C0297F2: // FC25
    fpu_compare_greater(cpu,2u,12u); goto L8C0297F4;
L8C0297F4: // 8F02
    branch=!cpu.t; fpu_binary(cpu,FpuBinaryOperation::Subtract,1u,4u); if (branch) goto L8C0297FC; goto L8C0297F8;
L8C0297F8: // A089
    ; goto L8C02990E;
L8C0297FC: // E004
    cpu.r[0]=0x00000004u; goto L8C0297FE;
L8C0297FE: // F3E6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[14]); goto L8C029800;
L8C029800: // E02C
    cpu.r[0]=0x0000002Cu; goto L8C029802;
L8C029802: // F2F6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[15]); goto L8C029804;
L8C029804: // E008
    cpu.r[0]=0x00000008u; goto L8C029806;
L8C029806: // F232
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,2u); goto L8C029808;
L8C029808: // F3E6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[14]); goto L8C02980A;
L8C02980A: // E030
    cpu.r[0]=0x00000030u; goto L8C02980C;
L8C02980C: // F1F6
    cpu.fr[1]=load(cpu.r[0]+cpu.r[15]); goto L8C02980E;
L8C02980E: // E00C
    cpu.r[0]=0x0000000Cu; goto L8C029810;
L8C029810: // F132
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,1u); goto L8C029812;
L8C029812: // F24D
    fpu_negate(cpu,2u); goto L8C029814;
L8C029814: // F3F6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[15]); goto L8C029816;
L8C029816: // E004
    cpu.r[0]=0x00000004u; goto L8C029818;
L8C029818: // F211
    fpu_binary(cpu,FpuBinaryOperation::Subtract,1u,2u); goto L8C02981A;
L8C02981A: // F241
    fpu_binary(cpu,FpuBinaryOperation::Subtract,4u,2u); goto L8C02981C;
L8C02981C: // F233
    fpu_binary(cpu,FpuBinaryOperation::Divide,3u,2u); goto L8C02981E;
L8C02981E: // FF27
    store(0x8C02981Eu,cpu.r[0]+cpu.r[15],cpu.fr[2],CodeWriteSource::Fpu); goto L8C029820;
L8C029820: // F3E8
    cpu.fr[3]=load(cpu.r[14]); goto L8C029822;
L8C029822: // F325
    fpu_compare_greater(cpu,2u,3u); goto L8C029824;
L8C029824: // 8F0E
    branch=!cpu.t; cpu.r[8]=cpu.r[11]; if (branch) goto L8C029844; goto L8C029828;
L8C029828: // 64E3
    cpu.r[4]=cpu.r[14]; goto L8C02982A;
L8C02982A: // 740C
    cpu.r[4]+=0x0000000Cu; goto L8C02982C;
L8C02982C: // 6242
    cpu.r[2]=load(cpu.r[4]); goto L8C02982E;
L8C02982E: // 2228
    cpu.t=(cpu.r[2]&cpu.r[2])==0u; goto L8C029830;
L8C029830: // 8905
    if (cpu.t) goto L8C02983E; goto L8C029832;
L8C029832: // E010
    cpu.r[0]=0x00000010u; goto L8C029834;
L8C029834: // F346
    cpu.fr[3]=load(cpu.r[0]+cpu.r[4]); goto L8C029836;
L8C029836: // E004
    cpu.r[0]=0x00000004u; goto L8C029838;
L8C029838: // F2F6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[15]); goto L8C02983A;
L8C02983A: // F235
    fpu_compare_greater(cpu,3u,2u); goto L8C02983C;
L8C02983C: // 8B0F
    if (!cpu.t) goto L8C02985E; goto L8C02983E;
L8C02983E: // 68E3
    cpu.r[8]=cpu.r[14]; goto L8C029840;
L8C029840: // A00D
    cpu.r[8]+=0x0000000Cu; goto L8C02985E;
L8C029844: // 64E3
    cpu.r[4]=cpu.r[14]; goto L8C029846;
L8C029846: // 742C
    cpu.r[4]+=0x0000002Cu; goto L8C029848;
L8C029848: // 6342
    cpu.r[3]=load(cpu.r[4]); goto L8C02984A;
L8C02984A: // 2338
    cpu.t=(cpu.r[3]&cpu.r[3])==0u; goto L8C02984C;
L8C02984C: // 8905
    if (cpu.t) goto L8C02985A; goto L8C02984E;
L8C02984E: // E010
    cpu.r[0]=0x00000010u; goto L8C029850;
L8C029850: // F346
    cpu.fr[3]=load(cpu.r[0]+cpu.r[4]); goto L8C029852;
L8C029852: // E004
    cpu.r[0]=0x00000004u; goto L8C029854;
L8C029854: // F2F6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[15]); goto L8C029856;
L8C029856: // F325
    fpu_compare_greater(cpu,2u,3u); goto L8C029858;
L8C029858: // 8B01
    if (!cpu.t) goto L8C02985E; goto L8C02985A;
L8C02985A: // 68E3
    cpu.r[8]=cpu.r[14]; goto L8C02985C;
L8C02985C: // 782C
    cpu.r[8]+=0x0000002Cu; goto L8C02985E;
L8C02985E: // 2888
    cpu.t=(cpu.r[8]&cpu.r[8])==0u; goto L8C029860;
L8C029860: // 8955
    if (cpu.t) goto L8C02990E; goto L8C029862;
L8C029862: // D213
    cpu.r[2]=load(0x8C0298B0u); goto L8C029864;
L8C029864: // 64F3
    cpu.r[4]=cpu.r[15]; goto L8C029866;
L8C029866: // 420B
    { const auto target=cpu.r[2]; cpu.pr=0x8C02986Au; cpu.r[4]+=0x00000028u; call(target); } goto L8C02986A;
L8C02986A: // F38D
    cpu.fr[3]=0u; goto L8C02986C;
L8C02986C: // F034
    fpu_compare_equal(cpu,3u,0u); goto L8C02986E;
L8C02986E: // 8B27
    if (!cpu.t) goto L8C0298C0; goto L8C029870;
L8C029870: // 63F3
    cpu.r[3]=cpu.r[15]; goto L8C029872;
L8C029872: // 7328
    cpu.r[3]+=0x00000028u; goto L8C029874;
L8C029874: // E004
    cpu.r[0]=0x00000004u; goto L8C029876;
L8C029876: // 62F3
    cpu.r[2]=cpu.r[15]; goto L8C029878;
L8C029878: // 7228
    cpu.r[2]+=0x00000028u; goto L8C02987A;
L8C02987A: // F2FA
    store(0x8C02987Au,cpu.r[2],cpu.fr[15],CodeWriteSource::Fpu); goto L8C02987C;
L8C02987C: // F3D7
    store(0x8C02987Cu,cpu.r[0]+cpu.r[3],cpu.fr[13],CodeWriteSource::Fpu); goto L8C02987E;
L8C02987E: // 63F3
    cpu.r[3]=cpu.r[15]; goto L8C029880;
L8C029880: // 7328
    cpu.r[3]+=0x00000028u; goto L8C029882;
L8C029882: // E008
    cpu.r[0]=0x00000008u; goto L8C029884;
L8C029884: // F3F7
    store(0x8C029884u,cpu.r[0]+cpu.r[3],cpu.fr[15],CodeWriteSource::Fpu); goto L8C029886;
L8C029886: // A01F
    ; goto L8C0298C8;
L8C0298C0: // D287
    cpu.r[2]=load(0x8C029AE0u); goto L8C0298C2;
L8C0298C2: // 64F3
    cpu.r[4]=cpu.r[15]; goto L8C0298C4;
L8C0298C4: // 420B
    { const auto target=cpu.r[2]; cpu.pr=0x8C0298C8u; cpu.r[4]+=0x00000028u; call(target); } goto L8C0298C8;
L8C0298C8: // E004
    cpu.r[0]=0x00000004u; goto L8C0298CA;
L8C0298CA: // 1FC9
    store(0x8C0298CAu,cpu.r[15]+36u,cpu.r[12],CodeWriteSource::Cpu); goto L8C0298CC;
L8C0298CC: // 28C2
    store(0x8C0298CCu,cpu.r[8],cpu.r[12],CodeWriteSource::Cpu); goto L8C0298CE;
L8C0298CE: // 53F4
    cpu.r[3]=load(cpu.r[15]+16u); goto L8C0298D0;
L8C0298D0: // 1831
    store(0x8C0298D0u,cpu.r[8]+4u,cpu.r[3],CodeWriteSource::Cpu); goto L8C0298D2;
L8C0298D2: // 63F3
    cpu.r[3]=cpu.r[15]; goto L8C0298D4;
L8C0298D4: // 7328
    cpu.r[3]+=0x00000028u; goto L8C0298D6;
L8C0298D6: // F3F6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[15]); goto L8C0298D8;
L8C0298D8: // E010
    cpu.r[0]=0x00000010u; goto L8C0298DA;
L8C0298DA: // F837
    store(0x8C0298DAu,cpu.r[0]+cpu.r[8],cpu.fr[3],CodeWriteSource::Fpu); goto L8C0298DC;
L8C0298DC: // E02C
    cpu.r[0]=0x0000002Cu; goto L8C0298DE;
L8C0298DE: // 6232
    cpu.r[2]=load(cpu.r[3]); goto L8C0298E0;
L8C0298E0: // 1825
    store(0x8C0298E0u,cpu.r[8]+20u,cpu.r[2],CodeWriteSource::Cpu); goto L8C0298E2;
L8C0298E2: // 5231
    cpu.r[2]=load(cpu.r[3]+4u); goto L8C0298E4;
L8C0298E4: // 1826
    store(0x8C0298E4u,cpu.r[8]+24u,cpu.r[2],CodeWriteSource::Cpu); goto L8C0298E6;
L8C0298E6: // 5232
    cpu.r[2]=load(cpu.r[3]+8u); goto L8C0298E8;
L8C0298E8: // D37E
    cpu.r[3]=load(0x8C029AE4u); goto L8C0298EA;
L8C0298EA: // 1827
    store(0x8C0298EAu,cpu.r[8]+28u,cpu.r[2],CodeWriteSource::Cpu); goto L8C0298EC;
L8C0298EC: // F5F6
    cpu.fr[5]=load(cpu.r[0]+cpu.r[15]); goto L8C0298EE;
L8C0298EE: // E028
    cpu.r[0]=0x00000028u; goto L8C0298F0;
L8C0298F0: // 430B
    { const auto target=cpu.r[3]; cpu.pr=0x8C0298F4u; cpu.fr[4]=load(cpu.r[0]+cpu.r[15]); call(target); } goto L8C0298F4;
L8C0298F4: // F0E2
    fpu_binary(cpu,FpuBinaryOperation::Multiply,14u,0u); goto L8C0298F6;
L8C0298F6: // D37C
    cpu.r[3]=load(0x8C029AE8u); goto L8C0298F8;
L8C0298F8: // E030
    cpu.r[0]=0x00000030u; goto L8C0298FA;
L8C0298FA: // F03D
    fpu_truncate_to_fpul(cpu,0u); goto L8C0298FC;
L8C0298FC: // 025A
    cpu.r[2]=cpu.fpul; goto L8C0298FE;
L8C0298FE: // 622B
    cpu.r[2]=0u-cpu.r[2]; goto L8C029900;
L8C029900: // 1823
    store(0x8C029900u,cpu.r[8]+12u,cpu.r[2],CodeWriteSource::Cpu); goto L8C029902;
L8C029902: // 430B
    { const auto target=cpu.r[3]; cpu.pr=0x8C029906u; cpu.fr[4]=load(cpu.r[0]+cpu.r[15]); call(target); } goto L8C029906;
L8C029906: // F0E2
    fpu_binary(cpu,FpuBinaryOperation::Multiply,14u,0u); goto L8C029908;
L8C029908: // F03D
    fpu_truncate_to_fpul(cpu,0u); goto L8C02990A;
L8C02990A: // 025A
    cpu.r[2]=cpu.fpul; goto L8C02990C;
L8C02990C: // 1822
    store(0x8C02990Cu,cpu.r[8]+8u,cpu.r[2],CodeWriteSource::Cpu); goto L8C02990E;
L8C02990E: // E008
    cpu.r[0]=0x00000008u; goto L8C029910;
L8C029910: // F8E8
    cpu.fr[8]=load(cpu.r[14]); goto L8C029912;
L8C029912: // F596
    cpu.fr[5]=load(cpu.r[0]+cpu.r[9]); goto L8C029914;
L8C029914: // F6D8
    cpu.fr[6]=load(cpu.r[13]); goto L8C029916;
L8C029916: // F7D6
    cpu.fr[7]=load(cpu.r[0]+cpu.r[13]); goto L8C029918;
L8C029918: // F28C
    cpu.fr[2]=cpu.fr[8]; goto L8C02991A;
L8C02991A: // F261
    fpu_binary(cpu,FpuBinaryOperation::Subtract,6u,2u); goto L8C02991C;
L8C02991C: // F35C
    cpu.fr[3]=cpu.fr[5]; goto L8C02991E;
L8C02991E: // F371
    fpu_binary(cpu,FpuBinaryOperation::Subtract,7u,3u); goto L8C029920;
L8C029920: // FAE6
    cpu.fr[10]=load(cpu.r[0]+cpu.r[14]); goto L8C029922;
L8C029922: // F998
    cpu.fr[9]=load(cpu.r[9]); goto L8C029924;
L8C029924: // FBA6
    cpu.fr[11]=load(cpu.r[0]+cpu.r[10]); goto L8C029926;
L8C029926: // E018
    cpu.r[0]=0x00000018u; goto L8C029928;
L8C029928: // F322
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,3u); goto L8C02992A;
L8C02992A: // F2AC
    cpu.fr[2]=cpu.fr[10]; goto L8C02992C;
L8C02992C: // F19C
    cpu.fr[1]=cpu.fr[9]; goto L8C02992E;
L8C02992E: // F271
    fpu_binary(cpu,FpuBinaryOperation::Subtract,7u,2u); goto L8C029930;
L8C029930: // F161
    fpu_binary(cpu,FpuBinaryOperation::Subtract,6u,1u); goto L8C029932;
L8C029932: // F43C
    cpu.fr[4]=cpu.fr[3]; goto L8C029934;
L8C029934: // F3BC
    cpu.fr[3]=cpu.fr[11]; goto L8C029936;
L8C029936: // F351
    fpu_binary(cpu,FpuBinaryOperation::Subtract,5u,3u); goto L8C029938;
L8C029938: // F122
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,1u); goto L8C02993A;
L8C02993A: // F28C
    cpu.fr[2]=cpu.fr[8]; goto L8C02993C;
L8C02993C: // F291
    fpu_binary(cpu,FpuBinaryOperation::Subtract,9u,2u); goto L8C02993E;
L8C02993E: // F411
    fpu_binary(cpu,FpuBinaryOperation::Subtract,1u,4u); goto L8C029940;
L8C029940: // F1A8
    cpu.fr[1]=load(cpu.r[10]); goto L8C029942;
L8C029942: // F322
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,3u); goto L8C029944;
L8C029944: // F2AC
    cpu.fr[2]=cpu.fr[10]; goto L8C029946;
L8C029946: // FF17
    store(0x8C029946u,cpu.r[0]+cpu.r[15],cpu.fr[1],CodeWriteSource::Fpu); goto L8C029948;
L8C029948: // F251
    fpu_binary(cpu,FpuBinaryOperation::Subtract,5u,2u); goto L8C02994A;
L8C02994A: // F191
    fpu_binary(cpu,FpuBinaryOperation::Subtract,9u,1u); goto L8C02994C;
L8C02994C: // F122
    fpu_binary(cpu,FpuBinaryOperation::Multiply,2u,1u); goto L8C02994E;
L8C02994E: // E018
    cpu.r[0]=0x00000018u; goto L8C029950;
L8C029950: // F53C
    cpu.fr[5]=cpu.fr[3]; goto L8C029952;
L8C029952: // F3F6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[15]); goto L8C029954;
L8C029954: // FAB1
    fpu_binary(cpu,FpuBinaryOperation::Subtract,11u,10u); goto L8C029956;
L8C029956: // F7B1
    fpu_binary(cpu,FpuBinaryOperation::Subtract,11u,7u); goto L8C029958;
L8C029958: // F24C
    cpu.fr[2]=cpu.fr[4]; goto L8C02995A;
L8C02995A: // F631
    fpu_binary(cpu,FpuBinaryOperation::Subtract,3u,6u); goto L8C02995C;
L8C02995C: // F831
    fpu_binary(cpu,FpuBinaryOperation::Subtract,3u,8u); goto L8C02995E;
L8C02995E: // F511
    fpu_binary(cpu,FpuBinaryOperation::Subtract,1u,5u); goto L8C029960;
L8C029960: // F6A2
    fpu_binary(cpu,FpuBinaryOperation::Multiply,10u,6u); goto L8C029962;
L8C029962: // F782
    fpu_binary(cpu,FpuBinaryOperation::Multiply,8u,7u); goto L8C029964;
L8C029964: // F252
    fpu_binary(cpu,FpuBinaryOperation::Multiply,5u,2u); goto L8C029966;
L8C029966: // F36C
    cpu.fr[3]=cpu.fr[6]; goto L8C029968;
L8C029968: // F67C
    cpu.fr[6]=cpu.fr[7]; goto L8C02996A;
L8C02996A: // F631
    fpu_binary(cpu,FpuBinaryOperation::Subtract,3u,6u); goto L8C02996C;
L8C02996C: // F38D
    cpu.fr[3]=0u; goto L8C02996E;
L8C02996E: // F325
    fpu_compare_greater(cpu,2u,3u); goto L8C029970;
L8C029970: // 8B01
    if (!cpu.t) goto L8C029976; goto L8C029972;
L8C029972: // A094
    ; goto L8C029A9E;
L8C029976: // F25C
    cpu.fr[2]=cpu.fr[5]; goto L8C029978;
L8C029978: // F262
    fpu_binary(cpu,FpuBinaryOperation::Multiply,6u,2u); goto L8C02997A;
L8C02997A: // F325
    fpu_compare_greater(cpu,2u,3u); goto L8C02997C;
L8C02997C: // 8B01
    if (!cpu.t) goto L8C029982; goto L8C02997E;
L8C02997E: // A08E
    ; goto L8C029A9E;
L8C029982: // F34C
    cpu.fr[3]=cpu.fr[4]; goto L8C029984;
L8C029984: // F362
    fpu_binary(cpu,FpuBinaryOperation::Multiply,6u,3u); goto L8C029986;
L8C029986: // F28D
    cpu.fr[2]=0u; goto L8C029988;
L8C029988: // F235
    fpu_compare_greater(cpu,3u,2u); goto L8C02998A;
L8C02998A: // 8B01
    if (!cpu.t) goto L8C029990; goto L8C02998C;
L8C02998C: // A087
    ; goto L8C029A9E;
L8C029990: // D356
    cpu.r[3]=load(0x8C029AECu); goto L8C029992;
L8C029992: // 67F3
    cpu.r[7]=cpu.r[15]; goto L8C029994;
L8C029994: // 6593
    cpu.r[5]=cpu.r[9]; goto L8C029996;
L8C029996: // 7728
    cpu.r[7]+=0x00000028u; goto L8C029998;
L8C029998: // 66A3
    cpu.r[6]=cpu.r[10]; goto L8C02999A;
L8C02999A: // 430B
    { const auto target=cpu.r[3]; cpu.pr=0x8C02999Eu; cpu.r[4]=cpu.r[13]; call(target); } goto L8C02999E;
L8C02999E: // E028
    cpu.r[0]=0x00000028u; goto L8C0299A0;
L8C0299A0: // F3D8
    cpu.fr[3]=load(cpu.r[13]); goto L8C0299A2;
L8C0299A2: // F6F6
    cpu.fr[6]=load(cpu.r[0]+cpu.r[15]); goto L8C0299A4;
L8C0299A4: // E02C
    cpu.r[0]=0x0000002Cu; goto L8C0299A6;
L8C0299A6: // F4F6
    cpu.fr[4]=load(cpu.r[0]+cpu.r[15]); goto L8C0299A8;
L8C0299A8: // E004
    cpu.r[0]=0x00000004u; goto L8C0299AA;
L8C0299AA: // F26C
    cpu.fr[2]=cpu.fr[6]; goto L8C0299AC;
L8C0299AC: // F232
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,2u); goto L8C0299AE;
L8C0299AE: // F3D6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[13]); goto L8C0299B0;
L8C0299B0: // E008
    cpu.r[0]=0x00000008u; goto L8C0299B2;
L8C0299B2: // F14C
    cpu.fr[1]=cpu.fr[4]; goto L8C0299B4;
L8C0299B4: // F132
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,1u); goto L8C0299B6;
L8C0299B6: // F3D6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[13]); goto L8C0299B8;
L8C0299B8: // F24D
    fpu_negate(cpu,2u); goto L8C0299BA;
L8C0299BA: // E030
    cpu.r[0]=0x00000030u; goto L8C0299BC;
L8C0299BC: // F211
    fpu_binary(cpu,FpuBinaryOperation::Subtract,1u,2u); goto L8C0299BE;
L8C0299BE: // F1F6
    cpu.fr[1]=load(cpu.r[0]+cpu.r[15]); goto L8C0299C0;
L8C0299C0: // F132
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,1u); goto L8C0299C2;
L8C0299C2: // F34C
    cpu.fr[3]=cpu.fr[4]; goto L8C0299C4;
L8C0299C4: // F342
    fpu_binary(cpu,FpuBinaryOperation::Multiply,4u,3u); goto L8C0299C6;
L8C0299C6: // F52C
    cpu.fr[5]=cpu.fr[2]; goto L8C0299C8;
L8C0299C8: // FC35
    fpu_compare_greater(cpu,3u,12u); goto L8C0299CA;
L8C0299CA: // 8D68
    branch=cpu.t; fpu_binary(cpu,FpuBinaryOperation::Subtract,1u,5u); if (branch) goto L8C029A9E; goto L8C0299CE;
L8C0299CE: // E008
    cpu.r[0]=0x00000008u; goto L8C0299D0;
L8C0299D0: // F3E6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[14]); goto L8C0299D2;
L8C0299D2: // E030
    cpu.r[0]=0x00000030u; goto L8C0299D4;
L8C0299D4: // F2F6
    cpu.fr[2]=load(cpu.r[0]+cpu.r[15]); goto L8C0299D6;
L8C0299D6: // E02C
    cpu.r[0]=0x0000002Cu; goto L8C0299D8;
L8C0299D8: // F232
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,2u); goto L8C0299DA;
L8C0299DA: // F3E8
    cpu.fr[3]=load(cpu.r[14]); goto L8C0299DC;
L8C0299DC: // F632
    fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,6u); goto L8C0299DE;
L8C0299DE: // F3F6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[15]); goto L8C0299E0;
L8C0299E0: // E004
    cpu.r[0]=0x00000004u; goto L8C0299E2;
L8C0299E2: // F24D
    fpu_negate(cpu,2u); goto L8C0299E4;
L8C0299E4: // F261
    fpu_binary(cpu,FpuBinaryOperation::Subtract,6u,2u); goto L8C0299E6;
L8C0299E6: // F251
    fpu_binary(cpu,FpuBinaryOperation::Subtract,5u,2u); goto L8C0299E8;
L8C0299E8: // F233
    fpu_binary(cpu,FpuBinaryOperation::Divide,3u,2u); goto L8C0299EA;
L8C0299EA: // FF2A
    store(0x8C0299EAu,cpu.r[15],cpu.fr[2],CodeWriteSource::Fpu); goto L8C0299EC;
L8C0299EC: // F3E6
    cpu.fr[3]=load(cpu.r[0]+cpu.r[14]); goto L8C0299EE;
L8C0299EE: // F325
    fpu_compare_greater(cpu,2u,3u); goto L8C0299F0;
L8C0299F0: // 8F0D
    branch=!cpu.t; cpu.r[8]=cpu.r[11]; if (branch) goto L8C029A0E; goto L8C0299F4;
L8C0299F4: // 64E3
    cpu.r[4]=cpu.r[14]; goto L8C0299F6;
L8C0299F6: // 744C
    cpu.r[4]+=0x0000004Cu; goto L8C0299F8;
L8C0299F8: // 6242
    cpu.r[2]=load(cpu.r[4]); goto L8C0299FA;
L8C0299FA: // 2228
    cpu.t=(cpu.r[2]&cpu.r[2])==0u; goto L8C0299FC;
L8C0299FC: // 8904
    if (cpu.t) goto L8C029A08; goto L8C0299FE;
L8C0299FE: // E010
    cpu.r[0]=0x00000010u; goto L8C029A00;
L8C029A00: // F2F8
    cpu.fr[2]=load(cpu.r[15]); goto L8C029A02;
L8C029A02: // F346
    cpu.fr[3]=load(cpu.r[0]+cpu.r[4]); goto L8C029A04;
L8C029A04: // F235
    fpu_compare_greater(cpu,3u,2u); goto L8C029A06;
L8C029A06: // 8B0E
    if (!cpu.t) goto L8C029A26; goto L8C029A08;
L8C029A08: // 68E3
    cpu.r[8]=cpu.r[14]; goto L8C029A0A;
L8C029A0A: // A00C
    cpu.r[8]+=0x0000004Cu; goto L8C029A26;
L8C029A0E: // 64E3
    cpu.r[4]=cpu.r[14]; goto L8C029A10;
L8C029A10: // 746C
    cpu.r[4]+=0x0000006Cu; goto L8C029A12;
L8C029A12: // 6342
    cpu.r[3]=load(cpu.r[4]); goto L8C029A14;
L8C029A14: // 2338
    cpu.t=(cpu.r[3]&cpu.r[3])==0u; goto L8C029A16;
L8C029A16: // 8904
    if (cpu.t) goto L8C029A22; goto L8C029A18;
L8C029A18: // E010
    cpu.r[0]=0x00000010u; goto L8C029A1A;
L8C029A1A: // F2F8
    cpu.fr[2]=load(cpu.r[15]); goto L8C029A1C;
L8C029A1C: // F346
    cpu.fr[3]=load(cpu.r[0]+cpu.r[4]); goto L8C029A1E;
L8C029A1E: // F325
    fpu_compare_greater(cpu,2u,3u); goto L8C029A20;
L8C029A20: // 8B01
    if (!cpu.t) goto L8C029A26; goto L8C029A22;
L8C029A22: // 68E3
    cpu.r[8]=cpu.r[14]; goto L8C029A24;
L8C029A24: // 786C
    cpu.r[8]+=0x0000006Cu; goto L8C029A26;
L8C029A26: // 2888
    cpu.t=(cpu.r[8]&cpu.r[8])==0u; goto L8C029A28;
L8C029A28: // 8939
    if (cpu.t) goto L8C029A9E; goto L8C029A2A;
L8C029A2A: // D231
    cpu.r[2]=load(0x8C029AF0u); goto L8C029A2C;
L8C029A2C: // 64F3
    cpu.r[4]=cpu.r[15]; goto L8C029A2E;
L8C029A2E: // 420B
    { const auto target=cpu.r[2]; cpu.pr=0x8C029A32u; cpu.r[4]+=0x00000028u; call(target); } goto L8C029A32;
L8C029A32: // F38D
    cpu.fr[3]=0u; goto L8C029A34;
L8C029A34: // F034
    fpu_compare_equal(cpu,3u,0u); goto L8C029A36;
L8C029A36: // 8B0C
    if (!cpu.t) goto L8C029A52; goto L8C029A38;
L8C029A38: // 63F3
    cpu.r[3]=cpu.r[15]; goto L8C029A3A;
L8C029A3A: // 7328
    cpu.r[3]+=0x00000028u; goto L8C029A3C;
L8C029A3C: // E004
    cpu.r[0]=0x00000004u; goto L8C029A3E;
L8C029A3E: // 62F3
    cpu.r[2]=cpu.r[15]; goto L8C029A40;
L8C029A40: // 7228
    cpu.r[2]+=0x00000028u; goto L8C029A42;
L8C029A42: // F2FA
    store(0x8C029A42u,cpu.r[2],cpu.fr[15],CodeWriteSource::Fpu); goto L8C029A44;
L8C029A44: // F3D7
    store(0x8C029A44u,cpu.r[0]+cpu.r[3],cpu.fr[13],CodeWriteSource::Fpu); goto L8C029A46;
L8C029A46: // 63F3
    cpu.r[3]=cpu.r[15]; goto L8C029A48;
L8C029A48: // 7328
    cpu.r[3]+=0x00000028u; goto L8C029A4A;
L8C029A4A: // E008
    cpu.r[0]=0x00000008u; goto L8C029A4C;
L8C029A4C: // F3F7
    store(0x8C029A4Cu,cpu.r[0]+cpu.r[3],cpu.fr[15],CodeWriteSource::Fpu); goto L8C029A4E;
L8C029A4E: // A004
    ; goto L8C029A5A;
L8C029A52: // D223
    cpu.r[2]=load(0x8C029AE0u); goto L8C029A54;
L8C029A54: // 64F3
    cpu.r[4]=cpu.r[15]; goto L8C029A56;
L8C029A56: // 420B
    { const auto target=cpu.r[2]; cpu.pr=0x8C029A5Au; cpu.r[4]+=0x00000028u; call(target); } goto L8C029A5A;
L8C029A5A: // E010
    cpu.r[0]=0x00000010u; goto L8C029A5C;
L8C029A5C: // 1FC9
    store(0x8C029A5Cu,cpu.r[15]+36u,cpu.r[12],CodeWriteSource::Cpu); goto L8C029A5E;
L8C029A5E: // 28C2
    store(0x8C029A5Eu,cpu.r[8],cpu.r[12],CodeWriteSource::Cpu); goto L8C029A60;
L8C029A60: // 53F4
    cpu.r[3]=load(cpu.r[15]+16u); goto L8C029A62;
L8C029A62: // 1831
    store(0x8C029A62u,cpu.r[8]+4u,cpu.r[3],CodeWriteSource::Cpu); goto L8C029A64;
L8C029A64: // 63F3
    cpu.r[3]=cpu.r[15]; goto L8C029A66;
L8C029A66: // 7328
    cpu.r[3]+=0x00000028u; goto L8C029A68;
L8C029A68: // F3F8
    cpu.fr[3]=load(cpu.r[15]); goto L8C029A6A;
L8C029A6A: // F837
    store(0x8C029A6Au,cpu.r[0]+cpu.r[8],cpu.fr[3],CodeWriteSource::Fpu); goto L8C029A6C;
L8C029A6C: // E02C
    cpu.r[0]=0x0000002Cu; goto L8C029A6E;
L8C029A6E: // 6232
    cpu.r[2]=load(cpu.r[3]); goto L8C029A70;
L8C029A70: // 1825
    store(0x8C029A70u,cpu.r[8]+20u,cpu.r[2],CodeWriteSource::Cpu); goto L8C029A72;
L8C029A72: // 5231
    cpu.r[2]=load(cpu.r[3]+4u); goto L8C029A74;
L8C029A74: // 1826
    store(0x8C029A74u,cpu.r[8]+24u,cpu.r[2],CodeWriteSource::Cpu); goto L8C029A76;
L8C029A76: // 5232
    cpu.r[2]=load(cpu.r[3]+8u); goto L8C029A78;
L8C029A78: // D31A
    cpu.r[3]=load(0x8C029AE4u); goto L8C029A7A;
L8C029A7A: // 1827
    store(0x8C029A7Au,cpu.r[8]+28u,cpu.r[2],CodeWriteSource::Cpu); goto L8C029A7C;
L8C029A7C: // F5F6
    cpu.fr[5]=load(cpu.r[0]+cpu.r[15]); goto L8C029A7E;
L8C029A7E: // E028
    cpu.r[0]=0x00000028u; goto L8C029A80;
L8C029A80: // 430B
    { const auto target=cpu.r[3]; cpu.pr=0x8C029A84u; cpu.fr[4]=load(cpu.r[0]+cpu.r[15]); call(target); } goto L8C029A84;
L8C029A84: // F0E2
    fpu_binary(cpu,FpuBinaryOperation::Multiply,14u,0u); goto L8C029A86;
L8C029A86: // D318
    cpu.r[3]=load(0x8C029AE8u); goto L8C029A88;
L8C029A88: // E030
    cpu.r[0]=0x00000030u; goto L8C029A8A;
L8C029A8A: // F03D
    fpu_truncate_to_fpul(cpu,0u); goto L8C029A8C;
L8C029A8C: // 025A
    cpu.r[2]=cpu.fpul; goto L8C029A8E;
L8C029A8E: // 622B
    cpu.r[2]=0u-cpu.r[2]; goto L8C029A90;
L8C029A90: // 1823
    store(0x8C029A90u,cpu.r[8]+12u,cpu.r[2],CodeWriteSource::Cpu); goto L8C029A92;
L8C029A92: // 430B
    { const auto target=cpu.r[3]; cpu.pr=0x8C029A96u; cpu.fr[4]=load(cpu.r[0]+cpu.r[15]); call(target); } goto L8C029A96;
L8C029A96: // F0E2
    fpu_binary(cpu,FpuBinaryOperation::Multiply,14u,0u); goto L8C029A98;
L8C029A98: // F03D
    fpu_truncate_to_fpul(cpu,0u); goto L8C029A9A;
L8C029A9A: // 025A
    cpu.r[2]=cpu.fpul; goto L8C029A9C;
L8C029A9C: // 1822
    store(0x8C029A9Cu,cpu.r[8]+8u,cpu.r[2],CodeWriteSource::Cpu); goto L8C029A9E;
L8C029A9E: // 53F5
    cpu.r[3]=load(cpu.r[15]+20u); goto L8C029AA0;
L8C029AA0: // 6232
    cpu.r[2]=load(cpu.r[3]); goto L8C029AA2;
L8C029AA2: // 1F25
    store(0x8C029AA2u,cpu.r[15]+20u,cpu.r[2],CodeWriteSource::Cpu); goto L8C029AA4;
L8C029AA4: // 53F5
    cpu.r[3]=load(cpu.r[15]+20u); goto L8C029AA6;
L8C029AA6: // 2338
    cpu.t=(cpu.r[3]&cpu.r[3])==0u; goto L8C029AA8;
L8C029AA8: // 8901
    if (cpu.t) goto L8C029AAE; goto L8C029AAA;
L8C029AAA: // AD19
    ; goto L8C0294E0;
L8C029AAE: // 50F9
    cpu.r[0]=load(cpu.r[15]+36u); goto L8C029AB0;
L8C029AB0: // 7F34
    cpu.r[15]+=0x00000034u; goto L8C029AB2;
L8C029AB2: // 4F26
    cpu.pr=load(cpu.r[15]); cpu.r[15]+=4u; goto L8C029AB4;
L8C029AB4: // FCF9
    cpu.fr[12]=load(cpu.r[15]); cpu.r[15]+=4u; goto L8C029AB6;
L8C029AB6: // FDF9
    cpu.fr[13]=load(cpu.r[15]); cpu.r[15]+=4u; goto L8C029AB8;
L8C029AB8: // FEF9
    cpu.fr[14]=load(cpu.r[15]); cpu.r[15]+=4u; goto L8C029ABA;
L8C029ABA: // FFF9
    cpu.fr[15]=load(cpu.r[15]); cpu.r[15]+=4u; goto L8C029ABC;
L8C029ABC: // 68F6
    { const auto a=cpu.r[15]; cpu.r[8]=load(a); cpu.r[15]+=4u; } goto L8C029ABE;
L8C029ABE: // 69F6
    { const auto a=cpu.r[15]; cpu.r[9]=load(a); cpu.r[15]+=4u; } goto L8C029AC0;
L8C029AC0: // 6AF6
    { const auto a=cpu.r[15]; cpu.r[10]=load(a); cpu.r[15]+=4u; } goto L8C029AC2;
L8C029AC2: // 6BF6
    { const auto a=cpu.r[15]; cpu.r[11]=load(a); cpu.r[15]+=4u; } goto L8C029AC4;
L8C029AC4: // 6CF6
    { const auto a=cpu.r[15]; cpu.r[12]=load(a); cpu.r[15]+=4u; } goto L8C029AC6;
L8C029AC6: // 6DF6
    { const auto a=cpu.r[15]; cpu.r[13]=load(a); cpu.r[15]+=4u; } goto L8C029AC8;
L8C029AC8: // 000B
    { const auto ret=cpu.pr; { const auto a=cpu.r[15]; cpu.r[14]=load(a); cpu.r[15]+=4u; } cpu.pc=ret; return true; }
}
} // namespace sonic::triangle_contacts
