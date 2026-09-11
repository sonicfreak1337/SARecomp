#include "katana/runtime/fpu.hpp"
#include "katana/runtime/memory.hpp"
#include "katana/runtime/native_port.hpp"
#include "katana/runtime/runtime.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t transform_stream_error_context = 0x5341470Eu;
constexpr std::uint32_t transform_stream_error_contract = 0x5341470Fu;
constexpr std::uint32_t transform_stream_error_commit = 0x53414710u;
constexpr std::uint32_t transform_stream_max_records = 65'536u;
constexpr std::uint32_t transform_stream_input_stride = 12u;
constexpr std::uint32_t transform_stream_output_stride = 32u;

constexpr std::uint32_t ninja_center_x = 0x8C88F530u;
constexpr std::uint32_t ninja_center_y = 0x8C88F534u;
constexpr std::uint32_t ninja_near_clip = 0x8C88F550u;
constexpr std::uint32_t ninja_far_clip = 0x8C88F554u;

struct TransformStreamScratch final {
    std::vector<std::uint32_t> points;
    std::vector<std::uint32_t> auxiliary;
    std::vector<std::uint32_t> output;
};

thread_local TransformStreamScratch transform_stream_scratch;

class ScopedFpuRegisters final {
  public:
    explicit ScopedFpuRegisters(katana::runtime::CpuState& cpu) noexcept
        : cpu_(cpu), fr_(cpu.fr), t_(cpu.t) {}

    ScopedFpuRegisters(const ScopedFpuRegisters&) = delete;
    ScopedFpuRegisters& operator=(const ScopedFpuRegisters&) = delete;

    ~ScopedFpuRegisters() noexcept {
        // The four replaced leaves are admitted only through the proven
        // 0x8C611384 owner.  The SH-4 implementations preserve FR12-FR15 but
        // leave FR0-FR11 and T as scratch; that owner consumes none of those
        // values and normalizes its own return status.  Restoring the wider
        // volatile set is therefore an owner-scoped native optimization, not
        // a claim that these hooks implement an arbitrary direct-call ABI.
        cpu_.fr = fr_;
        cpu_.t = t_;
    }

  private:
    katana::runtime::CpuState& cpu_;
    std::array<std::uint32_t, katana::runtime::fpu_register_count> fr_;
    bool t_ = false;
};

[[nodiscard]] bool checked_product(const std::uint32_t count,
                                   const std::uint32_t stride,
                                   std::size_t& bytes) noexcept {
    const auto product = static_cast<std::uint64_t>(count) * stride;
    if (product > std::numeric_limits<std::size_t>::max()) return false;
    bytes = static_cast<std::size_t>(product);
    return true;
}

[[nodiscard]] bool checked_address(const std::uint32_t base,
                                   const std::uint64_t byte_offset,
                                   std::uint32_t& address) noexcept {
    const auto value = static_cast<std::uint64_t>(base) + byte_offset;
    if (value > std::numeric_limits<std::uint32_t>::max()) return false;
    address = static_cast<std::uint32_t>(value);
    return true;
}

[[nodiscard]] bool normalize_main_ram_address(
    const std::uint32_t address,
    std::uint32_t& normalized) noexcept {
    const auto segment = address & 0xE0000000u;
    if (segment != 0u && segment != 0x80000000u &&
        segment != 0xA0000000u)
        return false;
    normalized =
        katana::runtime::canonical_physical_address(address) | 0x80000000u;
    return true;
}

[[nodiscard]] bool ranges_overlap(const std::uint32_t first,
                                  const std::size_t first_bytes,
                                  const std::uint32_t second,
                                  const std::size_t second_bytes) noexcept {
    const auto first_end = static_cast<std::uint64_t>(first) + first_bytes;
    const auto second_end = static_cast<std::uint64_t>(second) + second_bytes;
    return static_cast<std::uint64_t>(first) < second_end &&
           static_cast<std::uint64_t>(second) < first_end;
}

