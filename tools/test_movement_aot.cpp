// Exercise the real retained AOT scopes after a partial native owner, with
// inner PCs deliberately absent from the global static-entry query.
#define main movement_component_main
#include "test_movement_resolver.cpp"
#undef main
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "katana/runtime/native_port.hpp"
namespace {
Fixture* active_fixture{};
family::Outcome last_outcome{};
struct Host final:NativePortHostServices {
    std::uint64_t monotonic_time_nanoseconds()const noexcept override{return 1;}
    NativePortLifecycleState poll_lifecycle()override{return {};}
    void synchronize_simulation_boundary()override{}
    void begin_frame(std::uint64_t)override{}
    void present_frame(std::uint64_t)override{}
    std::uint64_t presented_frames()const noexcept override{return 0;}
} host;
void external(CpuState& c,std::uint32_t target){
    c.pc=target;require(Fixture::child(active_fixture,c,target),"AOT child failed");
}
}
namespace sonic::movement {
Outcome try_dispatch(CpuState& c,NativePortAotServices& s){
    last_outcome=execute(c,s.immutable_write_guard(),{active_fixture,Fixture::child});
    if(last_outcome==Outcome::ResumeOriginal &&
       (c.trap_pending || !s.can_chain_executable_block(entry) || !retained_source_matches(c,s.immutable_write_guard())))
        last_outcome=Outcome::Interrupted;
    return last_outcome;
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
#define BOUNDARY(name) void name(CpuState& c,std::uint32_t t){external(c,t);}
BOUNDARY(static_call) BOUNDARY(resolved_call) BOUNDARY(guarded_call) BOUNDARY(guarded_jump)
BOUNDARY(runtime_only_call) BOUNDARY(runtime_only_jump) BOUNDARY(unresolved_call) BOUNDARY(unresolved_jump)
#undef BOUNDARY
void exact_guarded_call(CpuState& c,std::uint32_t t,std::uint32_t){external(c,t);}
void exact_guarded_jump(CpuState& c,std::uint32_t t,std::uint32_t){external(c,t);}
BlockExit fn_8C073018_runtime_entry(CpuState&,BlockExecutionContext&);
BlockExit fn_8C028BFE_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C028BFEu);return {};}
BlockExit fn_8C029B00_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C029B00u);return {};}
BlockExit fn_8C049A4E_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C049A4Eu);return {};}
BlockExit fn_8C04F7E0_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C04F7E0u);return {};}
BlockExit fn_8C055C9A_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C055C9Au);return {};}
BlockExit fn_8C055CEC_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C055CECu);return {};}
BlockExit fn_8C06CC62_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C06CC62u);return {};}
BlockExit fn_8C074214_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C074214u);return {};}
BlockExit fn_8C074712_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C074712u);return {};}
BlockExit fn_8C074C30_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C074C30u);return {};}
BlockExit fn_8C074E24_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C074E24u);return {};}
BlockExit fn_8C074FAE_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C074FAEu);return {};}
BlockExit fn_8C075100_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C075100u);return {};}
BlockExit fn_8C075154_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C075154u);return {};}
BlockExit fn_8C075356_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C075356u);return {};}
BlockExit fn_8C0757A0_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C0757A0u);return {};}
BlockExit fn_8C075980_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C075980u);return {};}
BlockExit fn_8C078DF0_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C078DF0u);return {};}
BlockExit fn_8C078F40_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C078F40u);return {};}
BlockExit fn_8C10CF98_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C10CF98u);return {};}
BlockExit fn_8C10D038_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C10D038u);return {};}
BlockExit fn_8C638FD0_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C638FD0u);return {};}
BlockExit fn_8C63A69C_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C63A69Cu);return {};}
BlockExit fn_8C63A88C_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C63A88Cu);return {};}
BlockExit fn_8C63FFC0_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C63FFC0u);return {};}
BlockExit fn_8C640068_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C640068u);return {};}
}
int main(int argc,char** argv)try{
    require(argc==2,"usage: sonic-movement-aot-tests RAM");
    std::ifstream file(argv[1],std::ios::binary);const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000u,"RAM size");require(family::enabled(),"enable internal movement path");
    unsigned cases=0;
    for(unsigned kind=0;kind<10;++kind){
        Fixture a(image,0),b(image,0);a.oracle=&b;active_fixture=&a;
        if(kind<5){const unsigned variants[]{0,4,10,16,37};setup(a,variants[kind]);setup(b,variants[kind]);}
        if(kind==5){a.invalidate_after=b.invalidate_after=4;a.invalidation=b.invalidation=1;}
        if(kind==6){a.cpu.r[4]=b.cpu.r[4]=family::entry;a.cpu.pc=b.cpu.pc=family::body;}
        if(kind>6){a.unsafe=b.unsafe=kind-6;if(kind==9)a.flags=b.flags=0;}
        // The real AOT services install the production immutable observers.
        sonic::scalar_writes::unbind(&a.cpu.memory,&a.immutable);
        a.cpu.memory.set_guest_write_observer({});a.cpu.memory.set_guest_write_batch_observer({});
        NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,+[](std::uint32_t pc)noexcept{return pc==family::entry;},a.immutable);
        sonic::scalar_writes::bind(a.cpu.memory,a.immutable,a.cpu.memory.guest_write_observer_generation());
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        require(!aot.can_chain_executable_block(0x8C073A40u) && !aot.can_chain_executable_block(0x8C0730E2u),"fixture accidentally enables inner dispatcher");
        BlockExecutionContext block;katana_port_generated::fn_8C073018_runtime_entry(a.cpu,block);
        std::cerr<<"handoff-kind="<<kind<<" outcome="<<int(last_outcome)<<" calls="<<a.trace.size()<<" pc="<<std::hex<<a.cpu.pc<<std::dec<<'\n';
        if(kind<5)require(last_outcome==family::Outcome::Complete && a.cpu.pc==returned,"complete AOT owner");
        else if(kind==5)require(last_outcome==family::Outcome::Interrupted || last_outcome==family::Outcome::ResumeOriginal,"changed observer was ignored");
        else require(last_outcome==family::Outcome::ResumeOriginal,"native to retained transition missing");
        // Follow the original bytes to the retained owner's normal return,
        // memory exception or immutable-write exit, whichever occurred first.
        while(b.cpu.pc!=a.cpu.pc){
            require(++b.steps<200000u,"AOT comparison bound");
            if(Fixture::external(b.cpu.pc))require(b.stub(b.cpu.pc),"reference callback failed");
            else (void)execute_dynamic_sh4_block(b.cpu,services,1u);
        }
        compare(a,b,"AOT handoff final");
        require(a.trace==b.trace,"AOT handoff repeated callbacks");
        if(kind==9)require(a.cpu.exception_generation!=0,"delay-slot fault not executed");
        std::cout<<"aot-case="<<kind<<" outcome="<<int(last_outcome)<<" pc="<<std::hex<<a.cpu.pc<<std::dec<<'\n';
        ++cases;
    }
    std::cout<<"SONIC_MOVEMENT_AOT_OK cases="<<cases<<" sparse_global_entries=preserved cpu=exact ram=exact\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_MOVEMENT_AOT_FAILED "<<e.what()<<'\n';return 1;}
