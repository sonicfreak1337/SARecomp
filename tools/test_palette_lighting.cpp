#include "sonic_palette_lighting.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "katana/sh4/decoder.hpp"
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <stdexcept>
#include <tuple>

using namespace katana::runtime;
namespace {
constexpr auto entry = 0x8C037350u, returned = 0x8CF80000u;
constexpr auto work = 0x8CE00000u, model = 0x8CE01000u;
constexpr auto material = 0x8CE02000u, normals = 0x8CE10000u;
constexpr auto positions = 0x8CE20000u, palette = 0x8CE30000u;
constexpr std::array code_range{
    NativePortImmutableRange{0x0C037350u, 0x110u,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
};
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
struct Services final : PlatformServices {
    std::string_view name() const noexcept override { return "palette-byte-reference"; }
    std::uint32_t abi_version() const noexcept override { return 128u; }
    std::uint32_t guest_cycle_contract() const noexcept override { return 0u; }
    PlatformCapabilities capabilities() const noexcept override { return {}; }
    void read_memory(std::uint32_t, std::span<std::uint8_t>) override { throw std::runtime_error("device read"); }
    void write_memory(std::uint32_t, std::span<const std::uint8_t>) override { throw std::runtime_error("device write"); }
    std::uint64_t scheduler_cycle() const noexcept override { return 0u; }
    std::optional<std::uint64_t> next_scheduler_event_cycle() const noexcept override { return {}; }
    PlatformSchedulerResult consume_guest_cycles(std::uint64_t, std::size_t) override { return {}; }
    std::optional<PlatformInterruptRequest> poll_interrupt() override { return {}; }
    PlatformDmaResult start_dma(const PlatformDmaRequest&) override { throw std::runtime_error("DMA"); }
    PlatformFallbackResult controlled_fallback(CpuState&, const PlatformFallbackRequest&) override {
        throw std::runtime_error("executor fallback");
    }
    bool prefetch(CpuState&, GuestInstructionOrigin, std::uint32_t) override { throw std::runtime_error("PREF"); }
} services;

std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary); require(bool(file), "boot.bin missing");
    return {std::istreambuf_iterator<char>(file), {}};
}
std::string digest(std::span<const std::uint8_t> bytes) {
    std::array<unsigned char, 32> result{};
    require(bytes.size() <= ULONG_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
        const_cast<PUCHAR>(bytes.data()), ULONG(bytes.size()), result.data(), ULONG(result.size())) >= 0,
        "SHA-256 failed");
    constexpr char hex[] = "0123456789abcdef"; std::string text(64, '0');
    for (std::size_t i = 0; i < result.size(); ++i) {
        text[i * 2] = hex[result[i] >> 4]; text[i * 2 + 1] = hex[result[i] & 15];
    }
    return text;
}

