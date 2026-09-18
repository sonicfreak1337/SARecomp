// Compare the connected PAL state machine against original instructions and
// retained AOT. Only genuine calls outside this native family are fixtures.
#define SARECOMP_HIERARCHY_AOT_TEST_ENTRY retained_hierarchy_tests_unused
#include "test_render_hierarchy_aot.cpp"
namespace {
constexpr std::uint32_t actor_task=0x8CE50000u,actor_work=0x8CE51000u,
    actor_extra=0x8CE52000u,actor_contact=0x8CE53000u,actor_action=0x8CE54000u,
    actor_animation=0x8CE54100u,actor_alloc=0x8CE55000u;
struct ActorFixture:Fixture {
    ActorFixture* reference{};
    unsigned variant{};
    bool replace_observer{};
    using Fixture::Fixture;
    static bool boundary(std::uint32_t pc) {
        if(family::contains(pc))return false;
        switch(pc) {
        case 0x8C015720u:case 0x8C02F63Cu:case 0x8C036F12u:
        case 0x8C03700Cu:case 0x8C037098u:case 0x8C037108u:case 0x8C038D00u:
        case 0x8C041C92u:case 0x8C042DC4u:case 0x8C043AEEu:case 0x8C045736u:
        case 0x8C047126u:case 0x8C047D90u:case 0x8C047FCEu:case 0x8C047FFCu:
        case 0x8C0486A0u:case 0x8C0487B6u:case 0x8C0488A2u:case 0x8C0489E4u:
        case 0x8C055C60u:case 0x8C055C9Au:case 0x8C0582E0u:
        case 0x8C091928u:case 0x8C091AE4u:case 0x8C09216Cu:
        case 0x8C09401Au:case 0x8C09407Au:case 0x8C098BE0u:case 0x8C098DD8u:
        case 0x8C0C9B22u:case 0x8C0C9BDAu:case 0x8C0FACA0u:case 0x8C0FBCAAu:
        case 0x8C10D038u:case 0x8C10D274u:case 0x8C605CECu:case 0x8C605D4Au:
        case 0x8C608C0Cu:case 0x8C60ED30u:case 0x8C63A724u:case 0x8C63A744u:
        case 0x8C63A8F8u:case 0x8C640862u:return true;
        default:return false;
        }
    }
    void advance_to_boundary(std::uint32_t end=returned) {
        while(cpu.pc!=end && !boundary(cpu.pc) && !cpu.trap_pending) {
            require(++steps<200000u,"actor reference bound");reference_step(cpu);
        }
    }
    bool child(std::uint32_t target) {
        trace.emplace_back(target,cpu.pr);++callback_index;
        cpu.r[0]=0;
        if(target==0x8C038D00u)cpu.r[0]=variant==2?0:1;
        if(target==0x8C0489E4u)cpu.r[0]=variant==1;
        if(target==0x8C091928u)cpu.r[0]=variant==3;
        if(target==0x8C098BE0u)cpu.r[0]=actor_alloc;
        if(target==0x8C10D274u)cpu.r[0]=variant==1?0x4321u:0x1234u;
        if(replace_observer && callback_index==1)
            cpu.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{});
        cpu.t=!cpu.t;cpu.pc=cpu.pr;return true;
    }
    static bool invoke(void* p,CpuState& c,std::uint32_t target) {
        auto& f=*static_cast<ActorFixture*>(p);
        if(&c!=&f.cpu || !boundary(target)) {
            std::cerr<<"unexpected actor callback "<<std::hex<<target<<std::dec<<'\n';
            throw std::runtime_error("actor boundary");
        }
        if(f.reference){f.reference->advance_to_boundary();compare(f,*f.reference,"actor callback entry");}
        const auto ok=f.child(target);
        if(f.reference){require(f.reference->child(target)==ok,"actor callback result");compare(f,*f.reference,"actor callback return");}
        return ok;
    }
    static bool resume_instructions(void* p,CpuState& c,std::uint32_t,std::uint32_t end) {
        auto& f=*static_cast<ActorFixture*>(p);
        while(c.pc!=end && !c.trap_pending) {
            require(++f.steps<200000u,"actor fallback bound");
            if(boundary(c.pc)){if(!invoke(p,c,c.pc))return false;}
            else reference_step(c);
        }
        return !c.trap_pending;
    }
};
ActorFixture* actor_active{};
void actor_external(CpuState& c,std::uint32_t target) {
    c.pc=target;
    if(ActorFixture::boundary(target))require(ActorFixture::invoke(actor_active,c,target),"actor AOT boundary");
    else require(ActorFixture::resume_instructions(actor_active,c,target,c.pr),"actor AOT child");
}
void setup_actor(ActorFixture& f,unsigned state,unsigned variant) {
    f.variant=variant;f.cpu.pc=family::actor_entry;f.cpu.r[4]=actor_task;
    f.cpu.gbr=0x8C8FFE00u;
    f.put(actor_task+32,actor_work);f.put(actor_task+36,actor_extra);
    f.put(actor_work,state);f.put(actor_work+56,actor_contact);
    f.vector(actor_work+32,4.f,5.f,6.f);f.vector(actor_work+44,1.f,1.5f,2.f);
    f.put(actor_work+12,variant==1?601u:1u);
    f.put(actor_work+4,variant==1?0x00030200u:0u);
    f.put(actor_contact,variant==1?0x00020000u:0u);
    f.put(actor_extra+0x1F4u,actor_action);
    f.put(actor_extra+0x1F8u,actor_action);
    f.put(actor_action,nodes);f.put(actor_action+4,actor_animation);
    f.put(actor_animation,records);f.put(actor_animation+4,20u);f.put(actor_animation+8,0u);
    f.put(actor_alloc+32,actor_alloc+256);f.put(actor_alloc+36,actor_alloc+512);
    f.put(0x8C78C548u,actor_work);f.put(0x8C19B3A8u,0u);
    for(unsigned i=0;i<16;++i)f.put(0x8C67C580u+i*4,i%5==0?0x3F800000u:0u);
}
void finish_reference(ActorFixture& a,ActorFixture& b) {
    for(unsigned n=0;b.cpu.pc!=a.cpu.pc && !b.cpu.trap_pending;++n) {
        require(n<512u,"actor endpoint not reached");b.advance_to_boundary(a.cpu.pc);
        if(ActorFixture::boundary(b.cpu.pc) && b.cpu.pc!=a.cpu.pc)b.child(b.cpu.pc);
    }
    compare(a,b,"actor final");require(a.trace==b.trace,"actor boundary sequence");
}
}
int main(int argc,char** argv)try {
    std::cout<<std::unitbuf;
    require(argc==3,"actor-operation-tests <original-ram> <original-code-root>");
#ifdef _WIN32
    _putenv_s("SARECOMP_NATIVE_ACTOR_OPERATION","1");
    _putenv_s("SARECOMP_NATIVE_LAND_RENDER","1");
#else
    setenv("SARECOMP_NATIVE_ACTOR_OPERATION","1",1);
    setenv("SARECOMP_NATIVE_LAND_RENDER","1",1);
#endif
    std::ifstream file(argv[1],std::ios::binary);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000u,"actor RAM size");load_original_entries(argv[2]);unsigned cases=0;
    for(unsigned mode:{0u,1u})for(unsigned variant=0;variant<2;++variant)for(unsigned state=0;state<15;++state) {
        const auto input_state=state==14?255u:state;
        std::cout<<"actor state="<<input_state<<" variant="<<variant<<" mode="<<mode<<'\n';
        ActorFixture a(image,mode),b(image,mode);setup_actor(a,input_state,variant);setup_actor(b,input_state,variant);a.reference=&b;
        const auto outcome=family::execute(a.cpu,&a.immutable,{&a,ActorFixture::invoke,ActorFixture::resume_instructions});
        finish_reference(a,b);
        if(a.cpu.pc!=returned || a.cpu.trap_pending)
            std::cerr<<"actor pc="<<std::hex<<a.cpu.pc<<" spc="<<a.cpu.spc<<" tea="<<a.cpu.tea<<std::dec<<'\n';
        require(outcome==family::Outcome::Complete && a.cpu.pc==returned && !a.cpu.trap_pending,"actor original return");++cases;
    }
    for(unsigned kind=0;kind<4;++kind) {
        std::cout<<"actor special="<<kind<<'\n';
        ActorFixture a(image,fpscr_fr_mask),b(image,fpscr_fr_mask);
        for(auto* f:{&a,&b}) {
            setup_actor(*f,kind==0?1u:0u,kind==2?3u:2u);
            if(kind==1)f->put(0x8C19B3A8u,1u);
            if(kind==3){f->cpu.r[4]|=0x20000000u;f->cpu.r[15]|=0x20000000u;}
        }
        a.reference=&b;
        const auto outcome=family::execute(a.cpu,&a.immutable,{&a,ActorFixture::invoke,ActorFixture::resume_instructions});
        finish_reference(a,b);
        require(outcome==family::Outcome::Complete && a.cpu.pc==returned && !a.cpu.trap_pending,"actor special return");++cases;
    }
    for(unsigned kind=0;kind<20;++kind) {
        const bool whole_original=kind<16;
        std::cout<<"actor actual-aot="<<kind<<" whole-original="<<whole_original<<'\n';
        ActorFixture a(image,0),b(image,0);
        for(auto* f:{&a,&b}) {
            setup_actor(*f,kind<13?kind:kind==13?255u:1u,kind<13?kind%2:2u);
            if(kind==14)f->put(0x8C19B3A8u,1u);
            if(kind==16)f->cpu.r[15]+=2u;
            if(kind==17)f->put(actor_work+56,actor_contact+1u);
            if(kind==18)f->put(actor_task+36,actor_extra+1u);
            if(kind==19)f->replace_observer=true;
        }
        a.reference=&b;actor_active=&a;external_override=actor_external;
        sonic::scalar_writes::unbind(&a.cpu.memory,&a.immutable);
        a.cpu.memory.set_guest_write_observer({});a.cpu.memory.set_guest_write_batch_observer({});
        NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,a.immutable);
        sonic::scalar_writes::bind(a.cpu.memory,a.immutable,a.cpu.memory.guest_write_observer_generation());
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        if(!whole_original)(void)family::execute(a.cpu,&a.immutable,{&a,ActorFixture::invoke,resume});
        for(unsigned n=0;a.cpu.pc!=returned && !a.cpu.trap_pending;++n) {
            require(n<512u,"actor actual AOT continuation bound");
            if(ActorFixture::boundary(a.cpu.pc))ActorFixture::invoke(&a,a.cpu,a.cpu.pc);
            else {
                auto at=original_owners.find(a.cpu.pc);
                if(at==original_owners.end())std::cerr<<"missing actor AOT entry "<<std::hex<<a.cpu.pc<<std::dec<<'\n';
                require(at!=original_owners.end(),"actor original sparse entry");
                const auto ok=resume(&a,a.cpu,at->second,returned);
                require(ok || a.cpu.trap_pending,"actor retained continuation");
            }
        }
        finish_reference(a,b);
        if(kind>=16 && kind<=18)require(a.cpu.trap_pending,"actor actual AOT expected fault");
        else require(a.cpu.pc==returned && !a.cpu.trap_pending,"actor actual AOT return");
        katana_port_generated::runtime_dispatch_detail::active_services=nullptr;external_override=nullptr;
        ++cases;
    }
    std::cout<<"SONIC_ACTOR_OPERATION_PASS cases="<<cases<<" calls="<<family::counts.actor_calls
        <<" state_transfers="<<family::counts.state_transfers<<" internal="<<family::counts.internal_calls
        <<" resumes="<<family::counts.resumes<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_ACTOR_OPERATION_FAIL "<<e.what()<<'\n';return 1;}