[[nodiscard]] bool read_word(
    const katana::runtime::DirectLinearMemoryGuard& guard,
    const std::uint32_t base,
    const std::uint64_t byte_offset,
    std::uint32_t& value) noexcept {
    std::uint32_t address = 0u;
    return checked_address(base, byte_offset, address) &&
           katana::runtime::direct_linear_guard_read_u32(
               guard, address, value);
}

[[nodiscard]] bool read_words(
    const katana::runtime::DirectLinearMemoryGuard& guard,
    const std::uint32_t base,
    const std::uint32_t count,
    const std::uint32_t words_per_record,
    std::vector<std::uint32_t>& destination) {
    const auto word_count =
        static_cast<std::size_t>(count) * words_per_record;
    destination.resize(word_count);
    for (std::size_t index = 0u; index < word_count; ++index) {
        if (!read_word(guard, base, index * sizeof(std::uint32_t),
                       destination[index]))
            return false;
    }
    return true;
}

[[nodiscard]] std::span<const std::uint8_t> byte_span(
    const std::vector<std::uint32_t>& words) noexcept {
    return {reinterpret_cast<const std::uint8_t*>(words.data()),
            words.size() * sizeof(std::uint32_t)};
}

[[nodiscard]] std::span<const std::uint8_t> byte_span(
    const std::array<std::uint32_t, 2u>& words) noexcept {
    return {reinterpret_cast<const std::uint8_t*>(words.data()),
            words.size() * sizeof(std::uint32_t)};
}

[[nodiscard]] std::uint32_t clip_mask(
    katana::runtime::CpuState& cpu) noexcept {
    std::uint32_t mask = 0u;
    const auto append = [&](const std::uint8_t source,
                            const std::uint8_t destination) {
        katana::runtime::fpu_compare_greater(cpu, source, destination);
        mask = (mask << 1u) | static_cast<std::uint32_t>(cpu.t);
    };
    append(13u, 1u); // y > high-y
    append(1u, 12u); // low-y > y
    append(11u, 0u); // x > high-x
    append(0u, 10u); // low-x > x
    append(9u, 2u);  // z > far
    append(2u, 8u);  // near > z
    return mask;
}

void transform_point(katana::runtime::CpuState& cpu,
                     const std::uint32_t x,
                     const std::uint32_t y,
                     const std::uint32_t z,
                     const bool clipped,
                     std::uint32_t& output_x,
                     std::uint32_t& output_y,
                     std::uint32_t& output_depth,
                     std::uint32_t& mask) noexcept {
    cpu.fr[0] = x;
    cpu.fr[1] = y;
    cpu.fr[2] = z;
    cpu.fr[3] = 0x3F800000u;
    katana::runtime::fpu_transform_vector(cpu, 0u);

    if (!clipped) {
        katana::runtime::fpu_binary(
            cpu, katana::runtime::FpuBinaryOperation::Multiply, 2u, 2u);
        katana::runtime::fpu_reciprocal_square_root(cpu, 2u);
        katana::runtime::fpu_binary(
            cpu, katana::runtime::FpuBinaryOperation::Multiply, 7u, 1u);
        katana::runtime::fpu_binary(
            cpu, katana::runtime::FpuBinaryOperation::Multiply, 6u, 0u);
        katana::runtime::fpu_binary(
            cpu, katana::runtime::FpuBinaryOperation::Add, 15u, 1u);
        katana::runtime::fpu_binary(
            cpu, katana::runtime::FpuBinaryOperation::Add, 14u, 0u);
        mask = 0u;
        output_x = cpu.fr[0];
        output_y = cpu.fr[1];
        output_depth = cpu.fr[2];
        return;
    }

    katana::runtime::fpu_compare_greater(cpu, 2u, 8u);
    if (cpu.t) {
        cpu.fr[3] = cpu.fr[2];
    } else {
        cpu.fr[4] = cpu.fr[2];
        katana::runtime::fpu_binary(
            cpu, katana::runtime::FpuBinaryOperation::Multiply, 2u, 4u);
        katana::runtime::fpu_reciprocal_square_root(cpu, 4u);
        katana::runtime::fpu_binary(
            cpu, katana::runtime::FpuBinaryOperation::Multiply, 7u, 1u);
        katana::runtime::fpu_binary(
            cpu, katana::runtime::FpuBinaryOperation::Multiply, 6u, 0u);
        katana::runtime::fpu_binary(
            cpu, katana::runtime::FpuBinaryOperation::Add, 15u, 1u);
        katana::runtime::fpu_binary(
            cpu, katana::runtime::FpuBinaryOperation::Add, 14u, 0u);
        cpu.fr[3] = cpu.fr[4];
    }

    mask = clip_mask(cpu);
    output_x = cpu.fr[0];
    output_y = cpu.fr[1];
    output_depth = cpu.fr[3];
}

