#include "sonic_sdk_color.hpp"
#include "sonic_model_uv.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
using namespace katana::runtime;
namespace {
struct Services final : PlatformServices {
    std::string_view name() const noexcept override { return "cull-reference"; }
    std::uint32_t abi_version() const noexcept override { return 128u; }
    std::uint32_t guest_cycle_contract() const noexcept override { return 0u; }
    PlatformCapabilities capabilities() const noexcept override { return {}; }
    void read_memory(std::uint32_t,std::span<std::uint8_t>) override { throw std::runtime_error("unexpected service read"); }
    void write_memory(std::uint32_t,std::span<const std::uint8_t>) override { throw std::runtime_error("unexpected service write"); }
    std::uint64_t scheduler_cycle() const noexcept override { return 0u; }
    std::optional<std::uint64_t> next_scheduler_event_cycle() const noexcept override { return {}; }
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t,std::size_t) override { return {}; }
    std::optional<PlatformInterruptRequest> poll_interrupt() override { return {}; }
    PlatformDmaResult start_dma(const PlatformDmaRequest&) override { throw std::runtime_error("unexpected DMA"); }
    PlatformFallbackResult controlled_fallback(CpuState&,const PlatformFallbackRequest&) override { throw std::runtime_error("unexpected fallback"); }
    bool prefetch(CpuState&,GuestInstructionOrigin,std::uint32_t) override { throw std::runtime_error("unexpected PREF"); }
};

