#include "sonic_collision_math.hpp"
#include "sonic_native_collision_memory.hpp"

#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"

#include <array>
#include <bit>
#include <cstring>
#include <span>

namespace sonic::collision_math {
namespace {
using namespace katana::runtime;
// Entire SHA-bound spans, including RTS delay slots. No calls or literal pools.
constexpr std::array<std::uint16_t, 33> cross_words{
    0xF959,0xE004,0xF449,0xF369,0xF941,0xF859,0xF431,0xF649,
    0xF369,0xF861,0xF548,0xF631,0xF758,0xF368,0xF751,0xF531,
    0xF38C,0xF842,0xF27C,0xF262,0xF352,0xF742,0xF231,0xF72A,
    0xF29C,0xF252,0xF962,0xF271,0xF891,0xF727,0xE008,0x000B,0xF787,
};
constexpr std::array<std::uint16_t, 8> length_words{
    0xF78D,0xF449,0xF549,0xF649,0xF5ED,0xF76D,0x000B,0xF07C,
};
constexpr std::array<std::uint16_t, 16> normalize_words{
    0xF049,0xF149,0xF249,0xF38D,0xF0ED,0xF40C,0xF03C,0xF37D,
    0xF232,0xF132,0xF432,0xF42B,0xF41B,0xF44B,0x000B,0xF032,
};
static_assert(sizeof(cross_words) == cross_size && sizeof(length_words) == length_size &&
    sizeof(normalize_words) == normalize_size);

bool admitted(const DirectLinearMemoryGuard& g, bool p0, std::uint32_t address,
              std::uint32_t size) noexcept {
    const bool alias = (address & 0xC0000000u) == 0x80000000u;
    const auto physical = address & 0x1FFFFFFFu;
    return (alias || (p0 && address >= 0x0C000000u && address < 0x0D000000u)) &&
        (address & 3u) == 0u && physical >= 0x0C000000u && physical < 0x0D000000u &&
        size <= 0x0D000000u - physical && g && g.physical_base == 0x0C000000u &&
        g.physical_span >= 0x01000000u && g.backing_mask == 0x00FFFFFFu;
}
bool overlaps(std::uint32_t address, std::uint32_t size,
              std::uint32_t code, std::uint32_t code_size) noexcept {
    const auto a = address & 0x1FFFFFFFu, b = code & 0x1FFFFFFFu;
    return a < std::uint64_t(b) + code_size && b < std::uint64_t(a) + size;
}
template<class Load,class Store>
void execute_body(CpuState& cpu,std::uint32_t target,const Load& load,const Store& store) {
    const auto postload = [&](unsigned reg, unsigned fr) {
        cpu.fr[fr] = load(cpu.r[reg]); cpu.r[reg] += 4u;
    };
    const auto sub = [&](unsigned source, unsigned destination) {
        fpu_binary(cpu, FpuBinaryOperation::Subtract, std::uint8_t(source), std::uint8_t(destination));
    };
    const auto mul = [&](unsigned source, unsigned destination) {
        fpu_binary(cpu, FpuBinaryOperation::Multiply, std::uint8_t(source), std::uint8_t(destination));
    };
    if (target == cross_entry) {
        // 7360..737E: retain B-A and A-C, not reassociated vector arithmetic.
        postload(5u,9u); cpu.r[0] = 4u;
        postload(4u,4u); postload(6u,3u); sub(4u,9u);
        postload(5u,8u); sub(3u,4u); postload(4u,6u);
        postload(6u,3u); sub(6u,8u); cpu.fr[5] = load(cpu.r[4]);
        sub(3u,6u); cpu.fr[7] = load(cpu.r[5]); cpu.fr[3] = load(cpu.r[6]);
        sub(5u,7u); sub(3u,5u);
        // 7380..73A0: interleaved arithmetic/stores and the final delay slot.
        cpu.fr[3] = cpu.fr[8]; mul(4u,8u); cpu.fr[2] = cpu.fr[7];
        mul(6u,2u); mul(5u,3u); mul(4u,7u); sub(3u,2u);
        store(0x8C02738Eu,cpu.r[7],2u);
        cpu.fr[2] = cpu.fr[9]; mul(5u,2u); mul(6u,9u); sub(7u,2u); sub(9u,8u);
        store(0x8C02739Au,cpu.r[7] + cpu.r[0],2u); cpu.r[0] = 8u;
        store(0x8C0273A0u,cpu.r[7] + cpu.r[0],8u);
    } else if (target == length_entry) {
        cpu.fr[7] = 0u; postload(4u,4u); postload(4u,5u); postload(4u,6u);
        fpu_inner_product(cpu,4u,4u); fpu_square_root(cpu,7u); cpu.fr[0] = cpu.fr[7];
    } else {
        postload(4u,0u); postload(4u,1u); postload(4u,2u); cpu.fr[3] = 0u;
        fpu_inner_product(cpu,0u,0u); cpu.fr[4] = cpu.fr[0]; cpu.fr[0] = cpu.fr[3];
        fpu_reciprocal_square_root(cpu,3u); mul(3u,2u); mul(3u,1u); mul(3u,4u);
        // FMOV @-Rn commits Rn after its scalar store/observer notification.
        store(0x8C63A8A2u,cpu.r[4] - 4u,2u); cpu.r[4] -= 4u;
        store(0x8C63A8A4u,cpu.r[4] - 4u,1u); cpu.r[4] -= 4u;
        store(0x8C63A8A6u,cpu.r[4] - 4u,4u); cpu.r[4] -= 4u;
        mul(3u,0u); // RTS delay slot: preserve FSRRA-derived result and flags.
    }
    cpu.pc = cpu.pr;
}
} // namespace

bool try_execute(katana::runtime::CpuState& cpu,
                 const katana::runtime::NativePortImmutableWriteGuard* immutable_guard) {
    using namespace katana::runtime;
    static_assert(std::endian::native == std::endian::little);
    std::span<const std::uint16_t> words;
    if (cpu.pc == cross_entry) words = cross_words;
    else if (cpu.pc == length_entry) words = length_words;
    else if (cpu.pc == normalize_entry) words = normalize_words;
    else return false;
    const auto fpscr = cpu.read_fpscr();
    if (!immutable_guard || immutable_guard->write_detected() || !cpu.privileged_mode_inline() ||
        cpu.trap_pending || cpu.sleeping || (cpu.sr & sr_fd_mask) != 0u ||
        (fpscr & (fpscr_pr_mask | fpscr_sz_mask | fpscr_exception_enable_mask)) != 0u ||
        (fpscr & fpscr_dn_mask) == 0u || (fpscr & fpscr_rounding_mode_mask) > 1u)
        return false;
    auto& memory = cpu.memory;
    if (memory.watchpoint_count() || memory.has_trace_handler() || memory.has_guest_memory_access_sink() ||
        memory.has_mmio_trace_handler() || !memory.guest_write_observer_allows_prevalidated_linear_writes())
        return false;
    const auto g = memory.direct_linear_memory_guard(false);
    const bool p0 = (cpu.mmucr & 1u) == 0u &&
        (!cpu.address_space || cpu.address_space->mode() == AddressTranslationMode::NoMmu);
    if (!admitted(g, p0, cpu.pc, std::uint32_t(words.size_bytes())) ||
        std::memcmp(g.read_bytes + (cpu.pc & 0xFFFFFFu), words.data(), words.size_bytes()) != 0 ||
        !admitted(g, p0, cpu.r[4], 12u)) return false;
    if (cpu.pc == cross_entry &&
        (!admitted(g, p0, cpu.r[5], 12u) || !admitted(g, p0, cpu.r[6], 12u))) return false;
    if (cpu.pc != length_entry) {
        const auto output = cpu.r[cpu.pc == cross_entry ? 7u : 4u];
        if (!admitted(g, p0, output, 12u) ||
            immutable_guard->tracks_address(output & 0x1FFFFFFFu, 12u) ||
            !memory.is_writable_linear_range(output & 0x1FFFFFFFu, 12u, false) ||
            overlaps(output, 12u, cross_entry, cross_size) ||
            overlaps(output, 12u, length_entry, length_size) ||
            overlaps(output, 12u, normalize_entry, normalize_size)) return false;
    }
    // All operands are read before any store in each original body. This proves
    // arbitrary data read/write aliasing (including in-place normalization) safe.
    // Each output consists of three distinct aligned words. Stable callbacks
    // cannot mutate the latched operands or memory mapping. No fallbacks below.
    collision_memory::Access access;
    // The read-only length leaf has only three accesses: retain its small path.
    if(cpu.pc!=length_entry)access.capture(cpu,*immutable_guard,g);
    const auto load = [&](std::uint32_t address) {
        std::uint32_t result = 0u;
        if(access.try_read(address,result))return result;
        (void)direct_linear_guard_read_u32(g, (address & 0x1FFFFFFFu) | 0x80000000u, result);
        return result;
    };
    const auto store = [&](std::uint32_t pc, std::uint32_t address, unsigned fr) {
        if(access.try_store(address,cpu.fr[fr]))return;
        if (!memory.try_write_direct_linear_u32(address & 0x1FFFFFFFu, cpu.fr[fr], CodeWriteSource::Fpu))
            guest_write_u32_at(cpu, GuestInstructionOrigin{pc, pc, true}, address, cpu.fr[fr], CodeWriteSource::Fpu);
    };
    const HostFpuExecutionEpoch epoch(cpu);
    execute_body(cpu,cpu.pc,load,store);
    return true;
}

bool try_execute_closed(katana::runtime::CpuState& cpu,std::uint32_t target,
    collision_memory::Access& access,bool p0,std::span<const ClosedWriteRange> writes) {
    if(!collision_memory::closure_enabled() || !access.direct() ||
       (target!=cross_entry && target!=length_entry && target!=normalize_entry))return false;
    // The parent's admission proves the exact closed source spans and holds the
    // FPU epoch. Only data-dependent operands remain to be checked here. There
    // is no guest callback, dispatch, mapping change or observable store hook.
    const auto vector_ok=[&](std::uint32_t a){
        const auto p=a&0x1FFFFFFFu;
        return !(a&3u) && ((a&0xC0000000u)==0x80000000u ||
            (p0 && a>=0x0C000000u && a<0x0D000000u)) &&
            p>=0x0C000000u && p<=0x0D000000u-12u;
    };
    if(!vector_ok(cpu.r[4]) || (target==cross_entry &&
       (!vector_ok(cpu.r[5]) || !vector_ok(cpu.r[6]))))return false;
    if(target!=length_entry){
        const auto output=cpu.r[target==cross_entry?7u:4u];
        if(!vector_ok(output))return false;
        const auto p=output&0x1FFFFFFFu;
        bool covered=false;
        for(const auto w:writes){
            const auto base=w.address&0x1FFFFFFFu;
            if(p>=base && w.size>=12u && p-base<=w.size-12u){covered=true;break;}
        }
        if(!covered)return false;
    }
    const auto load=[&](std::uint32_t a){std::uint32_t value=0;
        (void)access.try_read(a,value);return value;};
    const auto store=[&](std::uint32_t,std::uint32_t a,unsigned fr){
        (void)access.try_store(a,cpu.fr[fr]);};
    if(target==cross_entry)++collision_memory::counts.fused_cross;
    else if(target==length_entry)++collision_memory::counts.fused_length;
    else ++collision_memory::counts.fused_normalize;
    cpu.pc=target;
    execute_body(cpu,target,load,store);
    return true;
}
} // namespace sonic::collision_math
