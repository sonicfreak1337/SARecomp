#include "sonic_collision_world.hpp"
#include "sonic_scalar_write_view.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include <bit>
#include <bitset>
#include <cstring>
#include <memory>
#include <optional>
namespace sonic::collision_world {
namespace {
using namespace katana::runtime;
#include "world-identities.inc"
struct Interrupted {};
struct ResumeOriginal {};
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

// One RAM capability per callback-free stretch. Candidate counts and records
// remain live guest RAM, including the two stack arrays; no host snapshot.
class Access {
    CpuState& c;
    const NativePortImmutableWriteGuard& immutable;
    DirectLinearMemoryGuard read{},write{};
    bool p0{};
    bool sdk;
    std::uint32_t stack{},stack_size{};
    std::uint64_t stores{};
public:
    std::uint64_t serial{};
    Access(CpuState& cpu,const NativePortImmutableWriteGuard& guard,bool closed_sdk):c(cpu),immutable(guard),sdk(closed_sdk){}
    ~Access(){flush();}
    bool refresh() {
        ++serial;
        read={};write={};
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
        return !stack_size || admit_stack(stack,stack_size);
    }
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
        if(!sdk && s.address>=0x8C638000u)return true;
        return range(s.address,std::uint32_t(s.bytes.size())) &&
            std::memcmp(read.read_bytes+(s.address&0xFFFFFFu),s.bytes.data(),s.bytes.size())==0;
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
        if(!write || !range(address,8u,4u) || immutable.tracks_address(physical,8u))restart(at);
        const auto value=read_fpu_pair_bits(c,reg);
        std::memcpy(write.write_bytes+(address&0xFFFFFFu),&value,8u);stores+=2;
    }
    bool admit_stack(std::uint32_t a,std::uint32_t size) {
        if(!write || !range(a,size,4u) || immutable.tracks_address(a&0x1FFFFFFFu,size) ||
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
        if(write && range(a,sizeof(T),sizeof(T)) && (on_stack || !immutable.tracks_address(physical,sizeof(T)))){
            const T value=static_cast<T>(v);std::memcpy(write.write_bytes+(a&0xFFFFFFu),&value,sizeof(T));++stores;return;
        }
        restart(at);
    }
    void u32(std::uint32_t a,std::uint32_t v,RestartPoint at){store<std::uint32_t>(a,v,at);}
};
// These indexes replace repeated nested searches, not the visible lists. Raw
// keys (including zero/P1/P2) and the FIRST matching record remain distinct.
class Index {
    struct Item {std::uint32_t key{},value{};};
    struct Tables {
        std::array<Item,512> members{};
        std::array<Item,2048> eligible{};
        std::uint32_t head{},last_key{},last_eligible{},count{};
        bool members_checked{},members_ok{},eligible_checked{},eligible_ok{};
    };
    std::unique_ptr<Tables> data;
    std::uint64_t serial{};
    bool enabled;
    static constexpr std::uint32_t objects=0x0C6BB1BCu,polygons=0x0C6BE1BCu;
    static bool object(std::uint32_t p){const auto q=p&0x1FFFFFFFu;return q>=objects && q<polygons && (q-objects)%64==0;}
    static bool polygon(std::uint32_t p){const auto q=p&0x1FFFFFFFu;return q>=polygons && q<polygons+8192u*64u && (q-polygons)%64==0;}
    static bool overlap(std::uint32_t a,std::uint32_t n,std::uint32_t b,std::uint32_t m){
        a&=0x1FFFFFFFu;b&=0x1FFFFFFFu;return std::uint64_t(a)<std::uint64_t(b)+m && std::uint64_t(b)<std::uint64_t(a)+n;
    }
    template<std::size_t N> static Item& slot(std::array<Item,N>& table,std::uint32_t key){
        std::size_t i=((key^(key>>16))*0x9E3779B1u)&(N-1u);
        while(table[i].value && table[i].key!=key)i=(i+1)&(N-1u);
        return table[i];
    }
    static bool active(Access& a,std::uint32_t& head){
        head=a.u32(0x8C02D548u,entry);
        auto node=head,previous=0u-0x8C02D548u;
        std::bitset<192> seen;
        while(node){
            if(!object(node) || !a.range(node,64,4))return false;
            const auto id=((node&0x1FFFFFFFu)-objects)/64u;
            if(seen[id])return false;seen.set(id);
            if(a.u32(node+4,entry)!=previous)return false;
            const auto first=a.u32(node+16,entry),last=a.u32(node+20,entry);
            if(first && last && (!polygon(last) || !a.range(last,4,4)))return false;
            previous=node;node=a.u32(node,entry);
        }
        return true;
    }
    Tables& tables(Access& a){
        if(!data || serial!=a.serial){data=std::make_unique<Tables>();serial=a.serial;}
        return *data;
    }
public:
    bool sdk;
    explicit Index(bool on,bool closed_sdk):enabled(on),sdk(closed_sdk){}
    bool membership(CpuState& cpu,Access& a){
        if(!enabled)return false;
        auto& d=tables(a);
        if(!d.members_checked){
            d.members_checked=true;
            // The only intervening writes are both 192-word output arrays and
            // the live special key. Prove they cannot mutate indexed records.
            if(!a.range(cpu.r[15],0x618,4) ||
               overlap(cpu.r[15],0x618,objects,192u*64u) ||
               overlap(cpu.r[15],0x618,0x8C754E30u,4u+192u*12u) ||
               overlap(cpu.r[15],0x618,0x8C02D548u,4) ||
               overlap(cpu.r[15],0x618,0x8C73ED04u,4) || !active(a,d.head))return false;
            for(auto p=d.head;p;p=a.u32(p,entry)){
                const auto key=a.u32(p+8,entry);auto& item=slot(d.members,key);
                if(!item.value)item={key,p};d.last_key=key;
            }
            d.members_ok=true;
        }
        if(!d.members_ok || !a.range(cpu.r[4]+4,4,4))return false;
        cpu.r[3]=0x8C02D548u;cpu.r[5]=d.head;cpu.r[8]=cpu.r[14];cpu.t=!d.head;
        if(d.head){
            const auto key=a.u32(cpu.r[4]+4,0x8C028FA8u),node=slot(d.members,key).value;
            cpu.r[1]=key;cpu.r[2]=node?key:d.last_key;cpu.r[5]=node;
            if(node)cpu.r[8]=cpu.r[11];cpu.t=true;
        }
        ++counts.membership_hits;return true;
    }
    bool first_eligible(CpuState& cpu,Access& a){
        if(!enabled)return false;
        auto& d=tables(a);
        if(!d.eligible_checked){
            d.eligible_checked=true;
            const auto n=std::bit_cast<std::int32_t>(cpu.r[7]);std::uint32_t head;
            // Larger/aliased/malformed inputs retain the original uncapped loop.
            if(n<=0 || n>1024 || !active(a,head))return false;
            d.count=std::uint32_t(n);
            for(std::uint32_t i=0;i<d.count;++i){
                const auto key=a.u32(0x8C754E38u+i*12u,entry);auto& item=slot(d.eligible,key);
                if(!item.value)item={key,i+1};d.last_eligible=key;
            }
            d.eligible_ok=true;
        }
        if(!d.eligible_ok || cpu.r[7]!=d.count || cpu.r[5]!=0x8C754E34u || !object(cpu.r[4]) || !a.range(cpu.r[4],64,4))return false;
        const auto key=a.u32(cpu.r[4]+8,0x8C02901Cu),found=slot(d.eligible,key).value;
        cpu.r[3]=key;cpu.r[2]=found?key:d.last_eligible;cpu.t=found!=0;
        const auto skipped=found?found-1:d.count;cpu.r[5]+=skipped*12;cpu.r[7]-=skipped;
        ++counts.eligibility_hits;return true;
    }
};
#ifdef SARECOMP_COLLISION_WORLD_TEST_COVERAGE
#define WORLD_SITE(pc) (visited[((pc)>=0x8C638000u?0x53000u+(pc)-0x8C638000u:(pc)-0x8C000000u)/2u]=true)
#else
#define WORLD_SITE(pc) ((void)0)
#endif
bool run(CpuState&,Access&,Calls,Index&,std::uint32_t);
void body(CpuState& cpu,Access& a,Calls calls,Index& index,std::uint32_t owner){
    const auto load=[&](RestartPoint at,std::uint32_t address){return a.u32(address,at);};
    const auto load16=[&](RestartPoint at,std::uint32_t address){return a.load<std::uint16_t>(address,at);};
    const auto load8=[&](RestartPoint at,std::uint32_t address){return a.load<std::uint8_t>(address,at);};
    const auto store=[&](RestartPoint at,std::uint32_t address,std::uint32_t value,CodeWriteSource source){a.store<std::uint32_t>(address,value,at,source);};
    const auto store16=[&](RestartPoint at,std::uint32_t address,std::uint16_t value,CodeWriteSource source){a.store<std::uint16_t>(address,value,at,source);};
    const auto fload=[&](RestartPoint at,unsigned reg,std::uint32_t address){a.fload(at,reg,address);};
    const auto fstore=[&](RestartPoint at,std::uint32_t address,unsigned reg){a.fstore(at,address,reg);};
    const auto set_t=[&](bool value){cpu.t=value;};
    // SDK arithmetic retains its original helper-local rounding scopes. In
    // particular FSCA -> FTRV must not inherit one larger rounding epoch.
    std::optional<HostFpuExecutionEpoch> epoch;
    if(!sdk_contains(owner))epoch.emplace(cpu);
    const auto call=[&](std::uint32_t target){
        // Sharing memory does not broaden the reviewed arithmetic epoch.
        epoch.reset();cpu.pc=target;const auto continuation=cpu.pr;
        if(contains(target) || (index.sdk && sdk_contains(target))){
#ifdef SARECOMP_COLLISION_WORLD_TEST_COVERAGE
            if(sdk_contains(target) && calls.sdk_boundary)calls.sdk_boundary(calls.context,cpu,target,false);
#endif
            ++counts.internal_calls;
            if(!run(cpu,a,calls,index,target))throw ResumeOriginal{};
            // SDK destinations are live RAM and may alias list/index inputs.
            // Retain the old callback's index lifetime without reacquiring RAM.
            if(sdk_contains(target))++a.serial;
#ifdef SARECOMP_COLLISION_WORLD_TEST_COVERAGE
            if(sdk_contains(target) && calls.sdk_boundary)calls.sdk_boundary(calls.context,cpu,target,true);
#endif
        }else{
            a.flush();++counts.callbacks;
            if(!calls.invoke(calls.context,cpu,target) || cpu.pc!=continuation)throw Interrupted{};
            if(!a.refresh())throw ResumeOriginal{};
            for(unsigned i=0;i<identities.size();++i)if(!a.identity(i))throw ResumeOriginal{};
        }
        if(cpu.pc!=continuation)throw Interrupted{};
        epoch.emplace(cpu);
    };
    switch(owner){
    case 0x8C028EC2u: {
#include "world-world.inc"
    }
    case 0x8C052518u: {
#include "world-eligibility.inc"
    }
    case 0x8C028B00u: {
#include "world-hierarchy.inc"
    }
    case 0x8C0287A0u: {
#include "world-polygons.inc"
    }
    case 0x8C028666u: {
#include "world-vertices.inc"
    }
    case 0x8C02CEF4u: {
#include "world-object_allocate.inc"
    }
    case 0x8C02CF48u: {
#include "world-object_release.inc"
    }
    case 0x8C02CF68u: {
#include "world-polygon_dot.inc"
    }
    case 0x8C02CFA4u: {
#include "world-buckets_clear.inc"
    }
    case 0x8C02CFC0u: {
#include "world-polygon_allocate.inc"
    }
    case 0x8C02D00Eu: {
#include "world-buckets_join.inc"
    }
    case 0x8C638E0Cu: {
#include "world-point.inc"
    }
    case 0x8C639BB0u: {
#include "world-push.inc"
    }
    case 0x8C639AD8u: {
#include "world-pop.inc"
    }
    case 0x8C639E08u: {
#include "world-rotate_x.inc"
    }
    case 0x8C639E9Cu: {
#include "world-rotate_y.inc"
    }
    case 0x8C63A10Cu: {
#include "world-rotate_z.inc"
    }
    case 0x8C63A52Cu: {
#include "world-scale.inc"
    }
    case 0x8C63A744u: {
#include "world-translate.inc"
    }
    case 0x8C63A820u: {
#include "world-identity.inc"
    }
    case 0x8C63A904u: {
#include "world-sqrt.inc"
    }
    default:throw Interrupted{};
    }
}
bool run(CpuState& cpu,Access& a,Calls calls,Index& index,std::uint32_t owner){
    const auto continuation=cpu.pr;
    try{body(cpu,a,calls,index,owner);return true;}
    catch(const ResumeOriginal&){
        a.flush();
        // Component/original bridges resume this precise activation, including
        // nested hierarchy/pool activations, before returning to the parent.
        if(!calls.resume)throw;
        if(!calls.resume(calls.context,cpu,owner) || cpu.pc!=continuation)throw Interrupted{};
        // The authenticated root has one RTS. A complete retained fallback
        // did not execute our emitted return marker; do not publish a stale
        // child's RTS (or zero) to the outer dispatch boundary.
        if(owner==entry)return_site=0x8C029398u;
        if(!a.refresh())return false;
        for(unsigned i=0;i<identities.size();++i)if(!a.identity(i))return false;
        return true;
    }
}
#undef WORLD_SITE
}
bool contains(std::uint32_t pc) noexcept {
    switch(pc){
    case 0x8C028EC2u:
    case 0x8C052518u:
    case 0x8C028B00u:
    case 0x8C0287A0u:
    case 0x8C028666u:
    case 0x8C02CEF4u:
    case 0x8C02CF48u:
    case 0x8C02CF68u:
    case 0x8C02CFA4u:
    case 0x8C02CFC0u:
    case 0x8C02D00Eu:
        return true;
    default:return false;
    }
}
std::span<const SourceSpan> source_spans() noexcept {return identities;}
bool sdk_contains(std::uint32_t pc) noexcept {
    switch(pc){
    case 0x8C638E0Cu:case 0x8C639BB0u:case 0x8C639AD8u:
    case 0x8C639E08u:case 0x8C639E9Cu:case 0x8C63A10Cu:
    case 0x8C63A52Cu:case 0x8C63A744u:case 0x8C63A820u:case 0x8C63A904u:return true;
    default:return false;
    }
}
bool retained_source_matches(CpuState& cpu,const NativePortImmutableWriteGuard* guard) noexcept {
    if(!guard || guard->write_detected())return false;
    const auto memory=cpu.memory.direct_linear_memory_guard(false);
    if(!memory || memory.physical_base!=0x0C000000u || memory.physical_span<0x1000000u || memory.backing_mask!=0xFFFFFFu)return false;
    for(const auto& s:identities)
        if(s.address<0x8C638000u || sdk_enabled())
        if(std::memcmp(memory.read_bytes+(s.address&0xFFFFFFu),s.bytes.data(),s.bytes.size()))return false;
    return true;
}
Outcome execute(CpuState& cpu,const NativePortImmutableWriteGuard* guard,Calls calls,bool indexed,bool sdk){
    ++counts.declined;
    if(!guard || !calls.invoke || !(contains(cpu.pc) || (sdk && sdk_contains(cpu.pc))))return Outcome::Declined;
    Access access(cpu,*guard,sdk);
    if(!access.refresh())return Outcome::Declined;
    for(unsigned i=0;i<identities.size();++i)if(!access.identity(i))return Outcome::Declined;
    --counts.declined;++counts.calls;Index index(indexed,sdk);
    try{run(cpu,access,calls,index,cpu.pc);return Outcome::Complete;}
    catch(const ResumeOriginal&){return Outcome::ResumeOriginal;}
    catch(const Interrupted&){return Outcome::Interrupted;}
}
}