// Architectural state only. Executor instruction/cycle/provenance bookkeeping
// is intentionally outside the native hook's architectural leaf contract.
auto architecture(const CpuState& c) {
    return std::tuple(c.r, c.r_bank, c.fr, c.xf, c.pc, c.pr, c.gbr, c.vbr, c.ssr,
        c.spc, c.sgr, c.dbr, c.tra, c.tea, c.expevt, c.intevt, c.pteh, c.ptel,
        c.ptea, c.ttb, c.mmucr, c.mach, c.macl, c.fpul, c.read_fpscr(), c.sr,
        c.t, c.s, c.q, c.m, c.trap_pending, c.exception_generation,
        c.last_exception_cause, c.sleeping, c.prefetch_count, c.tlb_load_count);
}
using Write = std::tuple<std::uint32_t, std::size_t, CodeWriteSource, bool>;
struct Fixture {
    CpuState cpu{.memory = Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram = std::make_shared<LinearMemoryDevice>(0x1000000u);
    NativePortImmutableWriteGuard immutable{code_range};
    std::vector<Write> events;
    std::string label;

    Fixture(std::span<const std::uint8_t> boot, unsigned count, unsigned bank,
            unsigned mode, unsigned variant, bool read_only = false) {
        label = "count=" + std::to_string(count) + " bank=" + std::to_string(bank) +
            " mode=" + std::to_string(mode) + " variant=" + std::to_string(variant);
        cpu.memory.map_region("main-ram", 0x0C000000u, ram,
            read_only ? MemoryRegionAccess::ReadOnly : MemoryRegionAccess::ReadWrite);
        if (!read_only) cpu.memory.bind_direct_linear_alias_window(0x0C000000u, 0x1000000u, *ram);
        auto bytes = ram->writable_bytes();
        std::fill(bytes.begin(), bytes.end(), 0xCDu);
        std::copy(boot.begin(), boot.end(), bytes.begin() + 0x10000u);
        cpu.pc = entry; cpu.pr = returned; cpu.gbr = work;
        cpu.write_sr(sr_md_mask); cpu.write_fpscr(fpscr_dn_mask | mode);
        cpu.t = true; cpu.s = true; cpu.q = true; cpu.m = true;
        cpu.fpul = 0xDEADBEEFu; cpu.mach = 0x12345678u; cpu.macl = 0x87654321u;
        for (unsigned i = 0; i < 16; ++i) {
            cpu.r[i] = 0xABCDEF00u + i;
            cpu.fr[i] = 0x3F000000u + i * 0x123u;
            cpu.xf[i] = std::bit_cast<std::uint32_t>(float(int(i % 5) - 2) * 0.25f);
        }
        for (unsigned i = 0; i < 8; ++i) cpu.r_bank[i] = 0x11220000u + i;
        cpu.r[4] = model; cpu.r[15] = 0x8CF00000u;
        // Raw fixture initialization also permits the read-only admission case.
        put(work + 44u, bank == 2u ? 0x80000u : 0u);
        put(work + 52u, bank == 2u ? 0xFFFFFFFFu : material);
        put(work + 60u, positions); put(work + 64u, 0x12341234u); put(work + 88u, palette);
        put(model + 4u, normals); put(model + 8u, count); put(material + 4u, bank == 1u ? 1u : 0u);
        put(0x8C038F18u, std::bit_cast<std::uint32_t>(variant == 1u ? -63.25f : 127.5f));
        put(0x8C038F24u, 0x3F800000u); put(0x8C038F28u, 0xBF000000u); put(0x8C038F2Cu, 0x3E800000u);
        constexpr std::array special{0u, 0x80000000u, 1u, 0x80000001u, 0x7F800000u,
            0xFF800000u, 0x7FC00001u, 0x7FA00001u, 0x7F7FFFFFu};
        for (unsigned i = 0; i < 3u * (count + 1u); ++i) {
            const auto bits = variant >= 2u ? special[(i + variant) % special.size()] :
                std::bit_cast<std::uint32_t>(float(int(i % 17u) - 8) * 0.1875f);
            put(normals + i * 4u, bits);
        }
        if (variant >= 3u) cpu.xf[4] = special[variant % special.size()];
        for (unsigned i = 0; i < 0x1000u / 4u; ++i) put(palette + i * 4u, 0xC0010000u ^ (i * 0x9E3779B9u));
        events.reserve(1024);
    }
    void put(std::uint32_t address, std::uint32_t value) {
        auto bytes = ram->writable_bytes(); const auto offset = address & 0x00FFFFFFu;
        for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = std::uint8_t(value >> (i * 8u));
    }
    // Six independently addressable operands: GBR, model, material, primary
    // output, palette and normals. Bit set selects physical P0; the remainder
    // use P1 or P2. Fixed retail literals (code/light/secondary) stay unchanged.
    void aliases(unsigned p0_mask, bool remaining_p2) {
        const auto alias = [&](std::uint32_t address, unsigned bit) {
            return (address & 0x1FFFFFFFu) |
                ((p0_mask & (1u << bit)) ? 0u : remaining_p2 ? 0xA0000000u : 0x80000000u);
        };
        cpu.gbr = alias(work, 0u); cpu.r[4] = alias(model, 1u);
        put(work + 52u, alias(material, 2u));
        put(work + 60u, alias(positions, 3u));
        put(work + 88u, alias(palette, 4u));
        put(model + 4u, alias(normals, 5u));
        label += " p0_mask=" + std::to_string(p0_mask) + " remaining_p2=" + std::to_string(remaining_p2);
    }
    void bind_address_space(bool mmu) {
        cpu.address_space = std::make_shared<RuntimeAddressSpace>();
        cpu.address_space->write_mmucr(cpu.mmucr);
        cpu.address_space->set_mode(mmu ? AddressTranslationMode::Mmu : AddressTranslationMode::NoMmu);
    }
    void observe(bool stable = true) {
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e) noexcept {
            events.emplace_back(e.address, e.size, e.source, e.bytes_changed);
            immutable.observe_write(e);
        }, stable ? GuestWriteObserverContract::StableForPrevalidatedLinearWrites :
                    GuestWriteObserverContract::General);
    }
};

