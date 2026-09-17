#include "sonic_palette_lighting.hpp"
#include "sonic_model_pipeline.hpp"
#include "sonic_native_model_memory.hpp"
#include "sonic_palette_batch.hpp"

#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"

#include <array>
#include <bit>
#include <cstring>

namespace sonic::palette_lighting {
namespace {
using namespace katana::runtime;
constexpr std::uint32_t entry = 0x8C037350u;
constexpr std::uint32_t secondary = 0x8C03D760u;
constexpr std::uint32_t light = 0x8C038F24u;
constexpr std::uint32_t scale = 0x8C038F18u;
// Little-endian words from the SHA-bound 0x110-byte retail span, including its
// literal island. An altered runtime image must use the original executor.
constexpr std::array<std::uint16_t, 136> original_words{
    0xC60Du,0x6503u,0xC60Bu,0x6A03u,0x5241u,0x5742u,0xC60Fu,0x6803u,
    0x780Cu,0xD013u,0xC210u,0x6903u,0xD112u,0xFC19u,0xFD19u,0xFE19u,
    0xFF8Du,0xF3FDu,0xF01Cu,0xF23Cu,0xF3EDu,0xF45Cu,0xF67Cu,0xF7EDu,
    0xF89Cu,0xFABCu,0xFBEDu,0xF3FDu,0xFC3Cu,0xFD7Cu,0xFEBCu,0xFF8Du,
    0xD009u,0xF708u,0x9C0Au,0xD009u,0x20A8u,0x8B11u,0x5051u,0x2008u,
    0x890Eu,0xC616u,0x6503u,0x9B02u,0xA00Eu,0xEA03u,0x00FFu,0x0800u,
    0xD760u,0x8C03u,0x8F24u,0x8C03u,0x8F18u,0x8C03u,0x0000u,0x0008u,
    0xC616u,0x6503u,0xEB00u,0xEA03u,0x77FFu,0xF829u,0xF929u,0xFA29u,
    0xFB9Du,0xFBEDu,0xF029u,0xFB72u,0xF129u,0xFB70u,0xF229u,0xFB3Du,
    0x005Au,0xF39Du,0x4011u,0xF3EDu,0x8900u,0xE000u,0xF372u,0x30C7u,
    0xF370u,0x8B00u,0x60C3u,0xF33Du,0x0E5Au,0x40ADu,0x4E11u,0x305Cu,
    0x8900u,0xEE00u,0x6306u,0x3EC7u,0x00BEu,0x8B00u,0x6EC3u,0x2832u,
    0x4EADu,0x2902u,0x3E5Cu,0x7810u,0x63E6u,0x7904u,0x3EBCu,0x6EE2u,
    0xF829u,0x2832u,0xF929u,0x29E2u,0xFA29u,0x7810u,0xFB9Du,0x77FEu,
    0x4715u,0x8DCEu,0x7904u,0x2778u,0x8B10u,0xFBEDu,0xFB72u,0xFB70u,
    0xFB3Du,0x005Au,0x4011u,0x8900u,0xE000u,0x30C7u,0x8B00u,0x60C3u,
    0x40ADu,0x305Cu,0x6306u,0x00BEu,0x2832u,0x2902u,0x000Bu,0x0009u,
};

struct Range {
    std::uint32_t address;
    std::uint32_t size;
};

bool overlaps(Range a, Range b) noexcept {
    const auto x = a.address & 0x1FFFFFFFu;
    const auto y = b.address & 0x1FFFFFFFu;
    return x < std::uint64_t(y) + b.size && y < std::uint64_t(x) + a.size;
}

bool admitted_range(const DirectLinearMemoryGuard& guard, bool allow_p0, Range range) noexcept {
    const bool p1_p2 = (range.address & 0xC0000000u) == 0x80000000u;
    const bool physical_p0 = allow_p0 && range.address >= 0x0C000000u &&
        range.address < 0x0D000000u;
    if ((!p1_p2 && !physical_p0) || (range.address & 3u) != 0u || range.size == 0u)
        return false;
    const auto physical = range.address & 0x1FFFFFFFu;
    // Deliberately admit only the first, contiguous 16 MiB main-RAM window.
    // This also makes overlap checks independent of admitted P0/P1/P2 aliases.
    return physical >= 0x0C000000u && physical < 0x0D000000u &&
        range.size <= 0x0D000000u - physical && guard &&
        guard.physical_base == 0x0C000000u &&
        guard.physical_span >= 0x01000000u && guard.backing_mask == 0x00FFFFFFu;
}

std::uint32_t peek(const DirectLinearMemoryGuard& guard, std::uint32_t address) noexcept {
    std::uint32_t word;
    std::memcpy(&word, guard.read_bytes + (address & 0x00FFFFFFu), sizeof(word));
    return word;
}
} // namespace

bool try_execute(katana::runtime::CpuState& cpu,
                 const katana::runtime::NativePortImmutableWriteGuard* immutable_guard) {
    using namespace katana::runtime;
    static_assert(std::endian::native == std::endian::little);
    const auto fpscr = cpu.read_fpscr();
    if (immutable_guard == nullptr || immutable_guard->write_detected() ||
        cpu.pc != entry || !cpu.privileged_mode_inline() || cpu.trap_pending || cpu.sleeping ||
        (cpu.sr & sr_fd_mask) != 0u ||
        (fpscr & (fpscr_pr_mask | fpscr_sz_mask | fpscr_exception_enable_mask)) != 0u ||
        (fpscr & fpscr_dn_mask) == 0u || (fpscr & fpscr_rounding_mode_mask) > 1u)
        return false;
    // General callbacks may mutate later operands or mappings. Stable code
    // invalidation observers are retained by every ordinary guest store below.
    auto& memory = cpu.memory;
    if (memory.watchpoint_count() != 0u || memory.has_trace_handler() ||
        memory.has_guest_memory_access_sink() || memory.has_mmio_trace_handler() ||
        !memory.guest_write_observer_allows_prevalidated_linear_writes())
        return false;
    const auto* shared=sonic::model_pipeline::active;
    if(shared && shared->cpu!=&cpu)shared=nullptr;
    const auto guard = shared ? shared->memory : memory.direct_linear_memory_guard(false);
    // MMU control projects MMUCR.AT into RuntimeAddressSpace::mode(). Require
    // both views to say NoMmu before admitting P0; a stale/inconsistent binding
    // must fall back. P1/P2 remain untranslated under either MMU mode.
    const bool allow_p0 = (cpu.mmucr & 1u) == 0u &&
        (!cpu.address_space || cpu.address_space->mode() == AddressTranslationMode::NoMmu);
    if (!admitted_range(guard, allow_p0, {entry, 0x110u}) ||
        !admitted_range(guard, allow_p0, {cpu.gbr, 92u}) ||
        !admitted_range(guard, allow_p0, {cpu.r[4], 12u}) ||
        std::memcmp(guard.read_bytes + 0x37350u, original_words.data(), 0x110u) != 0)
        return false;

    // Preflight reads deliberately do not publish register or metrics changes.
    const auto normals = peek(guard, cpu.r[4] + 4u);
    const auto count = peek(guard, cpu.r[4] + 8u);
    if (count < 2u || count > 65536u) return false;
    const auto material = peek(guard, cpu.gbr + 52u);
    const auto flags = peek(guard, cpu.gbr + 44u);
    const auto positions = peek(guard, cpu.gbr + 60u);
    const auto palette = peek(guard, cpu.gbr + 88u);
    const bool read_material = (flags & 0x80000u) == 0u;
    if (read_material && !admitted_range(guard, allow_p0, {material, 8u})) return false;
    const auto bank = read_material && peek(guard, material + 4u) != 0u ? 0x800u : 0u;
    const std::array reads{
        Range{entry, 0x110u}, Range{cpu.r[4], 12u},
        Range{cpu.gbr + 44u, 4u}, Range{cpu.gbr + 52u, 4u},
        Range{cpu.gbr + 60u, 4u}, Range{cpu.gbr + 88u, 4u},
        Range{light, 12u}, Range{scale, 4u},
        // The paired loop prefetches one extra normal for EVEN counts.
        Range{normals, (count + ((count & 1u) == 0u ? 1u : 0u)) * 12u},
        Range{palette, 0x800u + bank},
        Range{read_material ? material : light, read_material ? 8u : 12u},
    };
    // Bounding the strided primary records is intentionally conservative.
    const std::array writes{
        Range{cpu.gbr + 64u, 4u}, Range{positions, count * 16u},
        Range{secondary, count * 4u},
    };
    for (const auto range : reads)
        if (!admitted_range(guard, allow_p0, range)) return false;
    for (std::size_t i = 0; i < writes.size(); ++i) {
        const auto range = writes[i];
        if (!admitted_range(guard, allow_p0, range) ||
            immutable_guard->tracks_address(range.address & 0x1FFFFFFFu, range.size) ||
            !memory.is_writable_linear_range(range.address & 0x1FFFFFFFu, range.size, false))
            return false;
        for (const auto source : reads)
            if (overlaps(range, source)) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (overlaps(range, writes[j])) return false;
    }

    // From here admission is complete: no fallback after any guest mutation.
    sonic::model_memory::ClosedLeafWrites native_writes(cpu,*immutable_guard,guard);
    const auto load = [&](std::uint32_t address) {
        std::uint32_t value = 0;
        // This SDK helper accepts only P1/P2. Admission above proves that an
        // accepted P0 read has the same backing as its P1 alias. Registers and
        // the guest store's original virtual address are never rewritten.
        (void)direct_linear_guard_read_u32(guard, (address & 0x1FFFFFFFu) | 0x80000000u, value);
        return value; // all addresses and the stable observer contract proven above
    };
    const auto store = [&](std::uint32_t pc, std::uint32_t address, std::uint32_t value) {
        if(native_writes.try_store(address,value,CodeWriteSource::Cpu))return;
        // The preflight proves untranslated, writable RAM with a stable scalar
        // observer. This is the same Memory helper used by guest_write_u32_at:
        // it commits one store and immediately reports its original physical
        // address, Cpu source and changed flag. A miss is mutation-free; retain
        // the scalar guest path with its original PC/address in that case.
        if (!memory.try_write_direct_linear_u32(address & 0x1FFFFFFFu, value, CodeWriteSource::Cpu))
            guest_write_u32_at(cpu, GuestInstructionOrigin{pc, pc, true}, address, value);
    };
    const bool captured=shared && shared->model==cpu.r[4] && shared->normals_address==normals && shared->count==count;
    const auto* normal_data=captured ? reinterpret_cast<const std::uint8_t*>(shared->normals.data()) : guard.read_bytes+(normals&0xFFFFFFu);
    if(captured)sonic::model_pipeline::note_normal_reuse();
    const auto normal_word = [&](unsigned fr) {
        std::memcpy(&cpu.fr[fr],normal_data+(cpu.r[2]-normals),4u);cpu.r[2]+=4u;
    };
    const auto nonnegative = [](std::uint32_t value) { return (value & 0x80000000u) == 0u; };
    const auto above_max = [](std::uint32_t value) {
        return std::bit_cast<std::int32_t>(value) > 255;
    };
    const HostFpuExecutionEpoch epoch(cpu);
    // 7350..73C6: material selection and light vector in object coordinates.
    cpu.r[0] = load(cpu.gbr + 52u); cpu.r[5] = cpu.r[0];
    cpu.r[0] = load(cpu.gbr + 44u); cpu.r[10] = cpu.r[0];
    cpu.r[2] = load(cpu.r[4] + 4u); cpu.r[7] = load(cpu.r[4] + 8u);
    cpu.r[0] = load(cpu.gbr + 60u); cpu.r[8] = cpu.r[0] + 12u;
    cpu.r[0] = secondary; store(0x8C037364u, cpu.gbr + 64u, cpu.r[0]);
    cpu.r[9] = cpu.r[0]; cpu.r[1] = light;
    for (unsigned f = 12u; f < 15u; ++f) { cpu.fr[f] = load(cpu.r[1]); cpu.r[1] += 4u; }
    cpu.fr[15] = 0u;
    cpu.write_fpscr(cpu.read_fpscr() ^ fpscr_sz_mask);
    write_fpu_pair_bits(cpu, 0u, read_fpu_pair_bits(cpu, 1u));
    write_fpu_pair_bits(cpu, 2u, read_fpu_pair_bits(cpu, 3u));
    fpu_inner_product(cpu, 12u, 0u);
    write_fpu_pair_bits(cpu, 4u, read_fpu_pair_bits(cpu, 5u));
    write_fpu_pair_bits(cpu, 6u, read_fpu_pair_bits(cpu, 7u));
    fpu_inner_product(cpu, 12u, 4u);
    write_fpu_pair_bits(cpu, 8u, read_fpu_pair_bits(cpu, 9u));
    write_fpu_pair_bits(cpu, 10u, read_fpu_pair_bits(cpu, 11u));
    fpu_inner_product(cpu, 12u, 8u);
    cpu.write_fpscr(cpu.read_fpscr() ^ fpscr_sz_mask);
    cpu.fr[12] = cpu.fr[3]; cpu.fr[13] = cpu.fr[7]; cpu.fr[14] = cpu.fr[11]; cpu.fr[15] = 0u;
    cpu.r[0] = scale; cpu.fr[7] = load(cpu.r[0]); cpu.r[12] = 255u;
    cpu.r[0] = 0x80000u; cpu.t = (cpu.r[0] & cpu.r[10]) == 0u;
    if (cpu.t) { cpu.r[0] = load(cpu.r[5] + 4u); cpu.t = cpu.r[0] == 0u; }
    cpu.r[0] = load(cpu.gbr + 88u); cpu.r[5] = cpu.r[0];
    cpu.r[11] = bank; cpu.r[10] = 3u;
    --cpu.r[7]; normal_word(8u); normal_word(9u); normal_word(10u); cpu.fr[11] = 0x3F800000u;

    thread_local sonic::palette_batch::Result batch;
    const bool batched=sonic::palette_batch::enabled() &&
        sonic::palette_batch::prepare(cpu,normal_data,count,
            cpu.fr.data()+12u,cpu.fr[7],batch);
    if(batched){++sonic::palette_batch::counts.calls;sonic::palette_batch::counts.vertices+=count;}
    else if(sonic::palette_batch::enabled())++sonic::palette_batch::counts.declined;
    if(batched && native_writes.direct()) {
        // The exact registered product observer cannot inspect transient CPU
        // registers or alter operands. Commit the two color streams directly,
        // retaining their original order, then publish the loop's complete
        // architectural end state once. Arbitrary observers use the loop below.
        std::uint32_t primary=0,second_color=0,previous_second=0;
        for(std::uint32_t i=0;i<count;++i){
            const auto signed_index=std::bit_cast<std::int32_t>(batch.integers[i]);
            const auto index=std::uint32_t(signed_index<0?0:signed_index>255?255:signed_index);
            primary=peek(guard,palette+index*8u);
            previous_second=second_color;
            second_color=peek(guard,palette+index*8u+4u+bank);
            native_writes.try_store(positions+12u+i*16u,primary,CodeWriteSource::Cpu);
            native_writes.try_store(secondary+i*4u,second_color,CodeWriteSource::Cpu);
        }
        const auto paired=count&~1u;
        const auto paired_last=paired-1u;
        cpu.r[0]=(count&1u)?second_color:previous_second;
        cpu.r[2]=normals+(paired+1u)*12u;
        cpu.r[3]=primary;
        cpu.r[7]=(count&1u)?0u:0xFFFFFFFFu;
        cpu.r[8]=positions+12u+paired*16u;
        cpu.r[9]=secondary+paired*4u;
        cpu.r[14]=(count&1u)?previous_second:second_color;
        for(unsigned axis=0;axis<3;++axis){
            cpu.fr[axis]=peek(guard,normals+paired_last*12u+axis*4u);
            cpu.fr[8u+axis]=peek(guard,normals+paired*12u+axis*4u);
        }
        cpu.fr[3]=batch.scaled[paired_last];
        cpu.fr[11]=(count&1u)?batch.scaled[count-1u]:0x3F800000u;
        cpu.fpul=batch.integers[count-1u];
        cpu.t=(count&1u) && std::bit_cast<std::int32_t>(cpu.fpul)>255;
        cpu.fpscr &= ~fpscr_cause_mask;
        cpu.pc=cpu.pr;
        ++sonic::palette_batch::counts.closed_loops;
        return true;
    }
    std::size_t vertex=0;

    // 73D2..7434: original paired, pipelined loop, including interleaved stores
    // and the final read-ahead. Replacing it with a count loop loses end state.
    do {
        if(!batched)fpu_inner_product(cpu, 12u, 8u);
        normal_word(0u); if(!batched)fpu_binary(cpu, FpuBinaryOperation::Multiply, 7u, 11u);
        normal_word(1u); if(!batched)fpu_binary(cpu, FpuBinaryOperation::Add, 7u, 11u);
        normal_word(2u);
        if(batched){cpu.fr[11]=batch.scaled[vertex];cpu.fpul=batch.integers[vertex];}
        else fpu_truncate_to_fpul(cpu, 11u);
        cpu.r[0] = cpu.fpul; cpu.fr[3] = 0x3F800000u; cpu.t = nonnegative(cpu.r[0]);
        if(!batched)fpu_inner_product(cpu, 12u, 0u); if (!cpu.t) cpu.r[0] = 0u;
        if(!batched)fpu_binary(cpu, FpuBinaryOperation::Multiply, 7u, 3u);
        cpu.t = above_max(cpu.r[0]); if(!batched)fpu_binary(cpu, FpuBinaryOperation::Add, 7u, 3u);
        if (cpu.t) cpu.r[0] = cpu.r[12];
        if(batched){cpu.fr[3]=batch.scaled[vertex+1];cpu.fpul=batch.integers[vertex+1];}
        else fpu_truncate_to_fpul(cpu, 3u);
        cpu.r[14] = cpu.fpul; cpu.r[0] <<= 3u;
        cpu.t = nonnegative(cpu.r[14]); cpu.r[0] += cpu.r[5]; if (!cpu.t) cpu.r[14] = 0u;
        cpu.r[3] = load(cpu.r[0]); cpu.r[0] += 4u;
        cpu.t = above_max(cpu.r[14]); cpu.r[0] = load(cpu.r[0] + cpu.r[11]);
        if (cpu.t) cpu.r[14] = cpu.r[12];
        store(0x8C03740Eu, cpu.r[8], cpu.r[3]); cpu.r[14] <<= 3u;
        store(0x8C037412u, cpu.r[9], cpu.r[0]); cpu.r[14] += cpu.r[5]; cpu.r[8] += 16u;
        cpu.r[3] = load(cpu.r[14]); cpu.r[14] += 4u; cpu.r[9] += 4u;
        cpu.r[14] += cpu.r[11]; cpu.r[14] = load(cpu.r[14]);
        normal_word(8u); store(0x8C037422u, cpu.r[8], cpu.r[3]);
        normal_word(9u); store(0x8C037426u, cpu.r[9], cpu.r[14]);
        normal_word(10u); cpu.r[8] += 16u; cpu.fr[11] = 0x3F800000u;
        cpu.r[7] -= 2u; cpu.t = std::bit_cast<std::int32_t>(cpu.r[7]) > 0;
        cpu.r[9] += 4u; // BT/S delay slot, taken or not
        vertex+=2;
    } while (cpu.t);
    cpu.t = cpu.r[7] == 0u;
    if (cpu.t) {
        // 743A..745A: odd final record; output pointers are NOT incremented.
        if(batched){cpu.fr[11]=batch.scaled[vertex];cpu.fpul=batch.integers[vertex];}
        else {
            fpu_inner_product(cpu, 12u, 8u);
            fpu_binary(cpu, FpuBinaryOperation::Multiply, 7u, 11u);
            fpu_binary(cpu, FpuBinaryOperation::Add, 7u, 11u);
            fpu_truncate_to_fpul(cpu, 11u);
        }
        cpu.r[0] = cpu.fpul;
        cpu.t = nonnegative(cpu.r[0]); if (!cpu.t) cpu.r[0] = 0u;
        cpu.t = above_max(cpu.r[0]); if (cpu.t) cpu.r[0] = cpu.r[12];
        cpu.r[0] <<= 3u; cpu.r[0] += cpu.r[5];
        cpu.r[3] = load(cpu.r[0]); cpu.r[0] += 4u;
        cpu.r[0] = load(cpu.r[0] + cpu.r[11]);
        store(0x8C037458u, cpu.r[8], cpu.r[3]); store(0x8C03745Au, cpu.r[9], cpu.r[0]);
    }
    if(batched)cpu.fpscr &= ~fpscr_cause_mask;
    cpu.pc = cpu.pr;
    return true;
}
} // namespace sonic::palette_lighting
