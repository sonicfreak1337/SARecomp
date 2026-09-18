// Preserve the complete camera owner, its original adjustment modes and real
// foreign-call boundaries. The oracle executes authenticated original bytes.
#define SARECOMP_CONTACT_AOT_TEST_ENTRY contact_aot_tests_unused
#include "test_movement_contact_aot.cpp"
namespace {
constexpr std::uint32_t camera_callback=0x8CE7F000u;
void setup_camera(Fixture& f,unsigned kind){
    f.cpu.pc=family::camera_entry;f.cpu.r[4]=A;
    f.clear(0x8C6B6DECu,0x1000u);
    f.put(0x8C6B6FA4u,C); // live camera-area parameter table
    f.put(0x8C111F80u,C);f.put(0x8C111F84u,Q);
    f.put(0x8C111F88u,B);f.put(0x8C111F8Cu,Q+0x100u);
    f.vector(A+32,12.f,30.f,-42.f);f.vector(A+20,128.f,256.f,384.f);
    f.vector(Q+12,8.f,20.f,-30.f);f.vector(Q+24,0.f,8.f,1.f);
    f.put(Q+36,0x1234u);f.put(Q+40,0x2000u);f.put(Q+44,0x4000u);
    f.vector(C+48,2.f,3.f,4.f);f.vector(C+36,12.f,15.f,18.f);f.putf(C+60,32.f);
    f.vector(0x8C6B7274u+48,3.f,4.f,5.f);f.vector(0x8C6B7274u+36,9.f,12.f,15.f);
    f.vector(0x8C6B6F50u+4,7.f,8.f,9.f);
    f.ram->writable_bytes()[(B+7)&0xFFFFFFu]=std::uint8_t(kind%4);
    f.ram->writable_bytes()[(B+8)&0xFFFFFFu]=kind>=8?5u:0u;
    f.ram->writable_bytes()[(B+11)&0xFFFFFFu]=kind>=4 && kind<8?2u:1u;
    f.put(B+12,kind==0?0u:camera_callback);f.put(B+16,camera_callback);f.put(B+20,kind>=12?2u:0u);
    // An authentic, mutable foreign callback writes a counter and clobbers
    // two registers. Both original and native callers execute these bytes.
    f.half(camera_callback,0xD104);f.half(camera_callback+2,0x6012);
    f.half(camera_callback+4,0x7001);f.half(camera_callback+6,0x2102);
    f.half(camera_callback+8,0x000B);f.half(camera_callback+10,0x0009);
    f.put(camera_callback+20,Q+56);
    if(kind>=16)f.mutation=kind-15; // observer, FP mode, then mutable position
}
void bind_aot(Fixture& f){
    sonic::scalar_writes::unbind(&f.cpu.memory,&f.immutable);
    f.cpu.memory.set_guest_write_observer({});f.cpu.memory.set_guest_write_batch_observer({});
}
}
int main(int argc,char** argv)try{
    require(argc==3,"camera-operation-tests <RAM> <retained-code>");
    require(family::camera_enabled(),"SARECOMP_NATIVE_CAMERA_OPERATION=1 required");
    std::ifstream input(argv[1],std::ios::binary);const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(input),{}};
    require(image.size()==0x1000000u,"RAM size");load_original_entries(argv[2]);unsigned cases=0;
    for(unsigned mode:{0u,fpscr_fr_mask|1u})for(unsigned kind=0;kind<19;++kind){
        std::cout<<"camera case="<<kind<<" mode="<<mode<<std::endl;
        Fixture a(image,mode),b(image,mode);setup_camera(a,kind);setup_camera(b,kind);a.oracle=&b;
        const auto outcome=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,Fixture::resume});
        b.until(a.cpu.pc);compare(a,b,"camera complete operation");
        if(outcome!=family::Outcome::Complete || a.cpu.pc!=returned || a.cpu.trap_pending)
            std::cerr<<"outcome="<<unsigned(outcome)<<" pc="<<std::hex<<a.cpu.pc<<" tea="<<a.cpu.tea
                <<" spc="<<a.cpu.spc<<" pr="<<a.cpu.pr<<std::dec<<" callbacks="<<a.callbacks<<'\n';
        require(outcome==family::Outcome::Complete && a.cpu.pc==returned && !a.cpu.trap_pending,"camera original return");++cases;
    }
    for(unsigned kind:{0u,1u,2u,3u,7u,10u,13u,15u}){
        std::cout<<"camera AOT case="<<kind<<std::endl;
        Fixture a(image,0),b(image,0);setup_camera(a,kind);setup_camera(b,kind);active_fixture=&a;a.oracle=&b;
        bind_aot(a);NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,a.immutable);
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        for(unsigned n=0;a.cpu.pc!=returned && !a.cpu.trap_pending;++n){
            require(n<500,"camera AOT continuation bound");
            auto at=original_owners.find(a.cpu.pc);require(at!=original_owners.end(),"original camera continuation");
            require(resume(&a,a.cpu,at->second,returned),"actual camera AOT body");
        }
        b.until(a.cpu.pc);compare(a,b,"actual camera AOT body");
        require(a.cpu.pc==returned && !a.cpu.trap_pending,"camera AOT return");++cases;
    }
    for(unsigned kind=0;kind<4;++kind){
        std::cout<<"camera fallback case="<<kind<<std::endl;
        Fixture a(image,0),b(image,0);setup_camera(a,3);setup_camera(b,3);active_fixture=&a;a.oracle=&b;
        for(auto* f:{&a,&b}){
            if(kind==0)f->cpu.r[4]+=1u;
            if(kind==1)f->cpu.r[15]+=2u;
            if(kind==2)f->put(0x8C111F88u,B+1u);
            if(kind==3)f->put(0x8C111F84u,Q+1u);
        }
        bind_aot(a);NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,a.immutable);
        sonic::scalar_writes::bind(a.cpu.memory,a.immutable,a.cpu.memory.guest_write_observer_generation());
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        const auto outcome=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,resume});
        b.until(a.cpu.pc);compare(a,b,"camera original fault continuation");
        require(outcome==family::Outcome::Interrupted && a.cpu.trap_pending,"camera original fault");++cases;
    }
    std::cout<<"SONIC_CAMERA_OPERATION_PASS cases="<<cases<<" whole_pass="<<family::counts.camera_calls
        <<" internal="<<family::counts.internal_calls<<" external="<<family::counts.callbacks<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_CAMERA_OPERATION_FAIL "<<e.what()<<'\n';return 1;}
