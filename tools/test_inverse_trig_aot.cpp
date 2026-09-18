// Compare the parent extension against the actual retained AOT FPU scopes.
#define main atan_component_main
#include "test_atan_math.cpp"
#undef main
#include "katana/runtime/native_port.hpp"
namespace sonic::atan_math {void run_retained_reference(katana::runtime::CpuState&);bool reference_entry(std::uint32_t) noexcept;}
namespace {
bool original_entry(std::uint32_t pc) noexcept {
    return sonic::atan_math::reference_entry(pc);
}
struct Host final:NativePortHostServices {
    std::uint64_t monotonic_time_nanoseconds()const noexcept override{return 1;}
    NativePortLifecycleState poll_lifecycle()override{return {};}
    void synchronize_simulation_boundary()override{}
    void begin_frame(std::uint64_t)override{}
    void present_frame(std::uint64_t)override{}
    std::uint64_t presented_frames()const noexcept override{return 0;}
} host;
void original_call(CpuState& cpu,std::uint32_t target) {
    cpu.pc=target;sonic::atan_math::run_retained_reference(cpu);
}
}
namespace katana_port_generated {
thread_local bool native_bringup_dispatch_pending=false;
namespace runtime_dispatch_detail {
thread_local NativePortAotServices* active_services=nullptr;
thread_local BlockAddress active_exit_source;
thread_local BlockEndKind active_exit_kind=BlockEndKind::Fallthrough;
thread_local DynamicDispatchSiteClass active_exit_site_class=DynamicDispatchSiteClass::NotDynamic;
thread_local bool tail_dispatch_completed=false;
bool try_static_return_nop_callback(CpuState&,std::uint32_t)noexcept{return false;}
}
void preflight_native_bringup_indirect_dispatch(std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,bool){}
void consume_native_bringup_direct_aot_dispatch(std::uint32_t){}
#define BOUNDARY(name) void name(CpuState& cpu,std::uint32_t target){original_call(cpu,target);}
BOUNDARY(static_call) BOUNDARY(resolved_call) BOUNDARY(guarded_call) BOUNDARY(guarded_jump)
BOUNDARY(runtime_only_call) BOUNDARY(runtime_only_jump) BOUNDARY(unresolved_call) BOUNDARY(unresolved_jump)
#undef BOUNDARY
void exact_guarded_call(CpuState& cpu,std::uint32_t target,std::uint32_t){original_call(cpu,target);}
void exact_guarded_jump(CpuState& cpu,std::uint32_t target,std::uint32_t){original_call(cpu,target);}
}
int main(int argc,char** argv)try {
    require(argc==2,"inverse-trig-aot-tests <installed-content-root>");RestoreHost restore;
    const auto boot=read(std::filesystem::path(argv[1])/"boot.bin");
    require(digest(boot)=="b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af","boot identity");
    retained_reference=sonic::atan_math::run_retained_reference;unsigned cases=0;
    const auto check=[&](std::uint32_t entry,std::uint32_t x,unsigned mode,bool closed=false) {
        _mm_setcsr(0x1F80u|((cases&3u)<<13u)|(cases&0x3Fu)|((cases&4u)?0x8040u:0u));
        Fixture native(boot,entry,x,0xBF800000u,mode),reference(boot,entry,x,0xBF800000u,mode);
        NativePortContext context;context.cpu=&reference.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,reference.immutable);
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        compare(native,reference,closed);++cases;
    };
    for(const auto& parent:inverse_source_spans)for(auto mode:{0u,1u})
      for(auto x:{0x3E800000u,0xBE800000u,0x3EFFFFFFu,0x3F400000u,0xBF7FFFFFu,0x7FC00001u})
        check(parent.address,x,mode);
    for(auto entry:{atan_entry,quotient_entry,polynomial_entry,scale_entry})for(auto mode:{0u,1u})
        check(entry,0x3EBFFFFFu,mode);
    if(sonic::atan_math::closed_memory_enabled()) {
        for(const auto& parent:inverse_source_spans)for(auto mode:{0u,1u})
            for(auto x:{0x3EBFFFFFu,0xBF7FFFFFu,0x7FC00001u})check(parent.address,x,mode,true);
        for(auto entry:{atan_entry,quotient_entry,polynomial_entry,scale_entry})for(auto mode:{0u,1u})
            for(auto x:{0x3EBFFFFFu,0xBF7FFFFFu,0x7FC00001u})check(entry,x,mode,true);
    }
    std::cout<<"SONIC_INVERSE_TRIG_AOT_PASS cases="<<cases<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_INVERSE_TRIG_AOT_FAIL "<<e.what()<<'\n';return 1;}
