#include "sonic_collision_candidates.hpp"
#include "sonic_native_collision_memory.hpp"
#include "sonic_collision_math.hpp"
#include "sonic_matrix_stack.hpp"
#include "sonic_matrix_vectors.hpp"
#include "sonic_matrix_inverse.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <optional>
#include <stdexcept>
namespace sonic::collision_candidates {
namespace {
using namespace katana::runtime;
#include "touch-poly-identities.inc"
struct Range { std::uint32_t address,size; };
bool overlap(Range a,Range b) noexcept {
    const auto x=a.address&0x1FFFFFFFu,y=b.address&0x1FFFFFFFu;
    return x<std::uint64_t(y)+b.size && y<std::uint64_t(x)+a.size;
}
bool mode_ok(const CpuState& c) noexcept {
    const auto f=c.read_fpscr();
    return c.privileged_mode_inline() && !c.trap_pending && !c.sleeping && !(c.sr&sr_fd_mask) &&
        !(f&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) &&
        (f&fpscr_dn_mask) && (f&fpscr_rounding_mode_mask)<=1u;
}
bool observers_ok(const Memory& m) noexcept {
    return !m.watchpoint_count() && !m.has_trace_handler() && !m.has_guest_memory_access_sink() &&
        !m.has_mmio_trace_handler() && m.guest_write_observer_allows_prevalidated_linear_writes();
}
bool admitted(const DirectLinearMemoryGuard& g,bool p0,Range r) noexcept {
    const auto a=r.address,p=a&0x1FFFFFFFu;
    return g && g.physical_base==0x0C000000u && g.physical_span>=0x1000000u &&
        g.backing_mask==0xFFFFFFu && !(a&3u) && r.size &&
        ((a&0xC0000000u)==0x80000000u || (p0 && a>=0x0C000000u && a<0x0D000000u)) &&
        p>=0x0C000000u && p<0x0D000000u && r.size<=0x0D000000u-p;
}
std::uint32_t peek(const DirectLinearMemoryGuard& g,std::uint32_t a) noexcept {
    std::uint32_t v;std::memcpy(&v,g.read_bytes+(a&0xFFFFFFu),4u);return v;
}
[[noreturn]] void broken(){throw std::runtime_error("collision-candidates: interrupted after mutation; original fallback forbidden");}
} // namespace
std::span<const SourceSpan> source_spans() noexcept {return identities;}
bool try_execute(katana::runtime::CpuState& cpu,
    const katana::runtime::NativePortImmutableWriteGuard* immutable,const RetainedCallBridge& bridge){
    using namespace katana::runtime;
    static_assert(std::endian::native==std::endian::little);
    if(cpu.pc!=entry || !immutable || immutable->write_detected() || !bridge.invoke ||
       !mode_ok(cpu) || !observers_ok(cpu.memory))return false;
    auto& memory=cpu.memory;
    auto g=memory.direct_linear_memory_guard(false);
    const auto initial_mmucr=cpu.mmucr,initial_sr=cpu.sr;
    const auto initial_space=cpu.address_space.get();
    const auto initial_mode=initial_space?initial_space->mode():AddressTranslationMode::NoMmu;
    const bool p0=!(cpu.mmucr&1u) && initial_mode==AddressTranslationMode::NoMmu;
    const auto fpu_mask=fpscr_fr_mask|fpscr_rounding_mode_mask|fpscr_dn_mask;
    const auto initial_fpu=cpu.read_fpscr()&fpu_mask;
    const auto exceptions=cpu.exception_generation;
    const auto top=cpu.r[15],query=cpu.r[4];
    if(top<0x200u || !admitted(g,p0,{query,0x84u}) ||
       !admitted(g,p0,{0x8C752B1Cu,4u}) || !admitted(g,p0,{0x8C88F5D8u,8u}) ||
       !admitted(g,p0,{0x8C88F538u,4u}))return false;
    // The nonnegative branch invokes debug rendering/text; it is not RAM-only.
    if(!(g.read_bytes[0x752B1Cu]&0x80u))return false;
    const auto capacity=std::bit_cast<std::int32_t>(peek(g,0x8C88F5D8u));
    const auto depth=std::bit_cast<std::int32_t>(peek(g,0x8C88F5DCu));
    const auto matrix=peek(g,0x8C88F538u);
    if(depth<1 || depth>=capacity)return false;
    // Owner frame=0x124; deepest retained libm adds 100 bytes. Reserve 0x200.
    // R4 is the nonnull identity for Push: exactly the current 64-byte slot is
    // saved, then Pop restores it. The 16-contact path skips Pop, as authored.
    const std::array writes{
        Range{query+0x28u,4u},Range{query+0x34u,0x50u},Range{top-0x200u,0x200u},
        Range{0x8C73E340u,0x9C4u},Range{0x8C6BAC30u,0x580u},Range{0x8C7AC048u,4u},
        Range{0x8C88F5DCu,4u},Range{0x8C88F538u,4u},Range{matrix,64u}};
    const auto writable=[&](Range w){return admitted(g,p0,w) &&
        !immutable->tracks_address(w.address&0x1FFFFFFFu,w.size) &&
        memory.is_writable_linear_range(w.address&0x1FFFFFFFu,w.size,false);};
    for(std::size_t i=0;i<writes.size();++i){
        if(!writable(writes[i]))return false;
        for(std::size_t j=0;j<i;++j)if(overlap(writes[i],writes[j]))return false;
    }
    const auto readable=[&](Range r){
        if(!admitted(g,p0,r))return false;
        for(const auto w:writes)if(overlap(r,w))return false;
        return true;
    };
    for(const auto s:identities)
        if(!readable({s.address,std::uint32_t(s.bytes.size())}) ||
           std::memcmp(g.read_bytes+(s.address&0xFFFFFFu),s.bytes.data(),s.bytes.size()))return false;
    // The copier reads one discarded word beyond the last 88-byte contact.
    for(const auto r:std::array{Range{query,0x28u},Range{0x8C02D548u,4u},
            Range{0x8C752B1Cu,4u},Range{0x8C88F5D8u,4u},Range{0x8C67C580u,64u},
            Range{0x8C73ED04u,4u}})if(!readable(r))return false;
    auto object=peek(g,0x8C02D548u);
    unsigned budget=16384u;
    while(object){
        if(!budget-- || !readable({object,40u}))return false;
        if(peek(g,object+8u)==cpu.r[5])break;
        object=peek(g,object);
    }
    if(object){
        const auto count=std::bit_cast<std::int32_t>(peek(g,object+36u));
        if(count<0 || count>96)return false; // producer bound; never truncate
        if(count){
            const auto array=peek(g,object+28u);
            if(!readable({array,std::uint32_t(count)*4u}))return false;
            for(std::int32_t i=0;i<count;++i)
                if(!readable({peek(g,array+std::uint32_t(i)*4u),64u}))return false;
        }
    }
    // NO false returns beyond this point, including after retained calls.
    const auto backing=g.read_bytes;
    const auto generation=g.generation;
    const auto revalidate=[&]{
        g=memory.direct_linear_memory_guard(false);
        if(!g || g.read_bytes!=backing || g.generation!=generation ||
           !memory.direct_linear_memory_guard_current(g,false) || !mode_ok(cpu) ||
           !observers_ok(memory) || immutable->write_detected() ||
           cpu.mmucr!=initial_mmucr || cpu.sr!=initial_sr || cpu.exception_generation!=exceptions ||
           cpu.address_space.get()!=initial_space || (initial_space && initial_space->mode()!=initial_mode) ||
           (cpu.read_fpscr()&fpu_mask)!=initial_fpu)broken();
        for(const auto w:writes)if(!writable(w))broken();
    };
    collision_memory::Access access;
    access.capture(cpu,*immutable,g);
    const auto load=[&](std::uint32_t a){std::uint32_t v=0u;
        if(access.try_read(a,v))return v;
        if(!direct_linear_guard_read_u32(g,(a&0x1FFFFFFFu)|0x80000000u,v))broken();return v;};
    const auto load16=[&](std::uint32_t a){std::uint16_t v=0u;
        if(access.try_read(a,v))return v;
        if(!direct_linear_guard_read_u16(g,(a&0x1FFFFFFFu)|0x80000000u,v))broken();return v;};
    const auto load8=[&](std::uint32_t a){std::uint8_t v=0u;
        if(access.try_read(a,v))return v;
        if(!direct_linear_guard_read_u8(g,(a&0x1FFFFFFFu)|0x80000000u,v))broken();return v;};
    const auto store=[&](std::uint32_t pc,std::uint32_t a,std::uint32_t v,CodeWriteSource source){
        if(access.try_store(a,v))return;
        if(!memory.try_write_direct_linear_u32(a&0x1FFFFFFFu,v,source))
            guest_write_u32_at(cpu,GuestInstructionOrigin{pc,pc,true},a,v,source);};
    const auto store16=[&](std::uint32_t pc,std::uint32_t a,std::uint16_t v,CodeWriteSource source){
        if(access.try_store(a,v))return;
        if(!memory.try_write_direct_linear_u16(a&0x1FFFFFFFu,v,source))
            guest_write_u16_at(cpu,GuestInstructionOrigin{pc,pc,true},a,v,source);};
    const auto set_t=[&](bool value){cpu.t=value;};
    std::optional<HostFpuExecutionEpoch> epoch;epoch.emplace(cpu);
    const auto call=[&](std::uint32_t target){
        const auto ret=cpu.pr,sp=cpu.r[15];
        access.reset();epoch.reset();g={};cpu.pc=target;
        bool complete=false;
        if(target==collision_math::cross_entry || target==collision_math::length_entry || target==collision_math::normalize_entry)
            complete=collision_math::try_execute(cpu,immutable);
        else if(target==matrix_stack::push_entry || target==matrix_stack::pop_entry)
            complete=matrix_stack::try_execute(cpu,immutable);
        else if(target==0x8C638E0Cu)complete=matrix_vectors::try_execute(cpu,immutable);
        else if(target==matrix_inverse::inverse_entry)complete=matrix_inverse::try_execute(cpu,immutable);
        else if(target==0x8C10CF48u || target==0x8C10CF98u || target==0x8C10D038u ||
                target==0x8C639E08u || target==0x8C639E9Cu || target==0x8C10CD1Cu)
            complete=bridge.invoke(bridge.context,cpu,target);
        if(!complete || cpu.pc!=ret || cpu.pr!=ret || cpu.r[15]!=sp)broken();
        revalidate();access.capture(cpu,*immutable,g);epoch.emplace(cpu);
    };
    #include "touch-poly-body.inc"
}
} // namespace sonic::collision_candidates
