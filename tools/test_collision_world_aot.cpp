// Exercise the real retained AOT scopes after a partial native owner, with
// inner PCs deliberately absent from the global static-entry query.
#define main world_component_main
#include "test_collision_world.cpp"
#undef main
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "katana/runtime/native_port.hpp"
#include <filesystem>
#include <regex>
namespace {
Fixture* active_fixture{};
family::Outcome last_outcome{};
std::set<std::uint32_t> original_entries;
bool original_entry(std::uint32_t pc) noexcept {return original_entries.contains(pc);}
void load_original_entries(const std::filesystem::path& root){
    const std::regex row(R"(\{0x([A-F0-9]{8})u, &fn_([A-F0-9]{8})_runtime_entry, true, (?:true|false)\})");
    for(const auto* name:{"native-port-dispatch-shard-202757.cpp","native-port-dispatch-shard-202762.cpp"}){
        std::ifstream f(root/name);const std::string text{std::istreambuf_iterator<char>(f),{}};
        require(!text.empty(),"original dispatcher shard missing");
        for(auto i=std::sregex_iterator(text.begin(),text.end(),row);i!=std::sregex_iterator();++i)
            if(family::contains(std::stoul((*i)[2].str(),nullptr,16)))original_entries.insert(std::stoul((*i)[1].str(),nullptr,16));
    }
    require(original_entries.size()==735,"original static entry identity");
}
struct Host final:NativePortHostServices {
    std::uint64_t monotonic_time_nanoseconds()const noexcept override{return 1;}
    NativePortLifecycleState poll_lifecycle()override{return {};}
    void synchronize_simulation_boundary()override{}
    void begin_frame(std::uint64_t)override{}
    void present_frame(std::uint64_t)override{}
    std::uint64_t presented_frames()const noexcept override{return 0;}
} host;
void external(CpuState& c,std::uint32_t target){
    c.pc=target;require(Fixture::invoke(active_fixture,c,target),"AOT child failed");
}
}

namespace sonic::collision_world {
bool resume(void*,CpuState& c,std::uint32_t owner){
    struct Depth{Depth(){++resume_depth;}~Depth(){--resume_depth;}} depth;
    return (resume_geometry(c,owner) || resume_pools(c,owner) || resume_eligibility(c,owner)) && !c.trap_pending;
}
Outcome try_dispatch(CpuState& c,NativePortAotServices& s){
    last_outcome=execute(c,s.immutable_write_guard(),{active_fixture,Fixture::invoke,resume});
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
BlockExit fn_8C028EC2_runtime_entry(CpuState&,BlockExecutionContext&);
BlockExit fn_8C052E30_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C052E30u);return {};}
BlockExit fn_8C10C99C_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C10C99Cu);return {};}
BlockExit fn_8C638E0C_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C638E0Cu);return {};}
BlockExit fn_8C639AD8_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C639AD8u);return {};}
BlockExit fn_8C639BB0_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C639BB0u);return {};}
BlockExit fn_8C639E08_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C639E08u);return {};}
BlockExit fn_8C639E9C_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C639E9Cu);return {};}
BlockExit fn_8C63A10C_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C63A10Cu);return {};}
BlockExit fn_8C63A52C_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C63A52Cu);return {};}
BlockExit fn_8C63A744_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C63A744u);return {};}
BlockExit fn_8C63A820_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C63A820u);return {};}
BlockExit fn_8C63A904_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C63A904u);return {};}
BlockExit fn_8C6406A6_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C6406A6u);return {};}
}

int main(int argc,char** argv)try{
    require(argc==3,"world-aot-tests <original-ram> <original-code-root>");std::ifstream file(argv[1],std::ios::binary);
    load_original_entries(argv[2]);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};require(image.size()==0x1000000u,"RAM size");
    require(family::enabled(),"enable collision-world path");unsigned cases=0;
    for(unsigned kind=0;kind<8;++kind){
        Fixture a(image,0),b(image,0);setup(a,kind<4?kind:4);setup(b,kind<4?kind:4);
        a.oracle=&b;active_fixture=&a;
        if(kind==4)a.mutation=b.mutation=1; // arbitrary observer installed at real callback
        if(kind==5){a.put(njs+4,models+1);b.put(njs+4,models+1);} // nested polygon access faults
        if(kind==6){a.setup_pools(0,1);b.setup_pools(0,1);} // partial geometry production
        if(kind==7){a.cpu.r[15]+=2;b.cpu.r[15]+=2;} // first native store hands off unchanged
        sonic::scalar_writes::unbind(&a.cpu.memory,&a.immutable);
        a.cpu.memory.set_guest_write_observer({});a.cpu.memory.set_guest_write_batch_observer({});
        NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,a.immutable);
        sonic::scalar_writes::bind(a.cpu.memory,a.immutable,a.cpu.memory.guest_write_observer_generation());
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        require(!aot.can_chain_executable_block(0x8C0287C8u),"private continuation accidentally public");
        family::return_site=0;
        BlockExecutionContext block;katana_port_generated::fn_8C028EC2_runtime_entry(a.cpu,block);
        while(b.cpu.pc!=a.cpu.pc){
            require(++b.steps<3000000u,"fallback comparison bound");
            if(Fixture::external(b.cpu.pc))require(b.child(b.cpu.pc),"reference callback");
            else (void)execute_dynamic_sh4_block(b.cpu,services,1u);
        }
        compare(a,b,"AOT final");require(a.trace==b.trace,"duplicate external callback");
        std::cerr<<"world-aot frontier="<<kind<<" outcome="<<int(last_outcome)<<" pc="<<std::hex<<a.cpu.pc<<std::dec<<'\n';
        if(kind==5 || kind==7)require(a.cpu.trap_pending,"expected original exception");
        else {
            require(a.cpu.pc==returned,"world did not return");
            require(family::return_site==0x8C029398u,"stale original return boundary");
        }
        std::cout<<"world-aot case="<<kind<<" outcome="<<int(last_outcome)<<" pc="<<std::hex<<a.cpu.pc<<std::dec<<'\n';++cases;
    }
    std::cout<<"SONIC_COLLISION_WORLD_AOT_OK cases="<<cases<<" exact_cpu_ram=1 sparse_entries=preserved\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
