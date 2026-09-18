// Original PAL instructions and actual retained AOT, sharing the established
// full-register/full-RAM oracle. Model/GPU boundaries are explicit fixture calls.
#define SARECOMP_HIERARCHY_AOT_TEST_ENTRY retained_hierarchy_tests_unused
#include "test_render_hierarchy_aot.cpp"
namespace {
constexpr std::uint32_t land_table=0x8CE50000u,land_records=0x8CE51000u,
    land_motion=0x8CE52000u,land_task=0x8CE53000u,land_work=0x8CE53100u,
    land_action=0x8CE54000u,land_animation=0x8CE54100u,land_camera=0x8CE58000u;
struct LandFixture:Fixture {
    LandFixture* reference{};
    unsigned change{};
    using Fixture::Fixture;
    static bool boundary(std::uint32_t pc) {
        return pc==0x8C037108u || Fixture::external(pc) || pc==0x8C605CECu ||
            pc==0x8C605D4Au || pc==0x8C608C0Cu || pc==0x8C60ED30u ||
            pc==0x8C051A16u || pc==0x8C051CC0u || pc==0x8C052A00u;
    }
    void advance_to_boundary(std::uint32_t end=returned) {
        while(cpu.pc!=end && !boundary(cpu.pc) && !cpu.trap_pending) {
            require(++steps<500000u,"land reference bound");reference_step(cpu);
        }
    }
    bool child(std::uint32_t target) {
        trace.emplace_back(target,cpu.pr);++callback_index;
        if(target==0x8C608C0Cu && change && callback_index<8) {
            if(change==1)vector(nodes+256+8,19.f,-3.f,7.f);
            if(change==2)put(0x8C75A24Cu,land_records+72u);
            if(change==3)cpu.memory.set_guest_write_observer([](const GuestWriteEvent&)noexcept{});
        }
        cpu.r[0]=target;cpu.t=!cpu.t;cpu.pc=cpu.pr;return true;
    }
    static bool invoke(void* p,CpuState& c,std::uint32_t target) {
        auto& f=*static_cast<LandFixture*>(p);require(&c==&f.cpu && boundary(target),"land external target");
        if(f.reference){f.reference->advance_to_boundary();compare(f,*f.reference,"land callback entry");}
        const auto ok=f.child(target);
        if(f.reference){require(f.reference->child(target)==ok,"land callback result");compare(f,*f.reference,"land callback return");}
        return ok;
    }
    static bool resume_instructions(void* p,CpuState& c,std::uint32_t,std::uint32_t end) {
        auto& f=*static_cast<LandFixture*>(p);
        while(c.pc!=end && !c.trap_pending) {
            require(++f.steps<500000u,"land fallback bound");
            if(boundary(c.pc)){if(!invoke(p,c,c.pc))return false;}
            else reference_step(c);
        }
        return !c.trap_pending;
    }
};
LandFixture* land_active{};
void land_external(CpuState& c,std::uint32_t target) {
    c.pc=target;
    if(LandFixture::boundary(target))require(LandFixture::invoke(land_active,c,target),"land AOT boundary");
    else require(LandFixture::resume_instructions(land_active,c,target,c.pr),"land AOT child");
}
void setup_land(LandFixture& f,unsigned kind) {
    f.cpu.pc=family::land_entry;f.cpu.r[4]=land_task;f.cpu.gbr=0x8C8FFE00u;
    f.put(land_task+32,land_work);f.put(land_work,1u);
    f.put(0x8C759634u,land_table);f.put(0x8C759638u,land_table);
    f.put(land_table,4u|(2u<<16));
    constexpr std::uint32_t flags[]{0u,1u,3u,5u,9u,17u,21u,0u};
    f.put(land_table+4,flags[kind%8]);f.putf(land_table+8,200.f);
    f.put(land_table+12,land_records);f.put(land_table+16,land_motion);
    f.put(0x8C19E8B4u,1);f.put(0x8C754E08u,0);f.put(0x8C19E938u,~0u);
    f.put(0x8C75A248u,4);f.putf(0x8C759640u,.25f);f.putf(0x8C88F554u,180.f);
    f.put(0x8C18B72Cu,land_camera);f.vector(land_camera+32,2.f,5.f,-1.f);
    f.put(land_camera+20,kind&1?0x1200u:0u);f.put(land_camera+24,kind&2?0x2100u:0u);
    for(unsigned i=0;i<16;++i)f.put(0x8C67C580u+i*4,i%5==0?0x3F800000u:0u);
    for(unsigned i=0;i<4;++i) {
        const auto row=land_records+i*36u;
        f.vector(row,float(i)*20.f,2.f,float(i)*3.f);f.putf(row+12,2.f);
        f.put(row+24,nodes+i*256u);f.put(row+28,(kind==7 && i==2)?0x100u:0u);
        f.put(row+32,0x80000000u|((kind&2)?0x20u:0u));
        f.put(0x8C75A24Cu+i*4,row);
    }
    f.put(land_action,nodes);f.put(land_action+4,land_animation);
    f.put(land_animation,records);f.put(land_animation+4,20u);f.put(land_animation+8,0u);
    for(unsigned i=0;i<2;++i) {
        const auto row=land_motion+i*24u;
        f.putf(row,4.f+float(i));f.putf(row+4,20.f);f.putf(row+8,.5f);
        f.put(row+12,nodes);f.put(row+16,(flags[kind%8]&16u)?land_animation:land_action);
        f.put(row+20,0x8CE57000u);
    }
    if(kind==8)f.put(land_work,0u);
    if(kind==9)f.put(land_work,2u);
    if(kind==10)f.put(land_work,7u);
    if(kind==11)f.put(0x8C754E08u,1u);
    if(kind>=12 && kind<16){f.cpu.pc=0x8C051E56u;f.change=kind-12;}
    if(kind>=16 && kind<20)f.cpu.pc=0x8C0520C8u;
    if(kind==17)f.put(land_table,1u);
    if(kind==18)f.put(land_table,1025u);
    if(kind==19)f.cpu.r[15]+=2;
    if(kind>=20 && kind<24)f.cpu.pc=0x8C051F64u;
    if(kind==22)f.put(land_table,4u);
    if(kind==23)f.put(land_table+4,17u);
    if(kind==24){f.cpu.pc=0x8C052048u;f.put(land_table+4,17u);}
    if(kind==25){f.cpu.pc=0x8C052048u;f.put(land_table+4,1u);}
    if(kind==26){f.cpu.pc=0x8C051E56u;f.cpu.r[15]+=2;}
    if(kind==27){f.cpu.pc=0x8C051E56u;f.put(0x8C75A248u,0);}
    if(kind==28){f.cpu.pc=0x8C051F64u;f.put(land_table+4,1);f.put(land_motion+16,land_action+2);}
    if(kind==29){f.cpu.pc=0x8C051E56u;f.cpu.gbr=0x8CE59000u;}
    if(kind==30){f.cpu.pc=0x8C051E56u;f.put(0x8C75A248u,0xFFFFu);}
    if(kind==31){f.cpu.r[4]|=0x20000000u;f.cpu.r[15]|=0x20000000u;}
}
}
int main(int argc,char** argv)try {
    std::cout<<std::unitbuf;
    require(argc==3,"land-render-tests <original-ram> <original-code-root>");
#ifdef _WIN32
    _putenv_s("SARECOMP_NATIVE_LAND_RENDER","1");
#else
    setenv("SARECOMP_NATIVE_LAND_RENDER","1",1);
#endif
    std::ifstream file(argv[1],std::ios::binary);const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(file),{}};
    require(image.size()==0x1000000u,"land RAM size");load_original_entries(argv[2]);unsigned cases=0;
    for(unsigned mode:{0u,1u})for(unsigned kind=0;kind<32;++kind) {
        LandFixture a(image,mode),b(image,mode);setup_land(a,kind);setup_land(b,kind);a.reference=&b;
        const auto result=family::execute(a.cpu,&a.immutable,{&a,LandFixture::invoke,LandFixture::resume_instructions});
        for(unsigned n=0;b.cpu.pc!=a.cpu.pc && !b.cpu.trap_pending;++n){require(n<256u,"land endpoint not reached");b.advance_to_boundary();if(LandFixture::boundary(b.cpu.pc))b.child(b.cpu.pc);}
        compare(a,b,"land final");require(a.trace==b.trace,"land boundary sequence");
        require(a.cpu.trap_pending?result==family::Outcome::Interrupted:result==family::Outcome::Complete,"land outcome");
        std::cout<<"land case="<<kind<<" mode="<<mode<<" calls="<<a.trace.size()<<'\n';++cases;
    }
    for(unsigned kind:{0u,5u,8u,9u,15u,19u,23u,26u,28u}) {
        LandFixture a(image,0),b(image,0);setup_land(a,kind);setup_land(b,kind);a.reference=&b;
        land_active=&a;external_override=land_external;
        sonic::scalar_writes::unbind(&a.cpu.memory,&a.immutable);a.cpu.memory.set_guest_write_observer({});a.cpu.memory.set_guest_write_batch_observer({});
        NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,a.immutable);
        sonic::scalar_writes::bind(a.cpu.memory,a.immutable,a.cpu.memory.guest_write_observer_generation());
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        const bool whole_original=kind!=15u && kind!=19u && kind!=26u && kind!=28u;
        auto result=family::Outcome::Complete;
        if(whole_original) {
            // These original computed tails deliberately yield to the sparse
            // global dispatcher, including the caller's later continuation.
            // Only genuine entries from the retained table may be resumed.
            for(unsigned n=0;a.cpu.pc!=returned && !a.cpu.trap_pending;++n) {
                require(n<256u,"land original dispatch bound");
                if(LandFixture::boundary(a.cpu.pc))LandFixture::invoke(&a,a.cpu,a.cpu.pc);
                else {require(original_owners.contains(a.cpu.pc),"land original sparse entry missing");
                    require(resume(&a,a.cpu,original_owners.at(a.cpu.pc),returned),"land original dispatcher");}
            }
            if(a.cpu.trap_pending)result=family::Outcome::Interrupted;
        }else result=family::execute(a.cpu,&a.immutable,{&a,LandFixture::invoke,resume});
        for(unsigned n=0;b.cpu.pc!=a.cpu.pc && !b.cpu.trap_pending;++n){require(n<256u,"land AOT endpoint not reached");b.advance_to_boundary();if(LandFixture::boundary(b.cpu.pc))b.child(b.cpu.pc);}
        compare(a,b,"land actual AOT");require(a.trace==b.trace,"land AOT repeated boundary");
        if(kind==15u && result==family::Outcome::Interrupted && !a.cpu.trap_pending) {
            // Replacing the observer can deliberately yield at a genuine
            // retained continuation; finish only through that sparse table.
            for(unsigned n=0;a.cpu.pc!=returned && !a.cpu.trap_pending;++n) {
                require(n<256u && original_owners.contains(a.cpu.pc),"land observer sparse yield");
                require(resume(&a,a.cpu,original_owners.at(a.cpu.pc),returned),"land observer continuation");
            }
            for(unsigned n=0;b.cpu.pc!=a.cpu.pc && !b.cpu.trap_pending;++n){require(n<256u,"land observer endpoint");b.advance_to_boundary();if(LandFixture::boundary(b.cpu.pc))b.child(b.cpu.pc);}
            compare(a,b,"land observer completion");require(a.trace==b.trace,"land observer repeated boundary");
        }else require(a.cpu.trap_pending?result==family::Outcome::Interrupted:result==family::Outcome::Complete,"land AOT outcome");
        if(kind==15u && !a.cpu.trap_pending)require(a.cpu.pc==returned && family::return_site==0x8C051F60u,"land retained tail provenance");
        katana_port_generated::runtime_dispatch_detail::active_services=nullptr;external_override=nullptr;
        std::cout<<"land actual-aot="<<kind<<" whole-original="<<whole_original<<'\n';++cases;
    }
    std::cout<<"SONIC_LAND_RENDER_PASS cases="<<cases<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_LAND_RENDER_FAIL "<<e.what()<<'\n';return 1;}
