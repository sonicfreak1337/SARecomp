// Complete object-pair passes against original PAL instructions and retained
// AOT, including shared transforms, registration and original continuations.
#define SARECOMP_CONTACT_AOT_TEST_ENTRY contact_aot_tests_unused
#include "test_movement_contact_aot.cpp"
namespace {
constexpr std::uint32_t contacts=0x8CE10000u;
constexpr std::array<std::uint32_t,9> list_counts{0x8C745618u,0x8C74561Au,0x8C752B18u,
    0x8C752B1Au,0x8C74561Cu,0x8C74561Eu,0x8C745620u,0x8C745622u,0x8C745624u};
constexpr std::array<std::uint32_t,9> list_data{0x8C745628u,0x8C745648u,0x8C745848u,
    0x8C745A48u,0x8C745E48u,0x8C746648u,0x8C746848u,0x8C746C48u,0x8C747048u};
void setup_object(Fixture& f,unsigned kind){
    f.cpu.pc=0x8C0335D4u;f.cpu.gbr=0x8C8FFE00u;
    f.cpu.r[4]=contacts;f.cpu.r[5]=contacts+0x1000u;
    f.clear(0x8C73ED78u,0x10000u);f.clear(0x8C78C548u,32u);
    f.half(0x8C19B3A8u,0);f.half(0x8C752B18u,0);f.half(0x8C752B1Au,0);
    constexpr unsigned shapes[]{0u,1u,3u,4u};
    for(unsigned i=0;i<10;++i){
        const auto c=contacts+i*0x1000u,task=c+0x100u,work=c+0x200u,shape=c+0x300u;
        f.half(c,0);f.half(c+4,1);f.putf(c+8,64.f);f.put(c+12,shape);f.put(c+16,task);
        f.put(task+32,work);f.vector(work+44,1,1,1);
        const float x=i==0?0.f:(kind<32 && kind>=16?150.f:1.25f+float(i)*.25f);
        f.vector(work+32,x,.25f,.5f);f.vector(shape+8,.125f,.25f,-.375f);
        f.putf(shape+20,2.f);f.putf(shape+24,3.f);f.putf(shape+28,1.5f);
        const auto type=kind<32?shapes[i==0?(kind%16)/4:kind%4]:0u;
        f.put(shape,type);
    }
    if(kind>=32){
        f.cpu.pc=family::object_entry;
        if(kind==32)return; // empty lists
        if(kind==33){f.half(0x8C19B3A8u,1);return;} // original pause gate
        f.half(list_counts[0],2);f.put(list_data[0],contacts);f.put(list_data[0]+4,contacts+0x1000u);
        if(kind<43){const auto group=kind-34;f.half(list_counts[group],2);
            f.put(list_data[group],contacts+0x2000u);f.put(list_data[group]+4,contacts+0x3000u);}
        if(kind==43)for(unsigned g=1;g<list_counts.size();++g){f.half(list_counts[g],1);f.put(list_data[g],contacts+(g+1)*0x1000u);}
        if(kind==44){f.half(contacts,2);f.put(contacts+0x1300u,0x20000u);}
        if(kind==45){f.half(contacts+4,2);f.put(contacts+0x300u+52,0x10u);}
    }
}
void bind_aot(Fixture& f){
    sonic::scalar_writes::unbind(&f.cpu.memory,&f.immutable);
    f.cpu.memory.set_guest_write_observer({});f.cpu.memory.set_guest_write_batch_observer({});
}
}
int main(int argc,char** argv)try{
    require(argc==3 || (argc==4 && std::string(argv[3])=="--aot-only"),"object-contact-tests <RAM> <retained-code> [--aot-only]");
    require(family::object_enabled(),"SARECOMP_NATIVE_OBJECT_CONTACT=1 required");
    std::ifstream input(argv[1],std::ios::binary);const std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(input),{}};
    require(image.size()==0x1000000u,"RAM size");load_original_entries(argv[2]);unsigned cases=0;
    if(argc==3)for(unsigned mode:{0u,fpscr_fr_mask|1u})for(unsigned kind=0;kind<46;++kind){
        std::cout<<"object case="<<kind<<" mode="<<mode<<std::endl;
        Fixture a(image,mode),b(image,mode);setup_object(a,kind);setup_object(b,kind);a.oracle=&b;
        const auto outcome=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,Fixture::resume});
        b.until(a.cpu.pc);compare(a,b,"object complete operation");
        if(a.cpu.pc!=returned)std::cerr<<"pc="<<std::hex<<a.cpu.pc<<" tea="<<a.cpu.tea<<std::dec<<std::endl;
        require(outcome==family::Outcome::Complete && a.cpu.pc==returned && !a.cpu.trap_pending,"object original return");++cases;
    }
    for(unsigned kind:{0u,3u,5u,10u,15u,32u,43u,44u,45u}){
        std::cout<<"object AOT case="<<kind<<std::endl;
        Fixture a(image,0),b(image,0);setup_object(a,kind);setup_object(b,kind);active_fixture=&a;a.oracle=&b;
        bind_aot(a);NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,a.immutable);
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        for(unsigned n=0;a.cpu.pc!=returned && !a.cpu.trap_pending;++n){
            require(n<256u && original_owners.contains(a.cpu.pc),"object original sparse entry");
            require(resume(&a,a.cpu,original_owners.at(a.cpu.pc),returned),"actual object AOT body");
        }
        b.until(a.cpu.pc);compare(a,b,"actual object AOT body");
        require(a.cpu.pc==returned && !a.cpu.trap_pending,"object AOT return");++cases;
    }
    for(unsigned kind=0;kind<4;++kind){
        std::cout<<"object fallback case="<<kind<<std::endl;
        Fixture a(image,0),b(image,0);setup_object(a,0);setup_object(b,0);active_fixture=&a;a.oracle=&b;
        for(auto* f:{&a,&b}){
            if(kind==0)f->cpu.r[4]+=1u;
            if(kind==1)f->cpu.r[15]+=2u;
            if(kind==2)f->put(contacts+12,contacts+0x301u);
            if(kind==3)f->put(contacts+16,contacts+0x101u);
        }
        bind_aot(a);NativePortContext context;context.cpu=&a.cpu;context.host=&host;
        NativePortAotServices aot(context,original_entry,a.immutable);
        sonic::scalar_writes::bind(a.cpu.memory,a.immutable,a.cpu.memory.guest_write_observer_generation());
        katana_port_generated::runtime_dispatch_detail::active_services=&aot;
        const auto outcome=family::execute(a.cpu,&a.immutable,{&a,Fixture::invoke,resume});
        b.until(a.cpu.pc);compare(a,b,"object original fault continuation");
        require(outcome==family::Outcome::Interrupted && a.cpu.trap_pending,"object original fault");++cases;
    }
    std::cout<<"SONIC_OBJECT_CONTACT_PASS cases="<<cases<<" whole_pass="<<family::counts.object_calls
        <<" internal="<<family::counts.internal_calls<<" external="<<family::counts.callbacks<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_OBJECT_CONTACT_FAIL "<<e.what()<<'\n';return 1;}
