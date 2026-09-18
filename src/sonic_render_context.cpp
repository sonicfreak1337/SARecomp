#include "sonic_render_context.hpp"
#include "sonic_native_model_memory.hpp"
#include "sonic_model_pipeline.hpp"
#include "katana/runtime/block_guards.hpp"
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>

namespace sonic::render_context {
namespace {
using namespace katana::runtime;
#include "render-context-identities.inc"
constexpr std::uint32_t renderer_slot=0x8C88FBF4u,cursor_slot=0x8C88FC14u;
constexpr std::uint32_t flags_slot=0x8C88F56Cu,packet=0x8C890044u;
thread_local Statistics counters;
struct Range {std::uint32_t address,size;};
bool overlap(Range a,Range b)noexcept{
    const auto x=a.address&0x1FFFFFFFu,y=b.address&0x1FFFFFFFu;
    return x<std::uint64_t(y)+b.size && y<std::uint64_t(x)+a.size;
}
}
std::span<const SourceSpan> source_spans()noexcept{return identities;}
const Statistics& statistics()noexcept{return counters;}
bool try_execute(CpuState& cpu,const NativePortImmutableWriteGuard* immutable){
    static_assert(std::endian::native==std::endian::little);
    const bool capture=cpu.pc==capture_entry;
    auto& m=cpu.memory;
    if((!capture && cpu.pc!=commit_entry) || !immutable || immutable->write_detected() ||
       !cpu.privileged_mode_inline() || cpu.trap_pending || cpu.sleeping ||
       m.watchpoint_count() || m.has_trace_handler() || m.has_guest_memory_access_sink() ||
       m.has_mmio_trace_handler() || !m.guest_write_observer_allows_prevalidated_linear_writes())return false;
    const auto* operation=model_pipeline::borrowed_operation(cpu);
    if(operation && operation->immutable!=immutable)operation=nullptr;
    const auto g=operation?operation->read:m.direct_linear_memory_guard(false);
    const bool p0=!(cpu.mmucr&1u) && (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu);
    const auto valid=[&](Range r,unsigned alignment=4u){
        const auto p=r.address&0x1FFFFFFFu;
        return g && g.physical_base==0x0C000000u && g.physical_span>=0x1000000u &&
            g.backing_mask==0xFFFFFFu && !(r.address&(alignment-1u)) && r.size &&
            ((r.address&0xC0000000u)==0x80000000u ||
             (p0 && r.address>=0x0C000000u && r.address<0x0D000000u)) &&
            p>=0x0C000000u && p<0x0D000000u && r.size<=0x0D000000u-p;
    };
    const auto load=[&](std::uint32_t a){std::uint32_t v;std::memcpy(&v,g.read_bytes+(a&0xFFFFFFu),4u);return v;};
    constexpr std::array control{Range{renderer_slot,4u},Range{cursor_slot,4u},Range{flags_slot,4u}};
    for(auto r:control)if(!valid(r))return false;
    const auto renderer=load(renderer_slot),cursor=load(cursor_slot);
    if(renderer>0xFFFFFFFFu-0x90u)return false;
    const Range header{renderer+0x90u,16u},cursors{cursor,20u},snapshot{packet,36u};
    if(!valid(header) || !valid(cursors) || !valid(snapshot))return false;
    if(!operation)for(auto s:identities){
        if(!valid({s.address,std::uint32_t(s.bytes.size())},2u) ||
           std::memcmp(g.read_bytes+(s.address&0xFFFFFFu),s.bytes.data(),s.bytes.size()))return false;
    }
    const std::array outputs=capture?std::array{snapshot,snapshot}:std::array{header,cursors};
    for(auto out:outputs){
        if(immutable->tracks_address(out.address&0x1FFFFFFFu,out.size) ||
           !m.is_writable_linear_range(out.address&0x1FFFFFFFu,out.size,false))return false;
        if(operation && !operation->allows_write(operation->context,out.address&0x1FFFFFFFu,out.size))return false;
        for(auto r:control)if(overlap(out,r))return false;
        for(auto s:identities)if(overlap(out,{s.address,std::uint32_t(s.bytes.size())}))return false;
    }
    // No callback, dispatch or changed mapping can occur from here to return.
    // Keep live ordered reads/writes: header, packet and cursor data may alias.
    // Only pointers, branch flags and source bytes were required disjoint.
    sonic::model_memory::Writes writes(cpu,*immutable,g,operation?&operation->write:nullptr);
    if(writes.direct())++counters.direct_calls;
    const auto store=[&](std::uint32_t a,std::uint32_t v){writes.store(a,v,CodeWriteSource::Cpu);};
    auto& r=cpu.r;
    r[5]=renderer_slot;r[4]=packet;r[0]=0x90u;r[1]=flags_slot;
    for(unsigned word=0;word<4u;++word){
        if(capture){r[3]=load(r[5]);r[2]=load(r[3]+r[0]);}
        else{r[2]=load(r[4]+word*4u);r[3]=load(r[5]);}
        if(word==3u)r[5]=cursor_slot;
        if(capture)store(r[4]+word*4u,r[2]);else store(r[3]+r[0],r[2]);
        if(word!=3u)r[0]+=4u;
    }
    for(auto offset:std::array{0u,4u,12u}){
        const auto dest=offset==0u?16u:offset==4u?28u:32u;
        if(capture){r[3]=load(r[5]);r[2]=load(r[3]+offset);if(offset==12u)r[3]=0x4000u;store(r[4]+dest,r[2]);}
        else{r[2]=load(r[4]+dest);r[3]=load(r[5]);store(r[3]+offset,r[2]);}
    }
    r[2]=load(r[1]);
    if(!capture){r[6]=load(r[5]);r[3]=0x4000u;}
    cpu.t=(r[2]&r[3])==0u;
    if(capture){
        r[6]=load(r[5]); // BT/S delay slot
        if(!cpu.t){
            r[2]=load(r[6]+16u);store(r[4]+20u,r[2]);
            r[1]=load(r[5]);r[2]=load(r[1]+8u);store(r[4]+24u,r[2]);
        }else{
            r[0]=load(r[6]+8u);store(r[4]+20u,r[0]);
            r[2]=load(r[5]);r[1]=load(r[2]+16u);store(r[4]+24u,r[1]);
        }
    }else{
        r[7]=load(r[4]+20u); // BT/S delay slot
        if(!cpu.t){
            store(r[6]+16u,r[7]);r[1]=load(r[4]+24u);r[2]=load(r[5]);store(r[2]+8u,r[1]);
        }else{
            store(r[6]+8u,r[7]);r[2]=load(r[4]+24u);r[1]=load(r[5]);store(r[1]+16u,r[2]);
        }
    }
    cpu.pc=cpu.pr;
    return true;
}
bool try_dispatch(CpuState& cpu,const NativePortImmutableWriteGuard* immutable){
    static const bool enabled=native_cpu::enabled("SARECOMP_NATIVE_RENDER_CONTEXT");
    if(!enabled || (cpu.pc!=capture_entry && cpu.pc!=commit_entry))return false;
    const bool capture=cpu.pc==capture_entry;
    if(!try_execute(cpu,immutable)){++counters.fallbacks;return false;}
    if(capture)++counters.captures;else ++counters.commits;
    return true;
}
}
