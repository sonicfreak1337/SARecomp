#include "sonic_model_projection.hpp"
#include "katana/runtime/fpu.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <vector>
#include <xmmintrin.h>

using namespace katana::runtime;
using Op = FpuBinaryOperation;
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
bool candidate(CpuState& cpu, unsigned count, std::span<const std::uint8_t> input,
               std::span<std::uint8_t> output, std::span<std::uint8_t> clips) {
    const HostFpuExecutionEpoch epoch(cpu);
    return sonic::model_projection::try_execute(cpu, count, input, output, clips, epoch);
}

// Literal operation sequence from the pre-change native model owner, using
// the SDK helpers rather than the candidate's specialized arithmetic body.
void reference(CpuState& cpu, unsigned count, std::span<const std::uint8_t> points,
               std::span<std::uint8_t> output, std::span<std::uint8_t> clips) {
    const HostFpuExecutionEpoch epoch(cpu);
    auto* source = points.data();
    const auto load = [&](unsigned base) {
        for (unsigned axis = 0; axis < 3; ++axis)
            std::memcpy(&cpu.fr[base + axis], source + axis * 4, 4);
        cpu.fr[base + 3] = 0x3f800000;
        if (!try_fpu_transform_vector_simd(cpu, static_cast<std::uint8_t>(base)))
            fpu_transform_vector(cpu, static_cast<std::uint8_t>(base));
        cpu.fr[base + 3] = 0x3f800000;
        fpu_binary(cpu, Op::Divide, static_cast<std::uint8_t>(base + 2), static_cast<std::uint8_t>(base + 3));
        source += 12;
    };
    unsigned observed = 0;
    const auto clip = [&](unsigned z) {
        fpu_compare_greater(cpu, 14, static_cast<std::uint8_t>(z));
        if (observed < count) clips[observed] = cpu.t ? 0 : 1;
        ++observed;
        if (!cpu.t) ++cpu.r[13];
    };
    const auto store = [&](std::size_t offset, std::uint32_t word) {
        std::memcpy(output.data() + offset, &word, 4);
    };
    load(0);
    unsigned remaining = count;
    std::size_t offset = 0;
    do {
        clip(2); load(8); clip(10);
        const auto first_depth = cpu.fr[3];
        fpu_binary(cpu, Op::Multiply, 6, 0); fpu_binary(cpu, Op::Multiply, 7, 1);
        fpu_binary(cpu, Op::Multiply, 3, 0); fpu_binary(cpu, Op::Multiply, 3, 1);
        fpu_binary(cpu, Op::Add, 4, 0); cpu.fr[3] = 0x3f800000;
        fpu_binary(cpu, Op::Add, 5, 1);
        fpu_binary(cpu, Op::Multiply, 6, 8); fpu_binary(cpu, Op::Multiply, 7, 9);
        const auto first_x = cpu.fr[0], first_y = cpu.fr[1];
        load(0);
        fpu_binary(cpu, Op::Multiply, 11, 9); fpu_binary(cpu, Op::Multiply, 11, 8);
        fpu_binary(cpu, Op::Add, 5, 9); const auto second_depth = cpu.fr[11];
        fpu_binary(cpu, Op::Add, 4, 8);
        store(offset, first_x); store(offset + 4, first_y); store(offset + 8, first_depth);
        store(offset + 16, cpu.fr[8]); store(offset + 20, cpu.fr[9]); store(offset + 24, second_depth);
        offset += 32; remaining -= 2;
    } while (std::bit_cast<std::int32_t>(remaining) > 0);
}
auto state(const CpuState& c) {
    return std::tuple(c.r, c.r_bank, c.fr, c.xf, c.fpscr, c.t, c.sr, c.fpul,
        c.pc, c.pr, c.gbr, c.trap_pending, c.sleeping, c.exception_generation,
        c.attempted_guest_instructions, c.retired_guest_instructions, c.total_guest_cycles);
}
int main() try {
    constexpr std::array<std::uint32_t, 21> edges{0u, 0x80000000u, 1u, 0x80000001u,
        0x7fffffu, 0x807fffffu, 0x800000u, 0x80800000u, 0x3f800000u, 0xbf800000u,
        0x3f000001u, 0x4b000001u, 0x7f7fffffu, 0xff7fffffu, 0x7f800000u,
        0xff800000u, 0x7fbfffffu, 0xffbfffffu, 0x7fc00000u, 0x7ffffffeu, 0x00800001u};
    std::uint32_t rng = 0x53414d50u;
    const auto random = [&]() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    const auto value = [&](unsigned seed) {
        const auto bits = random();
        if (seed % 3 == 0) return edges[bits % edges.size()];
        return (bits & 0x807fffffu) | ((110u + (bits % 36u)) << 23);
    };
    unsigned checks = 0;
    for (unsigned test = 0; test < 4096; ++test) {
        const unsigned count = 1u + (random() % 31u), transformed = (count + 1u) & ~1u;
        CpuState a{.memory = Memory{0u}}, b{.memory = Memory{0u}};
        a.sr = b.sr = sr_md_mask;
        a.fpscr = b.fpscr = fpscr_dn_mask | (test & 1u) | (test & fpscr_flag_mask) |
            ((test & 2u) ? fpscr_fr_mask : 0u);
        for (unsigned reg = 0; reg < 16; ++reg) {
            a.fr[reg] = b.fr[reg] = value(test);
            a.xf[reg] = b.xf[reg] = value(test + 1u);
            a.r[reg] = b.r[reg] = random();
        }
        std::vector<std::uint8_t> input((transformed + 1u) * 12u);
        for (std::size_t at = 0; at < input.size(); at += 4) {
            const auto word = value(test + 2u); std::memcpy(input.data() + at, &word, 4);
        }
        std::vector<std::uint8_t> out_a(transformed * 16u, 0xa5), out_b = out_a;
        std::vector<std::uint8_t> clip_a(count, 0xcd), clip_b = clip_a;
        const auto mxcsr = _mm_getcsr();
        require(candidate(a, count, input, out_a, clip_a), "unexpected decline");
        require(_mm_getcsr() == mxcsr, "candidate changed incoming MXCSR");
        reference(b, count, input, out_b, clip_b);
        require(_mm_getcsr() == mxcsr, "reference changed incoming MXCSR");
        if (out_a != out_b || clip_a != clip_b || state(a) != state(b)) {
            std::cerr << "case=" << test << " count=" << count << '\n';
            std::cerr << std::hex << "fpscr=" << a.fpscr << '/' << b.fpscr
                      << " r13=" << a.r[13] << '/' << b.r[13] << " t=" << a.t << '/' << b.t << '\n';
            for (unsigned i = 0; i < 16; ++i)
                if (a.fr[i] != b.fr[i]) std::cerr << "fr" << i << '=' << a.fr[i] << '/' << b.fr[i] << '\n';
            for (std::size_t i = 0; i < out_a.size(); i += 4) {
                std::uint32_t x, y; std::memcpy(&x, out_a.data() + i, 4); std::memcpy(&y, out_b.data() + i, 4);
                if (x != y) std::cerr << "output+" << i << '=' << x << '/' << y << '\n';
            }
            std::cerr << std::dec;
            throw std::runtime_error("projection/clip/padding/register mismatch");
        }
        ++checks;
    }
    for (const auto mode : {0u, fpscr_dn_mask | fpscr_pr_mask,
            fpscr_dn_mask | fpscr_sz_mask, fpscr_dn_mask | 2u,
            fpscr_dn_mask | fpscr_exception_enable_mask}) {
        CpuState cpu{.memory = Memory{0u}}; cpu.sr = sr_md_mask; cpu.fpscr = mode;
        const auto before = state(cpu);
        std::array<std::uint8_t, 36> input{};
        std::array<std::uint8_t, 32> output{};
        std::array<std::uint8_t, 1> clips{};
        require(!candidate(cpu, 1, input, output, clips), "bad mode admitted");
        require(state(cpu) == before, "decline mutated registers");
        require(std::all_of(output.begin(), output.end(), [](auto v) { return v == 0; }) && clips[0] == 0,
                "decline mutated output");
        ++checks;
    }
    std::cout << "SONIC_MODEL_PROJECTION_TESTS passed=" << checks
              << " output=byte-exact clip=exact padding=preserved state=exact host=restored\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
