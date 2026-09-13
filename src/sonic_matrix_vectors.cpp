#include "sonic_matrix_vectors.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <bit>
#include <cstring>
#include <optional>
#include <span>
namespace sonic::matrix_vectors {
namespace {
using namespace katana::runtime;
constexpr std::array<std::uint16_t,44> point_words{
    0x2448u,0xF059u,0xF159u,0xF259u,0x8F06u,0xF39Du,0x760Cu,0xF1FDu,0xF62Bu,0xF61Bu,
    0x000Bu,0xF60Bu,0xE010u,0xF448u,0xF546u,0xE020u,0xF646u,0xE030u,0xF746u,0xF4EDu,
    0xE004u,0xF846u,0xE014u,0xF946u,0xE024u,0xFA46u,0xE034u,0xFB46u,0xF67Au,0xF8EDu,
    0x7604u,0xE008u,0xF446u,0xE018u,0xF546u,0xE028u,0xF646u,0xE038u,0xF746u,0xF6BAu,
    0xF4EDu,0x7604u,0x000Bu,0xF67Au,
};
constexpr std::array<std::uint16_t,52> direction_words{
    0xF059u,0x2448u,0xF159u,0xF259u,0x8F02u,0xF38Du,0xA01Bu,0xF1FDu,0xE010u,0xF448u,
    0xF546u,0xE020u,0xF646u,0xF78Du,0xF4EDu,0xE004u,0xF846u,0xE014u,0xF946u,0xE024u,
    0xFA46u,0xFB8Du,0xF8EDu,0xFA7Cu,0xE008u,0xF446u,0xE018u,0xF546u,0xE028u,0xF646u,
    0xF78Du,0xF4EDu,0xF0ACu,0xF1BCu,0xF27Cu,0xF38Du,0xD306u,0x6332u,0x2338u,0x8D05u,
    0x760Cu,0xF0EDu,0xF37Du,0xF232u,0xF132u,0xF032u,0xF62Bu,0xF61Bu,0x000Bu,0xF60Bu,
    0xFFB0u,0x8C88u,
};
constexpr std::array<std::uint16_t,22> store_words{
    0x7440u,0xFBFDu,0xF4FBu,0xF4EBu,0xF4DBu,0xF4CBu,0xF4BBu,0xF4ABu,0xF49Bu,0xF48Bu,
    0xF47Bu,0xF46Bu,0xF45Bu,0xF44Bu,0xF43Bu,0xF42Bu,0xF41Bu,0xF40Bu,0xFBFDu,0x0009u,
    0x000Bu,0x0009u,
};
constexpr std::array<std::uint16_t,16> translation_words{
    0x750Cu,0x2448u,0x8B04u,0xF3FDu,0xF0DCu,0xF2FCu,0xA004u,0xF3FDu,0x7430u,0xF049u,
    0xF149u,0xF249u,0xF52Bu,0xF51Bu,0x000Bu,0xF50Bu,
};

struct Range { std::uint32_t address,size; };
bool overlaps(Range a,Range b) noexcept {
    const auto x=a.address&0x1FFFFFFFu,y=b.address&0x1FFFFFFFu;
    return x<std::uint64_t(y)+b.size && y<std::uint64_t(x)+a.size;
}
bool admitted(const DirectLinearMemoryGuard& g,bool p0,Range r) noexcept {
    const auto p=r.address&0x1FFFFFFFu;
    const bool segment=(r.address&0xC0000000u)==0x80000000u ||
        (p0 && r.address>=0x0C000000u && r.address<0x0D000000u);
    return segment && !(r.address&3u) && r.size && p>=0x0C000000u &&
        p<0x0D000000u && r.size<=0x0D000000u-p && g &&
        g.physical_base==0x0C000000u && g.physical_span>=0x01000000u &&
        g.backing_mask==0x00FFFFFFu;
}
} // namespace
bool try_execute(katana::runtime::CpuState& cpu,
                 const katana::runtime::NativePortImmutableWriteGuard* immutable) {
    using namespace katana::runtime;
    static_assert(std::endian::native==std::endian::little);
    unsigned leaf=0u;
    while(leaf<leaves.size() && leaves[leaf].entry!=cpu.pc) ++leaf;
    if(leaf==leaves.size() || !immutable || immutable->write_detected() ||
       !cpu.privileged_mode_inline() || cpu.trap_pending || cpu.sleeping ||
       (cpu.sr&sr_fd_mask) || (cpu.fpscr&(fpscr_pr_mask|fpscr_sz_mask))) return false;
    if(leaf<2u && ((cpu.fpscr&fpscr_exception_enable_mask) ||
       !(cpu.fpscr&fpscr_dn_mask) || (cpu.fpscr&fpscr_rounding_mode_mask)>1u)) return false;
    auto& memory=cpu.memory;
    if(memory.watchpoint_count() || memory.has_trace_handler() || memory.has_guest_memory_access_sink() ||
       memory.has_mmio_trace_handler() || !memory.guest_write_observer_allows_prevalidated_linear_writes()) return false;
    const auto g=memory.direct_linear_memory_guard(false);
    const bool p0=!(cpu.mmucr&1u) &&
        (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu);
    const std::array<std::span<const std::uint16_t>,4> original{
        point_words,direction_words,store_words,translation_words};
    const auto code=leaves[leaf];
    if(!admitted(g,p0,{code.entry,code.size}) ||
       std::memcmp(g.read_bytes+(code.entry&0xFFFFFFu),original[leaf].data(),code.size)) return false;
    const Range output=leaf==2u?Range{cpu.r[4],64u}:Range{cpu.r[leaf==3u?5u:6u],12u};
    if(!admitted(g,p0,output) || immutable->tracks_address(output.address&0x1FFFFFFFu,output.size) ||
       !memory.is_writable_linear_range(output.address&0x1FFFFFFFu,output.size,false)) return false;
    for(const auto s:leaves) if(overlaps(output,{s.entry,s.size})) return false;
    if(leaf<2u && (!admitted(g,p0,{cpu.r[5],12u}) ||
       (cpu.r[4] && !admitted(g,p0,{cpu.r[4],64u})))) return false;
    if(leaf==1u && !admitted(g,p0,{0x8C88FFB0u,4u})) return false;
    if(leaf==3u && cpu.r[4] && !admitted(g,p0,{cpu.r[4],60u})) return false;
    // All possible loads and stores have been admitted. No callback, MMIO,
    // safepoint or original fallback may be introduced below this boundary.
    const auto load=[&](std::uint32_t a) {
        std::uint32_t value=0u;
        (void)direct_linear_guard_read_u32(g,(a&0x1FFFFFFFu)|0x80000000u,value);
        return value;
    };
    const auto store=[&](std::uint32_t pc,std::uint32_t a,std::uint32_t value) {
        if(!memory.try_write_direct_linear_u32(a&0x1FFFFFFFu,value,CodeWriteSource::Fpu))
            guest_write_u32_at(cpu,GuestInstructionOrigin{pc,pc,true},a,value,CodeWriteSource::Fpu);
    };
    const auto prestore=[&](unsigned reg,unsigned fr,std::uint32_t pc) {
        const auto a=cpu.r[reg]-4u;store(pc,a,cpu.fr[fr]);cpu.r[reg]=a;
    };
    const auto input=[&] {
        for(unsigned i=0;i<3u;++i) {cpu.fr[i]=load(cpu.r[5]);cpu.r[5]+=4u;}
    };
    const auto row=[&](unsigned base,unsigned column,bool translate) {
        // Preserve R0 and row-read order, including reads after earlier output
        // writes in the point RAM branch. Input/output/matrix aliasing is legal.
        if(column==0u) {
            cpu.r[0]=16u;cpu.fr[base]=load(cpu.r[4]);
            cpu.fr[base+1u]=load(cpu.r[4]+cpu.r[0]);
        } else {
            cpu.r[0]=column*4u;cpu.fr[base]=load(cpu.r[4]+cpu.r[0]);
            cpu.r[0]=16u+column*4u;cpu.fr[base+1u]=load(cpu.r[4]+cpu.r[0]);
        }
        cpu.r[0]=32u+column*4u;cpu.fr[base+2u]=load(cpu.r[4]+cpu.r[0]);
        if(translate) {cpu.r[0]=48u+column*4u;cpu.fr[base+3u]=load(cpu.r[4]+cpu.r[0]);}
        else cpu.fr[base+3u]=0u;
    };
    std::optional<HostFpuExecutionEpoch> epoch;
    if(leaf<2u) epoch.emplace(cpu);
    if(leaf==0u) {
        cpu.t=cpu.r[4]==0u;input();cpu.fr[3]=0x3F800000u;
        if(cpu.t) {
            cpu.r[6]+=12u;fpu_transform_vector(cpu,0u);
            prestore(6u,2u,0x8C638E1Cu);prestore(6u,1u,0x8C638E1Eu);prestore(6u,0u,0x8C638E22u);
        } else {
            row(4u,0u,true);fpu_inner_product(cpu,0u,4u);
            row(8u,1u,true);store(0x8C638E44u,cpu.r[6],cpu.fr[7]);
            fpu_inner_product(cpu,0u,8u);cpu.r[6]+=4u;
            row(4u,2u,true);store(0x8C638E5Au,cpu.r[6],cpu.fr[11]);
            fpu_inner_product(cpu,0u,4u);cpu.r[6]+=4u;
            store(0x8C638E62u,cpu.r[6],cpu.fr[7]);
        }
    } else if(leaf==1u) {
        input();cpu.t=cpu.r[4]==0u;cpu.fr[3]=0u;
        if(cpu.t) fpu_transform_vector(cpu,0u);
        else {
            row(4u,0u,false);fpu_inner_product(cpu,0u,4u);
            row(8u,1u,false);fpu_inner_product(cpu,0u,8u);cpu.fr[10]=cpu.fr[7];
            row(4u,2u,false);fpu_inner_product(cpu,0u,4u);
            cpu.fr[0]=cpu.fr[10];cpu.fr[1]=cpu.fr[11];cpu.fr[2]=cpu.fr[7];
        }
        cpu.fr[3]=0u;cpu.r[3]=0x8C88FFB0u;cpu.r[3]=load(cpu.r[3]);
        cpu.t=cpu.r[3]==0u;cpu.r[6]+=12u;
        if(!cpu.t) {
            fpu_inner_product(cpu,0u,0u);fpu_reciprocal_square_root(cpu,3u);
            fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,2u);
            fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,1u);
            fpu_binary(cpu,FpuBinaryOperation::Multiply,3u,0u);
        }
        prestore(6u,2u,0x8C638EC4u);prestore(6u,1u,0x8C638EC6u);prestore(6u,0u,0x8C638ECAu);
    } else if(leaf==2u) {
        cpu.r[4]+=64u;cpu.write_fpscr(cpu.fpscr^fpscr_fr_mask);
        for(unsigned i=0;i<16u;++i) prestore(4u,15u-i,0x8C638ED8u+2u*i);
        cpu.write_fpscr(cpu.fpscr^fpscr_fr_mask);
    } else {
        cpu.r[5]+=12u;cpu.t=cpu.r[4]==0u;
        if(cpu.t) {
            cpu.write_fpscr(cpu.fpscr^fpscr_sz_mask);
            write_fpu_pair_bits(cpu,0u,read_fpu_pair_bits(cpu,13u));
            write_fpu_pair_bits(cpu,2u,read_fpu_pair_bits(cpu,15u));
            cpu.write_fpscr(cpu.fpscr^fpscr_sz_mask);
        } else {
            cpu.r[4]+=48u;
            for(unsigned i=0;i<3u;++i) {cpu.fr[i]=load(cpu.r[4]);cpu.r[4]+=4u;}
        }
        prestore(5u,2u,0x8C638F18u);prestore(5u,1u,0x8C638F1Au);prestore(5u,0u,0x8C638F1Eu);
    }
    cpu.pc=cpu.pr;return true;
}
} // namespace sonic::matrix_vectors
