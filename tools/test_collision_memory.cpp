#include "test_collision_memory_support.hpp"
#include <iostream>
#include <tuple>
using namespace katana::runtime;
namespace cm=sonic::collision_memory;
namespace {
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
auto metrics(const Memory& m){const auto& c=m.performance_counters();return std::tuple(
    c.indexed_region_hits,c.reference_region_probes,c.observed_accesses,c.unobserved_accesses);}
struct Fixture {
    CpuState cpu{.memory=Memory{0u}};
    std::shared_ptr<LinearMemoryDevice> ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    std::array<NativePortImmutableRange,1> ranges{{{0x0C010000u,4096u,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)}}};
    NativePortImmutableWriteGuard immutable{ranges};
    Fixture(){cpu.memory.map_region("ram",0x0C000000u,ram,MemoryRegionAccess::ReadWrite);
        cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x1000000u,*ram);}
    void observe(){cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e)noexcept{
        immutable.observe_write(e);},GuestWriteObserverContract::StableForPrevalidatedLinearWrites);}
};
template<class T> void accesses(Fixture& f,cm::Access& a,std::uint32_t address){
    const T first=T(0xCAFE1234u),second=T(0xBABE5678u);T result=0;
    require(a.try_store(address,first) && a.try_read(address,result) && result==first,"initial store/read");
    require(a.try_store(address|0x20000000u,second) && a.try_read(address&0x1FFFFFFFu,result) && result==second,"alias store/read");
    T bytes=0;std::memcpy(&bytes,f.ram->bytes().data()+(address&0xFFFFFFu),sizeof(T));
    require(bytes==second,"stores must be immediate");
}
}
int main()try{
    sonic::diagnostics::internal_runtime_enabled=false;
    require(cm::enabled(),"set SARECOMP_NATIVE_COLLISION_MEMORY=1");
    unsigned cases=0;
    for(unsigned variant=0;variant<5u;++variant){
        Fixture n,r;collision_test::Comparison comparison(n,r);
        // This standalone test always starts with the actual product binding.
        require(comparison.product(),"set SARECOMP_COLLISION_TEST_PRODUCT=1");
        auto g=n.cpu.memory.direct_linear_memory_guard(false);
        cm::Access a;a.capture(n.cpu,n.immutable,g);require(a.direct(),"product capture rejected");
        const auto before=metrics(n.cpu.memory);
        for(auto address:{0x8C020000u,0x8CFFFFFCu}){
            accesses<std::uint8_t>(n,a,address);accesses<std::uint16_t>(n,a,address);accesses<std::uint32_t>(n,a,address);cases+=3;
        }
        a.reset();collision_test::bridge_boundary();
        const auto after=metrics(n.cpu.memory);
        require(std::get<0>(after)==std::get<0>(before)+24u && std::get<3>(after)==std::get<3>(before)+24u &&
            std::get<1>(after)==std::get<1>(before) && std::get<2>(after)==std::get<2>(before),"exact batched counters");
        if(variant==0u)n.observe(); // observer generation changed
        else if(variant==1u)n.cpu.memory.clear_direct_linear_alias_window();
        else if(variant==2u)sonic::scalar_writes::unbind(&n.cpu.memory,&n.immutable);
        else if(variant==3u)sonic::diagnostics::internal_runtime_enabled=true;
        else {g={};} // caller discarded its old read capability
        a.capture(n.cpu,n.immutable,g);require(!a.direct(),"revoked capability admitted");
        std::uint32_t value=0x12345678u;
        require(!a.try_store(0x8C020000u,value) && !a.try_read(0x8C020000u,value) && value==0x12345678u,"decline changed operands");
        require(after==metrics(n.cpu.memory),"decline changed counters");++cases;
        sonic::diagnostics::internal_runtime_enabled=false;
    }
    require(!cm::counts.active,"escaped capability");
    std::cout<<"collision-memory: PASS "<<cases<<" alias/width/counter/revocation cases\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
