#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace sonic_native_private::frame_completion {

// PAL owner 0x8C051810 uses major*32 + minor*4. The next independently
// referenced data object starts at 0x8C19E8A4: all 44 rows belong to this
// matrix, including adventure fields 32..43. Authoring and host dispatch
// share this definition so a truncated inventory cannot become a host cap.
inline constexpr std::uint32_t table = 0x8C19E324u;
inline constexpr std::uint32_t table_end = 0x8C19E8A4u;
inline constexpr std::uint32_t minor_count = 8u;
inline constexpr std::uint32_t entry_count = (table_end - table) / 4u;
inline constexpr std::uint32_t major_count = entry_count / minor_count;
inline constexpr std::string_view table_identity =
    "sha256:049d7e2ed5fd5ca1d474dde70b825b045fa8165fca3315f44f3e22e1e6e98528";
inline constexpr std::uint32_t noop = 0x8C05180Cu;
inline constexpr std::uint32_t state_a = 0x8C051900u;
inline constexpr std::uint32_t state_b = 0x8C051928u;
inline constexpr std::uint32_t state_c = 0x8C05193Au;
inline constexpr std::uint32_t state_d = 0x8C05194Cu;

[[nodiscard]] constexpr std::optional<std::uint32_t> index(
    const std::int16_t major_state, const std::int16_t minor_state) noexcept {
    // Preserve the signed-word getter followed by the owner's unsigned-byte
    // extraction, including sign extension from a negative minor word.
    const auto combined =
        (static_cast<std::uint32_t>(static_cast<std::int32_t>(major_state)) << 8u) |
        static_cast<std::uint32_t>(static_cast<std::int32_t>(minor_state));
    const auto major = (combined >> 8u) & 0xFFu;
    const auto minor = combined & 0xFFu;
    if (major >= major_count || minor >= minor_count) return std::nullopt;
    return major * minor_count + minor;
}

static_assert(major_count == 44u && entry_count == 352u);
static_assert(index(33, 0) == 264u); // Both reported train-transition stops.
static_assert(index(33, 2) == 266u); // Fifth fog-state callback.
static_assert(index(43, 7) == 351u);
static_assert(!index(44, 0).has_value());
static_assert(!index(0, 8).has_value());
static_assert(!index(-1, 0).has_value());
static_assert(!index(0, -1).has_value());

} // namespace sonic_native_private::frame_completion
