// Actual retained continuation scopes, without inventing global entries.
#define main contact_component_main
#include "test_movement_contact.cpp"
#undef main
#include "katana/runtime/native_port_aot_runtime.hpp"
#include "katana/runtime/native_port.hpp"
#include <filesystem>
#include <map>
#include <regex>
namespace katana_port_generated::runtime_dispatch_detail {extern thread_local katana::runtime::BlockEndKind active_exit_kind;}
namespace {
Fixture* active_fixture{};
std::set<std::uint32_t> original_entries;
std::map<std::uint32_t,std::uint32_t> original_owners;
bool original_entry(std::uint32_t pc) noexcept{return original_entries.contains(pc);}
void load_original_entries(const std::filesystem::path& root){
    const std::regex row(R"(\{0x([A-F0-9]{8})u, &fn_([A-F0-9]{8})_runtime_entry, true, (?:true|false)\})");
    for(const auto* name:{"native-port-dispatch-shard-202756.cpp","native-port-dispatch-shard-202757.cpp","native-port-dispatch-shard-202762.cpp","native-port-dispatch-shard-202766.cpp","native-port-dispatch-shard-202767.cpp","native-port-dispatch-shard-202951.cpp","native-port-dispatch-shard-202952.cpp"}){
        std::ifstream f(root/name);const std::string text{std::istreambuf_iterator<char>(f),{}};
        require(!text.empty(),"original entry shard missing");
        for(auto i=std::sregex_iterator(text.begin(),text.end(),row);i!=std::sregex_iterator();++i)
            if(family::contains(std::stoul((*i)[2].str(),nullptr,16))){
                const auto pc=std::stoul((*i)[1].str(),nullptr,16);
                original_entries.insert(pc);original_owners.emplace(pc,std::stoul((*i)[2].str(),nullptr,16));
            }
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
    if(family::contains(target)){require(family::resume_original(c,target),"private child");return;}
    require(Fixture::invoke(active_fixture,c,target),"AOT foreign child");
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
#include "contact-test-externals.inc"
}

int main(int argc,char** argv)try{
    require(argc==3,"movement-contact-aot-tests <RAM> <retained-code>");
    std::ifstream file(argv[1],std::ios::binary);const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000,"RAM size");load_original_entries(argv[2]);unsigned cases=0;
    for(unsigned kind=0;kind<12;++kind){
        Fixture a(image,0),b(image,0);a.oracle=&b;active_fixture=&a;
        const auto scenario=kind<3?30u:kind<6?24u:13u;
        setup(a,scenario);setup(b,scenario);
        for(auto* f:{&a,&b}){
            if(kind==0 || kind==3)f->mutation=1;
            if(kind==1 || kind==4)f->mutation=2;
            if(kind==2 || kind==5)f->mutation=3;
            if(kind==6)f->cpu.r[4]=Q+1;
            if(kind==7)f->cpu.r[15]+=2;
            if(kind==8)f->put(indices,0x8CFFFFE0u);
            if(kind==9)f->put(object+28,indices+2);
            if(kind==10){f->cpu.pc=0x8C63A820u;f->cpu.r[4]=0x8CFFFFF0u;}
            if(kind==11){f->cpu.pc=0x8C638E0Cu;f->cpu.r[4]=0x8CFFFFF8u;f->cpu.r[5]=Q;}
        }
        sonic::scalar_writes::unbind(&a.cpu.memory,&a.immutable);a.cpu.memory.set_guest_write_observer({});a.cpu.memory.set_guest_write_batch_observer({});
        NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,a.immutable);
        sonic::scalar_writes::bind(a.cpu.memory,a.immutable,a.cpu.memory.guest_write_observer_generation());
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        const auto outcome=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,resume});
        b.until(a.cpu.pc);compare(a,b,"actual original continuation");
        std::cout<<"frontier kind="<<kind<<" outcome="<<int(outcome)<<" pc="<<std::hex<<a.cpu.pc<<" pr="<<a.cpu.pr<<" trap="<<a.cpu.trap_pending<<" source="<<family::return_site<<std::dec<<std::endl;
        if(kind==1){
            // Changing SZ inside this original frame changes saved-register
            // widths. Retained AOT yields at its invalid original return;
            // the instruction oracle already matched that exact frontier.
            require(outcome==family::Outcome::Interrupted && a.cpu.pc==0 && a.cpu.pr==0,"original invalid SZ return");
        }
        else if(a.cpu.trap_pending)require(outcome==family::Outcome::Interrupted,"fault outcome");
        else if(a.cpu.pc==returned)require(outcome==family::Outcome::Complete,"return outcome");
        else require(outcome==family::Outcome::Interrupted && original_entry(a.cpu.pc),"original sparse frontier");
        std::cout<<"aot case="<<kind<<" outcome="<<int(outcome)<<" pc="<<std::hex<<a.cpu.pc<<std::dec<<std::endl;++cases;
    }
    std::cout<<"SONIC_MOVEMENT_CONTACT_AOT_PASS cases="<<cases<<" entries="<<original_entries.size()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_MOVEMENT_CONTACT_AOT_FAIL "<<e.what()<<'\n';return 1;}
