#include "sonic_animation_hierarchy.hpp"
#include "sonic_model_math.hpp"
#include "sonic_native_model_memory.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <vector>

namespace sonic::animation_hierarchy {
namespace {
using namespace katana::runtime;
#include "animation-identities.inc"
constexpr std::uint32_t visits_address=0x8C78B388u,limit_address=0x8C78B38Cu;
constexpr std::uint32_t capacity_address=0x8C88F5D8u,depth_address=0x8C88F5DCu,pointer_address=0x8C88F538u;
struct Range {std::uint32_t address,size;};
bool overlaps(Range a,Range b) noexcept {
    const auto x=a.address&0x1FFFFFFFu,y=b.address&0x1FFFFFFFu;
    return x<std::uint64_t(y)+b.size && y<std::uint64_t(x)+a.size;
}
struct Track {
    std::uint32_t first=0,current=0,next=0,frame=0,next_frame=0;
    std::array<std::uint32_t,3> a{},b{};
};
struct Node {
    std::uint32_t address=0,flags=0,child=0,sibling=0,row=0;
    std::array<std::uint32_t,3> position{},rotation{},scale{};
    std::array<Track,3> tracks{};
    bool exhausted=false;
};
struct Workspace {std::vector<Node> nodes;std::vector<Range> inputs;bool busy=false;};
thread_local Workspace workspace;
thread_local Statistics counters;
struct Execution : sonic::model_math::Transform {
    const NativePortImmutableWriteGuard& immutable;
    DirectLinearMemoryGuard guard;
    bool p0;
    std::vector<Node>& nodes;
    std::vector<Range>& inputs;
    std::uint32_t context=0,channels=0,frame_bits=0,frame_index=0,frame_limit=0;
    std::uint32_t visits=0,limit=0,row=0,output=0,matrix_pointer=0,matrix_depth=0;
    unsigned max_depth=0,max_matrix_depth=0,outputs=0;
    std::optional<sonic::model_memory::Writes> writes;
    bool valid(Range r,bool aligned=true)const noexcept {
        const auto p=r.address&0x1FFFFFFFu;
        const bool alias=(r.address&0xC0000000u)==0x80000000u ||
            (p0 && r.address>=0x0C000000u && r.address<0x0D000000u);
        return guard && guard.physical_base==0x0C000000u && guard.physical_span>=0x1000000u &&
            guard.backing_mask==0xFFFFFFu && alias && (!aligned || !(r.address&3u)) && r.size &&
            p>=0x0C000000u && p<0x0D000000u && r.size<=0x0D000000u-p;
    }
    bool read_range(Range r){if(!valid(r))return false;inputs.push_back(r);return true;}
    std::uint32_t peek(std::uint32_t a)const noexcept {
        std::uint32_t v;std::memcpy(&v,guard.read_bytes+(a&0xFFFFFFu),4u);return v;
    }
    auto triple(std::uint32_t a)const noexcept{return std::array{peek(a),peek(a+4u),peek(a+8u)};}
    bool track(Track& t,std::uint32_t first){
        t.first=first;if(!first)return true;
        auto current=first;
        for(unsigned k=0;k<4096u;++k){
            if(!valid({current,32u}))return false;
            const auto f=peek(current),next=peek(current+16u);
            if(frame_index>f && frame_index>=next){current+=16u;continue;}
            t.current=current;t.frame=f;
            t.next=f==frame_limit-1u?first:current+16u;
            // Every read in the original search, including its lookahead, is
            // covered before any output can invalidate these captured inputs.
            if(!read_range({first,current-first+32u}))return false;
            t.next_frame=peek(t.next);t.a=triple(current+4u);t.b=triple(t.next+4u);
            return true;
        }
        return false;
    }
    bool collect(std::uint32_t address,unsigned depth,std::uint32_t& sibling){
        if(depth>64u || nodes.size()>=256u)return false;
        max_depth=std::max(max_depth,depth);
        const auto index=nodes.size();nodes.emplace_back();
        auto& n=nodes[index];n.address=address;
        if(++visits>limit){visits=limit;n.exhausted=true;sibling=0;return true;}
        if(!read_range({address,52u}) || !read_range({row,channels*8u}))return false;
        n.flags=peek(address);n.position=triple(address+8u);n.rotation=triple(address+20u);
        n.scale=triple(address+32u);n.child=peek(address+44u);n.sibling=peek(address+48u);n.row=row;
        for(unsigned i=0;i<channels;++i)if(!track(n.tracks[i],peek(row+4u*i)))return false;
        row+=channels*8u;++outputs;max_matrix_depth=std::max(max_matrix_depth,depth);
        auto child=n.child;sibling=n.sibling;
        while(child)if(!collect(child,depth+1u,child))return false;
        return true;
    }
    bool preflight(){
        context=cpu.r[5];
        if(!valid({context,20u}) || !read_range({context+8u,12u}) ||
            !read_range({limit_address,4u}) || !read_range({capacity_address,4u}) ||
            !valid({visits_address,4u}) || !valid({depth_address,4u}) || !valid({pointer_address,4u}))return false;
        channels=std::bit_width(peek(context+8u));
        if(channels!=2u && channels!=3u)return false;
        frame_bits=peek(context+16u);frame_limit=peek(context+12u);
        // Ordinary authored frames only. Unusual unsigned/NaN/negative frame
        // conversions retain the original owner, without partial mutation.
        if(frame_bits>=0x4F000000u)return false;
        const auto exponent=(frame_bits>>23u)&255u;
        const auto mantissa=(frame_bits&0x7FFFFFu)|0x800000u;
        frame_index=exponent<127u?0u:exponent<=150u?mantissa>>(150u-exponent):mantissa<<(exponent-150u);
        visits=peek(visits_address);limit=peek(limit_address);
        matrix_depth=peek(depth_address);matrix_pointer=peek(pointer_address);
        row=peek(context);output=peek(context+4u);
        nodes.reserve(256);inputs.reserve(2048);
        std::uint32_t sibling=0;
        if(!collect(cpu.r[4],1u,sibling))return false;
        const auto capacity=peek(capacity_address);
        if(matrix_depth<1u || capacity>0x7FFFFFFFu || matrix_depth>capacity ||
            max_matrix_depth>capacity-matrix_depth || cpu.r[15]<max_depth*64u)return false;
        std::vector<Range> writes{{cpu.r[15]-max_depth*64u,max_depth*64u},{visits_address,4u}};
        if(outputs){
            writes.insert(writes.end(),{{context,8u},{output,outputs*64u},
                {matrix_pointer,(max_matrix_depth+1u)*64u},{depth_address,4u},{pointer_address,4u}});
        }
        for(const auto s:identities){
            const Range r{s.address,std::uint32_t(s.bytes.size())};
            if(!valid(r,false) || std::memcmp(guard.read_bytes+(r.address&0xFFFFFFu),s.bytes.data(),r.size))return false;
            inputs.push_back(r);
        }
        for(std::size_t i=0;i<writes.size();++i){
            const auto w=writes[i];
            if(!valid(w) || immutable.tracks_address(w.address&0x1FFFFFFFu,w.size) ||
                !cpu.memory.is_writable_linear_range(w.address&0x1FFFFFFFu,w.size,false))return false;
            for(std::size_t j=0;j<i;++j)if(overlaps(w,writes[j]))return false;
            for(const auto r:inputs)if(overlaps(w,r))return false;
        }
        // Subsequent recursion consumes the captured tree, never guest pointers.
        visits=peek(visits_address);row=peek(context);return true;
    }
    void store(std::uint32_t a,std::uint32_t value,CodeWriteSource source=CodeWriteSource::Cpu){
        writes->store(a,value,source);
    }
    void save_matrix(std::uint32_t p){
        store(p,cpu.r[0],CodeWriteSource::StoreQueue);store(p+32u,cpu.r[0],CodeWriteSource::StoreQueue);
        for(int i=14;i>=0;i-=2){store(p+4u*i,cpu.xf[i],CodeWriteSource::Fpu);store(p+4u*i+4u,cpu.xf[i+1],CodeWriteSource::Fpu);}
    }
    void push_matrix(){
        store(depth_address,++matrix_depth);save_matrix(matrix_pointer);
        matrix_pointer+=64u;store(pointer_address,matrix_pointer);save_matrix(matrix_pointer);
        cpu.r[0]=1u;cpu.r[6]=depth_address;
    }
    void pop_matrix(){
        store(depth_address,--matrix_depth);matrix_pointer-=64u;store(pointer_address,matrix_pointer);
        for(unsigned i=0;i<16u;++i)cpu.xf[i]=peek(matrix_pointer+4u*i);
        cpu.r[1]=matrix_pointer+64u;cpu.r[2]=1u;cpu.r[3]=matrix_depth;
        cpu.r[4]=64u;cpu.r[5]=pointer_address;cpu.t=true;
    }
    void unsigned_float(std::uint32_t value){
        cpu.fpul=value;fpu_float_from_fpul(cpu,3u);
        if(value&0x80000000u){cpu.fpul=0x4F800000u;cpu.fr[2]=cpu.fpul;binary(FpuBinaryOperation::Add,2u,3u);}
    }
    void weight(const Track& t){
        cpu.fr[4]=frame_bits;fpu_compare_greater(cpu,14u,4u);
        cpu.fr[3]=cpu.fr[4];fpu_truncate_to_fpul(cpu,3u);
        unsigned_float(t.frame);
        if(t.frame==frame_limit-1u){
            cpu.fr[15]=frame_bits;binary(FpuBinaryOperation::Subtract,3u,15u);
        }else{
            cpu.fr[1]=frame_bits;binary(FpuBinaryOperation::Subtract,3u,1u);
            unsigned_float(t.next_frame-t.frame);cpu.fr[15]=cpu.fr[1];binary(FpuBinaryOperation::Divide,3u,15u);
        }
    }
    void interpolate_vector(const Track& t){
        cpu.fr[0]=cpu.fr[15];cpu.fr[4]=t.a[0];cpu.fr[5]=t.a[1];cpu.fr[6]=t.a[2];
        for(unsigned i=0;i<3u;++i){
            cpu.fr[3]=t.b[i];binary(FpuBinaryOperation::Subtract,4u+i,3u);
            cpu.fr[2]=cpu.fr[4u+i];fpu_multiply_accumulate(cpu,3u,2u);cpu.fr[4u+i]=cpu.fr[2];
        }
        cpu.r[6]=t.current+12u;
    }
    void interpolate_rotation(const Track& t){
        std::array<std::uint32_t,3> deltas{},angles{};
        for(unsigned i=0;i<3u;++i)deltas[i]=std::uint32_t(std::int32_t(std::bit_cast<std::int16_t>(std::uint16_t(t.b[i]-t.a[i]))));
        const auto s=cpu.r[15];
        store(s,t.next+4u);store(s+8u,t.a[0]);store(s+4u,t.a[1]);store(s+12u,t.a[2]);
        store(s,t.next+8u);store(s+(channels==2u?16u:20u),deltas[0]);
        store(s,t.next+12u);store(s+(channels==2u?20u:16u),deltas[1]);
        for(unsigned i=0;i<3u;++i){
            cpu.fpul=deltas[i];fpu_float_from_fpul(cpu,3u);
            const unsigned dst=i==2u?2u:3u;if(i==2u)cpu.fr[2]=cpu.fr[3];
            binary(FpuBinaryOperation::Multiply,15u,dst);fpu_truncate_to_fpul(cpu,std::uint8_t(dst));
            angles[i]=t.a[i]+cpu.fpul;
        }
        rotate(angles);
    }
    void evaluate(std::size_t& cursor){
        const auto& n=nodes[cursor++];
        // Stack writes remain observable even though traversal and key search
        // are native. Do not clear unwritten locals shared by sibling calls.
        for(int i=14;i>=8;--i){cpu.r[15]-=4u;store(cpu.r[15],cpu.r[i]);}
        cpu.r[15]-=4u;store(cpu.r[15],cpu.fr[15],CodeWriteSource::Fpu);
        cpu.r[15]-=4u;store(cpu.r[15],cpu.fr[14],CodeWriteSource::Fpu);
        cpu.r[15]-=4u;store(cpu.r[15],cpu.pr);cpu.r[15]-=24u;
        cpu.r[13]=n.address;cpu.r[14]=context;
        store(visits_address,++visits);
        if(n.exhausted){
            const auto increment=visits;visits=limit;store(visits_address,limit);
            cpu.r[0]=0;cpu.r[1]=cpu.r[2]=limit;cpu.r[3]=increment;
            cpu.r[4]=visits_address;cpu.r[5]=limit_address;cpu.t=true;
        }else{
            cpu.r[9]=channels;push_matrix();cpu.fr[14]=0x4F000000u;
            cpu.r[11]=0x8C639C34u;cpu.r[12]=0x8C055C8Eu;cpu.r[10]=2u;cpu.r[8]=n.row;
            for(unsigned channel=0;channel<channels;++channel){
                cpu.r[9]=channel;cpu.r[7]=channel*4u;const auto& t=n.tracks[channel];
                if(t.first){
                    weight(t);
                    if(channel==1u)interpolate_rotation(t);
                    else{interpolate_vector(t);if(channel==0u)translate();else scale();}
                }else if(channel==0u){
                    if(channels==2u || !(n.flags&1u)){std::copy(n.position.begin(),n.position.end(),cpu.fr.begin()+4);translate();}
                }else if(channel==1u){
                    if(!(n.flags&2u)){store(cpu.r[15],n.address+20u);rotate(n.rotation);}
                }else if(!(n.flags&4u)){std::copy(n.scale.begin(),n.scale.end(),cpu.fr.begin()+4);scale();}
            }
            cpu.r[9]=channels;
            if(channels==2u && !(n.flags&4u)){std::copy(n.scale.begin(),n.scale.end(),cpu.fr.begin()+4);scale();}
            cpu.r[0]=(channels==2u || !n.tracks[2].first)?n.flags:2u;
            cpu.r[8]=n.row+channels*8u;store(context,cpu.r[8]);
            for(int i=15;i>=0;--i)store(output+4u*i,cpu.xf[i],CodeWriteSource::Fpu);
            output+=64u;store(context+4u,output);
            cpu.r[12]=n.child;
            while(cpu.r[12]){cpu.pr=0x8C057FF8u;evaluate(cursor);cpu.r[12]=cpu.r[0];}
            pop_matrix();cpu.r[0]=n.sibling;
        }
        cpu.r[15]+=24u;cpu.pr=peek(cpu.r[15]);cpu.r[15]+=4u;
        cpu.fr[14]=peek(cpu.r[15]);cpu.r[15]+=4u;cpu.fr[15]=peek(cpu.r[15]);cpu.r[15]+=4u;
        for(unsigned i=8;i<=14u;++i){cpu.r[i]=peek(cpu.r[15]);cpu.r[15]+=4u;}
        cpu.pc=cpu.pr;
    }
};
}
std::span<const SourceSpan> source_spans() noexcept{return identities;}
bool try_execute(CpuState& cpu,const NativePortImmutableWriteGuard* immutable){
    const auto fpscr=cpu.read_fpscr();auto& m=cpu.memory;
    if(cpu.pc!=entry || !immutable || immutable->write_detected() || !cpu.privileged_mode_inline() ||
        cpu.trap_pending || cpu.sleeping || (cpu.sr&sr_fd_mask) ||
        (fpscr&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) ||
        !(fpscr&fpscr_dn_mask) || (fpscr&fpscr_rounding_mode_mask)>1u ||
        m.watchpoint_count() || m.has_trace_handler() || m.has_guest_memory_access_sink() ||
        m.has_mmio_trace_handler() || !m.guest_write_observer_allows_prevalidated_linear_writes())return false;
    if(workspace.busy)return false;
    struct Scope {Workspace& w;Scope(Workspace& v):w(v){w.busy=true;w.nodes.clear();w.inputs.clear();}~Scope(){w.busy=false;}} scope(workspace);
    Execution e{{cpu},*immutable,m.direct_linear_memory_guard(false),
        !(cpu.mmucr&1u) && (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu),
        workspace.nodes,workspace.inputs};
    if(!e.preflight())return false;
    e.writes.emplace(cpu,*immutable,e.guard);
    std::size_t cursor=0;e.evaluate(cursor);counters.nodes+=e.outputs;
    if(e.writes->direct())++counters.direct_write_calls;
    return true;
}
const Statistics& statistics() noexcept{return counters;}
bool try_dispatch(CpuState& cpu,const NativePortImmutableWriteGuard* immutable){
    static const bool enabled=[] {const auto* flag=std::getenv("SARECOMP_NATIVE_ANIMATION_HIERARCHY");return flag && flag[0]=='1';}();
    if(!enabled || cpu.pc!=entry)return false;
    if(try_execute(cpu,immutable)){++counters.native_calls;return true;}
    ++counters.original_calls;return false;
}
}