std::uint64_t corrected_reference_ftrc = 0u;
void execute_reference(CpuState& cpu) {
    for (unsigned n = 0; cpu.pc != returned && n < 20000u; ++n) {
        require(cpu.pc >= entry && cpu.pc < entry + 0x110u, "reference left byte-bound leaf");
        const auto opcode = guest_fetch_u16(cpu, cpu.pc);
        const auto instruction = katana::sh4::decode(opcode);
        if (instruction.kind == katana::sh4::InstructionKind::Ftrc) {
            // The historical dynamic interpreter uses destination_register for
            // FTRC, although decode deliberately sets destination=0, source=FRn.
            // Thus it incorrectly truncates FR0 for FB3D/F33D. Repair this one
            // executor instruction, using the source decoded from the unchanged
            // retail opcode. The unchanged retained source
            // .local/baseline/r354/product/generated/code/
            // unit-v8C036BC0-8C037C3C-aa2f5ddfed3d4270.cpp confirms:
            // 8C0373DE FB3D -> fpu_truncate_to_fpul(cpu,11u), line 29725;
            // 8C0373F6 F33D -> fpu_truncate_to_fpul(cpu, 3u), line 29961;
            // 8C037440 FB3D -> fpu_truncate_to_fpul(cpu,11u), line 30908.
            // All other instructions, branches and delay slots use the original
            // executor. No output or comparison is replaced with candidate data.
            require(!cpu.fpu_disabled() && !cpu.fpu_double_precision() &&
                (cpu.read_fpscr() & fpscr_rounding_mode_mask) <= 1u,
                "unsupported reference FTRC mode");
            require(((cpu.pc == 0x8C0373DEu || cpu.pc == 0x8C037440u) && opcode == 0xFB3Du) ||
                (cpu.pc == 0x8C0373F6u && opcode == 0xF33Du), "unexpected retail FTRC word/address");
            require(instruction.destination_register == 0u &&
                instruction.source_register == (cpu.pc == 0x8C0373F6u ? 3u : 11u),
                "unexpected decoded retail FTRC operands");
            GuestInstructionAttempt attempt(cpu, cpu.pc, 2u);
            cpu.pc += 2u;
            fpu_truncate_to_fpul(cpu, instruction.source_register);
            ++corrected_reference_ftrc;
        } else {
            (void)execute_dynamic_sh4_block(cpu, services, 1u);
        }
        require(cpu.exception_generation == 0u, "reference FPU/memory exception");
    }
    require(cpu.pc == returned, "reference did not return");
}
void compare(Fixture& native, Fixture& reference) {
    native.observe(); reference.observe();
    require(sonic::palette_lighting::try_execute(native.cpu, &native.immutable), "eligible leaf declined");
    execute_reference(reference.cpu);
    if (architecture(native.cpu) != architecture(reference.cpu)) {
        std::cerr << native.label << '\n' << std::hex;
        for (unsigned i = 0; i < 16; ++i) {
            if (native.cpu.r[i] != reference.cpu.r[i]) std::cerr << "r" << i << " native=" << native.cpu.r[i] << " reference=" << reference.cpu.r[i] << '\n';
            if (native.cpu.fr[i] != reference.cpu.fr[i]) std::cerr << "fr" << i << " native=" << native.cpu.fr[i] << " reference=" << reference.cpu.fr[i] << '\n';
            if (native.cpu.xf[i] != reference.cpu.xf[i]) std::cerr << "xf" << i << " native=" << native.cpu.xf[i] << " reference=" << reference.cpu.xf[i] << '\n';
        }
        std::cerr << "T=" << native.cpu.t << '/' << reference.cpu.t << " FPSCR=" << native.cpu.read_fpscr() << '/' << reference.cpu.read_fpscr()
            << " FPUL=" << native.cpu.fpul << '/' << reference.cpu.fpul << std::dec << '\n';
        throw std::runtime_error("architectural register/status mismatch");
    }
    require(std::equal(native.ram->bytes().begin(), native.ram->bytes().end(), reference.ram->bytes().begin()),
        "RAM/stack mismatch");
    require(native.events == reference.events, "store order/source/changed flags differ");
    require(!native.immutable.write_detected(), "native wrote protected code");
}
void decline(Fixture& fixture, const NativePortImmutableWriteGuard* guard) {
    const auto before = architecture(fixture.cpu);
    const auto memory = std::vector<std::uint8_t>(fixture.ram->bytes().begin(), fixture.ram->bytes().end());
    const auto events = fixture.events;
    const auto metrics = fixture.cpu.memory.performance_counters();
    const auto translation = fixture.cpu.address_space ?
        std::optional{fixture.cpu.address_space->snapshot()} : std::nullopt;
    require(!sonic::palette_lighting::try_execute(fixture.cpu, guard), "unsafe admission accepted");
    require(before == architecture(fixture.cpu) && events == fixture.events &&
        std::equal(memory.begin(), memory.end(), fixture.ram->bytes().begin()), "decline mutated guest state");
    const auto after = fixture.cpu.memory.performance_counters();
    require(metrics.indexed_region_hits == after.indexed_region_hits &&
        metrics.reference_region_probes == after.reference_region_probes &&
        metrics.observed_accesses == after.observed_accesses && metrics.unobserved_accesses == after.unobserved_accesses,
        "decline changed memory metrics");
    require(!translation || *translation == fixture.cpu.address_space->snapshot(),
        "decline changed translation state");
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "usage: test_palette_lighting <installed-content-root>");
        const auto boot = read(std::filesystem::path(argv[1]) / "boot.bin");
        require(boot.size() == 6735296u &&
            digest(boot) == "b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af",
            "retail boot SHA differs");
        require(digest(std::span<const std::uint8_t>(boot).subspan(0x27350u, 0x110u)) ==
            sonic::palette_lighting::source_sha256, "retail leaf SHA differs");
        unsigned cases = 0;
        for (unsigned count : {2u, 3u, 4u, 5u, 16u, 17u, 128u})
            for (unsigned bank = 0; bank < 3u; ++bank)
                for (unsigned mode : {0u, 1u, fpscr_fr_mask, fpscr_fr_mask | 1u}) {
                    Fixture native(boot, count, bank, mode, bank % 2u);
                    Fixture reference(boot, count, bank, mode, bank % 2u);
                    compare(native, reference); ++cases;
                }
        for (unsigned variant = 2; variant < 9; ++variant) {
            Fixture native(boot, 7u, 1u, variant & 1u, variant);
            Fixture reference(boot, 7u, 1u, variant & 1u, variant);
            compare(native, reference); ++cases;
        }
        for (unsigned mode : {fpscr_flag_mask | fpscr_cause_mask, fpscr_fr_mask | fpscr_flag_mask | 1u}) {
            Fixture native(boot, 3u, 1u, mode, 0u);
            Fixture reference(boot, 3u, 1u, mode, 0u);
            compare(native, reference); ++cases;
        }
        {
            Fixture native(boot, 3u, 1u, 0u, 0u);
            Fixture reference(boot, 3u, 1u, 0u, 0u);
            for (auto* f : {&native, &reference}) {
                f->cpu.gbr |= 0x20000000u; f->cpu.r[4] |= 0x20000000u;
                f->put(work + 52u, material | 0x20000000u);
                f->put(work + 60u, positions | 0x20000000u);
                f->put(work + 88u, palette | 0x20000000u);
                f->put(model + 4u, normals | 0x20000000u);
            }
            compare(native, reference); ++cases;
        }
        {
            Fixture native(boot, 3u, 0u, 0u, 0u);
            Fixture reference(boot, 3u, 0u, 0u, 0u);
            for (auto* f : {&native, &reference}) {
                f->put(model + 4u, 0x8CFFFFDCu);
                for (unsigned i = 0; i < 9u; ++i) f->put(0x8CFFFFDCu + i * 4u, 0x3F000000u);
            }
            compare(native, reference); ++cases; // odd count needs no extra normal
            const auto first_events = native.events.size();
            native.cpu.pc = entry; reference.cpu.pc = entry;
            compare(native, reference); ++cases;
            require(std::all_of(native.events.begin() + first_events, native.events.end(),
                [](const Write& e) { return !std::get<3>(e); }), "unchanged stores lost changed=false");
        }
        // Exercise every mixture of P0 with P1/P2, odd/even read-ahead, both
        // palette banks and both null and explicitly bound NoMmu translation.
        // Reference loads/stores retain their actual virtual operands and run
        // through the unchanged SDK translator, not candidate alias helpers.
        for (unsigned count : {3u, 4u})
            for (bool p2 : {false, true})
                for (unsigned mask = 0; mask < 64u; ++mask) {
                    Fixture native(boot, count, p2 ? 1u : 0u, 0u, 0u);
                    Fixture reference(boot, count, p2 ? 1u : 0u, 0u, 0u);
                    for (auto* f : {&native, &reference}) {
                        f->aliases(mask, p2);
                        if (p2) f->bind_address_space(false);
                    }
                    compare(native, reference); ++cases;
                }
        // P1/P2 must remain eligible when AT=1 and a real MMU is bound.
        for (bool p2 : {false, true}) {
            Fixture native(boot, 4u, 1u, 0u, 0u);
            Fixture reference(boot, 4u, 1u, 0u, 0u);
            for (auto* f : {&native, &reference}) {
                f->aliases(0u, p2); f->cpu.mmucr = 1u; f->bind_address_space(true);
            }
            compare(native, reference); ++cases;
        }
        // Decline every individually translated P0 operand before any state,
        // RAM, observer, metric or translation mutation. Also reject AT=0 with
        // an inconsistent bound MMU, and AT=1 without an address-space object.
        for (unsigned operand = 0; operand < 6u; ++operand)
            for (unsigned binding = 0; binding < 3u; ++binding) {
                Fixture f(boot, 4u, 1u, 0u, 0u);
                f.aliases(1u << operand, true);
                f.cpu.mmucr = binding == 2u ? 0u : 1u;
                if (binding != 0u) f.bind_address_space(true);
                f.observe(); decline(f, &f.immutable); ++cases;
            }
        // P0 admission must not mask away mirror boundaries or alias overlap.
        for (unsigned scenario = 0; scenario < 6u; ++scenario) {
            Fixture f(boot, 4u, 1u, 0u, 0u);
            switch (scenario) {
            case 0: f.put(model + 4u, 0x0CFFFFD0u); break; // even read-ahead crosses
            case 1: f.put(work + 60u, 0x0CFFFFF0u); break; // primary range crosses
            case 2: f.put(work + 60u, normals & 0x1FFFFFFFu); break;
            case 3: f.put(work + 60u, (work + 64u) & 0x1FFFFFFFu); break;
            case 4: f.cpu.r[4] = 0x0D000000u; break; // next mirror, not first window
            case 5: f.put(work + 88u, 0x0CFFFF00u); break;
            }
            f.observe(); decline(f, &f.immutable); ++cases;
        }
        // Admission is architectural and RAM atomic, including its read-ahead
        // bounds and the live immutable-write contract supplied by AOT services.
        for (unsigned scenario = 0; scenario < 18u; ++scenario) {
            Fixture f(boot, 4u, 0u, 0u, 0u, scenario == 12u);
            switch (scenario) {
            case 0: f.cpu.write_fpscr(0u); break;
            case 1: f.cpu.write_fpscr(fpscr_dn_mask | fpscr_sz_mask); break;
            case 2: f.cpu.write_fpscr(fpscr_dn_mask | fpscr_pr_mask); break;
            case 3: f.cpu.write_fpscr(fpscr_dn_mask | fpscr_enable_invalid_mask); break;
            case 4: f.cpu.sr |= sr_fd_mask; break;
            case 5: f.put(model + 8u, 1u); break;
            case 6: f.put(model + 4u, 0x8CFFFFD0u); break; // four normals fit, fifth does not
            case 7: f.put(work + 60u, normals); break;
            case 8: f.put(work + 60u, work + 64u); break;
            case 9: f.put(work + 88u, 0x8CFFFF00u); break;
            case 10: f.observe(false); break;
            case 11: f.put(entry, 0u); break;
            case 12: break; // read-only mapped RAM
            case 13: f.put(model + 8u, 0u); break;
            case 14: f.put(model + 8u, 0xFFFFFFFFu); break;
            case 15: f.cpu.memory.clear_direct_linear_alias_window(); break;
            case 16: f.cpu.write_fpscr(fpscr_dn_mask | 2u); break;
            case 17:
                (void)f.cpu.memory.add_watchpoint(positions & 0x1FFFFFFFu, 64u,
                    MemoryWatchpointAccess::Write,
                    [](const MemoryAccessEvent&) { throw std::runtime_error("decline invoked watchpoint"); });
                break;
            }
            decline(f, &f.immutable); ++cases;
        }
        for (const auto kind : {NativePortImmutableRangeKind::Executable, NativePortImmutableRangeKind::ReadOnlyImage})
            for (auto address : {work + 64u, positions + 12u, 0x8C03D760u}) {
                Fixture f(boot, 4u, 0u, 0u, 0u); f.observe();
                const std::array ranges{NativePortImmutableRange{address & 0x1FFFFFFFu, 4u,
                    native_port_immutable_range_mask(kind)}};
                NativePortImmutableWriteGuard protected_output{ranges};
                decline(f, &protected_output); ++cases;
            }
        {
            Fixture f(boot, 4u, 0u, 0u, 0u); decline(f, nullptr); ++cases;
            f.immutable.observe_write(GuestWriteEvent{0x0C037350u, 4u, CodeWriteSource::Cpu, true});
            decline(f, &f.immutable); ++cases;
        }
        std::cout << "SONIC_PALETTE_LIGHTING_PASS cases=" << cases
            << " reference=sha_bound_retail_sh4 architectural_registers=exact RAM=exact stores=ordered"
            << " reference_ftrc_source_corrections=" << corrected_reference_ftrc
            << " accounting=native_hook_policy\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
