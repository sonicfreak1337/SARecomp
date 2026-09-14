#include "sonic_near_collision.hpp"
#include "sonic_collision_math.hpp"
#include "sonic_matrix_stack.hpp"
#include "sonic_matrix_vectors.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <array>
#include <bit>
#include <bitset>
#include <cstring>
#include <optional>
#include <stdexcept>

namespace sonic::near_collision {
namespace {
using namespace katana::runtime;
#include "near-identities.inc"
struct Range { std::uint32_t address,size; };
constexpr std::uint32_t object_base=0x8C6BB1BCu, object_count=192u;
constexpr std::uint32_t node_base=0x8C6BE1BCu, node_count=8192u;
bool overlap(Range a,Range b) noexcept {
    const auto x=a.address&0x1FFFFFFFu,y=b.address&0x1FFFFFFFu;
    return x<std::uint64_t(y)+b.size && y<std::uint64_t(x)+a.size;
}
bool mode_ok(const CpuState& c) noexcept {
    const auto f=c.read_fpscr();
    return c.privileged_mode_inline() && !c.trap_pending && !c.sleeping &&
        !(c.sr&sr_fd_mask) && !(f&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) &&
        (f&fpscr_dn_mask) && (f&fpscr_rounding_mode_mask)<=1u;
}
bool observers_ok(const Memory& m) noexcept {
    return !m.watchpoint_count() && !m.has_trace_handler() && !m.has_guest_memory_access_sink() &&
        !m.has_mmio_trace_handler() && m.guest_write_observer_allows_prevalidated_linear_writes();
}
bool alias_ok(std::uint32_t a,bool p0) noexcept {
    return (a&0xC0000000u)==0x80000000u || (p0 && a>=0x0C000000u && a<0x0D000000u);
}
bool admitted(const DirectLinearMemoryGuard& g,bool p0,Range r) noexcept {
    const auto p=r.address&0x1FFFFFFFu;
    return g && g.physical_base==0x0C000000u && g.physical_span>=0x1000000u &&
        g.backing_mask==0xFFFFFFu && !(r.address&3u) && r.size && alias_ok(r.address,p0) &&
        p>=0x0C000000u && p<0x0D000000u && r.size<=0x0D000000u-p;
}
bool pool_slot(std::uint32_t a,std::uint32_t base,std::uint32_t count,bool p0,
               std::uint32_t& index) noexcept {
    const auto p=a&0x1FFFFFFFu,b=base&0x1FFFFFFFu;
    if(!alias_ok(a,p0) || p<b || p-b>=count*64u || ((p-b)&63u))return false;
    index=(p-b)/64u;return true;
}
std::uint32_t peek(const DirectLinearMemoryGuard& g,std::uint32_t a) noexcept {
    std::uint32_t value;std::memcpy(&value,g.read_bytes+(a&0xFFFFFFu),4u);return value;
}
[[noreturn]] void broken(){throw std::runtime_error("near-collision: interrupted after mutation; fallback forbidden");}

struct Execution {
    CpuState& cpu;
    const NativePortImmutableWriteGuard& immutable;
    Memory& memory=cpu.memory;
    DirectLinearMemoryGuard g=memory.direct_linear_memory_guard(false);
    const std::uint32_t initial_mmucr=cpu.mmucr,initial_sr=cpu.sr;
    const RuntimeAddressSpace* initial_space=cpu.address_space.get();
    const AddressTranslationMode initial_mode=initial_space?initial_space->mode():AddressTranslationMode::NoMmu;
    const bool p0=!(initial_mmucr&1u) && initial_mode==AddressTranslationMode::NoMmu;
    static constexpr auto fpu_mask=fpscr_fr_mask|fpscr_rounding_mode_mask|fpscr_dn_mask;
    const std::uint32_t initial_fpu=cpu.read_fpscr()&fpu_mask;
    const std::uint64_t exceptions=cpu.exception_generation;
    const std::uint8_t* backing=g.read_bytes;
    const std::uint64_t generation=g.generation;
    std::array<Range,7u> writes{};
    std::optional<HostFpuExecutionEpoch> epoch;

