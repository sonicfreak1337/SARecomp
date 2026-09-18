#include "sonic_model_pipeline.hpp"
#include "sonic_native_model_memory.hpp"
#include "sonic_render_context.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace sonic::model_pipeline {
namespace {
using namespace katana::runtime;
#include "sonic_model_pipeline_identity.inc"
constexpr std::uint32_t frame=0x8C8FFE5Cu,frame_end=frame+48u;
struct Range {std::uint32_t address,size;};
bool overlap(Range a,Range b) {
    a.address&=0x1FFFFFFFu;b.address&=0x1FFFFFFFu;
    return a.address<std::uint64_t(b.address)+b.size && b.address<std::uint64_t(a.address)+a.size;
}
std::uint32_t read(const DirectLinearMemoryGuard& g,std::uint32_t a) {
    std::uint32_t v;std::memcpy(&v,g.read_bytes+(a&0xFFFFFFu),4);return v;
}
bool range(const DirectLinearMemoryGuard& g,bool p0,Range r) {
    const auto a=r.address&0x1FFFFFFFu;
    return g && g.physical_base==0x0C000000u && g.physical_span>=0x1000000u &&
        g.backing_mask==0xFFFFFFu && !(r.address&3u) && r.size &&
        ((r.address&0xC0000000u)==0x80000000u || (p0 && r.address==a)) &&
        a>=0x0C000000u && a<0x0D000000u && r.size<=0x0D000000u-a;
}
struct Scope {
    explicit Scope(const Capture& capture){active=&capture;}
    ~Scope(){active=nullptr;}
};
}
bool enabled() noexcept {
    static const bool on=sonic::native_cpu::model_group_enabled("SARECOMP_NATIVE_MODEL_PIPELINE");
    return on && !sonic::diagnostics::runtime_checks_enabled();
}
bool submission_enabled() noexcept {
    static const bool on=[] {const auto* v=std::getenv("SARECOMP_NATIVE_MODEL_SUBMISSION");
        return v && std::strcmp(v,"1")==0 && native_cpu::model_group_enabled("SARECOMP_NATIVE_MODEL_SUBMISSION");}();
    return on && enabled();
}
bool source_overlap(std::uint32_t physical,std::uint32_t size) noexcept {
    if(!size)return false;
    if(physical<0x0C038000u && std::uint64_t(physical)+size>0x0C036FFCu)
        for(const auto& s:identities)
            if(overlap({physical,size},{s.address,std::uint32_t(s.bytes.size())}))return true;
    if(physical<0x0C606000u && std::uint64_t(physical)+size>0x0C605CECu)
        for(const auto& s:render_context::source_spans())
            if(overlap({physical,size},{s.address,std::uint32_t(s.bytes.size())}))return true;
    return false;
}
Outcome execute(CpuState& cpu,const NativePortImmutableWriteGuard* immutable,Calls calls,SharedOperation* shared) {
    ++counts.declined;
    const auto entry=cpu.pc;
    const bool material_owner=entry==0x8C037108u;
    const bool context_owner=entry==0x8C03700Cu;
    const bool composed=shared || context_owner;
    auto& memory=cpu.memory;const auto fpscr=cpu.read_fpscr();
    if((entry!=0x8C037098u && !material_owner && !context_owner) || active || !calls.invoke || !immutable ||
       immutable->write_detected() || !cpu.privileged_mode_inline() || cpu.trap_pending || cpu.sleeping ||
       (cpu.sr&sr_fd_mask) || (fpscr&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) ||
       !(fpscr&fpscr_dn_mask) || (fpscr&fpscr_rounding_mode_mask)>1u ||
       memory.watchpoint_count() || memory.has_trace_handler() || memory.has_guest_memory_access_sink() ||
       memory.has_mmio_trace_handler() || !memory.guest_write_observer_allows_prevalidated_linear_writes())return Outcome::Declined;
    if(shared && (!shared->intact || shared->cpu!=&cpu || shared->immutable!=immutable ||
                  !shared->allows_write || !calls.closed))return Outcome::Declined;
    const auto guard=shared?shared->read:memory.direct_linear_memory_guard(false);
    const bool p0=!(cpu.mmucr&1u) && (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu);
    if(!range(guard,p0,{cpu.r[4],40u}) || !range(guard,p0,{cpu.gbr,96u}))return Outcome::Declined;
    if(!shared || !shared->sources_proven){
        for(const auto& identity:std::span{identities}.first(composed?identities.size():5u))
            if(std::memcmp(guard.read_bytes+(identity.address&0xFFFFFFu),identity.bytes.data(),identity.bytes.size()))return Outcome::Declined;
        if(composed)for(const auto& s:render_context::source_spans())
            if(std::memcmp(guard.read_bytes+(s.address&0xFFFFFFu),s.bytes.data(),s.bytes.size()))return Outcome::Declined;
        if(shared)shared->sources_proven=true;
    }
    // Only the authenticated product observer permits a callback-free captured
    // view. A revoked or merely 'stable' observer uses the retained owner.
    const auto writable=shared?shared->write:
        sonic::scalar_writes::View(memory,immutable,false,false,true).closed_region_snapshot();
    if(!writable || !writable.write_bytes || writable.write_bytes!=guard.read_bytes ||
       writable.generation!=guard.generation || writable.physical_base!=guard.physical_base ||
       writable.physical_span!=guard.physical_span || writable.backing_mask!=guard.backing_mask)
        return Outcome::Declined;
    const auto model=cpu.r[4],points=read(guard,model),normals=read(guard,model+4),count=read(guard,model+8);
    if(count<2u || count>65536u)return Outcome::Declined;
    const auto even=(count+1u)&~1u,point_bytes=(even+1u)*12u,normal_bytes=(count+!(count&1u))*12u;
    const auto output=read(guard,0x8C88F58Cu);
    if((output&31u) || !range(guard,p0,{points,point_bytes}) || !range(guard,p0,{normals,normal_bytes}))return Outcome::Declined;
    std::array writes{Range{frame,48},Range{output,even*16u},Range{0x8C03D760u,count*4u},
        Range{cpu.gbr+44u,4},Range{cpu.gbr+52u,4},Range{cpu.gbr+60u,8},
        context_owner?Range{0x8C890044u,36}:Range{0x8C89004Cu,4},Range{},Range{}};
    const std::array captured{Range{model,40},Range{points,point_bytes},Range{normals,normal_bytes},
        Range{0x8C8FFE00u,20},Range{0x8C88F58Cu,4},Range{0x8C88FBF4u,4},
        Range{0x8C88FC14u,4},Range{0x8C88F56Cu,4}};
    if(context_owner){
        const auto renderer=read(guard,0x8C88FBF4u),cursor=read(guard,0x8C88FC14u);
        if(renderer>0xFFFFFFFFu-0x90u)return Outcome::Declined;
        writes[7]={renderer+0x90u,16u};writes[8]={cursor,20u};
    }
    for(std::size_t i=0;i<(context_owner?writes.size():7u);++i){
        const auto w=writes[i];
        if(!range(guard,p0,w) || immutable->tracks_address(w.address&0x1FFFFFFFu,w.size) ||
           !memory.is_writable_linear_range(w.address&0x1FFFFFFFu,w.size,false))return Outcome::Declined;
        if(shared && !shared->allows_write(shared->context,w.address&0x1FFFFFFFu,w.size))return Outcome::Declined;
        for(const auto r:std::span{captured}.first(context_owner?captured.size():5u))if(overlap(w,r))return Outcome::Declined;
        for(std::size_t j=0;j<i;++j)if(overlap(w,writes[j]))return Outcome::Declined;
        for(const auto& identity:std::span{identities}.first(composed?identities.size():5u))if(overlap(w,{identity.address,std::uint32_t(identity.bytes.size())}))return Outcome::Declined;
        if(composed)for(const auto& s:render_context::source_spans())if(overlap(w,{s.address,std::uint32_t(s.bytes.size())}))return Outcome::Declined;
    }
    // The first material and mask prelude is part of 037108, before lighting.
    std::uint32_t mesh=0,materials=0,material=0;
    if(material_owner){
        mesh=read(guard,model+12);materials=read(guard,model+16);
        if(!range(guard,p0,{mesh,4}))return Outcome::Declined;
        material=materials+(read(guard,mesh)&0x3FFFu)*20u;
        if(material<materials || !range(guard,p0,{material,20}))return Outcome::Declined;
        if(overlap({frame,48},{mesh,4}) || overlap({frame,48},{material,20}) ||
           overlap({output,even*16u},{mesh,4}) || overlap({output,even*16u},{material,20}))return Outcome::Declined;
    }
    --counts.declined;++counts.calls;
    const auto invoke=[&](std::uint32_t address,std::uint32_t continuation){
        cpu.pc=address;cpu.pr=continuation;
        if(shared && shared->intact){
            const auto result=calls.closed(calls.context,cpu,address);
            if(result==ClosedCall::Complete)return cpu.pc==continuation;
            shared->revoke();active=nullptr;
            if(result==ClosedCall::Interrupted)return false;
        }
        return calls.invoke(calls.context,cpu,address) && cpu.pc==continuation;
    };
    if(!material_owner){
        cpu.r[0]=read(guard,0x8C754E08u);cpu.t=cpu.r[0]==0u;
        if(!cpu.t){cpu.pc=cpu.pr;++counts.culled;return Outcome::Complete;}
        cpu.r[7]=cpu.pr;
        if(!invoke(0x8C03718Cu,context_owner?0x8C037022u:0x8C0370AEu))return Outcome::Interrupted;
        cpu.pr=cpu.r[7];
        if(cpu.t){cpu.pc=cpu.pr;++counts.culled;return Outcome::Complete;}
    }
    // The authenticated cull contains no stores/callbacks. From here every
    // child is a reviewed model leaf; a retained fallback drops active first.
    // Buffers retain capacity between models, but their contents and identity
    // are replaced on every call. This is not a pointer-keyed asset cache.
    thread_local Capture capture;
    capture.cpu=&cpu;capture.memory=guard;capture.model=model;capture.count=count;
    capture.output_address=output;capture.gbr=cpu.gbr;
    capture.projected_bytes=writable.write_bytes+(output&0xFFFFFFu);
    capture.points_address=points;capture.normals_address=normals;
    capture.points.resize(even+1u);capture.normals.resize(count+!(count&1u));
    std::memcpy(capture.points.data(),guard.read_bytes+(points&0xFFFFFFu),point_bytes);
    std::memcpy(capture.normals.data(),guard.read_bytes+(normals&0xFFFFFFu),normal_bytes);
    Scope scope(capture);counts.points+=count;
    const auto store=[&](std::uint32_t a,std::uint32_t v,CodeWriteSource source=CodeWriteSource::Cpu){
        if(shared && shared->intact){
            std::memcpy(writable.write_bytes+(a&0xFFFFFFu),&v,4u);
            auto& perf=const_cast<MemoryPerformanceCounters&>(memory.performance_counters());
            ++perf.unobserved_accesses;++perf.indexed_region_hits;return;
        }
        if(!memory.try_write_direct_linear_u32(a&0x1FFFFFFFu,v,source))
            throw std::runtime_error("native model pipeline: admitted store failed");
    };
    cpu.r[0]=frame_end;
    store(cpu.r[0]-=4u,cpu.pr);
    for(int i=15;i>=12;--i)store(cpu.r[0]-=4u,cpu.fr[i],CodeWriteSource::Fpu);
    for(int i=14;i>=8;--i)store(cpu.r[0]-=4u,cpu.r[i]);
    cpu.r[13]=0;
    if(!invoke(0x8C037294u,context_owner?0x8C037048u:material_owner?0x8C037126u:0x8C0370D4u))return Outcome::Interrupted;
    cpu.r[0]=read(guard,model+8);
    cpu.t=std::bit_cast<std::int32_t>(cpu.r[13])>=std::bit_cast<std::int32_t>(cpu.r[0]);
    if(!cpu.t){
        if(material_owner){
            cpu.r[2]=mesh;cpu.r[3]=materials;cpu.r[0]=read(guard,mesh)&0x3FFFu;
            cpu.macl=cpu.r[0]*20u;cpu.r[5]=material;cpu.r[0]=material;store(cpu.gbr+52,cpu.r[0]);
            cpu.r[0]=read(guard,material+16);cpu.r[1]=0x8C8FFE20u;
            cpu.r[6]=read(guard,0x8C8FFE1Cu);cpu.t=cpu.r[6]==0u;
            if(!cpu.t){cpu.r[6]=read(guard,cpu.r[1]);cpu.r[1]+=4u;cpu.r[0]&=cpu.r[6];
                cpu.r[6]=read(guard,cpu.r[1]);cpu.r[1]+=4u;cpu.r[0]|=cpu.r[6];}
            store(cpu.gbr+44,cpu.r[0]);
        }
        if(!invoke(0x8C037350u,context_owner?0x8C037052u:material_owner?0x8C03715Au:0x8C0370DEu))return Outcome::Interrupted;
        if(context_owner){
            cpu.r[14]=cpu.r[4];cpu.r[0]=render_context::capture_entry;
            if(!invoke(render_context::capture_entry,0x8C03705Au))return Outcome::Interrupted;
            cpu.r[4]=cpu.r[14];
        }
        if(!invoke(0x8C0376D0u,context_owner?0x8C037060u:material_owner?0x8C03715Eu:0x8C0370E2u))return Outcome::Interrupted;
        if(context_owner){
            // Complete 036FFC state publication between drawing and the
            // original renderer-context commit; both call-visible PRs matter.
            cpu.pr=0x8C037064u;cpu.r[1]=0x8C890044u;
            cpu.r[0]=read(guard,cpu.r[1]+8u)|0xC0u;store(cpu.r[1]+8u,cpu.r[0]);
            cpu.r[0]=render_context::commit_entry;
            if(!invoke(render_context::commit_entry,0x8C03706Au))return Outcome::Interrupted;
        }
    }
    cpu.r[0]=frame;
    for(unsigned i=8;i<15;++i){cpu.r[i]=read(guard,cpu.r[0]);cpu.r[0]+=4;}
    for(unsigned i=12;i<16;++i){cpu.fr[i]=read(guard,cpu.r[0]);cpu.r[0]+=4;}
    cpu.pr=read(guard,cpu.r[0]);cpu.r[0]+=4;
    if(!material_owner && !context_owner){cpu.r[1]=0x8C890044u;cpu.r[0]=read(guard,cpu.r[1]+8u)|0xC0u;store(cpu.r[1]+8u,cpu.r[0]);}
    cpu.pc=cpu.pr;return Outcome::Complete;
}
std::span<std::uint8_t> projection_output(CpuState& cpu,std::uint32_t model,
    std::uint32_t count,std::uint32_t output) noexcept {
    const auto* capture=active;
    if(!capture || capture->cpu!=&cpu || capture->model!=model || capture->count!=count ||
       capture->output_address!=output || capture->gbr!=cpu.gbr || !capture->projected_bytes)
        return {};
    return {capture->projected_bytes,((std::size_t(count)+1u)&~std::size_t{1u})*16u};
}
bool publish_projection(CpuState& cpu,std::uint32_t model,std::uint32_t count,
    std::uint32_t output) noexcept {
    const auto bytes=projection_output(cpu,model,count,output);
    if(bytes.empty())return false;
    // The whole owner excluded every code/input alias and arbitrary observer.
    // The native transform cannot call guest code between borrowing and here.
    // Padding stays in RAM, so the ordinary transaction's temporary copies,
    // per-byte change mask and two allocations are unnecessary in this scope.
    auto* backing=active->projected_bytes-(output&0xFFFFFFu);
    std::memcpy(backing+((cpu.gbr+60u)&0xFFFFFFu),&output,4u);
    auto& counters=const_cast<MemoryPerformanceCounters&>(cpu.memory.performance_counters());
    counters.unobserved_accesses+=bytes.size()+4u;
    ++counts.direct_outputs;
    return true;
}
}
