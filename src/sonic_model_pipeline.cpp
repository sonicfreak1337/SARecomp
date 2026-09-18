#include "sonic_model_pipeline.hpp"
#include "sonic_native_model_memory.hpp"
#include "sonic_render_context.hpp"
#include "sonic_fpu_body.hpp"
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
    const bool owns_operation=!shared && calls.closed && submission_enabled();
    const bool composed=shared || owns_operation || context_owner;
    SharedOperation local_operation;
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
    // Direct model owners use the same complete synchronous operation as
    // hierarchy children. This capability lasts only until the first retained
    // call; no pointer identity or mapping generation is used as a source cache.
    if(owns_operation){
        local_operation={&cpu,immutable,guard,writable,nullptr,
            [](void*,std::uint32_t a,std::uint32_t n)noexcept{return !source_overlap(a,n);},true,true};
        shared=&local_operation;
    }
    const auto model=cpu.r[4],points=read(guard,model),normals=read(guard,model+4),count=read(guard,model+8);
    if(count<2u || count>65536u)return Outcome::Declined;
    const auto even=(count+1u)&~1u,point_bytes=(even+1u)*12u,normal_bytes=(count+!(count&1u))*12u;
    const auto output=read(guard,0x8C88F58Cu);
    if((output&31u) || !range(guard,p0,{points,point_bytes}) || !range(guard,p0,{normals,normal_bytes}))return Outcome::Declined;
    std::array writes{Range{frame,48},Range{output,even*16u},Range{0x8C03D760u,count*4u},
        material_owner?Range{cpu.gbr+44u,4}:Range{cpu.gbr+28u,20},Range{cpu.gbr+52u,4},Range{cpu.gbr+60u,8},
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
    if(owns_operation)++counts.root_operations;
    const auto invoke=[&](std::uint32_t address,std::uint32_t continuation){
        cpu.pc=address;cpu.pr=continuation;
        if(shared && shared->intact){
            const auto result=calls.closed(calls.context,cpu,address,shared);
            if(result==ClosedCall::Complete){++counts.closed_children;return cpu.pc==continuation;}
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
    // Cull publishes GBR material state before these snapshots. Its complete
    // store footprint is admitted above; a retained fallback drops active first.
    // Buffers retain capacity between models, but their contents and identity
    // are replaced on every call. This is not a pointer-keyed asset cache.
    thread_local Capture capture;
    capture.cpu=&cpu;capture.memory=guard;capture.model=model;capture.count=count;
    capture.output_address=output;capture.gbr=cpu.gbr;
    capture.operation=shared && shared->intact?shared:nullptr;
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
ClosedCall visibility(CpuState& c,SharedOperation& operation,float extra) {
    if(c.pc!=0x8C03718Cu || !operation.intact || !operation.sources_proven ||
       operation.cpu!=&c || !operation.immutable || operation.immutable->write_detected() ||
       !operation.allows_write || !std::isfinite(extra) || extra<0.0f)return ClosedCall::Declined;
    const auto& guard=operation.read;const auto& writable=operation.write;
    const bool p0=!(c.mmucr&1u) && (!c.address_space || c.address_space->mode()==AddressTranslationMode::NoMmu);
    if(!writable || !writable.write_bytes || writable.write_bytes!=guard.read_bytes ||
       writable.generation!=guard.generation || writable.physical_base!=guard.physical_base ||
       writable.physical_span!=guard.physical_span || writable.backing_mask!=guard.backing_mask ||
       !range(guard,p0,{c.r[4],40u}) || !range(guard,p0,{c.gbr,56u}))return ClosedCall::Declined;
    const auto mesh=read(guard,c.r[4]+12u),materials=read(guard,c.r[4]+16u);
    if(!range(guard,p0,{mesh,4u}))return ClosedCall::Declined;
    const auto offset=(read(guard,mesh)&0x3FFFu)*20u;
    if(materials>0xFFFFFFFFu-offset)return ClosedCall::Declined;
    const auto material=materials+offset;
    const std::array inputs{Range{c.r[4],40u},Range{c.gbr+8u,12u},Range{mesh,4u},Range{material,20u},
        Range{0x8C88F530u,8u},Range{0x8C88F540u,24u},Range{0x8C88F56Cu,4u},
        Range{0x8C88F5A0u,8u},Range{0x8C8FFE1Cu,12u}};
    for(const auto r:inputs)if(!range(guard,p0,r))return ClosedCall::Declined;
    // Cull is also the material-state owner: all six publications must be
    // admitted before mutation, including parent source and input aliases.
    for(const auto w:{Range{c.gbr+28u,20u},Range{c.gbr+52u,4u}}){
        const auto physical=w.address&0x1FFFFFFFu;
        if(operation.immutable->tracks_address(physical,w.size) || source_overlap(physical,w.size) ||
           !c.memory.is_writable_linear_range(physical,w.size,false) ||
           !operation.allows_write(operation.context,physical,w.size))return ClosedCall::Declined;
        // At the normal GBR=8C8FFE00, the mask is precisely GBR+28..36.
        // It is intentionally written, then read live in instruction order.
        // Only the pre-write inputs need to be disjoint; never snapshot masks.
        for(const auto r:std::span{inputs}.first(inputs.size()-1u))
            if(overlap(w,r))return ClosedCall::Declined;
    }
    HostFpuExecutionEpoch epoch(c);
    sonic::fpu_body::NontrappingSingleBody fp(c,epoch);
    if(!fp.admitted() || c.fpu_transfer_pair())return ClosedCall::Declined;
    struct Accounting {
        CpuState& c;std::uint64_t accesses{};
        ~Accounting(){auto& p=const_cast<MemoryPerformanceCounters&>(c.memory.performance_counters());
            p.unobserved_accesses+=accesses;p.indexed_region_hits+=accesses;}
    } accounting{c};
    const auto load=[&](CpuState&,std::uint32_t a){++accounting.accesses;return read(guard,a);};
    const auto load_half=[&](std::uint32_t a){++accounting.accesses;std::uint16_t v;
        std::memcpy(&v,guard.read_bytes+(a&0xFFFFFFu),2);return v;};
    const auto post=[&](CpuState&,unsigned r){const auto v=load(c,c.r[r]);c.r[r]+=4u;return v;};
    const auto store_gbr=[&](CpuState&,std::uint32_t offset){++accounting.accesses;
        std::memcpy(writable.write_bytes+((c.gbr+offset)&0xFFFFFFu),&c.r[0],4u);};
    const auto compare_x=[&](CpuState&,unsigned point,float widen){
        const auto original=c.fr[8];const auto value=std::bit_cast<float>(original);
        if(widen!=0.0f && std::isfinite(value))c.fr[8]=std::bit_cast<std::uint32_t>(value+widen);
        fpu_compare_greater(c,8u,point);c.fr[8]=original;
    };
    const auto reject=[&] {c.t=true;c.pc=c.pr;return ClosedCall::Complete;};
    // 03718C..0371B6: sphere transform and unchanged near/far gates.
    c.r[0]=c.r[4]+24u;
    c.fr[0]=post(c,0); c.fr[1]=post(c,0); c.fr[2]=post(c,0);
    c.fr[3]=0x3f800000u; if(!try_fpu_transform_vector_simd(c,0u))fpu_transform_vector(c,0u); c.fr[3]=post(c,0);
    c.r[0]=load(c,c.gbr+16u); c.fpul=c.r[0]; c.fr[4]=c.fpul;
    fp.binary<FpuBinaryOperation::Subtract,3u,4u>(); fpu_compare_greater(c,4u,2u);
    if (!c.t) return reject();
    c.r[1]=0x8c88f554u; c.fr[4]=load(c,c.r[1]); fp.binary<FpuBinaryOperation::Add,3u,4u>();
    fpu_compare_greater(c,4u,2u); if (c.t) return reject();
    c.fr[5]=0x3f800000u; fp.binary<FpuBinaryOperation::Add,3u,2u>(); fp.binary<FpuBinaryOperation::Divide,2u,5u>();
    // 0371B8..0371E2: only these two comparisons widen.
    c.r[0]=load(c,c.gbr+8u); c.fpul=c.r[0]; c.fr[4]=c.fpul; c.fr[6]=c.fr[4];
    c.r[3]=0x8c88f540u; c.fr[8]=post(c,3);
    c.r[1]=0x8c88f530u; c.fr[4]=post(c,1); fp.binary<FpuBinaryOperation::Multiply,5u,6u>(); c.fr[11]=c.fr[0];
    fp.binary<FpuBinaryOperation::Add,3u,0u>(); fp.binary<FpuBinaryOperation::Multiply,6u,0u>(); fp.binary<FpuBinaryOperation::Add,4u,0u>(); compare_x(c,0,-extra);
    if (!c.t) return reject();
    c.fr[9]=post(c,3); c.fr[8]=post(c,3);
    fp.binary<FpuBinaryOperation::Subtract,3u,11u>(); fp.binary<FpuBinaryOperation::Multiply,6u,11u>(); fp.binary<FpuBinaryOperation::Add,4u,11u>(); compare_x(c,11,extra);
    if (c.t) return reject();
    // 0371E4..03720A: retain the original cross-register Y calculations.
    c.r[0]=load(c,c.gbr+12u); c.fpul=c.r[0]; c.fr[7]=c.fpul;
    fp.binary<FpuBinaryOperation::Multiply,4u,7u>(); c.fr[4]=post(c,1); fp.binary<FpuBinaryOperation::Multiply,5u,7u>(); c.fr[11]=c.fr[1];
    fp.binary<FpuBinaryOperation::Add,3u,1u>(); fp.binary<FpuBinaryOperation::Multiply,7u,1u>(); fp.binary<FpuBinaryOperation::Add,4u,1u>(); fpu_compare_greater(c,9u,1u);
    if (!c.t) return reject();
    c.fr[9]=post(c,3); fp.binary<FpuBinaryOperation::Subtract,3u,11u>(); fp.binary<FpuBinaryOperation::Multiply,6u,11u>(); fp.binary<FpuBinaryOperation::Add,4u,11u>();
    fpu_compare_greater(c,8u,11u); if (c.t) return reject();
    // 037218..037274: visible models must publish the complete material ABI.
    c.r[0]=0u; c.r[2]=0x8c88f56cu; c.r[6]=load(c,c.r[2]); c.r[3]=4u;
    c.t=(c.r[3]&c.r[6])==0u; if (!c.t) c.r[0]=1u; store_gbr(c,28u);
    c.r[0]=0x8c88f5a0u; c.r[0]=load(c,c.r[0]); store_gbr(c,32u);
    c.r[0]=0x8c88f5a4u; c.r[0]=load(c,c.r[0]); store_gbr(c,36u);
    c.r[0]=0u; c.r[3]=0x20u; c.t=(c.r[3]&c.r[6])==0u;
    if (!c.t) c.r[0]=0xffffffffu;
    c.r[3]=0x10u; c.t=(c.r[3]&c.r[6])==0u; if (!c.t) c.r[0]=1u;
    store_gbr(c,40u);
    c.r[2]=load(c,c.r[4]+12u); c.r[3]=load(c,c.r[4]+16u);
    c.r[0]=static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(load_half(c.r[2]))));
    c.r[6]=0x3fffu; c.r[0]&=c.r[6]; c.r[5]=20u;
    c.macl=(c.r[0]&0xffffu)*(c.r[5]&0xffffu);
    c.r[5]=c.macl; c.r[5]+=c.r[3]; c.r[0]=c.r[5]; store_gbr(c,52u);
    c.r[0]=load(c,c.r[5]+16u); c.r[1]=0x8c8ffe1cu; c.r[6]=post(c,1);
    c.t=c.r[6]==0u;
    if (!c.t) { c.r[6]=post(c,1); c.r[0]&=c.r[6]; c.r[6]=post(c,1); c.r[0]|=c.r[6]; }
    store_gbr(c,44u); c.t=false; c.pc=c.pr;
    return ClosedCall::Complete;
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
