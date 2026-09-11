#pragma once

#include "katana/runtime/native_port_sound_bank.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>

namespace sonic::native_qsound_reverb_medium {
namespace detail {

// Hardware semantics are implemented from SEGA's AICA (FQ8005) Sound-block
// User's Manual v1.00, "Configuration of DSP" and "Overview of DSP Program"
// (pp. 34-38). The immutable constants below come only from the private,
// SHA-bound Sonic SFPB payload. Emulator implementations are differential
// test oracles, never source, dependency, dispatch model, or runtime fallback.
inline constexpr std::uint32_t output_sample_rate = 44'100u;
inline constexpr std::size_t effect_bus_count = 16u;
inline constexpr std::size_t ring_word_count = 0x8000u;
inline constexpr std::size_t program_size = 0xC60u;
inline constexpr std::uint32_t authored_work_area_size = 0x10040u;
static_assert(authored_work_area_size ==
              0x40u + ring_word_count * sizeof(std::uint16_t));

#define SONIC_QSOUND_PROGRAM_CONSTANTS
#include "sonic_qsound_reverb_medium_program.inc"
#undef SONIC_QSOUND_PROGRAM_CONSTANTS

struct alignas(64) State final {
    std::array<std::int32_t, 128u> temporary{};
    std::array<std::int32_t, 32u> memory{};
    std::array<std::uint16_t, ring_word_count> ring{};
    std::uint32_t memory_decrement = 1u;
    // Make the cache-line-sized state extent explicit rather than relying on
    // compiler-added tail padding for alignas(64).
    std::array<std::byte, 60u> alignment_padding{};
};

struct FrameState final {
    std::int32_t accumulation = 0;
    std::int32_t shifter_output = 0;
    std::int32_t multiplier_input_24 = 0;
    std::int32_t multiplier_input_13 = 0;
    std::int32_t adder_input = 0;
    std::int32_t selected_input = 0;
    std::array<std::int32_t, 4u> delayed_memory_reads{};
    std::int32_t fraction_latch = 0;
    std::int32_t multiplier_latch = 0;
    std::uint32_t address_latch = 0u;
};

[[nodiscard]] constexpr std::int64_t arithmetic_shift_right(
    const std::int64_t value, const std::uint32_t amount) noexcept {
    if (amount == 0u) return value;
    if (value >= 0) return value >> amount;
    return -(((-value) + ((std::int64_t{1} << amount) - 1)) >> amount);
}

[[nodiscard]] constexpr std::int32_t wrap_signed_32(
    const std::int64_t value) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

[[nodiscard]] constexpr std::int32_t sign_extend_24(
    const std::uint32_t value) noexcept {
    auto bits = value & 0x00FF'FFFFu;
    if ((bits & 0x0080'0000u) != 0u) bits |= 0xFF00'0000u;
    return std::bit_cast<std::int32_t>(bits);
}

[[nodiscard]] constexpr std::int32_t sign_extend_16(
    const std::uint16_t value) noexcept {
    return value <= 0x7FFFu ? static_cast<std::int32_t>(value)
                            : static_cast<std::int32_t>(value) - 0x1'0000;
}

[[nodiscard]] constexpr std::uint16_t encode_ring_word(
    const std::int32_t value) noexcept {
    const auto bits = static_cast<std::uint32_t>(value) & 0x00FF'FFFFu;
    const auto sign = (bits >> 23u) & 1u;
    auto transition = (bits ^ (bits << 1u)) & 0x00FF'FFFFu;
    std::uint32_t exponent = 0u;
    while (exponent < 12u && (transition & 0x0080'0000u) == 0u) {
        transition = (transition << 1u) & 0x00FF'FFFFu;
        ++exponent;
    }
    const auto shift = std::min(exponent, 11u);
    const auto mantissa = ((bits << shift) >> 11u) & 0x7FFu;
    return static_cast<std::uint16_t>((sign << 15u) |
                                      (exponent << 11u) | mantissa);
}

[[nodiscard]] constexpr std::int32_t decode_ring_word(
    const std::uint16_t value) noexcept {
    const auto sign = (static_cast<std::uint32_t>(value) >> 15u) & 1u;
    auto exponent = (static_cast<std::uint32_t>(value) >> 11u) & 0xFu;
    auto unpacked = (static_cast<std::uint32_t>(value) & 0x7FFu) << 11u;
    unpacked |= sign << 22u;
    if (exponent > 11u)
        exponent = 11u;
    else
        unpacked ^= 1u << 22u;
    unpacked |= sign << 23u;
    return static_cast<std::int32_t>(
        arithmetic_shift_right(sign_extend_24(unpacked), exponent));
}

[[nodiscard]] inline std::uint32_t read_u32_le(
    const std::span<const std::byte> bytes,
    const std::size_t offset) noexcept {
    return std::to_integer<std::uint32_t>(bytes[offset]) |
           (std::to_integer<std::uint32_t>(bytes[offset + 1u]) << 8u) |
           (std::to_integer<std::uint32_t>(bytes[offset + 2u]) << 16u) |
           (std::to_integer<std::uint32_t>(bytes[offset + 3u]) << 24u);
}

[[nodiscard]] inline bool matches_program(
    const std::span<const std::byte> bytes) noexcept {
    if (bytes.size() != program_size) return false;
    for (std::size_t index = 0u; index < program_prefix.size(); ++index)
        if (std::to_integer<std::uint8_t>(bytes[index]) !=
            program_prefix[index])
            return false;
    for (std::size_t index = 0u; index < coefficients.size(); ++index)
        if (read_u32_le(bytes, 0x54u + index * 4u) != coefficients[index])
            return false;
    for (std::size_t index = 0u; index < memory_addresses.size(); ++index)
        if (read_u32_le(bytes, 0x254u + index * 4u) !=
            memory_addresses[index])
            return false;
    for (std::size_t index = 0x354u; index < 0x454u; ++index)
        if (bytes[index] != std::byte{0}) return false;
    for (std::size_t index = 0u; index < program_words.size(); ++index)
        if (read_u32_le(bytes, 0x454u + index * 4u) != program_words[index])
            return false;
    for (std::size_t index = 0u; index < program_suffix.size(); ++index)
        if (std::to_integer<std::uint8_t>(bytes[0xC54u + index]) !=
            program_suffix[index])
            return false;
    return true;
}

template <std::size_t Step,
          std::uint16_t Word0,
          std::uint16_t Word1,
          std::uint16_t Word2,
          std::uint16_t Word3>
inline void execute_step(State& state,
                         FrameState& frame,
                         const std::span<const std::int32_t, effect_bus_count>
                             input_buses,
                         const std::span<std::int32_t, effect_bus_count>
                             output_buses) noexcept {
    constexpr std::uint32_t temporary_read = (Word0 >> 9u) & 0x7Fu;
    constexpr bool temporary_write = (Word0 & 0x100u) != 0u;
    constexpr std::uint32_t temporary_write_address =
        (Word0 >> 1u) & 0x7Fu;
    constexpr bool x_select_input = (Word1 & 0x8000u) != 0u;
    constexpr std::uint32_t y_select = (Word1 >> 13u) & 3u;
    constexpr std::uint32_t input_read = (Word1 >> 7u) & 0x3Fu;
    constexpr bool input_write = (Word1 & 0x40u) != 0u;
    constexpr std::uint32_t input_write_address = (Word1 >> 1u) & 0x1Fu;
    constexpr bool table_address = (Word2 & 0x8000u) != 0u;
    constexpr bool memory_write = (Word2 & 0x4000u) != 0u;
    constexpr bool memory_read = (Word2 & 0x2000u) != 0u;
    constexpr bool effect_write = (Word2 & 0x1000u) != 0u;
    constexpr std::uint32_t effect_write_address =
        (Word2 >> 8u) & 0xFu;
    constexpr bool address_load = (Word2 & 0x80u) != 0u;
    constexpr bool fraction_load = (Word2 & 0x40u) != 0u;
    constexpr std::uint32_t shift_mode = (Word2 >> 4u) & 3u;
    constexpr bool y_register_load = (Word2 & 8u) != 0u;
    constexpr bool negate_b = (Word2 & 4u) != 0u;
    constexpr bool zero_b = (Word2 & 2u) != 0u;
    constexpr bool b_select_accumulator = (Word2 & 1u) != 0u;
    constexpr bool no_float = (Word3 & 0x8000u) != 0u;
    constexpr std::uint32_t memory_address_select =
        (Word3 >> 9u) & 0x3Fu;
    constexpr bool address_register_add = (Word3 & 0x100u) != 0u;
    constexpr bool next_address = (Word3 & 0x80u) != 0u;

    static_assert(!table_address,
                  "the identity-bound kernel has no table-memory access");
    static_assert(!no_float,
                  "the identity-bound kernel uses packed DSP memory only");
    static_assert(!(memory_read || memory_write) || (Step & 1u) != 0u,
                  "DSP memory access is only valid on odd steps");

    if constexpr (input_read <= 0x1Fu) {
        frame.selected_input = state.memory[input_read];
    } else if constexpr (input_read <= 0x2Fu) {
        frame.selected_input = wrap_signed_32(
            static_cast<std::int64_t>(input_buses[input_read - 0x20u]) *
            16);
    } else {
        // This exact title program does not bind the external CD input pair.
        frame.selected_input = 0;
    }

    if constexpr (input_write)
        state.memory[input_write_address] =
            frame.delayed_memory_reads[Step & 3u];

    if constexpr (zero_b) {
        frame.adder_input = 0;
    } else {
        frame.adder_input =
            b_select_accumulator
                ? frame.accumulation
                : state.temporary[(temporary_read +
                                   state.memory_decrement) &
                                  0x7Fu];
        if constexpr (negate_b)
            frame.adder_input = wrap_signed_32(
                -static_cast<std::int64_t>(frame.adder_input));
    }

    frame.multiplier_input_24 =
        x_select_input
            ? frame.selected_input
            : state.temporary[(temporary_read + state.memory_decrement) &
                              0x7Fu];
    if constexpr (y_select == 0u) {
        frame.multiplier_input_13 = frame.fraction_latch;
    } else if constexpr (y_select == 1u) {
        constexpr auto coefficient = sign_extend_16(coefficients[Step]);
        frame.multiplier_input_13 = static_cast<std::int32_t>(
            arithmetic_shift_right(coefficient, 3u));
    } else if constexpr (y_select == 2u) {
        frame.multiplier_input_13 = static_cast<std::int32_t>(
            arithmetic_shift_right(frame.multiplier_latch, 11u));
    } else {
        frame.multiplier_input_13 =
            static_cast<std::int32_t>(
                arithmetic_shift_right(frame.multiplier_latch, 4u)) &
            0x0FFF;
    }
    if constexpr (y_register_load)
        frame.multiplier_latch = frame.selected_input;

    if constexpr (shift_mode == 0u || shift_mode == 3u)
        frame.shifter_output = frame.accumulation;
    else
        frame.shifter_output = wrap_signed_32(
            static_cast<std::int64_t>(frame.accumulation) * 2);
    if constexpr (shift_mode < 2u)
        frame.shifter_output = std::clamp(
            frame.shifter_output, -0x0080'0000, 0x007F'FFFF);

    frame.accumulation = wrap_signed_32(
        arithmetic_shift_right(
            static_cast<std::int64_t>(frame.multiplier_input_24) *
                frame.multiplier_input_13,
            12u) +
        frame.adder_input);

    if constexpr (temporary_write)
        state.temporary[(temporary_write_address + state.memory_decrement) &
                        0x7Fu] = frame.shifter_output;

    if constexpr (fraction_load) {
        if constexpr (shift_mode == 3u)
            frame.fraction_latch = frame.shifter_output & 0x0FFF;
        else
            frame.fraction_latch = static_cast<std::int32_t>(
                arithmetic_shift_right(frame.shifter_output, 11u));
    }

    if constexpr (memory_read || memory_write) {
        auto address =
            static_cast<std::uint32_t>(memory_addresses[memory_address_select]);
        if constexpr (address_register_add)
            address += frame.address_latch & 0x0FFFu;
        if constexpr (next_address) ++address;
        address = (address + state.memory_decrement) &
                  static_cast<std::uint32_t>(ring_word_count - 1u);
        if constexpr (memory_read)
            frame.delayed_memory_reads[(Step + 2u) & 3u] =
                decode_ring_word(state.ring[address]);
        if constexpr (memory_write)
            state.ring[address] = encode_ring_word(frame.shifter_output);
    }

    if constexpr (address_load) {
        if constexpr (shift_mode == 3u)
            frame.address_latch = static_cast<std::uint32_t>(
                arithmetic_shift_right(frame.shifter_output, 12u));
        else
            frame.address_latch = static_cast<std::uint32_t>(
                arithmetic_shift_right(frame.selected_input, 16u));
    }

    if constexpr (effect_write) {
        const auto bits = static_cast<std::uint16_t>(
            arithmetic_shift_right(frame.shifter_output, 8u));
        output_buses[effect_write_address] =
            std::bit_cast<std::int16_t>(bits);
    }
}

inline bool initialize(const std::span<const std::byte> program_bytes,
                       const std::uint32_t sample_rate,
                       void* const state) noexcept {
    if (sample_rate != output_sample_rate || state == nullptr ||
        (reinterpret_cast<std::uintptr_t>(state) & (alignof(State) - 1u)) !=
            0u ||
        !matches_program(program_bytes))
        return false;
    std::construct_at(static_cast<State*>(state));
    return true;
}

inline void destroy(void* const state) noexcept {
    if (state != nullptr) std::destroy_at(static_cast<State*>(state));
}

inline bool render(void* const opaque_state,
                   const std::span<const std::int32_t> input_buses,
                   const std::span<std::int32_t> output_buses,
                   const std::uint32_t frame_count) noexcept {
    if (opaque_state == nullptr ||
        (reinterpret_cast<std::uintptr_t>(opaque_state) &
         (alignof(State) - 1u)) != 0u ||
        frame_count >
            std::numeric_limits<std::size_t>::max() / effect_bus_count)
        return false;
    const auto bus_values =
        static_cast<std::size_t>(frame_count) * effect_bus_count;
    if (input_buses.size() != bus_values ||
        output_buses.size() != bus_values)
        return false;
    for (const auto input : input_buses)
        if (input < -(1 << 19) || input > (1 << 19) - 1) return false;

    auto& state = *static_cast<State*>(opaque_state);
    for (std::uint32_t frame_index = 0u; frame_index < frame_count;
         ++frame_index) {
        const auto input =
            std::span<const std::int32_t, effect_bus_count>(
                input_buses.data() +
                    static_cast<std::size_t>(frame_index) * effect_bus_count,
                effect_bus_count);
        const auto output = std::span<std::int32_t, effect_bus_count>(
            output_buses.data() +
                static_cast<std::size_t>(frame_index) * effect_bus_count,
            effect_bus_count);
        std::fill(output.begin(), output.end(), 0);
        FrameState frame;
#define SONIC_QSOUND_PROGRAM_STEPS
#define SONIC_QSOUND_STEP(step, word0, word1, word2, word3)                 \
    execute_step<step, word0, word1, word2, word3>(state, frame, input,     \
                                                   output);
#include "sonic_qsound_reverb_medium_program.inc"
#undef SONIC_QSOUND_STEP
#undef SONIC_QSOUND_PROGRAM_STEPS
        --state.memory_decrement;
        if (state.memory_decrement == 0u)
            state.memory_decrement = static_cast<std::uint32_t>(ring_word_count);
    }
    return true;
}

// Persist only the authored kernel's semantic fields, never alignment padding
// or an opaque C++ object image. The format is independent of host endianness.
inline constexpr std::size_t snapshot_bytes =
    4u + 128u * 4u + 32u * 4u + ring_word_count * 2u + 4u;

inline bool capture_snapshot(const void* opaque,
                             std::span<std::uint8_t> destination,
                             std::uint64_t& written) noexcept {
    written = 0u;
    if (opaque == nullptr || destination.size() < snapshot_bytes ||
        (reinterpret_cast<std::uintptr_t>(opaque) & (alignof(State) - 1u)) != 0u)
        return false;
    const auto& state = *static_cast<const State*>(opaque);
    if (state.memory_decrement == 0u || state.memory_decrement > ring_word_count)
        return false;
    std::size_t offset = 0u;
    const auto put = [&](std::uint32_t value, const std::uint32_t bytes) {
        for (std::uint32_t i = 0u; i < bytes; ++i) {
            destination[offset++] = static_cast<std::uint8_t>(value);
            value >>= 8u;
        }
    };
    put(1u, 4u);
    for (const auto value : state.temporary) put(std::bit_cast<std::uint32_t>(value), 4u);
    for (const auto value : state.memory) put(std::bit_cast<std::uint32_t>(value), 4u);
    for (const auto value : state.ring) put(value, 2u);
    put(state.memory_decrement, 4u);
    written = offset;
    return true;
}

inline bool validate_snapshot(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() != snapshot_bytes) return false;
    const auto word = [&](const std::size_t offset) {
        std::uint32_t value = 0u;
        for (std::uint32_t i = 0u; i < 4u; ++i)
            value |= static_cast<std::uint32_t>(bytes[offset + i]) << (i * 8u);
        return value;
    };
    const auto cursor = word(snapshot_bytes - 4u);
    return word(0u) == 1u && cursor != 0u && cursor <= ring_word_count;
}

inline bool restore_snapshot(void* opaque,
                             std::span<const std::uint8_t> bytes) noexcept {
    if (opaque == nullptr || !validate_snapshot(bytes) ||
        (reinterpret_cast<std::uintptr_t>(opaque) & (alignof(State) - 1u)) != 0u)
        return false;
    auto& state = *static_cast<State*>(opaque);
    std::size_t offset = 4u;
    const auto get = [&](const std::uint32_t count) {
        std::uint32_t value = 0u;
        for (std::uint32_t i = 0u; i < count; ++i)
            value |= static_cast<std::uint32_t>(bytes[offset++]) << (i * 8u);
        return value;
    };
    for (auto& value : state.temporary) value = std::bit_cast<std::int32_t>(get(4u));
    for (auto& value : state.memory) value = std::bit_cast<std::int32_t>(get(4u));
    for (auto& value : state.ring) value = static_cast<std::uint16_t>(get(2u));
    state.memory_decrement = get(4u);
    return true;
}

inline const katana::runtime::NativePortSoundEffectKernel kernel{
    katana::runtime::native_port_sound_effect_kernel_contract_version,
    static_cast<std::uint32_t>(effect_bus_count),
    static_cast<std::uint32_t>(effect_bus_count),
    authored_work_area_size,
    sizeof(State),
    alignof(State),
    &initialize,
    &destroy,
    &render,
    snapshot_bytes,
    &capture_snapshot,
    &validate_snapshot,
    &restore_snapshot,
};

inline const katana::runtime::NativePortSoundEffectKernel* resolve(
    void*, const std::span<const std::byte> program_bytes) noexcept {
    return matches_program(program_bytes) ? &kernel : nullptr;
}

} // namespace detail

[[nodiscard]] inline katana::runtime::NativePortSoundEffectKernelProvider
provider() noexcept {
    return {katana::runtime::native_port_sound_effect_kernel_contract_version,
            nullptr,
            &detail::resolve};
}

} // namespace sonic::native_qsound_reverb_medium
