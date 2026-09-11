#pragma once
#include "sonic_presentation.hpp"
#include "katana/runtime/native_port.hpp"
#include <cstdint>

namespace sonic::language {
// PAL v1.003: three 0x4A0-byte records. Only these option masks are ours.
inline constexpr std::uint32_t records = 0x8C7988E0u;
inline constexpr std::uint32_t selected_record = 0x8C161BE4u;
inline constexpr std::uint32_t record_bytes = 0x4A0u;
inline constexpr std::uint32_t options_offset = 0x251u;
inline constexpr std::uint32_t text_global = 0x8C754B3Cu;
inline constexpr std::uint32_t voice_global = 0x8C754B40u;
inline constexpr std::uint32_t subtitles_global = 0x8C788B50u;
[[nodiscard]] std::uint8_t save_options(std::uint8_t original,
    const presentation::Settings& settings) noexcept;
}

extern "C" {
katana::runtime::NativePortHookResult sonic_language_initial(katana::runtime::NativePortContext&) noexcept;
katana::runtime::NativePortHookResult sonic_language_text(katana::runtime::NativePortContext&) noexcept;
katana::runtime::NativePortHookResult sonic_language_voice(katana::runtime::NativePortContext&) noexcept;
katana::runtime::NativePortHookResult sonic_language_subtitles(katana::runtime::NativePortContext&) noexcept;
katana::runtime::NativePortHookResult sonic_language_subtitles_loaded(katana::runtime::NativePortContext&) noexcept;
katana::runtime::NativePortHookResult sonic_language_save(katana::runtime::NativePortContext&) noexcept;
katana::runtime::NativePortHookResult sonic_language_loaded(katana::runtime::NativePortContext&) noexcept;
}
