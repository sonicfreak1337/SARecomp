#include "sonic_read_test_fixture.hpp"
#include "sonic_write_observer_guard.hpp"

namespace {
unsigned cases=0;
void observer(Fixture& f, unsigned mode) {
    if(!mode) { f.cpu.memory.clear_guest_write_observer(); return; }
    f.cpu.memory.set_guest_write_observer([&f](const GuestWriteEvent& e) {
        f.log.add({9,{e.address,e.size,static_cast<unsigned>(e.source),e.bytes_changed}});
    },mode==1?GuestWriteObserverContract::StableForPrevalidatedLinearWrites:
               GuestWriteObserverContract::General);
}
void mutate(Fixture& f,unsigned mode) {
    switch(mode) {
    case 0: break;
    case 1: observer(f,0);break;
    case 2: observer(f,1);break;
    case 3: observer(f,2);break;
    case 4: f.trace();break;
    case 5: f.watch(MemoryWatchpointAccess::ReadWrite);break;
    case 6: f.sink();break;
    case 7: f.cpu.memory.set_lookup_mode(MemoryLookupMode::Reference);break;
    case 8: f.cpu.memory.clear_direct_linear_alias_window();break;
    case 9: f.cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x20000u,*f.ram);break;
    case 10: f.cpu.memory.clear_guest_write_batch_observer();break;
    }
}
template<class T,class Guard>
bool execute(Fixture& f,const Guard& guard,std::uint32_t address) {
    auto& cpu=f.cpu;
    std::uint32_t offset=0;
    bool allows;
    if constexpr(requires { guard.permits_observed_writes; }) allows=guard.permits_observed_writes;
    else allows=cpu.memory.guest_write_observer_allows_prevalidated_linear_writes();
    const bool admitted=allows && direct_linear_guard_offset(guard,address,sizeof(T),offset);
    ExplicitGuestInstructionAttempt attempt(cpu,source_pc,2);
    try {
        const auto value=T(0x12345678u);
        bool written=false;
        if(admitted) {
            if constexpr(sizeof(T)==1) written=cpu.memory.try_write_direct_linear_u8(address&0x1FFFFFFF,value,CodeWriteSource::Cpu);
            if constexpr(sizeof(T)==2) written=cpu.memory.try_write_direct_linear_u16(address&0x1FFFFFFF,value,CodeWriteSource::Cpu);
            if constexpr(sizeof(T)==4) written=cpu.memory.try_write_direct_linear_u32(address&0x1FFFFFFF,value,CodeWriteSource::Cpu);
        }
        if(!written) {
            const GuestInstructionOrigin origin{source_pc,source_pc,true};
            if constexpr(sizeof(T)==1) guest_write_u8_at(cpu,origin,address,value,CodeWriteSource::Cpu);
            if constexpr(sizeof(T)==2) guest_write_u16_at(cpu,origin,address,value,CodeWriteSource::Cpu);
            if constexpr(sizeof(T)==4) guest_write_u32_at(cpu,origin,address,value,CodeWriteSource::Cpu);
        }
        attempt.complete();
    } catch(const MemoryAccessError& e) {
        enter_memory_exception_with_provenance(cpu,e,source_pc,0x2242);
    }
    return admitted;
}
template<class T>
void compare(unsigned initial,unsigned mutation,bool refresh,std::uint32_t address) {
    Fixture a,b;
    observer(a,initial);observer(b,initial);
    auto old=a.cpu.memory.direct_linear_memory_guard(false);
    auto now=sonic::write_observer::capture(b.cpu.memory);
    mutate(a,mutation);mutate(b,mutation);
    if(refresh) {old=a.cpu.memory.direct_linear_memory_guard(false);now=sonic::write_observer::capture(b.cpu.memory);}
    const bool accepted=execute<T>(a,old,address);
    require(execute<T>(b,now,address)==accepted,"admission differs");
    if(!a.cpu.trap_pending && !b.cpu.trap_pending) {
        const bool repeated=execute<T>(a,old,address);
        require(execute<T>(b,now,address)==repeated,"repeat admission differs");
    }
    require(state(a.cpu)==state(b.cpu),"CPU/cycle/fault state differs");
    require(provenance(a.cpu)==provenance(b.cpu),"fault provenance differs");
    require(counts(a.cpu.memory)==counts(b.cpu.memory),"memory accounting differs");
    require(a.log==b.log && !a.log.overflow,"observer order/value differs");
    require(std::ranges::equal(a.ram->bytes(),b.ram->bytes()),"written bytes differ");
    ++cases;
}
}
int main() {
    try {
        for(unsigned initial=0;initial<3;++initial)for(unsigned mutation=0;mutation<11;++mutation)
            for(bool refresh:{false,true})
                for(auto address:{0x8C000100u,0xAC000100u,0x8C010100u,0x8C000101u,0x8C00FFFEu,0x8C020000u,0x0C000100u,0xFFFFFFFFu}) {
                    compare<std::uint8_t>(initial,mutation,refresh,address);
                    compare<std::uint16_t>(initial,mutation,refresh,address);
                    compare<std::uint32_t>(initial,mutation,refresh,address);
                }
        std::cout<<"SONIC_WRITE_OBSERVER_GUARD_OK cases="<<cases<<" admission=exact writes=exact observers=exact faults=exact\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
