#pragma once
#include "sonic_native_collision_memory.hpp"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#else
#include "katana/runtime/native_port_content.hpp"
#endif
#include <stdexcept>

namespace collision_test {
inline unsigned mode() {
    const char* p=std::getenv("SARECOMP_COLLISION_TEST_PRODUCT");
    return p && p[0]=='1'?1u:p && p[0]=='2'?2u:0u;
}
// Mode 0 keeps every original arbitrary observer event. Mode 1 uses precisely
// the authenticated product observer. Mode 2 revokes that binding before entry
// and must retain the ordered public-memory path. Registration is scoped to
// one comparison, never the fixture address or an earlier Memory lifetime.
class Comparison {
public:
    template<class Fixture> Comparison(Fixture& native,Fixture& reference)
        :memory_(native.cpu.memory),immutable_(native.immutable),before_(sonic::collision_memory::counts) {
        sonic::diagnostics::internal_runtime_enabled=false;
        native.observe();reference.observe();
        if(!mode())return;
        using namespace katana::runtime;
        memory_.set_guest_write_observer([guard=&immutable_](const GuestWriteEvent& e)noexcept{guard->observe_write(e);},
            GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        memory_.set_guest_write_batch_observer({&immutable_,
            [](void*,std::span<const GuestWriteEvent>)noexcept{return true;},
            [](void* p,std::span<const GuestWriteEvent> events)noexcept{
                for(auto e:events)static_cast<NativePortImmutableWriteGuard*>(p)->observe_write(e);}});
        sonic::scalar_writes::bind(memory_,immutable_,memory_.guest_write_observer_generation());
        if(mode()==2u)native.observe();
    }
    ~Comparison(){sonic::scalar_writes::unbind(&memory_,&immutable_);}
    bool product()const{return mode()==1u;}
    void verify(bool writable=true,bool closed_owner=false)const {
        const auto after=sonic::collision_memory::counts;
        if(after.active!=before_.active)throw std::runtime_error("collision capability escaped owner/call");
        const bool expected=product() && ((writable && sonic::collision_memory::enabled()) ||
            (closed_owner && sonic::collision_memory::closure_enabled()));
        if((after.intervals>before_.intervals)!=expected)
            throw std::runtime_error("collision direct/fallback evidence differs");
        if(expected && (after.reads<=before_.reads || (writable && after.writes<=before_.writes)))
            throw std::runtime_error("collision direct interval did no actual accesses");
    }
private:
    katana::runtime::Memory& memory_;
    katana::runtime::NativePortImmutableWriteGuard& immutable_;
    sonic::collision_memory::Counts before_;
};
inline void verify_fusion(){
    const auto& c=sonic::collision_memory::counts;
    const bool expected=mode()==1u && sonic::collision_memory::closure_enabled();
    if(expected ? (!c.fused_cross || !c.fused_length || !c.fused_normalize) :
       (c.fused_cross || c.fused_length || c.fused_normalize))
        throw std::runtime_error("closed-child direct/fallback coverage differs");
}
inline void bridge_boundary(){
    if(sonic::collision_memory::counts.active)throw std::runtime_error("live collision capability at retained bridge");
}
}