    bool writable(Range r) const noexcept {
        return admitted(g,p0,r) && !immutable.tracks_address(r.address&0x1FFFFFFFu,r.size) &&
            memory.is_writable_linear_range(r.address&0x1FFFFFFFu,r.size,false);
    }
    bool readable(Range r) const noexcept {
        if(!admitted(g,p0,r))return false;
        for(const auto w:writes)if(overlap(r,w))return false;
        return true;
    }
    bool preflight() {
        const auto top=cpu.r[15],query=cpu.r[4];
        if(top<0x200u || !admitted(g,p0,{0x8C88F5D8u,8u}) ||
           !admitted(g,p0,{0x8C88F538u,4u}) || !admitted(g,p0,{0x8C752B1Cu,4u}))
            return false;
        if(!(g.read_bytes[0x752B1Cu]&0x80u))return false; // debug rendering is not RAM-only
        const auto capacity=std::bit_cast<std::int32_t>(peek(g,0x8C88F5D8u));
        const auto depth=std::bit_cast<std::int32_t>(peek(g,0x8C88F5DCu));
        if(depth<1 || depth>=capacity)return false;
        writes={Range{object_base,object_count*64u},Range{top-0x200u,0x200u},
            Range{0x8C73E1BCu,0x184u},Range{0x8C754E30u,0x3004u},
            Range{0x8C88F5DCu,4u},Range{0x8C88F538u,4u},
            Range{peek(g,0x8C88F538u),128u}};
        for(std::size_t i=0;i<writes.size();++i){
            if(!writable(writes[i]))return false;
            for(std::size_t j=0;j<i;++j)if(overlap(writes[i],writes[j]))return false;
        }
        for(const auto s:identities)
            if(!readable({s.address&~3u,std::uint32_t(s.bytes.size())+(s.address&3u)}) ||
               std::memcmp(g.read_bytes+(s.address&0xFFFFFFu),s.bytes.data(),s.bytes.size()))return false;
        for(const auto r:std::array{Range{query,28u},Range{node_base,node_count*64u},
            Range{0x8C02D548u,4u},Range{0x8C752B1Cu,4u},Range{0x8C88F5D8u,4u},
            Range{0x8C67C580u,64u},Range{0x8C19E8A4u,8u},
            Range{0x8C19E8B8u,4u},Range{0x8C759634u,4u}})
            if(!readable(r))return false;
        const auto count=std::bit_cast<std::int32_t>(peek(g,0x8C19E8A4u));
        if(count<0 || count>4096)return false; // admission budget, never truncate
        if(count && !readable({0x8C757E34u,std::uint32_t(count)*12u}))return false;
        for(std::int32_t i=0;i<count;++i){
            const auto object=peek(g,0x8C757E38u+std::uint32_t(i)*12u);
            if(!readable({object,32u}))return false;
            const auto model=peek(g,object+4u);
            if(!readable({model,40u}))return false;
        }
        std::int32_t count2=0;
        if(peek(g,0x8C19E8B8u)==1u && peek(g,0x8C759634u)){
            count2=std::int16_t(peek(g,0x8C19E8A8u));
            if(count2<0 || count2>4096)return false;
            if(count2 && !readable({0x8C758434u,std::uint32_t(count2)*36u}))return false;
        }
        // Fixed 64-byte pools are authored by the original allocators. Their
        // slot bases end in BC, so validate base-relative alignment. Lists are
        // not assumed exactly Z-sorted (the original uses coarse buckets).
        std::bitset<object_count> seen_objects;
        std::bitset<node_count> seen_nodes;
        for(auto object=peek(g,0x8C02D548u);object;object=peek(g,object)){
            std::uint32_t index=0;
            if(!pool_slot(object,object_base,object_count,p0,index) || seen_objects[index])return false;
            seen_objects[index]=true;
            const auto head=peek(g,object+16u),tail=peek(g,object+20u);
            if(!head){
                if(tail)return false;
                // An empty list is safe only if neither eligibility input
                // can possibly emit this object's raw key.
                const auto key=peek(g,object+8u);
                for(std::int32_t i=0;i<count;++i)
                    if(peek(g,0x8C757E38u+std::uint32_t(i)*12u)==key)return false;
                for(std::int32_t i=0;i<count2;++i){
                    const auto p=0x8C758434u+std::uint32_t(i)*36u;
                    if((peek(g,p+32u)&0x00400003u) && peek(g,p+24u)==key)return false;
                }
                continue;
            }
            std::uint32_t previous=0;
            for(auto node=head;node;node=peek(g,node)){
                if(!pool_slot(node,node_base,node_count,p0,index) || seen_nodes[index] ||
                   peek(g,node+4u)!=previous)return false;
                seen_nodes[index]=true;previous=node;
            }
            if(previous!=tail)return false;
        }
        return true;
    }
    void revalidate() {
        g=memory.direct_linear_memory_guard(false);
        if(!g || g.read_bytes!=backing || g.generation!=generation ||
           !memory.direct_linear_memory_guard_current(g,false) || !mode_ok(cpu) ||
           !observers_ok(memory) || immutable.write_detected() ||
           cpu.mmucr!=initial_mmucr || cpu.sr!=initial_sr || cpu.exception_generation!=exceptions ||
           cpu.address_space.get()!=initial_space || (initial_space && initial_space->mode()!=initial_mode) ||
           (cpu.read_fpscr()&fpu_mask)!=initial_fpu)broken();
        for(const auto w:writes)if(!writable(w))broken();
    }
    std::uint32_t load(std::uint32_t a){
        std::uint32_t v=0;if(!direct_linear_guard_read_u32(g,(a&0x1FFFFFFFu)|0x80000000u,v))broken();return v;
    }
    std::uint16_t load16(std::uint32_t a){
        std::uint16_t v=0;if(!direct_linear_guard_read_u16(g,(a&0x1FFFFFFFu)|0x80000000u,v))broken();return v;
    }
    std::uint8_t load8(std::uint32_t a){
        std::uint8_t v=0;if(!direct_linear_guard_read_u8(g,(a&0x1FFFFFFFu)|0x80000000u,v))broken();return v;
    }
    void store(std::uint32_t pc,std::uint32_t a,std::uint32_t v,CodeWriteSource source){
        if(!memory.try_write_direct_linear_u32(a&0x1FFFFFFFu,v,source))
            guest_write_u32_at(cpu,GuestInstructionOrigin{pc,pc,true},a,v,source);
    }
    void store16(std::uint32_t pc,std::uint32_t a,std::uint16_t v,CodeWriteSource source){
        if(!memory.try_write_direct_linear_u16(a&0x1FFFFFFFu,v,source))
            guest_write_u16_at(cpu,GuestInstructionOrigin{pc,pc,true},a,v,source);
    }
    void set_t(bool value){cpu.t=value;}

