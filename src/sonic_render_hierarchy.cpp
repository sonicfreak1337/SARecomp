#include "sonic_render_hierarchy.hpp"
#include "sonic_scalar_write_view.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include <bit>
#include <bitset>
#include <cstring>
#include <memory>
#include <optional>
namespace sonic::render_hierarchy {
namespace {
using namespace katana::runtime;
#include "hierarchy-identities.inc"
struct Interrupted {};
struct ResumeOriginal {bool completed_tail{};std::uint32_t tail_site{};};
// Unsupported accesses resume before the instruction. Nothing device-visible
// is performed inside the native transaction. A delay slot resumes its branch;
// a call also restores PR so that the retained branch executes exactly once.
struct RestartPoint {
    std::uint32_t pc;
    bool restore_pr{};
    std::uint32_t pr{};
    constexpr RestartPoint(std::uint32_t p):pc(p){}
    constexpr RestartPoint(std::uint32_t p,std::uint32_t old_pr):pc(p),restore_pr(true),pr(old_pr){}
};
bool mode_ok(const CpuState& c) noexcept {
    const auto f=c.read_fpscr();
    return c.privileged_mode_inline() && !c.trap_pending && !c.sleeping && !(c.sr&sr_fd_mask) &&
        !(f&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) &&
        (f&fpscr_dn_mask) && (f&fpscr_rounding_mode_mask)<=1u;
}
bool nonnegative(std::uint32_t value) noexcept {return !(value&0x80000000u);}
std::uint32_t signed8(std::uint8_t v) noexcept {return std::uint32_t(std::int32_t(std::bit_cast<std::int8_t>(v)));}
std::uint32_t signed16(std::uint16_t v) noexcept {return std::uint32_t(std::int32_t(std::bit_cast<std::int16_t>(v)));}

// One capability for live nodes, sampling records and matrix work between
// real callbacks. Never snapshot the mutable hierarchy or callback targets.
class Access {
    CpuState& c;
    const NativePortImmutableWriteGuard& immutable;
    DirectLinearMemoryGuard read{},write{};
    bool p0{};
    std::uint32_t stack{},stack_size{};
    std::uint64_t stores{};
    std::bitset<owner_sources.size()> proven_sources{};
    const bool share_models=model_pipeline::submission_enabled() || land_enabled();
    model_pipeline::SharedOperation models{};
public:
    Access(CpuState& cpu,const NativePortImmutableWriteGuard& guard):c(cpu),immutable(guard){}
    ~Access(){flush();}
    bool refresh() {
        read={};write={};proven_sources=0;models.revoke();
        auto& m=c.memory;
        if(!mode_ok(c) || immutable.write_detected() || m.watchpoint_count() || m.has_trace_handler() ||
           m.has_guest_memory_access_sink() || m.has_mmio_trace_handler() ||
           !m.guest_write_observer_allows_prevalidated_linear_writes())return false;
        read=m.direct_linear_memory_guard(false);
        p0=!(c.mmucr&1u) && (!c.address_space || c.address_space->mode()==AddressTranslationMode::NoMmu);
        if(!read || read.physical_base!=0x0C000000u || read.physical_span<0x1000000u || read.backing_mask!=0xFFFFFFu)return false;
        const scalar_writes::View view(m,&immutable,false,false,true);
        const auto candidate=view.closed_region_snapshot();
        if(candidate && candidate.write_bytes && candidate.write_bytes==read.read_bytes &&
           candidate.generation==read.generation && candidate.physical_base==read.physical_base &&
           candidate.physical_span==read.physical_span && candidate.backing_mask==read.backing_mask)write=candidate;
        if(share_models)models={&c,&immutable,read,write,this,
            [](void* p,std::uint32_t a,std::uint32_t n)noexcept{
                return !static_cast<Access*>(p)->source_overlap(a,n);
            },false,true};
        return !stack_size || admit_stack(stack,stack_size);
    }
    model_pipeline::SharedOperation* model_operation()noexcept{return share_models?&models:nullptr;}
    bool range(std::uint32_t a,std::uint32_t size,unsigned alignment=1)const noexcept {
        const auto physical=a&0x1FFFFFFFu;
        return read && !(a&(alignment-1u)) && ((a&0xC0000000u)==0x80000000u || (p0 && a==physical)) &&
            physical>=0x0C000000u && physical<0x0D000000u && size<=0x0D000000u-physical;
    }
    const std::uint8_t* point(std::uint32_t a)const noexcept {
        return range(a,12u,4u)?read.read_bytes+(a&0xFFFFFFu):nullptr;
    }
    bool identity(unsigned i)const noexcept {
        const auto& s=identities[i];
        return range(s.address,std::uint32_t(s.bytes.size())) &&
            std::memcmp(read.read_bytes+(s.address&0xFFFFFFu),s.bytes.data(),s.bytes.size())==0;
    }
    bool authenticate_all() noexcept {
        for(unsigned i=0;i<identities.size();++i)if(!identity(i))return false;
        proven_sources.set();return true;
    }
    bool authenticate(std::uint32_t owner) noexcept {
        const auto i=source_owner_index(owner);
        if(i>=owner_sources.size())return false;
        if(proven_sources[i])return true;
        for(const auto& s:owner_sources[i])
            if(!range(s.address,std::uint32_t(s.bytes.size())) ||
               std::memcmp(read.read_bytes+(s.address&0xFFFFFFu),s.bytes.data(),s.bytes.size()))return false;
        proven_sources.set(i);return true;
    }
    // Native stores may never alter a proved body or literal, even if an
    // unusual module range set does not mark those bytes immutable. Real
    // callbacks invalidate every proof unconditionally in refresh().
    bool source_overlap(std::uint32_t physical,std::uint32_t size)const noexcept {
        // Model proofs are part of this same root operation. Even a narrow
        // installed immutable set must not let a hierarchy store alter them.
        if(share_models && model_pipeline::source_overlap(physical,size))return true;
        bool candidate=false;
        for(auto p=(physical-0x0C000000u)>>12u;p<=((physical-0x0C000000u)+size-1u)>>12u;++p)
            candidate|=source_pages[p];
        if(!candidate)return false;
        for(const auto& s:identities)
            if(physical<std::uint64_t(s.address&0x1FFFFFFFu)+s.bytes.size() &&
               (s.address&0x1FFFFFFFu)<std::uint64_t(physical)+size)return true;
        return false;
    }
    void fload(RestartPoint at,unsigned reg,std::uint32_t address) {
        if(!(c.fpscr&fpscr_sz_mask)){c.fr[reg]=u32(address,at);return;}
        if(!range(address,8u,4u))restart(at);
        std::uint64_t value;std::memcpy(&value,read.read_bytes+(address&0xFFFFFFu),8u);
        write_fpu_pair_bits(c,reg,value);
    }
    void fstore(RestartPoint at,std::uint32_t address,unsigned reg) {
        if(!(c.fpscr&fpscr_sz_mask)){u32(address,c.fr[reg],at);return;}
        const auto physical=address&0x1FFFFFFFu;
        if(!write || !range(address,8u,4u) || immutable.tracks_address(physical,8u) || source_overlap(physical,8u))restart(at);
        const auto value=read_fpu_pair_bits(c,reg);
        std::memcpy(write.write_bytes+(address&0xFFFFFFu),&value,8u);stores+=2;
    }
    bool admit_stack(std::uint32_t a,std::uint32_t size) {
        if(!write || !range(a,size,4u) || immutable.tracks_address(a&0x1FFFFFFFu,size) || source_overlap(a&0x1FFFFFFFu,size) ||
           !c.memory.is_writable_linear_range(a&0x1FFFFFFFu,size,false))return false;
        stack=a;stack_size=size;return true;
    }
    void flush() noexcept {
        auto& perf=const_cast<MemoryPerformanceCounters&>(c.memory.performance_counters());
        perf.unobserved_accesses+=stores;perf.indexed_region_hits+=stores;stores=0;
    }
    [[noreturn]] void restart(RestartPoint at) {
        ++counts.resumes;flush();c.pc=at.pc;
        if(at.restore_pr)c.pr=at.pr;
        throw ResumeOriginal{};
    }
    template<class T> T load(std::uint32_t a,RestartPoint at) {
        T value;
        if(range(a,sizeof(T),sizeof(T))){std::memcpy(&value,read.read_bytes+(a&0xFFFFFFu),sizeof(T));return value;}
        restart(at);
    }
    std::uint32_t u32(std::uint32_t a,RestartPoint at){return load<std::uint32_t>(a,at);}
    template<class T> void store(std::uint32_t a,std::uint32_t v,RestartPoint at,CodeWriteSource =CodeWriteSource::Cpu) {
        const auto physical=a&0x1FFFFFFFu;
        const bool on_stack=stack_size && physical>=(stack&0x1FFFFFFFu) &&
            std::uint64_t(physical)+sizeof(T)<=std::uint64_t(stack&0x1FFFFFFFu)+stack_size;
        if(write && range(a,sizeof(T),sizeof(T)) && (on_stack ||
           (!immutable.tracks_address(physical,sizeof(T)) && !source_overlap(physical,sizeof(T))))){
            const T value=static_cast<T>(v);std::memcpy(write.write_bytes+(a&0xFFFFFFu),&value,sizeof(T));++stores;return;
        }
        restart(at);
    }
    void u32(std::uint32_t a,std::uint32_t v,RestartPoint at){store<std::uint32_t>(a,v,at);}
};

struct Flow {unsigned depth{},backedges{};};
#ifdef SARECOMP_RENDER_HIERARCHY_TEST_COVERAGE
constexpr std::size_t coverage_index(std::uint32_t pc) {
    if(pc>=0x8C639000u)return (0xD000u+pc-0x8C639000u)/2u;
    if(pc>=0x8C638000u)return (0xF0000u+pc-0x8C638000u)/2u;
    if(pc>=0x8C036000u && pc<0x8C046000u)return (pc-0x8C036000u)/2u;
    return (0x10000u+(pc&0xFFFFFFu))/2u;
}
#define HIERARCHY_SITE(pc) (visited[coverage_index(pc)]=true)
#else
#define HIERARCHY_SITE(pc) ((void)0)
#endif
bool run(CpuState&,Access&,Calls,Flow&,std::uint32_t);
void body(CpuState& cpu,Access& a,Calls calls,Flow& flow,std::uint32_t owner){
    const auto load=[&](RestartPoint at,std::uint32_t address){return a.u32(address,at);};
    const auto load16=[&](RestartPoint at,std::uint32_t address){return a.load<std::uint16_t>(address,at);};
    const auto load8=[&](RestartPoint at,std::uint32_t address){return a.load<std::uint8_t>(address,at);};
    const auto store=[&](RestartPoint at,std::uint32_t address,std::uint32_t value,CodeWriteSource source){a.store<std::uint32_t>(address,value,at,source);};
    const auto store16=[&](RestartPoint at,std::uint32_t address,std::uint16_t value,CodeWriteSource source){a.store<std::uint16_t>(address,value,at,source);};
    const auto store8=[&](RestartPoint at,std::uint32_t address,std::uint8_t value,CodeWriteSource source){a.store<std::uint8_t>(address,value,at,source);};
    const auto fload=[&](RestartPoint at,unsigned reg,std::uint32_t address){a.fload(at,reg,address);};
    const auto fstore=[&](RestartPoint at,std::uint32_t address,unsigned reg){a.fstore(at,address,reg);};
    const auto set_t=[&](bool value){cpu.t=value;};
    // Generated register-only sequences retain the exact original AOT epochs,
    // including the original FSCA/FTRV groups. No owner-wide scope may leak
    // across an ungrouped helper, a callback or an original continuation.
    const auto backedge=[&](std::uint32_t pc){if(++flow.backedges>=100000u)a.restart(pc);};
    const auto call=[&](std::uint32_t target,bool tail=false,std::uint32_t tail_site=0){
        cpu.pc=target;const auto continuation=cpu.pr;
        if(contains(target)){
            ++counts.internal_calls;
            if(!run(cpu,a,calls,flow,target))throw ResumeOriginal{tail,tail_site};
        }else{
            a.flush();
            auto* models=a.model_operation();
            auto model=model_pipeline::Outcome::Declined;
            if(models && calls.model && (target==0x8C03700Cu || target==0x8C037098u || target==0x8C037108u))
                model=calls.model(calls.context,cpu,*models);
            if(model==model_pipeline::Outcome::Interrupted)throw Interrupted{};
            if(model==model_pipeline::Outcome::Complete)++counts.model_calls;
            const bool closed=model==model_pipeline::Outcome::Complete && models->intact;
            if(!closed){
                ++counts.callbacks;
                if(models){if(model==model_pipeline::Outcome::Complete)++counts.model_revocations;models->revoke();}
                if(model==model_pipeline::Outcome::Declined &&
                   (!calls.invoke(calls.context,cpu,target) || cpu.pc!=continuation))throw Interrupted{};
                if(!a.refresh())throw ResumeOriginal{tail,tail_site};
                if(!local_sources_enabled() && !a.authenticate_all())throw ResumeOriginal{tail,tail_site};
            }
        }
        if(cpu.pc!=continuation)throw Interrupted{};
        // A nested child may have crossed a callback and revoked all source
        // proofs. Authenticate this parent's continuation before using it.
        if(!tail && local_sources_enabled() && !a.authenticate(owner))throw ResumeOriginal{};
    };
    switch(owner){
#include "hierarchy-switch.inc"
    default:throw Interrupted{};
    }
}
bool run(CpuState& cpu,Access& a,Calls calls,Flow& flow,std::uint32_t owner){
    const auto continuation=cpu.pr;
    struct Depth {Flow& f;Depth(Flow& v):f(v){++f.depth;}~Depth(){--f.depth;}} depth(flow);
    try{
        // Invalid/very deep data retains the exact original owner and its
        // scheduler/fault behavior; never truncate or repair a tree/key list.
        if(flow.depth>=128u || (owner==0x8C03FEB8u && !cpu.r[5]))a.restart(owner);
        if(local_sources_enabled() && !a.authenticate(owner))a.restart(owner);
        body(cpu,a,calls,flow,owner);return true;
    }catch(const ResumeOriginal& state){
        a.flush();
        // A tail child can finish this owner before invalidating our borrowed
        // memory/mode. Only the caller has instructions left at this address.
        // A normal call/access must still resume when the incoming PR happens
        // to alias its internal continuation. The original transfer kind matters.
        if(state.completed_tail && cpu.pc==continuation){
            if(state.tail_site)return_site=state.tail_site;
            if(owner==entry)return_site=0x8C04082Cu;
            if(owner==blended_entry)return_site=0x8C041B0Au;
            if(owner==rigid_entry)return_site=0x8C036F0Eu;
            if(owner==morph_entry)return_site=0x8C04093Eu;
            return false;
        }
        if(!calls.resume)throw;
        if(!calls.resume(calls.context,cpu,owner,continuation) || cpu.pc!=continuation)throw Interrupted{};
        if(owner==entry)return_site=0x8C04082Cu;
        if(owner==blended_entry)return_site=0x8C041B0Au;
        if(owner==rigid_entry)return_site=0x8C036F0Eu;
        if(owner==morph_entry)return_site=0x8C04093Eu;
        if(!a.refresh())return false;
        if(!local_sources_enabled() && !a.authenticate_all())return false;
        return true;
    }
}
#undef HIERARCHY_SITE
}
bool contains(std::uint32_t pc) noexcept {
    switch(pc){
#include "hierarchy-members.inc"
    default:return false;
    }
}
std::span<const SourceSpan> source_spans() noexcept {return identities;}
bool retained_source_matches(CpuState& cpu,const NativePortImmutableWriteGuard* guard) noexcept {
    if(!guard || guard->write_detected())return false;
    const auto memory=cpu.memory.direct_linear_memory_guard(false);
    if(!memory || memory.physical_base!=0x0C000000u || memory.physical_span<0x1000000u || memory.backing_mask!=0xFFFFFFu)return false;
    for(const auto& s:identities)
        if(std::memcmp(memory.read_bytes+(s.address&0xFFFFFFu),s.bytes.data(),s.bytes.size()))return false;
    return true;
}
Outcome execute(CpuState& cpu,const NativePortImmutableWriteGuard* guard,Calls calls){
    ++counts.declined;
    if(!guard || !calls.invoke || !contains(cpu.pc))return Outcome::Declined;
    Access access(cpu,*guard);
    if(!access.refresh())return Outcome::Declined;
    // The root proves its own body/literals here. Each entered child proves
    // its own sources in run(), sharing that proof until a foreign callback.
    // Authenticating every unrelated family at every tiny rigid root turns
    // a cheap object draw into a whole-inventory scan.
    if(!(local_sources_enabled() && root_sources_enabled()
            ?access.authenticate(cpu.pc):access.authenticate_all()))return Outcome::Declined;
    // Prove a bounded shared scratch-stack window once for this connected
    // hierarchy. Unusual/deeper accesses keep the per-store checked path.
    // Every foreign return revalidates the window with the current mappings,
    // observer and immutable ranges; this is not a persistent permission.
    if(local_sources_enabled())(void)access.admit_stack(cpu.r[15]-4096u,4096u);
    --counts.declined;++counts.calls;Flow flow;
    if(cpu.pc==rigid_entry)++counts.rigid_calls;
    if(cpu.pc==morph_entry)++counts.morph_calls;
    if(cpu.pc==land_entry)++counts.land_calls;
    try{run(cpu,access,calls,flow,cpu.pc);return Outcome::Complete;}
    catch(const ResumeOriginal&){return Outcome::ResumeOriginal;}
    catch(const Interrupted&){return Outcome::Interrupted;}
}
}