void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
std::uint32_t bits(float x) { return std::bit_cast<std::uint32_t>(x); }
std::uint32_t flycast_byte(std::uint32_t word) {
    const float x = std::bit_cast<float>((word >> 16u) << 16u);
    return static_cast<std::uint32_t>(x == x
        ? std::min(1.0f, std::max(0.0f, x)) * 255.0f : 255.0f);
}
void put(CpuState& cpu, std::uint32_t a, std::uint32_t v) {
    cpu.memory.write_u32(a & 0x1FFFFFFFu, v);
}
struct Reader {
    CpuState& cpu;
    unsigned calls = 0u;
    std::uint32_t end = 0x8C88F5B8u;
    bool u32(std::uint32_t address, std::uint32_t& value) {
        ++calls;
        if (address < 0x8C88F5A8u || address >= end) return false;
        value = cpu.memory.read_u32(address & 0x1FFFFFFFu);
        return true;
    }
};
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "boot.bin argument required");
        std::ifstream file(argv[1], std::ios::binary);
        std::vector<std::uint8_t> boot{std::istreambuf_iterator<char>(file), {}};
        require(boot.size() == 6735296u, "unexpected retail image");
        CpuState original{.memory=Memory{0u}}, native{.memory=Memory{0u}};
        auto ram = std::make_shared<LinearMemoryDevice>(0x1000000u);
        std::copy(boot.begin(), boot.end(), ram->writable_bytes().begin() + 0x10000u);
        original.memory.map_region("ram", 0x0C000000u, ram);
        const std::array words{0u,0x80000000u,0x3F000000u,0x3F800000u,0xBF800000u,
            0x40000000u,0x7F800000u,0xFF800000u,0x7FFFFFFFu,0xFFFFFFFFu,
            0x7FBFFFFFu,0x7FC00000u,0xFFBFFFFFu,0xFFC00000u,0x7F800001u,
            0xFF800001u,1u,0x007FFFFFu,0x7F7FFFFFu,0xFF7FFFFFu};
        Services services;
        require(original.memory.read_u32(0x0C038F10u)==0x3B808083u,
                "retail title UV factor changed");
        require(original.memory.read_u32(0x0C610A84u)==0x3B800000u,
                "retail resident UV factor changed");
        unsigned uv_cases=0;
        for (auto fpscr : {0u,1u,fpscr_dn_mask,fpscr_dn_mask|1u})
        for (auto u = -32768; u <= 32767; ++u) {
            const auto v=static_cast<std::int16_t>(u== -32768 ? 32767 : -u);
            original.r.fill(0u);original.fr.fill(0u);original.sr=sr_md_mask;
            original.write_fpscr(fpscr);original.pc=0x8C0379E0u;
            original.exception_generation=0;original.trap_pending=false;
            original.r[1]=0x8CF00000u;original.fr[11]=original.memory.read_u32(0x0C038F10u);
            put(original,original.r[1],std::uint16_t(u)|(std::uint32_t(std::uint16_t(v))<<16));
            unsigned steps=0;
            while(original.pc!=0x8C0379F0u && ++steps<=8)
                (void)execute_dynamic_sh4_block(original,services,1u);
            require(original.pc==0x8C0379F0u && original.exception_generation==0,
                    "retail UV sequence failed");
            native.write_fpscr(fpscr);
            const auto before=native.read_fpscr();
            const auto decoded=sonic::model_uv::decode(static_cast<std::int16_t>(u),v,true,native);
            require(bits(decoded[0])==original.fr[5] && bits(decoded[1])==original.fr[6],
                    "title UV differs from retail FLOAT/FMUL");
            require(native.read_fpscr()==before,"host UV conversion changed guest FPSCR");
            const auto resident=sonic::model_uv::decode(static_cast<std::int16_t>(u),v,false,native);
            require(resident[0]==static_cast<float>(u)/256.0f && resident[1]==static_cast<float>(v)/256.0f,
                    "resident SDK UV changed");
            ++uv_cases;
        }
        std::cout<<"SONIC_MODEL_UV_TEST_OK retail_instruction_cases="<<uv_cases
                 <<" exhaustive_int16_two_rounding_modes_dn_on_off resident_unchanged\n";
        unsigned cases = 0u, denormal_traps = 0u;
        for (auto entry : {0x8C620A72u, 0x8C6385F0u})
        for (auto fpscr : {0u,1u,fpscr_dn_mask,fpscr_dn_mask|1u,0x4106Du})
        for (auto control : {0u,0x10u,0x20u,0x30u})
        for (unsigned variant = 0u; variant < words.size(); ++variant) {
            original.r.fill(0u); original.fr.fill(0u);
            original.pc = entry; original.r[9u] = original.r[10u] = 0x8C88F56Cu;
            original.r[14u] = 0x8CF00000u;
            original.sr = sr_md_mask; original.write_fpscr(fpscr);
            original.exception_generation = 0u; original.trap_pending = false;
            native.sr = sr_md_mask; native.write_fpscr(fpscr);
            native.exception_generation = 0u; native.trap_pending = false;
            put(original, 0x8C88F56Cu, control);
            sonic::color::Color face{0.125f,0.5f,0.7f,1.0f};
            sonic::color::ArgbWords expected_words{};
            for (unsigned i = 0u; i < 4u; ++i) {
                expected_words[i] = words[(variant + i * 3u) % words.size()];
                put(original, 0x8C88F5A8u + i*4u, expected_words[i]);
                put(original, original.r[14u] + 16u + i*4u, bits(face[(i+3u)%4u]));
            }
            // DN=0 arithmetic on a subnormal raises the SH-4 FPU error
            // exception before color publication. It is not a TA color case.
            // FMOV-only replacement and DN=1 arithmetic remain covered.
            if ((control & 0x30u) == 0x20u && (fpscr & fpscr_dn_mask) == 0u &&
                std::ranges::any_of(expected_words, [](std::uint32_t word) {
                    return (word & 0x7F800000u) == 0u && (word & 0x007FFFFFu) != 0u;
                })) { ++denormal_traps; continue; }
            Reader reader{original};
            sonic::color::ArgbWords loaded{};
            require(sonic::color::read_constant_argb(reader, control, loaded), "raw FMOV input rejected");
            require(reader.calls == (control ? 4u : 0u), "unexpected color read count");
            if (control) require(loaded == expected_words, "color bits changed during read");
            const auto end = entry == 0x8C620A72u ? 0x8C620B04u : 0x8C638680u;
            unsigned steps = 0u;
            while (original.pc != end && ++steps < 100u) {
                (void)execute_dynamic_sh4_block(original, services, 1u);
                if (original.exception_generation != 0u)
                    std::cerr << std::hex << "reference entry=" << entry
                        << " fpscr=" << fpscr << " control=" << control
                        << " variant=" << variant << " pc=" << original.pc
                        << " spc=" << original.spc << " cause=" << original.read_fpscr()
                        << std::dec << '\n';
                require(original.exception_generation == 0u, "unexpected retail exception");
            }
            require(original.pc == end, "retail color branch did not terminate");
            {
                HostFpuExecutionEpoch epoch(native);
                face = sonic::color::apply_constant_argb(native, control, loaded, face);
            }
            for (unsigned i = 0u; i < 4u; ++i)
                require(bits(face[(i+3u)%4u]) == original.memory.read_u32(0x0CF00010u+i*4u),
                        "SDK color differs from retail FMOV/FADD");
            for (auto packed_intensity : {0u,0x80FF40u,0xFFFFFFu}) {
            const auto header = sonic::color::intensity_header_color(face, packed_intensity);
            require(!sonic::color::has_nonfinite(header), "nonfinite header reaches host");
            for (unsigned i = 0u; i < 4u; ++i) {
                auto byte = flycast_byte(bits(face[i]));
                if (i < 3u) byte = byte * ((packed_intensity >> ((2u-i)*8u))&255u) / 256u;
                require(header[i] == static_cast<float>(byte)/255.0f, "TA header/intensity mismatch");
            }
            }
            // Exercise the late vertex boundary with zero lighting: 0*NaN/Inf
            // must reach FPU/TA conversion, not be replaced by black early.
            native.write_fpscr(fpscr);
            sonic::color::Color vertex;
            {
                HostFpuExecutionEpoch epoch(native);
                vertex = sonic::color::lit_vertex_color(native, face, {0,0,0,1});
            }
            require(!sonic::color::has_nonfinite(vertex), "nonfinite vertex reaches host");
            ++cases;
        }
        Reader short_read{original,0u,0x8C88F5B4u};
        sonic::color::ArgbWords raw{};
        require(!sonic::color::read_constant_argb(short_read,0x10u,raw), "unmapped color accepted");
        for (std::uint32_t hi = 0u; hi < 65536u; ++hi)
            for (auto lo : {0u,1u,0xFFFFu}) {
                const auto word = hi*65536u+lo;
                require(sonic::color::ta_float_color_byte(word) == flycast_byte(word),
                        "Flycast color table mismatch");
            }
        // Exact boot model implicated by the user's capsule.
        require(original.memory.read_u32(0x0C1B7EE8u) == 0x8C1B7A84u &&
                original.memory.read_u32(0x0C1B7A94u) == 0x2671A400u,
                "crash model/material proof changed");
        std::cout << "SONIC_SDK_COLOR_REFERENCE_OK retail_cases=" << cases
                  << " table_cases=196608 range_rejection=ok excluded_guest_traps="
                  << denormal_traps << '\n';
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
