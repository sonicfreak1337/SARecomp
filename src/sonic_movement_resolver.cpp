#include "sonic_movement_resolver.hpp"
#include "sonic_scalar_write_view.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include <bit>
#include <cstring>
#include <optional>
namespace sonic::movement {
namespace {
using namespace katana::runtime;
#include "movement-identities.inc"
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
    std::uint32_t stack{},stack_size{};
    std::uint64_t stores{};
public:
    Access(CpuState& cpu,const NativePortImmutableWriteGuard& guard):c(cpu),immutable(guard){}
    ~Access(){flush();}
    bool refresh() {
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
        return range(s.address,std::uint32_t(s.bytes.size())) &&
            std::memcmp(read.read_bytes+(s.address&0xFFFFFFu),s.bytes.data(),s.bytes.size())==0;
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
        ++counts.slow_accesses;flush();c.pc=at.pc;
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


#ifdef SARECOMP_MOVEMENT_TEST_COVERAGE
#define MOVEMENT_SITE(pc) (visited[((pc)-entry)/2u]=true)
#else
#define MOVEMENT_SITE(pc) ((void)0)
#endif
bool run(CpuState& cpu,Access& a,Calls calls,bool selector) {
    const auto load=[&](RestartPoint at,std::uint32_t address){return a.u32(address,at);};
    const auto load16=[&](RestartPoint at,std::uint32_t address){return a.load<std::uint16_t>(address,at);};
    const auto load8=[&](RestartPoint at,std::uint32_t address){return a.load<std::uint8_t>(address,at);};
    const auto store=[&](RestartPoint at,std::uint32_t address,std::uint32_t value,CodeWriteSource source){a.store<std::uint32_t>(address,value,at,source);};
    const auto store16=[&](RestartPoint at,std::uint32_t address,std::uint16_t value,CodeWriteSource source){a.store<std::uint16_t>(address,value,at,source);};
    const auto set_t=[&](bool value){cpu.t=value;};
    std::optional<HostFpuExecutionEpoch> epoch;epoch.emplace(cpu);
    const auto call=[&](std::uint32_t target){
        a.flush();epoch.reset();cpu.pc=target;const auto continuation=cpu.pr;
        ++counts.callbacks;
        if(!calls.invoke(calls.context,cpu,target) || cpu.pc!=continuation)throw Interrupted{};
        if(!a.refresh())throw ResumeOriginal{};
        for(unsigned i=0;i<identities.size();++i)if(!a.identity(i))throw ResumeOriginal{};
        epoch.emplace(cpu);
    };
    if(selector){
        // Authenticated stage selector. 0902/0903 retain their complete special
        // movement owner as a tail call, with the original caller PR restored.
        auto& r=cpu.r;
        a.u32(r[15]-4,cpu.pr,0x8C073018u);r[15]-=16;
        r[3]=0x8C04F7E0u;a.u32(r[15],r[4],0x8C07301Eu);a.u32(r[15]+4,r[5],0x8C073020u);
        const auto old_pr=cpu.pr;cpu.pr=0x8C073026u;
        a.u32(r[15]+8,r[6],RestartPoint{0x8C073022u,old_pr});call(r[3]);
        r[2]=9;r[2]<<=8;r[2]+=2;r[0]&=0xFFFFu;cpu.t=r[0]==r[2];
        bool tail=cpu.t;
        if(!tail){r[1]=0x8C04F7E0u;cpu.pr=0x8C073038u;call(r[1]);
            r[3]=9;r[3]<<=8;r[3]+=3;r[0]&=0xFFFFu;cpu.t=r[0]==r[3];tail=cpu.t;}
        const auto restore=tail?0x8C073044u:0x8C073052u;
        r[4]=a.u32(r[15],restore);r[5]=a.u32(r[15]+4,restore+2);r[6]=a.u32(r[15]+8,restore+4);r[15]+=12;
        if(tail){r[3]=0x8C077720u;cpu.pr=a.u32(r[15],0x8C07304Eu);r[15]+=4;call(r[3]);return_site=0x8C07304Eu;return true;}
        cpu.pr=a.u32(r[15],0x8C07305Au);r[15]+=4;
    }
    return_site=0x8C074210u;
    #include "movement-body.inc"
}
#undef MOVEMENT_SITE
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
Outcome execute(katana::runtime::CpuState& cpu,const katana::runtime::NativePortImmutableWriteGuard* guard,Calls calls){
    ++counts.declined;
    if(!guard || !calls.invoke || (cpu.pc!=entry && cpu.pc!=body))return Outcome::Declined;
    Access a(cpu,*guard);
    if(!a.refresh() || cpu.r[15]<624u || !a.admit_stack(cpu.r[15]-624u,624u))return Outcome::Declined;
    for(unsigned i=0;i<identities.size();++i)if(!a.identity(i))return Outcome::Declined;
    if(!a.range(cpu.r[4],44,4) || !a.range(cpu.r[5],40,4) || !a.range(cpu.r[6],276,4))return Outcome::Declined;
    if(!a.range(a.u32(cpu.r[6]+100,entry),120,4))return Outcome::Declined;
    --counts.declined;++counts.calls;
    try {run(cpu,a,calls,cpu.pc==entry);return Outcome::Complete;}
    catch(const ResumeOriginal&){return Outcome::ResumeOriginal;}
    catch(const Interrupted&){return Outcome::Interrupted;}
}
}
