#include "sonic_object_activation.hpp"
#include "sonic_scalar_write_view.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/fpu.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <cmath>
#include <limits>
#include <span>

namespace sonic::object_activation {
namespace {
using namespace katana::runtime;
struct SourceSpan {std::uint32_t address;std::span<const std::uint8_t> bytes;};
#include "sonic_object_activation_identity.inc"
struct Interrupted {};
bool mode_ok(const CpuState& c) noexcept {
    const auto f=c.read_fpscr();
    return c.privileged_mode_inline() && !c.trap_pending && !c.sleeping && !(c.sr&sr_fd_mask) &&
        !(f&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) &&
        (f&fpscr_dn_mask) && (f&fpscr_rounding_mode_mask)<=1u;
}
bool nonnegative(std::uint32_t value) noexcept {return !(value&0x80000000u);}
std::uint32_t signed8(std::uint8_t v) noexcept {return std::uint32_t(std::int32_t(std::bit_cast<std::int8_t>(v)));}
std::uint32_t signed16(std::uint16_t v) noexcept {return std::uint32_t(std::int32_t(std::bit_cast<std::int16_t>(v)));}

// One RAM capability per callback-free stretch. The loop rereads mutable
// records, metadata and globals at the original points; there is no table cache.
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
    template<class T> T load(std::uint32_t a,std::uint32_t pc) {
        T value;
        if(range(a,sizeof(T),sizeof(T))){std::memcpy(&value,read.read_bytes+(a&0xFFFFFFu),sizeof(T));return value;}
        ++counts.slow_accesses;flush();const auto exceptions=c.exception_generation;
        const GuestInstructionOrigin origin{pc,pc,true};
        if constexpr(sizeof(T)==4)value=guest_read_u32_at(c,origin,a);
        if constexpr(sizeof(T)==2)value=guest_read_u16_at(c,origin,a);
        if constexpr(sizeof(T)==1)value=guest_read_u8_at(c,origin,a);
        if(c.trap_pending || c.exception_generation!=exceptions || !refresh())throw Interrupted{};
        return value;
    }
    std::uint32_t u32(std::uint32_t a,std::uint32_t pc){return load<std::uint32_t>(a,pc);}
    std::uint32_t s16(std::uint32_t a,std::uint32_t pc){return signed16(load<std::uint16_t>(a,pc));}
    std::uint32_t s8(std::uint32_t a,std::uint32_t pc){return signed8(load<std::uint8_t>(a,pc));}
    template<class T> void store(std::uint32_t a,std::uint32_t v,std::uint32_t pc,CodeWriteSource source=CodeWriteSource::Cpu) {
        const auto physical=a&0x1FFFFFFFu;
        const bool on_stack=stack_size && physical>=(stack&0x1FFFFFFFu) &&
            std::uint64_t(physical)+sizeof(T)<=std::uint64_t(stack&0x1FFFFFFFu)+stack_size;
        if(write && range(a,sizeof(T),sizeof(T)) && (on_stack || !immutable.tracks_address(physical,sizeof(T)))){
            const T value=static_cast<T>(v);std::memcpy(write.write_bytes+(a&0xFFFFFFu),&value,sizeof(T));++stores;return;
        }
        ++counts.slow_accesses;flush();const auto exceptions=c.exception_generation;
        const GuestInstructionOrigin origin{pc,pc,true};
        if constexpr(sizeof(T)==4)guest_write_u32_at(c,origin,a,v,source);
        if constexpr(sizeof(T)==2)guest_write_u16_at(c,origin,a,static_cast<T>(v));
        if constexpr(sizeof(T)==1)guest_write_u8_at(c,origin,a,static_cast<T>(v));
        if(c.trap_pending || c.exception_generation!=exceptions || !refresh())throw Interrupted{};
    }
    void u32(std::uint32_t a,std::uint32_t v,std::uint32_t pc){store<std::uint32_t>(a,v,pc);}
};

// Compute the complete finite predicate in locals, with one rounding scope.
// Binary32 products and admitted subtractions are exact in binary64. For the
// nonnegative square-add, nested truncation is exact under RM=1. Under RM=0,
// exclude binary32 midpoints, the only possible double-rounding boundary.
// No host FMA is used: the VM's FMA result depends on incoming sticky status.
// Anything outside the proof falls back before changing guest state.
bool finite_distance(CpuState& c,Access& a) {
    const auto* bytes=a.point(c.r[4]);
    if(!bytes)return false;
    std::array<std::uint32_t,3> p{};std::memcpy(p.data(),bytes,12u);
    auto regular=[](std::uint32_t b){const auto m=b&0x7FFFFFFFu;
        return m==0 || (m>=0x00800000u && m<0x7F800000u);};
    for(unsigned i=0;i<3;++i)if(!regular(p[i]) || !regular(c.fr[4+i]))return false;
    if(!regular(c.fr[7]))return false;
    const HostFpuExecutionEpoch epoch(c);
    auto f=c.fr;std::uint32_t flags=0;
    auto narrow=[&](double exact,float& out,bool arithmetic){
        const auto magnitude=std::fabs(exact);
        if(magnitude!=0.0 && (magnitude<double(std::numeric_limits<float>::min()) ||
           magnitude>double(std::numeric_limits<float>::max())))return false;
        out=static_cast<float>(exact);
        if(!regular(std::bit_cast<std::uint32_t>(out)))return false;
        if(arithmetic && double(out)!=exact)flags|=4u;
        return true;
    };
    auto subtract=[&](std::uint32_t x,std::uint32_t y,float& out){
        if((x&0x7FFFFFFFu) && (y&0x7FFFFFFFu)){
            const int gap=int((x>>23)&255u)-int((y>>23)&255u);
            if(gap < -28 || gap > 28)return false;
        }
        return narrow(double(std::bit_cast<float>(x))-double(std::bit_cast<float>(y)),out,true);
    };
    auto accumulate=[&](float d,float previous,float& out){
        const double product=double(d)*double(d),addend=double(previous);
        const double sum=product+addend;
        if(!(c.read_fpscr()&fpscr_rounding_mode_mask) &&
           (std::bit_cast<std::uint64_t>(sum)&0x1FFFFFFFull)==0x10000000ull)return false;
        // The retained FMAC clears Cause but does not accumulate arithmetic
        // sticky flags. Only SUB and the initial MUL contribute the I flag.
        return narrow(sum,out,false);
    };
    float x{},sum{};
    if(!subtract(f[4],p[0],x) || !narrow(double(x)*double(x),sum,true))return false;
    f[3]=std::bit_cast<std::uint32_t>(x);f[4]=std::bit_cast<std::uint32_t>(sum);
    const auto limit=std::bit_cast<float>(f[7]);bool outside=sum>limit;
    for(unsigned axis=1;axis<3 && !outside;++axis){
        float d{},next{};
        if(!subtract(f[4+axis],p[axis],d) || !accumulate(d,sum,next))return false;
        f[4+axis]=f[0]=std::bit_cast<std::uint32_t>(d);
        sum=next;f[4]=std::bit_cast<std::uint32_t>(sum);outside=sum>limit;
    }
    f[8]=p[0];f[9]=p[1];f[10]=p[2];c.fr=f;
    // Arithmetic changes Cause/Flags directly; LDFPSCR normalization would
    // incorrectly discard reserved incoming bits that the original preserves.
    c.fpscr=(c.fpscr&~0x0003F000u)|flags;
    c.r[4]+=8;c.r[0]=outside?0u:1u;c.t=outside;c.pc=c.pr;
    return_site=outside?0x8C091084u:0x8C09107Eu;
    return true;
}

void distance(CpuState& c,Access& a) {
    ++counts.distance_calls;
    if(finite_distance(c,a)){++counts.distance_native;return;}
    ++counts.distance_fallback;
    // Retain the original per-operation host rounding boundaries. Extending
    // an epoch across this predicate changes Linux/TCG FMAC rounding at an
    // exact distance threshold; even one ULP changes object activation.
    auto& r=c.r;auto& f=c.fr;
    f[8]=a.u32(r[4],0x8C09105Au);r[4]+=4;
    fpu_binary(c,FpuBinaryOperation::Subtract,8u,4u);
    f[9]=a.u32(r[4],0x8C09105Eu);r[4]+=4;f[3]=f[4];
    fpu_binary(c,FpuBinaryOperation::Multiply,3u,4u);
    fpu_compare_greater(c,7u,4u);
    f[10]=a.u32(r[4],0x8C091068u); // read-ahead in the taken OR untaken delay slot
    if(!c.t){
        fpu_binary(c,FpuBinaryOperation::Subtract,9u,5u);f[0]=f[5];
        fpu_multiply_accumulate(c,5u,4u);fpu_compare_greater(c,7u,4u);
        if(!c.t){
            fpu_binary(c,FpuBinaryOperation::Subtract,10u,6u);f[0]=f[6];
            fpu_multiply_accumulate(c,6u,4u);fpu_compare_greater(c,7u,4u);
        }
    }
    r[0]=c.t?0u:1u;return_site=c.t?0x8C091084u:0x8C09107Eu;c.pc=c.pr;
}

void save(CpuState& c,Access& a,unsigned first,std::uint32_t pc) {
    for(int i=14;i>=int(first);--i){c.r[15]-=4;a.u32(c.r[15],c.r[i],pc);pc+=2;}
    for(int i=15;i>=12;--i){c.r[15]-=4;a.store<std::uint32_t>(c.r[15],c.fr[i],pc,CodeWriteSource::Fpu);pc+=2;}
    c.r[15]-=4;a.u32(c.r[15],c.pr,pc);c.r[15]-=4;
}
void restore(CpuState& c,Access& a,unsigned first,std::uint32_t pc) {
    c.r[15]+=4;c.pr=a.u32(c.r[15],pc+2);c.r[15]+=4;pc+=4;
    for(unsigned i=12;i<16;++i){c.fr[i]=a.u32(c.r[15],pc);c.r[15]+=4;pc+=2;}
    for(unsigned i=first;i<15;++i){c.r[i]=a.u32(c.r[15],pc+(i==14u?2u:0u));c.r[15]+=4;pc+=2;}
    c.pc=c.pr;
}
void invoke(CpuState& c,Access& a,Calls calls,std::uint32_t target,std::uint32_t continuation) {
    a.flush();c.pc=target;c.pr=continuation;
    if(!calls.invoke(calls.context,c,target) || c.pc!=continuation || !a.refresh())throw Interrupted{};
    // These are real retained calls, not pre-captured task side effects.
    for(unsigned i:{0u,1u,2u,5u,6u,7u,8u})if(!a.identity(i))throw Interrupted{};
}

void activate(CpuState& c,Access& a,Calls calls) {
    ++counts.activation_calls;auto& r=c.r;auto& f=c.fr;
    save(c,a,8u,0x8C0912C0u);
    r[1]=0x8C78C548u;r[3]=a.u32(r[1],0x8C0912DCu);c.t=r[3]==0;
    bool alternate=c.t;
    if(!alternate){
        r[3]=0x8C19B3A8u;r[0]=a.s16(r[3],0x8C0912E4u);c.t=r[0]==1;
        alternate=c.t;
        if(!alternate){
            // Complete authenticated, read-only 04F7E0 body. Keep signed
            // halfword composition and all live scratch registers; no bridge.
            c.pr=0x8C0912F0u;r[2]=0x8C7492FAu;r[1]=0x8C7492FCu;
            r[0]=a.s16(r[2],0x8C04F7E4u);r[3]=a.s16(r[1],0x8C04F7E6u);
            r[0]<<=8;r[0]|=r[3];c.pc=c.pr;
            r[3]=0xFF00u;r[2]=42;r[0]&=0xFFFFu;r[2]<<=8;r[0]&=r[3];c.t=r[0]==r[2];
            alternate=c.t;
            if(!alternate){
                // 04F698 returns an unsigned halfword and leaves T unchanged.
                r[1]=0x8C04F698u;c.pr=0x8C091304u;r[2]=0x8C749308u;
                r[0]=a.s16(r[2],0x8C04F69Au)&0xFFFFu;c.pc=c.pr;c.t=r[0]==7;
                if(c.t){r[3]=0x8C7A4028u;r[0]=a.s16(r[3],0x8C09130Au);r[0]&=0xFFFFu;c.t=(r[0]&64u)==0;alternate=!c.t;}
            }
        }
    }
    if(alternate){r[3]=0x8C18B72Cu;r[4]=a.u32(r[3],0x8C091314u);c.t=r[4]==0;}
    else {r[4]=0x8C78C548u;r[4]=a.u32(r[4],0x8C091324u);}
    if(!alternate || !c.t){
        r[0]=32;r[3]=0x8C79A31Cu;f[14]=a.u32(r[4]+r[0],0x8C09132Au);
        r[0]=36;f[13]=a.u32(r[4]+r[0],0x8C09132Eu);r[0]=40;
        r[9]=a.s16(r[3],0x8C091332u);f[15]=a.u32(r[4]+r[0],0x8C091334u);
        r[12]=0x8C79A320u;--r[9];r[8]=1;
        while((c.t=nonnegative(r[9]))){
            ++counts.records;
            r[0]=a.s16(r[12]+2u,0x8C09133Eu);r[4]=r[0];c.t=nonnegative(r[4]);
            if(!c.t){
                r[2]=1;c.t=(r[4]&r[2])==0;
                if(c.t){
                    r[11]=a.u32(r[12]+8u,0x8C091354u);r[1]=0xFFFFFF80u;r[1]<<=8;
                    r[3]=0x8C79A30Cu;r[2]=a.s16(r[11],0x8C09135Cu);r[1]=~r[1];
                    r[10]=a.u32(r[3],0x8C091360u);r[2]&=0xFFFFu;r[2]&=r[1];
                    r[10]=a.u32(r[10]+4u,0x8C091366u);r[0]=r[2];r[2]<<=2;r[2]+=r[0];r[2]<<=2;r[10]+=r[2];
                    r[2]=2;r[4]&=r[2];a.u32(r[15],r[4],0x8C091376u);
                    r[0]=a.s16(r[10]+2u,0x8C091378u);r[3]=r[0];c.t=(r[8]&r[3])==0;r[4]=r[0];
                    bool unconditional=false;
                    if(!c.t){r[0]=4;f[12]=a.u32(r[10]+r[0],0x8C091386u);}
                    else {r[2]=2;c.t=(r[4]&r[2])==0;
                        if(!c.t){r[0]=0x8C0913E8u;f[12]=a.u32(r[0],0x8C091392u);unconditional=true;}
                        else {r[0]=0x8C0913ECu;f[12]=a.u32(r[0],0x8C091396u);}}
                    if(!unconditional){r[3]=4;c.t=(r[3]&r[4])==0;
                        if(!c.t){r[2]=0x8C1C0E18u;r[1]=a.u32(r[2],0x8C0913A0u);c.t=r[1]==0;unconditional=c.t;}}
                    if(unconditional)r[4]=r[8];
                    else {
                        r[3]=a.u32(r[15],0x8C0913AAu);c.t=r[3]==0;
                        if(c.t)r[4]=r[11]+8u;
                        else r[4]=a.u32(r[12]+12u,0x8C091400u)+12u;
                        f[4]=f[14];f[5]=f[13];f[7]=f[12];f[6]=f[15];
                        c.pr=0x8C091410u;distance(c,a);r[4]=r[0];
                    }
                    c.t=r[4]==0;
                    if(!c.t){
                        r[0]=a.s8(r[10]+1u,0x8C091416u);r[4]=a.s8(r[10],0x8C091418u);
                        r[3]=0x8C09846Eu;r[5]=r[0]&0xFFu;r[6]=a.u32(r[10]+12u,0x8C09141Eu);r[4]&=0xFFu;
                        invoke(c,a,calls,r[3],0x8C091424u);r[13]=r[0];c.t=r[13]==0;
                        if(!c.t){
                            ++counts.created;r[3]=a.s8(r[12],0x8C09142Au);++r[3];a.store<std::uint8_t>(r[12],r[3],0x8C09142Eu);
                            r[0]=a.s16(r[12]+2u,0x8C091430u);r[0]|=1;a.store<std::uint16_t>(r[12]+2u,r[0],0x8C091434u);
                            a.u32(r[13]+28u,r[12],0x8C091436u);r[14]=a.u32(r[13]+32u,0x8C091438u);c.t=r[14]==0;
                            if(!c.t){
                                r[2]=a.u32(r[15],0x8C09143Eu);c.t=r[2]==0;
                                if(!c.t){
                                    r[3]=a.u32(r[12]+12u,0x8C091444u);
                                    r[2]=a.u32(r[3]+12u,0x8C091446u);r[1]=a.u32(r[3]+16u,0x8C091448u);
                                    a.u32(r[14]+32u,r[2],0x8C09144Au);a.u32(r[14]+36u,r[1],0x8C09144Cu);
                                    r[2]=a.u32(r[3]+20u,0x8C09144Eu);a.u32(r[14]+40u,r[2],0x8C091450u);
                                    r[0]=a.u32(r[12]+12u,0x8C091452u);r[3]=a.u32(r[0],0x8C091454u);r[2]=a.u32(r[0]+4u,0x8C091456u);
                                    a.u32(r[14]+20u,r[3],0x8C091458u);a.u32(r[14]+24u,r[2],0x8C09145Au);r[3]=a.u32(r[0]+8u,0x8C09145Cu);a.u32(r[14]+28u,r[3],0x8C09145Eu);
                                    r[1]=a.u32(r[12]+12u,0x8C091460u);r[3]=a.u32(r[1]+24u,0x8C091462u);r[2]=a.u32(r[1]+28u,0x8C091464u);
                                    a.u32(r[14]+44u,r[3],0x8C091466u);a.u32(r[14]+48u,r[2],0x8C091468u);r[3]=a.u32(r[1]+32u,0x8C09146Au);a.u32(r[14]+52u,r[3],0x8C09146Cu);
                                    r[3]=0x8C098A82u;r[4]=a.u32(r[12]+12u,0x8C091472u);invoke(c,a,calls,r[3],0x8C091474u);
                                    r[0]=a.s16(r[12]+2u,0x8C091474u);r[3]=0xFFFFFFFDu;r[0]&=r[3];a.store<std::uint16_t>(r[12]+2u,r[0],0x8C09147Cu);
                                } else {
                                    r[1]=r[11]+8u;r[3]=a.u32(r[1],0x8C091482u);r[2]=a.u32(r[1]+4u,0x8C091484u);
                                    a.u32(r[14]+32u,r[3],0x8C091486u);a.u32(r[14]+36u,r[2],0x8C091488u);r[3]=a.u32(r[1]+8u,0x8C09148Au);a.u32(r[14]+40u,r[3],0x8C09148Cu);
                                    r[3]=r[11];r[0]=a.s16(r[11]+2u,0x8C091490u);r[3]+=20u;a.u32(r[14]+20u,r[0],0x8C091494u);
                                    r[0]=a.s16(r[11]+4u,0x8C091496u);a.u32(r[14]+24u,r[0],0x8C091498u);r[0]=a.s16(r[11]+6u,0x8C09149Au);a.u32(r[14]+28u,r[0],0x8C09149Cu);
                                    r[2]=a.u32(r[3],0x8C09149Eu);r[1]=a.u32(r[3]+4u,0x8C0914A0u);a.u32(r[14]+44u,r[2],0x8C0914A2u);a.u32(r[14]+48u,r[1],0x8C0914A4u);r[2]=a.u32(r[3]+8u,0x8C0914A6u);a.u32(r[14]+52u,r[2],0x8C0914A8u);
                                }
                                r[0]=a.u32(r[10]+8u,0x8C0914AAu);a.u32(r[14]+8u,r[0],0x8C0914ACu);
                            }
                            r[0]=0x8C091588u;f[3]=a.u32(r[0],0x8C0914B0u);r[0]=12u;
                            fpu_binary(c,FpuBinaryOperation::Add,3u,12u);a.store<std::uint32_t>(r[12]+r[0],f[12],0x8C0914B6u,CodeWriteSource::Fpu);
                            a.u32(r[12]+4u,r[13],0x8C0914B8u);
                        }
                    }
                }
            }
            --r[9];r[12]+=16;
        }
    }
    restore(c,a,8u,0x8C0914C6u);return_site=0x8C0914DEu;
}

void lifetime(CpuState& c,Access& a) {
    ++counts.lifetime_calls;auto& r=c.r;auto& f=c.fr;
    save(c,a,13u,0x8C091928u);r[14]=r[4];
    bool remove=false;
    do {
        r[4]=a.u32(r[14]+28u,0x8C09193Au);c.t=r[4]==0;f[12]=f[4];
        if(!c.t){r[0]=a.s16(r[4]+2u,0x8C091942u);c.t=(r[0]&8u)==0;if(!c.t)break;}
        f[3]=0;fpu_compare_equal(c,3u,12u);if(c.t)break;
        r[4]=a.u32(r[14]+32u,0x8C09194Eu);r[0]=32;r[2]=0x8C18B72Cu;
        f[14]=a.u32(r[4]+r[0],0x8C091954u);r[0]=36;f[13]=a.u32(r[4]+r[0],0x8C091958u);r[0]=40;
        r[3]=a.u32(r[2],0x8C09195Cu);f[15]=a.u32(r[4]+r[0],0x8C09195Eu);c.t=r[3]==0;a.u32(r[15],r[3],0x8C091964u);
        if(!c.t){
            r[4]=a.u32(r[15],0x8C091966u);f[4]=f[14];f[5]=f[13];f[7]=f[12];f[6]=f[15];r[4]+=32;
            c.pr=0x8C091974u;distance(c,a);c.t=r[0]==0;if(!c.t)break;
        }
        r[3]=0x8C78C548u;r[2]=a.u32(r[3],0x8C09197Au);c.t=r[2]==0;a.u32(r[15],r[2],0x8C091980u);
        if(!c.t){
            r[4]=a.u32(r[15],0x8C091982u);f[4]=f[14];f[5]=f[13];f[7]=f[12];f[6]=f[15];r[4]+=32;
            c.pr=0x8C091990u;distance(c,a);c.t=r[0]==0;if(!c.t)break;
        }
        r[13]=0x8C78C54Cu;r[13]=a.u32(r[13],0x8C091996u);c.t=r[13]==0;
        if(c.t){remove=true;break;}
        r[0]=a.s8(r[13]+9u,0x8C09199Cu);c.t=r[0]==0;
        if(!c.t){remove=true;break;}
        r[0]=0x8C091A10u;f[4]=f[14];f[7]=a.u32(r[0],0x8C0919A6u);r[4]=r[13];f[6]=f[15];f[5]=f[13];r[4]+=32;
        c.pr=0x8C0919B2u;distance(c,a);c.t=r[0]==0;remove=c.t;
    } while(false);
    if(remove){r[3]=0x8C09859Cu;r[0]=1;a.u32(r[14]+16u,r[3],0x8C0919BEu);++counts.retired;}
    else r[0]=0;
    restore(c,a,13u,0x8C0919C0u);return_site=0x8C0919CEu;
}
}
Outcome execute(katana::runtime::CpuState& cpu,const katana::runtime::NativePortImmutableWriteGuard* guard,Calls calls) {
    ++counts.declined;const auto entry=cpu.pc;
    if(!guard || (entry!=0x8C09105Au && entry!=0x8C0912C0u && entry!=0x8C091928u))return Outcome::Declined;
    Access a(cpu,*guard);
    if(!a.refresh() || !a.identity(0))return Outcome::Declined;
    if(entry==0x8C0912C0u && (!calls.invoke || !a.identity(1) || !a.identity(2) ||
       !a.identity(5) || !a.identity(6) || !a.identity(7) || !a.identity(8) ||
       cpu.r[15]<52u || !a.admit_stack(cpu.r[15]-52u,52u)))return Outcome::Declined;
    if(entry==0x8C091928u && (!a.identity(3) || !a.identity(4) ||
       cpu.r[15]<32u || !a.admit_stack(cpu.r[15]-32u,32u)))return Outcome::Declined;
    if(entry==0x8C09105Au && !a.range(cpu.r[4],12u,4u))return Outcome::Declined;
    --counts.declined;
    try {
        if(entry==0x8C09105Au)distance(cpu,a);
        else if(entry==0x8C0912C0u)activate(cpu,a,calls);
        else lifetime(cpu,a);
        return Outcome::Complete;
    }catch(const Interrupted&){return Outcome::Interrupted;}
}
}
