#include "sonic_motion_sampling.hpp"
#include "sonic_native_model_memory.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>

namespace sonic::motion_sampling {
namespace {
using namespace katana::runtime;
#include "motion-identities.inc"
struct Range {std::uint32_t address,size;};
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
[[noreturn]] void broken(){throw std::runtime_error("motion-sampling: failed after admission; Original fallback forbidden");}
struct Execution {
    CpuState& cpu;
    const NativePortImmutableWriteGuard& immutable;
    Memory& memory=cpu.memory;
    DirectLinearMemoryGuard guard=memory.direct_linear_memory_guard(false);
    const bool p0=!(cpu.mmucr&1u) && (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu);
    unsigned owner;
    std::array<Range,2> writes{};
    std::optional<sonic::model_memory::ClosedLeafWrites> native_writes;
    bool admitted(Range r)const noexcept {
        const auto p=r.address&0x1FFFFFFFu;
        const bool alias=(r.address&0xC0000000u)==0x80000000u ||
            (p0 && r.address>=0x0C000000u && r.address<0x0D000000u);
        return guard && guard.physical_base==0x0C000000u && guard.physical_span>=0x1000000u &&
            guard.backing_mask==0xFFFFFFu && alias && !(r.address&3u) && r.size &&
            p>=0x0C000000u && p<0x0D000000u && r.size<=0x0D000000u-p;
    }
    bool readable(Range r)const noexcept {
        return admitted(r) && std::ranges::none_of(writes,[&](Range w){return overlap(r,w);});
    }
    bool writable(Range r)const noexcept {
        return admitted(r) && !immutable.tracks_address(r.address&0x1FFFFFFFu,r.size) &&
            memory.is_writable_linear_range(r.address&0x1FFFFFFFu,r.size,false);
    }
    std::uint32_t peek(std::uint32_t a)const noexcept {
        std::uint32_t result;std::memcpy(&result,guard.read_bytes+(a&0xFFFFFFu),4u);return result;
    }
    bool preflight(){
        const auto sp=cpu.r[15];
        if(sp<40u)return false;
        writes={Range{sp-40u,40u},Range{0x8C88FD94u,4u}};
        if(!writable(writes[0]) || !writable(writes[1]) || overlap(writes[0],writes[1]))return false;
        for(const auto s:identities){
            if(!readable({s.address,std::uint32_t(s.bytes.size())}) ||
               std::memcmp(guard.read_bytes+(s.address&0xFFFFFFu),s.bytes.data(),s.bytes.size()))return false;
        }
        // The slot index is the one intentional read/write context field.
        if(!readable({0x8C88FD84u,12u}))return false;
        const auto slot=peek(0x8C88FD94u);
        if(slot>65535u)return false; // finite authoring admission, no truncation
        const auto table=peek(0x8C88FD84u);
        const auto table_slot=std::uint64_t(table)+std::uint64_t(slot)*4u;
        if(table_slot>UINT32_MAX || !readable({std::uint32_t(table_slot),4u}))return false;
        const auto keys=peek(std::uint32_t(table_slot));
        if(keys){
            const auto counts=peek(0x8C88FD88u);
            const auto count_slot=std::uint64_t(counts)+std::uint64_t(slot)*4u;
            if(count_slot>UINT32_MAX || !readable({std::uint32_t(count_slot),4u}))return false;
            const auto count=peek(std::uint32_t(count_slot));
            // N=0 makes the original unsigned binary search nonterminating.
            if(!count || count>65536u || !readable({keys,count*16u}))return false;
        }else{
            const auto offset=owner==0u?8u:owner==1u?32u:20u;
            const auto source=std::uint64_t(cpu.r[4])+offset;
            if(source>UINT32_MAX || !readable({std::uint32_t(source),12u}))return false;
        }
        return true;
    }
    std::uint32_t load(std::uint32_t a){
        std::uint32_t result=0u;
        if(!direct_linear_guard_read_u32(guard,(a&0x1FFFFFFFu)|0x80000000u,result))broken();
        return result;
    }
    void store(std::uint32_t,std::uint32_t a,std::uint32_t value,CodeWriteSource source){
        if(native_writes && native_writes->try_store(a,value,source))return;
        // All stores stay in the admitted stack/index spans. The only allowed
        // observer cannot inspect/mutate CPU, backing, mappings or scheduling.
        if(!memory.try_write_direct_linear_u32(a&0x1FFFFFFFu,value,source))broken();
    }
    void set_t(bool value){cpu.t=value;}
    void toggle_pairs(){cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask);}
    void pair(unsigned to,unsigned from){write_fpu_pair_bits(cpu,std::uint8_t(to),read_fpu_pair_bits(cpu,std::uint8_t(from)));}
    void axis(char which,std::uint32_t angle){
        cpu.t=angle==0u;cpu.fpul=angle; // LDS executes even in a skipped-axis delay slot
        if(!angle)return;
        cpu.fr[3]=0u;fpu_sine_cosine(cpu,0u);cpu.fr[7]=0u;
        if(which=='x'){
            cpu.fr[4]=0u;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=0u;
            cpu.fr[5]=cpu.fr[2];cpu.fr[6]=cpu.fr[1];fpu_negate(cpu,5u);
        }else if(which=='y'){
            cpu.fr[5]=0u;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=cpu.fr[1];cpu.fr[1]=0u;
            cpu.fr[6]=cpu.fr[0];cpu.fr[4]=cpu.fr[2];fpu_negate(cpu,2u);
        }else{
            cpu.fr[6]=0u;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=cpu.fr[1];cpu.fr[1]=cpu.fr[2];
            cpu.fr[2]=0u;cpu.fr[5]=cpu.fr[0];cpu.fr[4]=cpu.fr[1];fpu_negate(cpu,4u);
        }
        fpu_transform_vector(cpu,0u);fpu_transform_vector(cpu,4u);
        const auto to=which=='x'?std::array{5u,7u,9u,11u}:
            which=='y'?std::array{1u,3u,9u,11u}:std::array{1u,3u,5u,7u};
        toggle_pairs();for(unsigned i=0;i<4u;++i)pair(to[i],2u*i);toggle_pairs();
    }
    bool sdk(std::uint32_t target){
        if(cpu.r[4])broken();
        if(target==0x8C63A7B8u){
            cpu.t=true;cpu.fr[7]=0x3F800000u;fpu_transform_vector(cpu,4u);
            toggle_pairs();pair(13u,4u);pair(15u,6u);toggle_pairs();
        }else if(target==0x8C63A5DCu){
            cpu.t=true;toggle_pairs();pair(0u,1u);pair(2u,3u);pair(8u,5u);pair(10u,7u);toggle_pairs();
            for(unsigned i=0;i<4u;++i)fpu_binary(cpu,FpuBinaryOperation::Multiply,4u,std::uint8_t(i));
            for(unsigned i=8u;i<12u;++i)fpu_binary(cpu,FpuBinaryOperation::Multiply,5u,std::uint8_t(i));
            toggle_pairs();pair(1u,0u);pair(3u,2u);pair(5u,8u);pair(7u,10u);toggle_pairs();
            toggle_pairs();pair(0u,9u);pair(2u,11u);toggle_pairs();
            for(unsigned i=0;i<4u;++i)fpu_binary(cpu,FpuBinaryOperation::Multiply,6u,std::uint8_t(i));
            toggle_pairs();pair(9u,0u);pair(11u,2u);toggle_pairs();
        }else if(target==0x8C639C34u){
            axis('z',cpu.r[7]);axis('y',cpu.r[6]);axis('x',cpu.r[5]);
        }else if(target==0x8C639F38u){
            axis('y',cpu.r[6]);axis('x',cpu.r[5]);axis('z',cpu.r[7]);
        }else return false;
        cpu.pc=cpu.pr;return true;
    }
    void call(std::uint32_t target){
        const auto ret=cpu.pr,sp=cpu.r[15];cpu.pc=target;bool complete=false;
        if(target==0x8C03FEB8u)complete=key_index();
        else if(target==0x8C03FF2Cu)complete=float_key();
        else if(target==0x8C03FF90u)complete=angle_key();
        else complete=sdk(target);
        if(!complete || cpu.pc!=ret || cpu.pr!=ret || cpu.r[15]!=sp)broken();
    }
    bool key_index(){
        #include "key-index-body.inc"
    }
    bool float_key(){
        #include "float-key-body.inc"
    }
    bool angle_key(){
        #include "angle-key-body.inc"
    }
    bool position(){
        #include "position-body.inc"
    }
    bool scale(){
        #include "scale-body.inc"
    }
    bool rotate_zyx(){
        #include "rotate-zyx-body.inc"
    }
    bool rotate_yxz(){
        #include "rotate-yxz-body.inc"
    }
    bool run(){
        const HostFpuExecutionEpoch epoch(cpu);
        switch(owner){case 0:return position();case 1:return scale();case 2:return rotate_zyx();default:return rotate_yxz();}
    }
};
} // namespace
std::span<const SourceSpan> source_spans() noexcept{return identities;}
bool try_execute(katana::runtime::CpuState& cpu,const katana::runtime::NativePortImmutableWriteGuard* immutable){
    const auto found=std::ranges::find(entries,cpu.pc);
    if(found==entries.end() || !immutable || immutable->write_detected() || !mode_ok(cpu) || !observers_ok(cpu.memory))return false;
    Execution execution{cpu,*immutable, cpu.memory,cpu.memory.direct_linear_memory_guard(false),
        !(cpu.mmucr&1u) && (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu),
        unsigned(found-entries.begin())};
    if(!execution.preflight())return false;
    execution.native_writes.emplace(cpu,*immutable,execution.guard);
    return execution.run();
}
} // namespace sonic::motion_sampling
