#include "sonic_amy_hammer_effect.hpp"
#include "katana/runtime/dynamic_interpreter.hpp"
#include "katana/runtime/fpu.hpp"
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <tuple>
#include <vector>
using namespace katana::runtime;
namespace effect = sonic::amy_hammer_effect;
namespace {
constexpr auto returned = 0x8CF80000u;
void require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
struct Services final : PlatformServices {
    std::string_view name() const noexcept override { return "amy-effect-original-byte-reference"; }
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
std::string digest(std::span<const std::uint8_t> bytes) {
    std::array<unsigned char, 32> result{};
    require(bytes.size() <= ULONG_MAX && BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
        const_cast<PUCHAR>(bytes.data()), ULONG(bytes.size()), result.data(), ULONG(result.size())) >= 0, "SHA-256");
    constexpr char hex[] = "0123456789abcdef"; std::string text(64, '0');
    for (std::size_t i = 0; i < result.size(); ++i) {
        text[i * 2] = hex[result[i] >> 4]; text[i * 2 + 1] = hex[result[i] & 15];
    }
    return text;
}
// Native-hook instruction/cycle bookkeeping is deliberately excluded.
auto architecture(const CpuState& c) {
    return std::tuple(c.r, c.r_bank, c.fr, c.xf, c.pc, c.pr, c.gbr, c.vbr, c.ssr, c.spc,
        c.sgr, c.dbr, c.tra, c.tea, c.expevt, c.intevt, c.pteh, c.ptel, c.ptea, c.ttb,
        c.mmucr, c.mach, c.macl, c.fpul, c.read_fpscr(), c.sr, c.t, c.s, c.q, c.m,
        c.trap_pending, c.exception_generation, c.last_exception_cause, c.sleeping);
}
using Write = std::tuple<std::uint32_t, std::size_t, CodeWriteSource, bool>;
using Call = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>;
struct Fixture {
    CpuState cpu{.memory = Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram = std::make_shared<LinearMemoryDevice>(0x1000000u);
    std::vector<Write> writes;
    std::vector<Call> calls;
    static constexpr auto task = 0x8CE00000u, work = 0x8CE01000u, character = 0x8CE02000u, animation = 0x8CE04000u;
    Fixture(std::span<const std::uint8_t> boot, std::uint32_t fpscr) {
        cpu.memory.map_region("main-ram", 0x0C000000u, ram, MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u, 0x1000000u, *ram);
        std::copy(boot.begin(), boot.end(), ram->writable_bytes().begin() + 0x10000u);
        cpu.write_sr(sr_md_mask); cpu.write_fpscr(fpscr);
        for (unsigned i = 0; i < 16u; ++i) { cpu.r[i] = 0xABD01200u + i; cpu.fr[i] = 0x3F500000u + i; cpu.xf[i] = 0x3E123000u + i; }
        for (unsigned i = 0; i < 8u; ++i) cpu.r_bank[i] = 0xBAC000u + i;
        cpu.pc = effect::entry; cpu.pr = returned; cpu.r[4] = task; cpu.r[15] = 0x8CF00000u;
        cpu.fpul = 0xAABBCCDDu; cpu.t = true; cpu.q = true; cpu.m = true;
        put(task + 16u, effect::entry); put(task + 20u, 0u); put(task + 32u, work);
        put(0x8C78C548u, character); put(0x8C78C5A8u, animation);
        put(character + 8u, 5u << 8u); put(animation + 292u, 87u);
        put(work + 24u, 0u); put(work + 44u, 0u);
        put(0x8C4BF00Cu, 0x3F800000u); put(0x8C4BF010u, 0x3F800000u);
        writes.reserve(1024); calls.reserve(64);
    }
    void put(std::uint32_t a, std::uint32_t v) { std::memcpy(ram->writable_bytes().data() + (a & 0xFFFFFFu), &v, 4u); }
    std::uint32_t peek(std::uint32_t a) const { std::uint32_t v; std::memcpy(&v, ram->bytes().data() + (a & 0xFFFFFFu), 4u); return v; }
    void observe() {
        cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e) noexcept {
            writes.emplace_back(e.address, e.size, e.source, e.bytes_changed);
        }, GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
    }
};
void step(Fixture& f) {
    auto& cpu = f.cpu;
    const auto pc = cpu.pc;
    if (std::ranges::find(effect::retained_entries, pc) != effect::retained_entries.end() || pc == 0x8C07CF1Eu)
        f.calls.emplace_back(pc, cpu.r[4], cpu.pr);
    if (pc == 0x8C0DF6A0u) {
        // Graphics boundary only: both sides arrive with the actual task and
        // original PR. The game uses retained drawing; this is NOT a visual test.
        require(cpu.r[4] == Fixture::task, "wrong draw task");
        cpu.pc = cpu.pr; return;
    }
    const bool bound = (pc >= 0x8C0DF84Cu && pc < 0x8C0DF938u) ||
        (pc >= 0x8C0DF804u && pc < 0x8C0DF84Cu) ||
        (pc >= 0x8C0DDD5Cu && pc < 0x8C0DDE80u) ||
        (pc >= 0x8C07CF1Eu && pc < 0x8C07CF28u) ||
        (pc >= 0x8C63A8F8u && pc < 0x8C63A900u) ||
        (pc >= 0x8C0986C6u && pc < 0x8C0986CCu);
    if (!bound) std::cerr << "unbound pc=" << std::hex << pc << '\n';
    require(bound, "reference left original callback family");
    (void)execute_dynamic_sh4_block(cpu, services, 1u);
    require(!cpu.trap_pending && cpu.exception_generation == 0u, "original byte execution exception");
}
void run_until(Fixture& f, std::uint32_t stop) {
    for (unsigned i = 0; f.cpu.pc != stop && i < 10000u; ++i) step(f);
    require(f.cpu.pc == stop, "original callback did not return");
}
bool bridge(void* opaque, CpuState& cpu, std::uint32_t target) {
    auto& f = *static_cast<Fixture*>(opaque);
    require(&f.cpu == &cpu && cpu.pc == target, "bridge context");
    const auto ret = cpu.pr;
    require((target == 0x8C0DDD5Cu && ret == 0x8C0DF8EAu) ||
        (target == 0x8C0DF804u && ret == 0x8C0DF8FAu) ||
        (target == 0x8C63A8F8u && ret == 0x8C0DF912u) ||
        ((target == 0x8C0DF6A0u || target == 0x8C0986C6u) && ret == returned), "bridge lost original PR");
    run_until(f, ret); return true;
}
void compare(Fixture& n, Fixture& r) {
    n.observe(); r.observe();
    const auto result = effect::execute(n.cpu, {&n, bridge});
    const NativePortHookBinding binding{effect::entry,effect::size,
        NativePortHookKind::FunctionEntry,NativePortHookRequirement::Required,
        NativePortHookOriginalPolicy::ReplacesOriginal,{},{}};
    require(valid_native_port_hook_result(binding,result) && result.action==NativePortHookAction::Return &&
        n.cpu.pc==returned,"replacement did not complete its original tail");
    require(!valid_native_port_hook_result(binding,{NativePortHookAction::Jump,0x8C0DF6A0u,0u}),
        "fixture no longer reproduces the reported product contract");
    run_until(n, returned); run_until(r, returned);
    if (architecture(n.cpu) != architecture(r.cpu)) {
        for (unsigned i = 0; i < 16; ++i) {
            if (n.cpu.r[i] != r.cpu.r[i]) std::cerr << "r" << i << ' ' << std::hex << n.cpu.r[i] << '/' << r.cpu.r[i] << '\n';
            if (n.cpu.fr[i] != r.cpu.fr[i]) std::cerr << "fr" << i << ' ' << std::hex << n.cpu.fr[i] << '/' << r.cpu.fr[i] << '\n';
        }
        std::cerr << "FPSCR=" << std::hex << n.cpu.read_fpscr() << '/' << r.cpu.read_fpscr() << '\n';
        throw std::runtime_error("architecture differs");
    }
    require(std::ranges::equal(n.ram->bytes(), r.ram->bytes()), "RAM including stack differs");
    require(n.writes == r.writes, "ordered writes/sources differ");
    require(n.calls == r.calls, "original call targets/arguments/PR differ");
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "expected immutable boot.bin path");
        std::ifstream in(std::filesystem::path(argv[1]), std::ios::binary);
        const std::vector<std::uint8_t> boot{std::istreambuf_iterator<char>(in), {}};
        require(digest(boot) == "b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af", "wrong PAL boot identity");
        require(digest(std::span(boot).subspan(effect::entry - 0x8C010000u, effect::size)) == effect::source_sha256, "callback source identity");
        unsigned cases = 0u;
        for (const auto fpscr : {0x00040000u, 0x0004006Du}) {
            for (int variant = -1; variant < 14; ++variant) {
                Fixture n(boot, fpscr), r(boot, fpscr);
                for (auto* f : {&n, &r}) {
                    if (variant < 0) f->put(0x8C78C548u, 0u);
                    else if (variant < 5) {
                        constexpr std::array angles{0u, 0x2000u, 0x7FFFu, 0xFFFFFE00u, 0xFFFFFFFFu};
                        f->put(Fixture::work + 24u, angles[variant]);
                        f->put(Fixture::animation + 292u, variant & 1 ? 88u : 87u);
                    } else {
                        constexpr std::array states{0u, 11u, 17u, 18u, 15u, 20u, 16u, 21u, 22u};
                        f->put(Fixture::animation + 292u, 0u);
                        f->put(Fixture::character, states[variant - 5]);
                    }
                }
                compare(n, r); ++cases;
                if (variant >= 5) {
                    // Exercise the real fade owner through its deferred-delete
                    // handoff; it must already be in the retained callback family.
                    for (unsigned tick = 0; n.peek(Fixture::task + 16u) != 0x8C09859Cu && tick < 60u; ++tick) {
                        n.cpu.r[4] = Fixture::task; n.cpu.pr = returned; n.cpu.pc = n.peek(Fixture::task + 16u);
                        run_until(n, returned);
                    }
                    require(n.peek(Fixture::task + 16u) == 0x8C09859Cu, "fade never schedules deletion");
                }
            }
        }
        {
            Fixture f(boot, 0x40000u); f.observe(); bool caught = false;
            try { (void)effect::execute(f.cpu, {nullptr, [](void*, CpuState&, std::uint32_t) { return false; }}); }
            catch (const std::exception&) { caught = true; }
            require(caught && f.cpu.pc == 0x8C0DDD5Cu, "interrupted call was hidden"); ++cases;
        }
        for (const auto target : {0x8C0DF6A0u,0x8C0986C6u}) {
            Fixture f(boot,0x40000u);bool caught=false;
            if(target==0x8C0986C6u)f.put(0x8C78C548u,0u);
            struct TailFailure { Fixture* fixture; std::uint32_t target; } failure{&f,target};
            try {
                (void)effect::execute(f.cpu,{&failure,[](void* opaque,CpuState& cpu,std::uint32_t entry){
                    auto& value=*static_cast<TailFailure*>(opaque);
                    return entry!=value.target && bridge(value.fixture,cpu,entry);
                }});
            } catch(const std::exception&) {caught=true;}
            require(caught && f.cpu.pc==target,"interrupted original tail was hidden");++cases;
        }
        for (const auto fpscr : {fpscr_pr_mask, fpscr_sz_mask}) {
            Fixture f(boot, fpscr); f.observe(); bool caught = false;
            try { (void)effect::execute(f.cpu, {&f, bridge}); } catch (const std::exception&) { caught = true; }
            require(caught && f.writes.empty() && f.calls.empty(), "invalid ABI changed state"); ++cases;
        }
        std::cout << "AMY_HAMMER_EFFECT_TEST_OK cases=" << cases
            << " original_bytes registers_fpscr ram_stack ordered_writes calls_pr animation_sine fade_deletion failure_contract\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "AMY_HAMMER_EFFECT_TEST_FAIL " << error.what() << '\n'; return 1;
    }
}
