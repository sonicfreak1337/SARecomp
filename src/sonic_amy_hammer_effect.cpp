#include "sonic_amy_hammer_effect.hpp"
#include "katana/runtime/fpu.hpp"
#include <stdexcept>

namespace sonic::amy_hammer_effect {
katana::runtime::NativePortHookResult execute(
    katana::runtime::CpuState& cpu, const RetainedCallBridge& bridge) {
    using namespace katana::runtime;
    // The original caller uses single precision/moves (FLDI1 and FMAC here
    // require PR=0). Reject an invalid calling contract before changing RAM.
    if (cpu.pc != entry || !bridge.invoke || cpu.fpu_disabled() ||
        (cpu.read_fpscr() & (fpscr_pr_mask | fpscr_sz_mask)) || cpu.trap_pending)
        throw std::runtime_error("Amy hammer effect entry contract");
    auto& r = cpu.r;
    auto& fr = cpu.fr;
    const auto load = [&](std::uint32_t pc, std::uint32_t address) {
        cpu.pc = pc;
        return guest_read_u32_at(cpu, GuestInstructionOrigin{pc, pc, true}, address);
    };
    const auto store = [&](std::uint32_t pc, std::uint32_t address,
                           std::uint32_t value, CodeWriteSource source = CodeWriteSource::Cpu) {
        cpu.pc = pc;
        guest_write_u32_at(cpu, GuestInstructionOrigin{pc, pc, true}, address, value, source);
    };
    const auto push = [&](std::uint32_t pc, std::uint32_t value) {
        r[15] -= 4u; store(pc, r[15], value);
    };
    const auto pop = [&](std::uint32_t pc) {
        const auto value = load(pc, r[15]); r[15] += 4u; return value;
    };
    const auto call = [&](std::uint32_t target, std::uint32_t continuation) {
        cpu.pc = target; cpu.pr = continuation;
        if (!bridge.invoke(bridge.context, cpu, target) || cpu.pc != continuation || cpu.trap_pending)
            throw std::runtime_error("Amy hammer effect interrupted retained call");
    };
    const auto tail = [&](std::uint32_t target) {
        cpu.pc = target;
        return NativePortHookResult{NativePortHookAction::Jump, target, 0u};
    };

    r[3] = load(0x8C0DF84Cu, 0x8C0DF880u);
    push(0x8C0DF84Eu, r[14]);
    r[14] = r[4];
    push(0x8C0DF852u, r[13]);
    push(0x8C0DF854u, cpu.pr);
    r[2] = load(0x8C0DF856u, r[3]);
    cpu.t = r[2] == 0u;
    r[13] = load(0x8C0DF85Cu, r[14] + 32u); // BF/S delay slot, both branches
    if (cpu.t) {
        cpu.pr = pop(0x8C0DF85Eu);
        r[2] = load(0x8C0DF860u, 0x8C0DF8D8u);
        r[4] = r[14];
        r[13] = pop(0x8C0DF864u);
        const auto target = r[2];
        r[14] = pop(0x8C0DF868u); // JMP delay slot
        return tail(target); // original deferred deletion, never free synchronously
    }

    r[3] = load(0x8C0DF8E0u, 0x8C0DFAA0u);
    r[2] = load(0x8C0DF8E2u, 0x8C0DFAA4u);
    store(0x8C0DF8E4u, r[14] + 20u, r[3]); // original display callback
    r[4] = 0u;
    call(r[2], 0x8C0DF8EAu);
    cpu.t = r[0] == 9u;
    if (!cpu.t) {
        r[0] = 0x8C0DFAA8u;
        fr[3] = load(0x8C0DF8F0u, r[0]);
        r[0] = 44u;
        store(0x8C0DF8F4u, r[13] + r[0], fr[3], CodeWriteSource::Fpu);
        r[4] = r[14];
        call(0x8C0DF804u, 0x8C0DF8FAu); // execute fade once before installing it
        r[2] = load(0x8C0DF8FAu, 0x8C0DFAACu);
        store(0x8C0DF8FEu, r[14] + 16u, r[2]); // BRA delay slot
        cpu.pr = pop(0x8C0DF930u);
        r[13] = pop(0x8C0DF932u);
        const auto target = cpu.pr;
        r[14] = pop(0x8C0DF936u);
        cpu.pc = target;
        return {NativePortHookAction::Return, 0u, 0u};
    }

    r[1] = load(0x8C0DF900u, r[13] + 24u);
    r[3] = 2u << 8u;
    r[2] = load(0x8C0DF906u, 0x8C0DFAB0u);
    r[1] += r[3];
    r[4] = r[1];
    store(0x8C0DF90Cu, r[13] + 24u, r[1]);
    r[4] <<= 2u; // JSR delay slot; original integer wrap and sine table
    call(r[2], 0x8C0DF912u);
    r[0] = 0x8C0DFAB4u;
    fr[3] = fr[0];
    fr[0] = load(0x8C0DF916u, r[0]);
    r[0] = 12u;
    fr[4] = 0x3F800000u; // FLDI1
    cpu.pc = 0x8C0DF91Cu;
    fpu_multiply_accumulate(cpu, 3u, 4u);
    if (cpu.trap_pending) throw std::runtime_error("Amy hammer effect FMAC exception");
    r[4] = load(0x8C0DF91Eu, 0x8C0DFAB8u);
    store(0x8C0DF920u, r[4] + r[0], fr[4], CodeWriteSource::Fpu);
    r[0] = 16u;
    store(0x8C0DF924u, r[4] + r[0], fr[4], CodeWriteSource::Fpu);
    r[4] = r[14];
    cpu.pr = pop(0x8C0DF928u);
    r[13] = pop(0x8C0DF92Au);
    r[14] = pop(0x8C0DF92Eu);
    return tail(0x8C0DF6A0u);
}
} // namespace sonic::amy_hammer_effect
