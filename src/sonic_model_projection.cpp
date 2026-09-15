#include "sonic_model_projection.hpp"
#include "sonic_fpu_body.hpp"
#include <bit>
#include <cstring>

namespace sonic::model_projection {
using namespace katana::runtime;

bool try_execute(CpuState& cpu, std::uint32_t point_count,
                 std::span<const std::uint8_t> points,
                 std::span<std::uint8_t> output,
                 std::span<std::uint8_t> clipped,
                 const HostFpuExecutionEpoch& epoch) noexcept {
    static_assert(std::endian::native == std::endian::little);
    // Reuse the already admitted owner's epoch, including its host status;
    // no repeated save/reset/restore on exceptional arithmetic operands.
    sonic::fpu_body::NontrappingSingleBody fp(cpu, epoch);
    const auto pairs = (std::uint64_t(point_count) + 1u) / 2u;
    if (!fp.admitted() || (cpu.fpscr & fpscr_sz_mask) || !point_count ||
        point_count > 65536u || points.size() != (pairs * 2u + 1u) * 12u ||
        output.size() != pairs * 32u || clipped.size() != point_count)
        return false;
    // Identical FTRV/FDIV/FCMP/FMUL/FADD order to the retained synchronous
    // owner. Reuse one arithmetic admission and whole-span RAM admission.
    // Exceptional operands continue through the original SDK helpers.
    const auto* source = points.data();
    const auto load = [&]<unsigned Base>() {
        std::memcpy(cpu.fr.data() + Base, source, 12u);
        cpu.fr[Base + 3u] = 0x3F800000u;
        if (!try_fpu_transform_vector_simd(cpu, Base))
            fpu_transform_vector(cpu, Base);
        cpu.fr[Base + 3u] = 0x3F800000u;
        fp.binary<FpuBinaryOperation::Divide, Base + 2u, Base + 3u>();
        source += 12u;
    };
    std::uint32_t observed = 0u;
    const auto clip = [&]<unsigned Z>() {
        fpu_compare_greater(cpu, 14u, Z);
        if (observed < point_count) clipped[observed] = cpu.t ? 0u : 1u;
        ++observed;
        if (!cpu.t) ++cpu.r[13];
    };
    const auto store = [&](std::size_t offset, std::uint32_t value) {
        std::memcpy(output.data() + offset, &value, sizeof(value));
    };
    load.template operator()<0u>();
    for (std::size_t pair = 0u; pair < pairs; ++pair) {
        clip.template operator()<2u>();
        load.template operator()<8u>();
        clip.template operator()<10u>();
        const auto first_depth = cpu.fr[3];
        fp.binary<FpuBinaryOperation::Multiply, 6u, 0u>();
        fp.binary<FpuBinaryOperation::Multiply, 7u, 1u>();
        fp.binary<FpuBinaryOperation::Multiply, 3u, 0u>();
        fp.binary<FpuBinaryOperation::Multiply, 3u, 1u>();
        fp.binary<FpuBinaryOperation::Add, 4u, 0u>();
        cpu.fr[3] = 0x3F800000u;
        fp.binary<FpuBinaryOperation::Add, 5u, 1u>();
        fp.binary<FpuBinaryOperation::Multiply, 6u, 8u>();
        fp.binary<FpuBinaryOperation::Multiply, 7u, 9u>();
        const auto first_x = cpu.fr[0], first_y = cpu.fr[1];
        load.template operator()<0u>();
        fp.binary<FpuBinaryOperation::Multiply, 11u, 9u>();
        fp.binary<FpuBinaryOperation::Multiply, 11u, 8u>();
        fp.binary<FpuBinaryOperation::Add, 5u, 9u>();
        const auto second_depth = cpu.fr[11];
        fp.binary<FpuBinaryOperation::Add, 4u, 8u>();
        store(pair * 32u, first_x);
        store(pair * 32u + 4u, first_y);
        store(pair * 32u + 8u, first_depth);
        store(pair * 32u + 16u, cpu.fr[8]);
        store(pair * 32u + 20u, cpu.fr[9]);
        store(pair * 32u + 24u, second_depth);
    }
    return true;
}
}