    bool sdk_leaf(std::uint32_t target) {
        if(target==0x8C63A904u){
            cpu.fr[0]=0u;fpu_compare_greater(cpu,4u,0u);
            const bool negative=cpu.t;cpu.fr[0]=cpu.fr[4];
            if(negative)fpu_absolute(cpu,0u);
            fpu_square_root(cpu,0u);
            if(negative)fpu_negate(cpu,0u);
        }else if(target==0x8C63A820u){
            if(cpu.r[4])broken();
            cpu.t=true;cpu.r[5]=0x8C67C580u;
            cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask);
            for(unsigned i=0;i<8u;++i){
                const auto low=load(cpu.r[5]),high=load(cpu.r[5]+4u);
                write_fpu_pair_bits(cpu,std::uint8_t(2u*i+1u),std::uint64_t(low)|(std::uint64_t(high)<<32u));
                cpu.r[5]+=8u;
            }
            cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask);
        }else if(target==0x8C639E08u || target==0x8C639E9Cu || target==0x8C63A10Cu){
            if(cpu.r[4])broken();
            cpu.t=true;cpu.fpul=cpu.r[5];cpu.fr[3]=0u;fpu_sine_cosine(cpu,0u);
            cpu.fr[7]=0u;
            if(target==0x8C639E08u){
                cpu.fr[4]=0u;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=0u;
                cpu.fr[5]=cpu.fr[2];cpu.fr[6]=cpu.fr[1];fpu_negate(cpu,5u);
            }else if(target==0x8C639E9Cu){
                cpu.fr[5]=0u;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=cpu.fr[1];cpu.fr[1]=0u;
                cpu.fr[6]=cpu.fr[0];cpu.fr[4]=cpu.fr[2];fpu_negate(cpu,2u);
            }else{
                cpu.fr[6]=0u;cpu.fr[2]=cpu.fr[0];cpu.fr[0]=cpu.fr[1];cpu.fr[1]=cpu.fr[2];
                cpu.fr[2]=0u;cpu.fr[5]=cpu.fr[0];cpu.fr[4]=cpu.fr[1];fpu_negate(cpu,4u);
            }
            fpu_transform_vector(cpu,0u);fpu_transform_vector(cpu,4u);
            cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask);
            const std::array<unsigned,4u> destination=target==0x8C639E08u?std::array{5u,7u,9u,11u}:
                (target==0x8C639E9Cu?std::array{1u,3u,9u,11u}:std::array{1u,3u,5u,7u});
            for(unsigned i=0;i<4u;++i)
                write_fpu_pair_bits(cpu,std::uint8_t(destination[i]),read_fpu_pair_bits(cpu,std::uint8_t(2u*i)));
            cpu.write_fpscr(cpu.read_fpscr()^fpscr_sz_mask);
        }else return false;
        cpu.pc=cpu.pr;return true;
    }
    void call(std::uint32_t target) {
        const auto ret=cpu.pr,sp=cpu.r[15];cpu.pc=target;
        bool complete=false;
        if(target==0x8C0522C0u)complete=eligibility();
        else if(target==0x8C63A820u || target==0x8C63A904u || target==0x8C639E08u ||
                target==0x8C639E9Cu || target==0x8C63A10Cu)complete=sdk_leaf(target);
        else{
            epoch.reset();g={};
            if(target==collision_math::length_entry)complete=collision_math::try_execute(cpu,&immutable);
            else if(target==matrix_stack::push_entry || target==matrix_stack::pop_entry)
                complete=matrix_stack::try_execute(cpu,&immutable);
            else if(target==0x8C638E0Cu)complete=matrix_vectors::try_execute(cpu,&immutable);
            revalidate();epoch.emplace(cpu);
        }
        if(!complete || cpu.pc!=ret || cpu.pr!=ret || cpu.r[15]!=sp)broken();
    }
    bool eligibility(){
        #include "near-eligibility-body.inc"
    }
    bool run(){
        epoch.emplace(cpu);
        #include "near-poly-body.inc"
    }
};
} // namespace
std::span<const SourceSpan> source_spans() noexcept {return identities;}
bool try_execute(katana::runtime::CpuState& cpu,const katana::runtime::NativePortImmutableWriteGuard* immutable){
    if(cpu.pc!=entry || !immutable || immutable->write_detected() || !mode_ok(cpu) || !observers_ok(cpu.memory))
        return false;
    Execution execution{cpu,*immutable};
    if(!execution.preflight())return false;
    return execution.run();
}
} // namespace sonic::near_collision
