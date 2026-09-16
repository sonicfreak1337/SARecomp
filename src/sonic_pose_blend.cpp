#include "sonic_pose_blend.hpp"
#include "sonic_model_math.hpp"
#include "sonic_native_model_memory.hpp"
#include "katana/runtime/block_guards.hpp"
#include "katana/runtime/native_port_aot_runtime.hpp"
#include <cstdlib>
#include <cstring>
#include <optional>
#include <stdexcept>

namespace sonic::pose_blend {
namespace {
using namespace katana::runtime;
#include "pose-blend-identities.inc"
thread_local Statistics counters;
constexpr std::uint32_t inputs=0x8C88FE7Cu,rotation_slot=0x8C19AC84u;
bool overlap(std::uint32_t a,std::uint32_t size,std::uint32_t b,std::uint32_t length){
    a&=0x1FFFFFFFu;b&=0x1FFFFFFFu;
    return a<std::uint64_t(b)+length && b<std::uint64_t(a)+size;
}
struct Execution : sonic::model_math::Transform {
    DirectLinearMemoryGuard memory;
    std::optional<sonic::model_memory::Writes> writes;
    std::uint32_t read(std::uint32_t a)const{
        std::uint32_t value;std::memcpy(&value,memory.read_bytes+(a&0xFFFFFFu),4u);return value;
    }
    auto triple(std::uint32_t a)const{return std::array{read(a),read(a+4u),read(a+8u)};}
    void store(std::uint32_t a,std::uint32_t v,CodeWriteSource source=CodeWriteSource::Cpu){
        writes->store(a,v,source);
    }
    std::array<std::uint32_t,3> blend(std::uint32_t source,std::uint32_t weight,bool angles){
        const auto first=triple(source),second=triple(source+12u);
        std::array<std::uint32_t,3> result{};
        cpu.fr[0]=weight;cpu.fr[1]=0x3F800000u;
        binary(FpuBinaryOperation::Subtract,0u,1u);
        for(unsigned i=0;i<3u;++i){
            if(angles){cpu.fpul=first[i];fpu_float_from_fpul(cpu,2u);}
            else cpu.fr[2]=first[i];
            binary(FpuBinaryOperation::Multiply,1u,2u);
            if(angles){cpu.fpul=second[i];fpu_float_from_fpul(cpu,3u);}
            else cpu.fr[3]=second[i];
            binary(FpuBinaryOperation::Multiply,0u,3u);
            binary(FpuBinaryOperation::Add,3u,2u);
            if(angles){fpu_truncate_to_fpul(cpu,2u);result[i]=cpu.fpul;}
            else result[i]=cpu.fr[2];
        }
        return result;
    }
    void run(std::uint32_t rotation){
        const auto saved_sp=cpu.r[15],saved_pr=cpu.pr;
        const auto frame=saved_sp-48u;
        store(saved_sp-4u,cpu.r[14]);store(saved_sp-8u,cpu.r[12]);store(saved_sp-12u,saved_pr);
        const auto weight=read(inputs),switches=read(inputs+4u);
        const auto position=(switches&0xFFFFu)?blend(inputs+16u,weight,false):triple(inputs+16u);
        for(int i=2;i>=0;--i)store(frame+24u+4u*i,position[i],CodeWriteSource::Fpu);
        const auto angles=(switches>>16u)?blend(inputs+112u,weight,true):triple(inputs+112u);
        for(unsigned i=0;i<3u;++i)store(frame+4u*i,angles[i]);
        // Preserve the authored oddity: blended scale reuses the POSITION
        // pair. Only the unmixed scale reads the separate scale vector.
        const auto scales=(read(inputs+8u)&0xFFFFu)?blend(inputs+16u,weight,false):triple(inputs+64u);
        for(int i=2;i>=0;--i)store(frame+12u+4u*i,scales[i],CodeWriteSource::Fpu);
        std::copy(position.begin(),position.end(),cpu.fr.begin()+4);translate();
        cpu.r[5]=angles[0];cpu.r[6]=angles[1];cpu.r[7]=angles[2];
        if(rotation==0x8C639C34u)rotate(angles);else rotate_yxz(angles);
        std::copy(scales.begin(),scales.end(),cpu.fr.begin()+4);scale();
        cpu.r[0]=angles[1];cpu.r[1]=angles[0];cpu.r[2]=rotation;cpu.r[3]=0x8C63A5DCu;
        cpu.r[4]=0;cpu.t=true;cpu.pc=saved_pr;
    }
};
}
std::span<const SourceSpan> source_spans() noexcept{return identities;}
const Statistics& statistics() noexcept{return counters;}
bool try_execute(katana::runtime::CpuState& cpu,const katana::runtime::NativePortImmutableWriteGuard* immutable){
    const auto fpscr=cpu.read_fpscr();auto& memory=cpu.memory;
    if(cpu.pc!=entry || !immutable || immutable->write_detected() || !cpu.privileged_mode_inline() ||
       cpu.trap_pending || cpu.sleeping || (cpu.sr&sr_fd_mask) ||
       (fpscr&(fpscr_pr_mask|fpscr_sz_mask|fpscr_exception_enable_mask)) ||
       !(fpscr&fpscr_dn_mask) || (fpscr&fpscr_rounding_mode_mask)>1u ||
       memory.watchpoint_count() || memory.has_trace_handler() || memory.has_guest_memory_access_sink() ||
       memory.has_mmio_trace_handler() || !memory.guest_write_observer_allows_prevalidated_linear_writes())return false;
    const auto guard=memory.direct_linear_memory_guard(false);
    if(!guard || guard.physical_base!=0x0C000000u || guard.physical_span<0x1000000u || guard.backing_mask!=0xFFFFFFu)return false;
    const auto stack=cpu.r[15]-48u,physical=stack&0x1FFFFFFFu;
    const bool p0=!(cpu.mmucr&1u) && (!cpu.address_space || cpu.address_space->mode()==AddressTranslationMode::NoMmu);
    if((cpu.r[15]&3u) || ((stack&0xC0000000u)!=0x80000000u && !(p0 && stack==physical)) ||
       physical<0x0C000000u || physical>0x0CFFFFD0u || overlap(stack,48u,inputs,136u) ||
       overlap(stack,48u,rotation_slot,4u) || immutable->tracks_address(physical,48u) ||
       !memory.is_writable_linear_range(physical,48u,false))return false;
    for(const auto span:identities){
        if(overlap(stack,48u,span.address,std::uint32_t(span.bytes.size())) ||
           std::memcmp(guard.read_bytes+(span.address&0xFFFFFFu),span.bytes.data(),span.bytes.size()))return false;
    }
    Execution execution{{cpu},guard};const auto rotation=execution.read(rotation_slot);
    if(rotation!=0x8C639C34u && rotation!=0x8C639F38u)return false;
    execution.writes.emplace(cpu,*immutable,guard);
    execution.run(rotation);
    if(execution.writes->direct())++counters.direct_write_calls;
    return true;
}
bool try_dispatch(CpuState& cpu,const NativePortImmutableWriteGuard* immutable){
    static const bool enabled=[] {const auto* flag=std::getenv("SARECOMP_NATIVE_POSE_BLEND");return flag && flag[0]=='1';}();
    if(!enabled || cpu.pc!=entry)return false;
    if(try_execute(cpu,immutable)){++counters.native_calls;return true;}
    ++counters.original_calls;return false;
}
}
