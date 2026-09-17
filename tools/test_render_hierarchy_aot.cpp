// Actual retained continuation scopes, without inventing global entries.
#define main hierarchy_component_main
#include "test_render_hierarchy.cpp"
#undef main
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "katana/runtime/native_port.hpp"
#include <filesystem>
#include <regex>
namespace katana_port_generated::runtime_dispatch_detail {extern thread_local katana::runtime::BlockEndKind active_exit_kind;}
namespace {
Fixture* active_fixture{};
std::set<std::uint32_t> original_entries;
bool original_entry(std::uint32_t pc) noexcept{return original_entries.contains(pc);}
void load_original_entries(const std::filesystem::path& root){
    const std::regex row(R"(\{0x([A-F0-9]{8})u, &fn_([A-F0-9]{8})_runtime_entry, true, (?:true|false)\})");
    for(const auto* name:{"native-port-dispatch-shard-101379.cpp","native-port-dispatch-shard-202760.cpp","native-port-dispatch-shard-202951.cpp"}){
        std::ifstream f(root/name);const std::string text{std::istreambuf_iterator<char>(f),{}};
        require(!text.empty(),"original entry shard missing");
        for(auto i=std::sregex_iterator(text.begin(),text.end(),row);i!=std::sregex_iterator();++i)
            if(family::contains(std::stoul((*i)[2].str(),nullptr,16)))original_entries.insert(std::stoul((*i)[1].str(),nullptr,16));
    }
    require(original_entries.contains(family::entry),"original entry table missing");
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
    c.pc=target;
    if(Fixture::external(target)){require(Fixture::invoke(active_fixture,c,target),"AOT child failed");return;}
    const auto end=c.pr;
    require(Fixture::resume(active_fixture,c,target,end),"AOT child reference failed");
}
bool resume(void*,CpuState& c,std::uint32_t owner,std::uint32_t continuation){
    struct Depth{Depth(){++family::resume_depth;}~Depth(){--family::resume_depth;}} depth;
    const bool handled=family::resume_original(c,owner);
    if(handled && !c.trap_pending && c.pc!=continuation && c.pr==continuation &&
       katana_port_generated::runtime_dispatch_detail::active_exit_kind==BlockEndKind::DynamicBranch)
        external(c,c.pc);
    return handled && !c.trap_pending;
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
BlockExit fn_8C037098_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C037098u);return {};}
BlockExit fn_8C03700C_runtime_entry(CpuState& c,BlockExecutionContext&){external(c,0x8C03700Cu);return {};}
}
int main(int argc,char** argv)try{
    require(argc==3,"render-hierarchy-aot-tests <original-ram> <original-code-root>");
    std::ifstream file(argv[1],std::ios::binary);const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000u,"RAM size");load_original_entries(argv[2]);unsigned cases=0;
    for(unsigned kind=0;kind<19;++kind){
        Fixture a(image,0),b(image,0);a.oracle=&b;active_fixture=&a;
        if(kind<10){
            setup(a,kind==0?1:kind==1?23:kind==2?28:kind==3?29:16);
            setup(b,kind==0?1:kind==1?23:kind==2?28:kind==3?29:16);
        }else{
            const auto variant=kind==10?16u:kind==11?23u:kind==12?30u:kind==13?31u:kind==14?28u:kind==15?29u:kind==16?16u:35u;
            setup_blended(a,variant);setup_blended(b,variant);
            if(kind==16)for(auto* f:{&a,&b})f->put(records,keys+1u);
            if(kind==17)for(auto* f:{&a,&b})f->put(0x8C88FD98u,0x8CE50002u);
            if(kind==18)for(auto* f:{&a,&b}){
                // Incoming PR aliases a normal call continuation, not a return.
                f->cpu.pc=0x8C041516u;f->cpu.pr=0x8C04152Cu;f->mutation=6;
                f->put(0x8C88FE78u,0);f->put(0x8C88FDD8u,callback);
                f->put(0x8C88FDE0u,alternate);f->put(0x8C88FDE8u,alternate);
            }
        }
        if(kind>=4 && kind<8){for(auto* f:{&a,&b}){f->cpu.pc=kind<6?0x8C63A7B8u:0x8C639C34u;f->cpu.r[4]=kind&1u?0x8CFFFFE0u:0x8CE30002u;f->cpu.r[5]=0x1111;f->cpu.r[6]=0x2222;f->cpu.r[7]=0x3333;}}
        if(kind>=8 && kind<10)for(auto* f:{&a,&b}){
            // The final SRT operation tail-calls a mutable foreign callback.
            f->put(0x8C88FD64u,0x8C0405B2u);
            f->put(0x8C88FD6Cu,0x8C04057Au);f->put(0x8C88FD70u,0x8C040588u);
            f->put(0x8C88FD74u,callback);f->mutation=kind==8?6:8;
        }
        sonic::scalar_writes::unbind(&a.cpu.memory,&a.immutable);a.cpu.memory.set_guest_write_observer({});a.cpu.memory.set_guest_write_batch_observer({});
        NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,a.immutable);
        sonic::scalar_writes::bind(a.cpu.memory,a.immutable,a.cpu.memory.guest_write_observer_generation());
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        family::return_site=0;
        const auto result=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,resume});
        while(b.cpu.pc!=a.cpu.pc && !b.cpu.trap_pending){advance(b);if(Fixture::external(b.cpu.pc))require(b.child(b.cpu.pc),"original child");}
        compare(a,b,"actual AOT continuation");require(a.trace==b.trace,"duplicate AOT callback");
        if(kind<2 || (kind>=8 && kind<14))require(result==family::Outcome::Complete && family::return_site==(kind<10?0x8C04082Cu:0x8C041B0Au),"root AOT return");
        else if(kind==18)require(result==family::Outcome::Complete && a.trace.size()==3 && a.cpu.pc==0x8C04152Cu,"ordinary call mistaken for completed owner");
        else require(a.cpu.trap_pending && result==family::Outcome::Interrupted,"expected actual AOT fault");
        std::cout<<"hierarchy-aot case="<<kind<<" outcome="<<int(result)<<'\n';++cases;
    }
    std::cout<<"SONIC_RENDER_HIERARCHY_AOT_PASS cases="<<cases<<" original_entries="<<original_entries.size()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_RENDER_HIERARCHY_AOT_FAIL "<<e.what()<<'\n';return 1;}