[[nodiscard]] katana::runtime::NativePortHookResult abort_contract(
    const std::string_view variant,
    const std::uint32_t descriptor,
    const std::uint32_t count,
    const std::uint32_t code) noexcept {
    std::cerr << "SONIC_NATIVE_TRANSFORM_STREAM failure=" << variant
              << " descriptor=0x" << std::hex << descriptor << std::dec
              << " count=" << count << " code=0x" << std::hex << code
              << std::dec << '\n';
    return {katana::runtime::NativePortHookAction::Abort, 0u, code};
}

[[nodiscard]] katana::runtime::NativePortHookResult transform_stream(
    katana::runtime::NativePortContext& context,
    const bool has_auxiliary,
    const bool clipped,
    const std::string_view variant) noexcept {
    if (context.cpu == nullptr)
        return abort_contract(variant, 0u, 0u,
                              transform_stream_error_context);

    try {
        auto& cpu = *context.cpu;
        if (cpu.fpu_disabled() || cpu.fpu_double_precision())
            return abort_contract(variant, cpu.r[4], 0u,
                                  transform_stream_error_contract);

        const auto guard = cpu.memory.direct_linear_memory_guard(false);
        std::uint32_t descriptor = 0u;
        if (!guard || !normalize_main_ram_address(cpu.r[4], descriptor))
            return abort_contract(variant, cpu.r[4], 0u,
                                  transform_stream_error_contract);

        std::uint32_t count = 0u;
        std::uint32_t output_address = 0u;
        std::uint32_t point_address = 0u;
        std::uint32_t auxiliary_address = 0u;
        if (!read_word(guard, descriptor, 0u, count) ||
            !read_word(guard, descriptor, 4u, output_address) ||
            !read_word(guard, descriptor, 8u, point_address) ||
            (has_auxiliary &&
             !read_word(guard, descriptor, 12u, auxiliary_address)) ||
            count > transform_stream_max_records)
            return abort_contract(variant, descriptor, count,
                                  transform_stream_error_contract);

        if (count == 0u)
            return {katana::runtime::NativePortHookAction::Return, 0u, 0u};

        if (!normalize_main_ram_address(output_address, output_address) ||
            !normalize_main_ram_address(point_address, point_address) ||
            (has_auxiliary &&
             !normalize_main_ram_address(auxiliary_address,
                                        auxiliary_address)))
            return abort_contract(variant, descriptor, count,
                                  transform_stream_error_contract);

        std::size_t input_bytes = 0u;
        std::size_t output_bytes = 0u;
        if (!checked_product(count, transform_stream_input_stride,
                             input_bytes) ||
            !checked_product(count, transform_stream_output_stride,
                             output_bytes) ||
            // The original loops read each 12-byte input immediately before
            // writing its 32-byte result.  Same-base and partial aliases are
            // consequently order-dependent (and same-base is unsafe beyond
            // one record).  The proven owner supplies disjoint ranges, so the
            // native transaction rejects every input/output overlap instead
            // of inventing broader alias semantics.
            ranges_overlap(point_address, input_bytes, output_address,
                           output_bytes) ||
            (has_auxiliary &&
             ranges_overlap(auxiliary_address, input_bytes, output_address,
                            output_bytes)))
            return abort_contract(variant, descriptor, count,
                                  transform_stream_error_contract);

        auto& scratch = transform_stream_scratch;
        if (!read_words(guard, point_address, count, 3u,
                        scratch.points) ||
            (has_auxiliary &&
             !read_words(guard, auxiliary_address, count, 3u,
                         scratch.auxiliary)))
            return abort_contract(variant, descriptor, count,
                                  transform_stream_error_contract);

        scratch.output.resize(static_cast<std::size_t>(count) * 8u);
        for (std::uint32_t index = 0u; index < count; ++index) {
            const auto output_word = static_cast<std::size_t>(index) * 8u;
            const auto output_byte = static_cast<std::uint64_t>(index) *
                                     transform_stream_output_stride;
            if (has_auxiliary) {
                if (!read_word(guard, output_address, output_byte + 28u,
                               scratch.output[output_word + 7u]))
                    return abort_contract(variant, descriptor, count,
                                          transform_stream_error_contract);
            } else {
                for (std::size_t preserved = 0u; preserved < 3u;
                     ++preserved) {
                    if (!read_word(guard, output_address,
                                   output_byte + preserved * 4u,
                                   scratch.output[output_word + preserved]))
                        return abort_contract(
                            variant, descriptor, count,
                            transform_stream_error_contract);
                }
                if (!read_word(guard, output_address, output_byte + 28u,
                               scratch.output[output_word + 7u]))
                    return abort_contract(variant, descriptor, count,
                                          transform_stream_error_contract);
            }
        }

        std::uint32_t scale_x = 0u;
        std::uint32_t scale_y = 0u;
        std::uint32_t center_x = 0u;
        std::uint32_t center_y = 0u;
        // The unclipped descriptor is four bytes shorter before its scale
        // pair.  The two clipped leaves insert the AND/OR accumulator words
        // at +0x10/+0x14 and consequently read scale at +0x18/+0x1C.
        const auto scale_x_offset = clipped ? 24u : 20u;
        const auto scale_y_offset = clipped ? 28u : 24u;
        if (!read_word(guard, descriptor, scale_x_offset, scale_x) ||
            !read_word(guard, descriptor, scale_y_offset, scale_y) ||
            !read_word(guard, ninja_center_x, 0u, center_x) ||
            !read_word(guard, ninja_center_y, 0u, center_y))
            return abort_contract(variant, descriptor, count,
                                  transform_stream_error_contract);

        std::uint32_t and_accumulator = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t or_accumulator = 0u;
        std::uint32_t low_x = 0u;
        std::uint32_t high_x = 0u;
        std::uint32_t low_y = 0u;
        std::uint32_t high_y = 0u;
        std::uint32_t near_clip = 0u;
        std::uint32_t far_clip = 0u;
        std::array<std::uint32_t, 2u> status_words{};
        if (clipped) {
            if (!read_word(guard, descriptor, 16u, and_accumulator) ||
                !read_word(guard, descriptor, 20u, or_accumulator) ||
                !read_word(guard, descriptor, 32u, low_x) ||
                !read_word(guard, descriptor, 36u, high_x) ||
                !read_word(guard, descriptor, 40u, low_y) ||
                !read_word(guard, descriptor, 44u, high_y) ||
                !read_word(guard, ninja_near_clip, 0u, near_clip) ||
                !read_word(guard, ninja_far_clip, 0u, far_clip))
                return abort_contract(variant, descriptor, count,
                                      transform_stream_error_contract);
        }

        ScopedFpuRegisters restore_fpu(cpu);
        cpu.fr[6] = scale_x;
        cpu.fr[7] = scale_y;
        cpu.fr[8] = near_clip;
        cpu.fr[9] = far_clip;
        cpu.fr[10] = low_x;
        cpu.fr[11] = high_x;
        cpu.fr[12] = low_y;
        cpu.fr[13] = high_y;
        cpu.fr[14] = center_x;
        cpu.fr[15] = center_y;

        for (std::uint32_t index = 0u; index < count; ++index) {
            const auto input_word = static_cast<std::size_t>(index) * 3u;
            const auto output_word = static_cast<std::size_t>(index) * 8u;
            std::uint32_t output_x = 0u;
            std::uint32_t output_y = 0u;
            std::uint32_t output_depth = 0u;
            std::uint32_t mask = 0u;
            transform_point(cpu, scratch.points[input_word],
                            scratch.points[input_word + 1u],
                            scratch.points[input_word + 2u], clipped,
                            output_x, output_y, output_depth, mask);
            if (has_auxiliary) {
                scratch.output[output_word] = scratch.auxiliary[input_word];
                scratch.output[output_word + 1u] =
                    scratch.auxiliary[input_word + 1u];
                scratch.output[output_word + 2u] =
                    scratch.auxiliary[input_word + 2u];
            }
            scratch.output[output_word + 3u] = mask;
            scratch.output[output_word + 4u] = output_x;
            scratch.output[output_word + 5u] = output_y;
            scratch.output[output_word + 6u] = output_depth;
            if (clipped) {
                and_accumulator &= mask;
                or_accumulator |= mask;
            }
        }

        if (clipped) {
            status_words = {and_accumulator, or_accumulator};
            // The owner is single-threaded across this leaf call.  Publishing
            // the completed output and the final AND/OR status atomically
            // preserves the observed final order without exposing partial
            // records to an unproven concurrent consumer.  Descriptor/output
            // aliasing has no PAL evidence and remains outside this contract.
            constexpr std::size_t write_count = 2u;
            const std::array<katana::runtime::LinearMemoryTransactionWrite,
                             write_count>
                writes{{
                    {output_address, byte_span(scratch.output)},
                    {descriptor + 16u, byte_span(status_words)},
                }};
            if (!cpu.memory.commit_linear_transaction_batch(
                    writes, katana::runtime::CodeWriteSource::Copy))
                return abort_contract(variant, descriptor, count,
                                      transform_stream_error_commit);
        } else if (!cpu.memory.commit_linear_transaction_bytes(
                       output_address, byte_span(scratch.output),
                       katana::runtime::CodeWriteSource::Copy)) {
            return abort_contract(variant, descriptor, count,
                                  transform_stream_error_commit);
        }

        return {katana::runtime::NativePortHookAction::Return, 0u, 0u};
    } catch (const std::exception& error) {
        std::cerr << "SONIC_NATIVE_TRANSFORM_STREAM exception=" << variant
                  << " what=" << error.what() << '\n';
    } catch (...) {
        std::cerr << "SONIC_NATIVE_TRANSFORM_STREAM exception=" << variant
                  << " what=non-standard\n";
    }
    return {katana::runtime::NativePortHookAction::Abort, 0u,
            transform_stream_error_contract};
}

} // namespace

extern "C" katana::runtime::NativePortHookResult
sonic_native_transform_stream_aux_unclipped(
    katana::runtime::NativePortContext& context) noexcept {
    return transform_stream(context, true, false, "aux-unclipped");
}

extern "C" katana::runtime::NativePortHookResult
sonic_native_transform_stream_unclipped(
    katana::runtime::NativePortContext& context) noexcept {
    return transform_stream(context, false, false, "unclipped");
}

extern "C" katana::runtime::NativePortHookResult
sonic_native_transform_stream_aux_clipped(
    katana::runtime::NativePortContext& context) noexcept {
    return transform_stream(context, true, true, "aux-clipped");
}

extern "C" katana::runtime::NativePortHookResult
sonic_native_transform_stream_clipped(
    katana::runtime::NativePortContext& context) noexcept {
    return transform_stream(context, false, true, "clipped");
}
