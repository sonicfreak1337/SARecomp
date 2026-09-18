// Connected character state/movement/draw operations against original bytes.
#define SARECOMP_HIERARCHY_AOT_TEST_ENTRY player_retained_tests_unused
#include "test_render_hierarchy_aot.cpp"
namespace {
constexpr std::uint32_t task=0x8CE50000u,work=0x8CE51000u,motion_work=0x8CE52000u,
    player_work=0x8CE53000u,aux_work=0x8CE54000u,contact_work=0x8CE55000u,
    contact_shape=0x8CE56000u,allocated=0x8CE57000u;
struct PlayerFixture:Fixture {
    PlayerFixture* reference{};
    unsigned variant{};
    bool replace_observer{};
    using Fixture::Fixture;
    static bool boundary(std::uint32_t pc) {
        if(family::contains(pc))return false;
        switch(pc) {
        case 0x8C02D060u:
        case 0x8C02F4B0u:
        case 0x8C02F63Cu:
        case 0x8C02FC12u:
        case 0x8C036F12u:
        case 0x8C03700Cu:
        case 0x8C037098u:
        case 0x8C037108u:
        case 0x8C038CECu:
        case 0x8C038D00u:
        case 0x8C041C92u:
        case 0x8C042DC4u:
        case 0x8C043AEEu:
        case 0x8C045736u:
        case 0x8C0466E8u:
        case 0x8C047126u:
        case 0x8C047D90u:
        case 0x8C047FCEu:
        case 0x8C047FFCu:
        case 0x8C0486A0u:
        case 0x8C0487B6u:
        case 0x8C0488A2u:
        case 0x8C0489E4u:
        case 0x8C049A4Eu:
        case 0x8C04B57Cu:
        case 0x8C04B9E0u:
        case 0x8C04F7E0u:
        case 0x8C04FE40u:
        case 0x8C050190u:
        case 0x8C0502D8u:
        case 0x8C051A16u:
        case 0x8C051CC0u:
        case 0x8C052A00u:
        case 0x8C055C60u:
        case 0x8C055C8Eu:
        case 0x8C055C9Au:
        case 0x8C055CECu:
        case 0x8C05689Cu:
        case 0x8C0568B2u:
        case 0x8C056C9Eu:
        case 0x8C056CB6u:
        case 0x8C0582E0u:
        case 0x8C058374u:
        case 0x8C06C290u:
        case 0x8C06C50Au:
        case 0x8C06C638u:
        case 0x8C06C7C0u:
        case 0x8C06C970u:
        case 0x8C06C98Eu:
        case 0x8C06CA80u:
        case 0x8C06CC62u:
        case 0x8C06CD80u:
        case 0x8C06CDFEu:
        case 0x8C06CE3Eu:
        case 0x8C06CEA0u:
        case 0x8C06CF0Cu:
        case 0x8C06D058u:
        case 0x8C06D076u:
        case 0x8C06F19Cu:
        case 0x8C06F2C6u:
        case 0x8C071112u:
        case 0x8C0713F6u:
        case 0x8C071640u:
        case 0x8C071C80u:
        case 0x8C0720B4u:
        case 0x8C0724AAu:
        case 0x8C072500u:
        case 0x8C0725AEu:
        case 0x8C072A84u:
        case 0x8C078248u:
        case 0x8C078650u:
        case 0x8C078DF0u:
        case 0x8C078FD6u:
        case 0x8C0793B6u:
        case 0x8C07A49Cu:
        case 0x8C07B260u:
        case 0x8C07BB02u:
        case 0x8C07C1D8u:
        case 0x8C07CA94u:
        case 0x8C07D6F4u:
        case 0x8C07DC20u:
        case 0x8C086E9Eu:
        case 0x8C08AA6Eu:
        case 0x8C08AA82u:
        case 0x8C08AAA0u:
        case 0x8C09190Cu:
        case 0x8C091928u:
        case 0x8C091ACAu:
        case 0x8C091AE4u:
        case 0x8C09216Cu:
        case 0x8C09401Au:
        case 0x8C09407Au:
        case 0x8C094270u:
        case 0x8C0944E0u:
        case 0x8C094510u:
        case 0x8C09846Eu:
        case 0x8C09859Cu:
        case 0x8C0986C6u:
        case 0x8C0989C0u:
        case 0x8C098A4Cu:
        case 0x8C098A82u:
        case 0x8C098BE0u:
        case 0x8C098DD8u:
        case 0x8C09DD3Cu:
        case 0x8C09DD7Au:
        case 0x8C0C9B22u:
        case 0x8C0C9BDAu:
        case 0x8C0FACA0u:
        case 0x8C0FBCAAu:
        case 0x8C109DECu:
        case 0x8C109F1Eu:
        case 0x8C10C99Cu:
        case 0x8C10CAF8u:
        case 0x8C10D038u:
        case 0x8C10D274u:
        case 0x8C605568u:
        case 0x8C605CECu:
        case 0x8C60D138u:
        case 0x8C60ED20u:
        case 0x8C60ED30u:
        case 0x8C621516u:
        case 0x8C638DECu:
        case 0x8C638E68u:
        case 0x8C638FD0u:
        case 0x8C6398BCu:
        case 0x8C63A10Cu:
        case 0x8C63A688u:
        case 0x8C63A69Cu:
        case 0x8C63A724u:
        case 0x8C63A744u:
        case 0x8C63A88Cu:
        case 0x8C63A8B0u:
        case 0x8C63A8F8u:
        case 0x8C63A904u:
        case 0x8C63FA28u:
        case 0x8C63FA58u:
        case 0x8C63FEC4u:
        case 0x8C63FFC0u:
        case 0x8C640068u:return true;
        case 0x8C0568BCu:
        case 0x8C06D194u:
        case 0x8C06E844u:
        case 0x8C070E1Au:
        case 0x8C098EEEu:
        case 0x8C098F3Cu:
        case 0x8C09DD74u:
        case 0x8C608C0Cu:
        case 0x8C63CD64u:
        case 0x8C63EDF4u:
        case 0x8C63EEB4u:
            return true;
        case 0x8C015720u:
        case 0x8C605D4Au:
        case 0x8C640862u:
            return true;
        default:return false;
        }
    }
    void advance_to_boundary(std::uint32_t end=returned) {
        while(cpu.pc!=end && !boundary(cpu.pc) && !cpu.trap_pending) {
            require(++steps<400000u,"player reference bound");reference_step(cpu);
        }
    }
    bool child(std::uint32_t target) {
        trace.emplace_back(target,cpu.pr);++callback_index;
        cpu.r[0]=0;
        // Model-buffer allocation failure is an explicit negative fixture;
        // live initialization is covered by the hidden game entry check.
        if(target==0x8C098BE0u)cpu.r[0]=allocated;
        if(target==0x8C098EEEu || target==0x8C098F3Cu)cpu.r[0]=player_work;
        if(target==0x8C098A4Cu)cpu.r[0]=aux_work;
        if(target==0x8C049A4Eu)cpu.r[0]=variant;
        if(target==0x8C09407Au)cpu.r[0]=variant;
        if(replace_observer && callback_index==1)
            cpu.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{});
        cpu.t=!cpu.t;cpu.pc=cpu.pr;return true;
    }
    static bool invoke(void* p,CpuState& c,std::uint32_t target) {
        auto& f=*static_cast<PlayerFixture*>(p);
        if(&c!=&f.cpu || !boundary(target)) {
            std::cerr<<"unexpected player callback "<<std::hex<<target<<" PR "<<c.pr<<std::dec<<'\n';
            throw std::runtime_error("player boundary");
        }
        if(f.reference){f.reference->advance_to_boundary();compare(f,*f.reference,"player callback entry");}
        const auto ok=f.child(target);
        if(f.reference){require(f.reference->child(target)==ok,"player callback result");compare(f,*f.reference,"player callback return");}
        return ok;
    }
    static bool resume_instructions(void* p,CpuState& c,std::uint32_t,std::uint32_t end) {
        auto& f=*static_cast<PlayerFixture*>(p);
        while(c.pc!=end && !c.trap_pending) {
            require(++f.steps<400000u,"player fallback bound");
            if(boundary(c.pc)){if(!invoke(p,c,c.pc))return false;}
            else reference_step(c);
        }
        return !c.trap_pending;
    }
};
PlayerFixture* player_active{};
void player_external(CpuState& c,std::uint32_t target) {
    c.pc=target;
    if(PlayerFixture::boundary(target))require(PlayerFixture::invoke(player_active,c,target),"player AOT boundary");
    else require(PlayerFixture::resume_instructions(player_active,c,target,c.pr),"player AOT child");
}
void setup_player(PlayerFixture& f,unsigned state,unsigned variant,std::uint32_t root) {
    f.variant=variant;f.cpu.pc=root;f.cpu.r[4]=task;f.cpu.r[5]=motion_work;f.cpu.r[6]=player_work;
    f.cpu.gbr=0x8C8FFE00u;
    f.put(task+32,work);f.put(task+36,motion_work);f.put(motion_work,player_work);
    f.put(task+60,aux_work);f.put(work+60,aux_work);
    f.put(player_work+0x7C,aux_work);
    constexpr auto action_table=records+0x4000u,action=records+0x6000u,animation=records+0x6020u;
    f.put(player_work+0x140,action_table);f.put(player_work+0x148,action);
    for(unsigned i=0;i<256;++i)f.put(action_table+i*16,action);
    // Keep the separate original model template at 8C42305C intact.
    for(unsigned i=0;i<64;++i)f.put(0x8C422B28u+i*16,action);
    f.put(action,nodes);f.put(action+4,animation);
    f.put(animation,records);f.put(animation+4,20u);f.put(animation+8,0u);
    f.put(work,state);f.put(work+56,contact_work);
    f.put(contact_work+12,contact_shape);f.put(contact_work+16,allocated);
    f.put(allocated+32,work);f.put(allocated+36,motion_work);
    f.vector(work+32,4.f,5.f,6.f);f.vector(work+44,1.f,1.5f,2.f);
    f.put(0x8C19B3A8u,0u);
    f.put(0x8C78C548u,work);
    for(unsigned i=0;i<16;++i)f.put(0x8C67C580u+i*4,i%5==0?0x3F800000u:0u);
}
void finish_player(PlayerFixture& a,PlayerFixture& b) {
    for(unsigned n=0;b.cpu.pc!=a.cpu.pc && !b.cpu.trap_pending;++n) {
        require(n<1024u,"player endpoint not reached");b.advance_to_boundary(a.cpu.pc);
        if(PlayerFixture::boundary(b.cpu.pc) && b.cpu.pc!=a.cpu.pc)b.child(b.cpu.pc);
    }
    compare(a,b,"player final");require(a.trace==b.trace,"player boundary sequence");
}
}
int main(int argc,char** argv)try {
    std::cout<<std::unitbuf;
    require(argc==3 || (argc==4 && std::string(argv[3])=="--transfer-only"),"player-operation-tests <original-ram> <original-code-root> [--transfer-only]");
#ifdef _WIN32
    _putenv_s("SARECOMP_NATIVE_PLAYER_OPERATION","1");_putenv_s("SARECOMP_NATIVE_LAND_RENDER","1");
#else
    setenv("SARECOMP_NATIVE_PLAYER_OPERATION","1",1);setenv("SARECOMP_NATIVE_LAND_RENDER","1",1);
#endif
    std::ifstream file(argv[1],std::ios::binary);
    const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000u,"player RAM size");load_original_entries(argv[2]);unsigned cases=0,returned_cases=0;
    if(argc==3)for(auto root:{0x8C0CBD40u,0x8C0CCFE8u})for(unsigned state=0;state<58;++state) {
        const auto input_state=state==57?255u:state;
        std::cout<<"player root="<<std::hex<<root<<std::dec<<" state="<<input_state<<'\n';
        PlayerFixture a(image,0),b(image,0);setup_player(a,input_state,0,root);setup_player(b,input_state,0,root);a.reference=&b;
        const auto outcome=family::execute(a.cpu,&a.immutable,{&a,PlayerFixture::invoke,PlayerFixture::resume_instructions});
        finish_player(a,b);
        if(a.cpu.pc!=returned || a.cpu.trap_pending)std::cout<<"fixture fault pc="<<std::hex<<a.cpu.pc<<" spc="<<a.cpu.spc<<" tea="<<a.cpu.tea<<std::dec<<'\n';
        else ++returned_cases;
        if(root==0x8C0CBD40u && input_state==0)
            require(a.cpu.trap_pending && a.cpu.spc==0x8C0D5442u && a.cpu.tea==0,"player explicit allocation-failure boundary");
        else require(outcome==family::Outcome::Complete && a.cpu.pc==returned && !a.cpu.trap_pending,"player state return");
        ++cases;
    }
    if(argc==3)for(auto root:{0x8C0CBD40u,0x8C0CCFE8u})for(unsigned state:{1u,34u,54u}) {
        PlayerFixture a(image,fpscr_fr_mask|1u),b(image,fpscr_fr_mask|1u);
        for(auto* f:{&a,&b})setup_player(*f,state,1u,root);
        a.reference=&b;
        const auto outcome=family::execute(a.cpu,&a.immutable,{&a,PlayerFixture::invoke,PlayerFixture::resume_instructions});
        finish_player(a,b);
        require(outcome==family::Outcome::Complete && a.cpu.pc==returned && !a.cpu.trap_pending,"player alternate FPSCR return");++cases;
    }
    if(argc==3)for(unsigned kind=0;kind<16;++kind) {
        constexpr unsigned selected_states[]{0u,1u,2u,34u,54u,255u};
        const bool whole_original=kind<12;
        const auto root=kind<6?0x8C0CBD40u:0x8C0CCFE8u;
        const auto state=kind<12?selected_states[kind%6]:1u;
        std::cout<<"player actual-aot="<<kind<<" whole-original="<<whole_original<<'\n';
        PlayerFixture a(image,0),b(image,0);
        for(auto* f:{&a,&b}) {
            setup_player(*f,state,0,root);
            if(kind==12)f->cpu.r[15]+=2u;
            if(kind==13)f->put(task+32,work+1u);
            if(kind==14)f->replace_observer=true;
            if(kind==15){f->cpu.r[4]|=0x20000000u;f->cpu.r[15]|=0x20000000u;}
        }
        a.reference=&b;player_active=&a;external_override=player_external;
        sonic::scalar_writes::unbind(&a.cpu.memory,&a.immutable);
        a.cpu.memory.set_guest_write_observer({});a.cpu.memory.set_guest_write_batch_observer({});
        NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,a.immutable);
        sonic::scalar_writes::bind(a.cpu.memory,a.immutable,a.cpu.memory.guest_write_observer_generation());
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        if(!whole_original)(void)family::execute(a.cpu,&a.immutable,{&a,PlayerFixture::invoke,resume});
        for(unsigned n=0;a.cpu.pc!=returned && !a.cpu.trap_pending;++n) {
            require(n<1024u,"player actual AOT continuation bound");
            if(PlayerFixture::boundary(a.cpu.pc))PlayerFixture::invoke(&a,a.cpu,a.cpu.pc);
            else {
                auto at=original_owners.find(a.cpu.pc);
                if(at==original_owners.end())std::cerr<<"missing player AOT entry "<<std::hex<<a.cpu.pc<<std::dec<<'\n';
                require(at!=original_owners.end(),"player original sparse entry");
                const auto ok=resume(&a,a.cpu,at->second,returned);
                require(ok || a.cpu.trap_pending,"player retained continuation");
            }
        }
        finish_player(a,b);
        if(kind==0 || kind==12 || kind==13)require(a.cpu.trap_pending,"player actual AOT expected fault");
        else require(a.cpu.pc==returned && !a.cpu.trap_pending,"player actual AOT return");
        katana_port_generated::runtime_dispatch_detail::active_services=nullptr;external_override=nullptr;
        ++cases;
    }
    for(unsigned changed_source=0;changed_source<2;++changed_source) {
        std::cout<<"player local-transfer source-change="<<changed_source<<'\n';
        PlayerFixture a(image,0),b(image,0);
        for(auto* f:{&a,&b}) {
            setup_player(*f,1u,0u,0x8C0CED2Eu);
            f->cpu.r[2]=aux_work;f->cpu.r[1]=0x12345678u;
            f->put(f->cpu.r[15]+24u,returned);f->put(f->cpu.r[15]+28u,0x87654321u);
            if(changed_source)f->put(0x8C0CCFE8u,f->get(0x8C0CCFE8u)^1u);
        }
        a.reference=&b;player_active=&a;external_override=player_external;
        sonic::scalar_writes::unbind(&a.cpu.memory,&a.immutable);
        a.cpu.memory.set_guest_write_observer({});a.cpu.memory.set_guest_write_batch_observer({});
        NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,a.immutable);
        sonic::scalar_writes::bind(a.cpu.memory,a.immutable,a.cpu.memory.guest_write_observer_generation());
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        const auto before=family::counts.resumes;
        const auto outcome=family::execute(a.cpu,&a.immutable,{&a,PlayerFixture::invoke,resume});
        finish_player(a,b);
        require(outcome==family::Outcome::Complete && a.cpu.pc==returned && !a.cpu.trap_pending,"player local transfer return");
        require(family::counts.resumes-before==changed_source,"player local transfer source fallback");
        katana_port_generated::runtime_dispatch_detail::active_services=nullptr;external_override=nullptr;++cases;
    }
    std::cout<<"SONIC_PLAYER_OPERATION_PASS cases="<<cases<<" returns="<<returned_cases<<" calls="<<family::counts.player_calls
        <<" transfers="<<family::counts.state_transfers<<" internal="<<family::counts.internal_calls<<" resumes="<<family::counts.resumes<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_PLAYER_OPERATION_FAIL "<<e.what()<<'\n';return 1;}
