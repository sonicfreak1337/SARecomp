#include "sonic_mesh_plan.hpp"
#include "sonic_model_uv.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <xmmintrin.h>
using namespace sonic::geometry;
using namespace katana::runtime;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct Fixture {
    static constexpr std::uint32_t base=0x8C000000u,stream=base+0x1000u,uv=base+0x2000u;
    std::array<std::uint8_t,0x10000u> ram{};
    bool readable=true;
    CpuState cpu{.memory=Memory{0u}};
    MeshPlanRequest request{base+0x300u,base+0x400u,16u,65536u};
    Fixture(unsigned type=2,unsigned polygons=2){
        cpu.write_fpscr(fpscr_dn_mask);
        put16(request.descriptor.data(),std::uint16_t((type<<14)|7u));
        put16(request.descriptor.data()+2,std::uint16_t(polygons));
        put32(request.descriptor.data()+4,stream);put32(request.descriptor.data()+20,uv);
        const std::array<std::uint16_t,12> strip{5,9,4,2,6,8,0x8005,1,3,5,7,9};
        for(unsigned i=0;i<strip.size();++i)put16(ram.data()+0x1000+i*2,strip[i]);
        for(unsigned i=0;i<10;++i){put16(ram.data()+0x2000+i*4,std::uint16_t(int(i)*255-510));put16(ram.data()+0x2002+i*4,std::uint16_t(32767-int(i)*257));}
    }
    static void put16(std::uint8_t* p,std::uint16_t v){p[0]=std::uint8_t(v);p[1]=std::uint8_t(v>>8);}
    static void put32(std::uint8_t* p,std::uint32_t v){for(unsigned i=0;i<4;++i)p[i]=std::uint8_t(v>>(i*8));}
    MeshPlanReader reader(){return {this,[](const void* context,std::uint32_t a,std::size_t n) noexcept ->std::span<const std::uint8_t>{
        const auto& f=*static_cast<const Fixture*>(context);
        if(!f.readable || a<base || std::uint64_t(a-base)+n>f.ram.size())return {};
        return {f.ram.data()+(a-base),n};
    }};}
};
void triangle(const MeshPlan& p,std::size_t i,std::array<std::uint16_t,3> points,std::array<std::uint32_t,3> corners){
    require(i<p.triangles.size() && p.triangles[i].points==points && p.triangles[i].corners==corners,"authored triangle order/seam");
}
int main()try{
    unsigned cases=0;
    {
        Fixture f;MeshPlanCache cache;
        const auto original=f.ram;const auto fpscr=f.cpu.read_fpscr(),host=_mm_getcsr();
        const auto* p=cache.get(f.request,f.reader(),f.cpu);require(p && p->triangles.size()==6 && p->polygons.size()==2,"two strips");
        triangle(*p,0,{9,4,2},{0,1,2});triangle(*p,1,{2,4,6},{2,1,3});triangle(*p,2,{2,6,8},{2,3,4});
        triangle(*p,3,{3,1,5},{6,5,7});triangle(*p,4,{3,5,7},{6,7,8});triangle(*p,5,{7,5,9},{8,7,9});
        require(p->polygons[1].first_triangle==3 && p->polygons[1].first_corner==5 && p->corner_count==10,"polygon boundaries");
        for(unsigned i=0;i<10;++i){const auto expected=sonic::model_uv::decode(std::int16_t(int(i)*255-510),std::int16_t(32767-int(i)*257),true,f.cpu);
            require(!std::memcmp(expected.data(),p->uvs[i].data(),sizeof(expected)),"TitleBasic UV bits");}
        const auto* hit=cache.get(f.request,f.reader(),f.cpu);require(hit==p && cache.stats.hits==1,"exact source hit");
        require(original==f.ram && fpscr==f.cpu.read_fpscr() && host==_mm_getcsr(),"cache changed guest/host state");++cases;
        Fixture::put16(f.ram.data()+0x1002,10);p=cache.get(f.request,f.reader(),f.cpu);
        require(p && p->triangles[0].points[0]==10 && cache.stats.changes==1,"topology mutation invalidation");++cases;
        Fixture::put16(f.ram.data()+0x2000,255);p=cache.get(f.request,f.reader(),f.cpu);
        require(p && p->uvs[0][0]==sonic::model_uv::decode(255,0,true,f.cpu)[0] && cache.stats.changes==2,"UV mutation invalidation");++cases;
        f.readable=false;require(!cache.get(f.request,f.reader(),f.cpu),"stale/unavailable reader");++cases;
        f.readable=true;require(cache.get(f.request,f.reader(),f.cpu),"same-address source reload");++cases;
        Fixture::put16(f.request.descriptor.data()+2,1);p=cache.get(f.request,f.reader(),f.cpu);require(p && p->polygons.size()==1,"fresh descriptor");++cases;
    }
    for(unsigned type:{0u,1u,3u}){
        Fixture f(type,1);MeshPlanCache cache;
        if(type<2u){for(unsigned i=0;i<4;++i)Fixture::put16(f.ram.data()+0x1000+i*2,std::uint16_t(i==3?2:i));}
        const auto* p=cache.get(f.request,f.reader(),f.cpu);require(p,"triangle/quad/type3 admission");
        if(type==0u){require(p->triangles.size()==1,"triangle count");triangle(*p,0,{0,1,2},{0,1,2});}
        if(type==1u){require(p->triangles.size()==2,"quad count");triangle(*p,1,{2,1,2},{2,1,3});}
        if(type==3u){require(p->triangles.size()==3,"type3 strips");triangle(*p,1,{2,4,6},{2,1,3});}
        ++cases;
    }
    for(unsigned variant=0;variant<9;++variant){
        Fixture f;MeshPlanCache cache;
        if(variant==0)Fixture::put16(f.ram.data()+0x1000,2);
        if(variant==1)Fixture::put16(f.ram.data()+0x1002,16);
        if(variant==2)f.request.corner_budget=4;
        if(variant==3)Fixture::put32(f.request.descriptor.data()+4,0xFFFFFFFEu);
        if(variant==4)Fixture::put32(f.request.descriptor.data()+20,0xFFFFFFFCu);
        if(variant==5)f.request.point_count=0;
        if(variant==6)Fixture::put16(f.request.descriptor.data()+2,65535);
        if(variant==7)f.request.corner_budget=65537;
        if(variant==8)Fixture::put16(f.request.descriptor.data()+2,0);
        require(!cache.get(f.request,f.reader(),f.cpu),"invalid plan accepted");++cases;
    }
    for(unsigned mode=0;mode<4;++mode){
        Fixture f;MeshPlanCache cache;
        struct Restore{unsigned value=_mm_getcsr();~Restore(){_mm_setcsr(value);}} restore;
        _mm_setcsr(0x1F80u|(mode<<13u)|((mode&1u)?0x8040u:0u)|0x21u);
        f.cpu.write_fpscr(fpscr_dn_mask|(mode&1u));
        const auto before=_mm_getcsr();require(cache.get(f.request,f.reader(),f.cpu),"rounding build");
        require(cache.get(f.request,f.reader(),f.cpu) && cache.stats.hits==1,"rounding hit");
        require(_mm_getcsr()==before,"host state restoration");
        f.cpu.write_fpscr(f.cpu.read_fpscr()^1u);require(cache.get(f.request,f.reader(),f.cpu) && cache.stats.changes==1,"guest rounding key");
        _mm_setcsr(before^0x2000u);require(cache.get(f.request,f.reader(),f.cpu) && cache.stats.changes==2,"ambient rounding key");
        _mm_setcsr((before&~0x20u)&~0x1000u);require(!cache.get(f.request,f.reader(),f.cpu),"unmasked host FP must retain original");
        ++cases;
    }
    {
        Fixture f;MeshPlanCache cache(16u*1024u*1024u,1);
        require(cache.get(f.request,f.reader(),f.cpu),"first cache entry");++f.request.mesh;
        require(cache.get(f.request,f.reader(),f.cpu) && cache.stats.evictions==1,"bounded eviction");
        MeshPlanCache moved=std::move(cache);require(moved.get(f.request,f.reader(),f.cpu) && moved.stats.hits==1,"move retains age iterators");
        require(cache.get(f.request,f.reader(),f.cpu),"moved-from reuse accounting");
        moved=std::move(cache);require(moved.get(f.request,f.reader(),f.cpu),"move assignment");++cases;
        MeshPlanCache tiny(1,1);require(!tiny.get(f.request,f.reader(),f.cpu),"byte budget fallback");++cases;
        Fixture::put32(f.request.descriptor.data()+20,0);const auto* p=moved.get(f.request,f.reader(),f.cpu);
        require(p && p->uvs.empty() && p->uv_bytes.empty(),"untextured plan");++cases;
    }
    {
        struct Owner {MeshPlanCache cache;};Owner state;Fixture f;
        require(state.cache.get(f.request,f.reader(),f.cpu),"owner cache admission");
        state={};require(state.cache.stats.hits==0 && state.cache.stats.builds==0,"title-owner reset");++cases;
    }
    std::cout<<"SONIC_MESH_PLAN_TEST_OK cases="<<cases<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"SONIC_MESH_PLAN_TEST_FAILED "<<e.what()<<'\n';return 1;}
