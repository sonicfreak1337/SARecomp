#include "sonic_read_test_fixture.hpp"
#include "sonic_scalar_write_view.hpp"

namespace {
struct Owned {
    Fixture f;
    std::array<NativePortImmutableRange,2> ranges{{
        {0x0C000800u,16u,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
        {0x0C000A00u,16u,native_port_immutable_range_mask(NativePortImmutableRangeKind::ReadOnlyImage)}}};
    NativePortImmutableWriteGuard guard{ranges};
    Owned() {
        f.cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e) noexcept {guard.observe_write(e);},
            GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        f.cpu.memory.set_guest_write_batch_observer({&guard,
            [](void*,std::span<const GuestWriteEvent>) noexcept{return true;},
            [](void* p,std::span<const GuestWriteEvent> events) noexcept {
                for(const auto& e:events)static_cast<NativePortImmutableWriteGuard*>(p)->observe_write(e);
            }});
        sonic::scalar_writes::bind(f.cpu.memory,guard,f.cpu.memory.guest_write_observer_generation());
    }
    ~Owned(){sonic::scalar_writes::unbind(&f.cpu.memory,&guard);}
};
unsigned cases=0, fast_hits=0;
void real_ram_mirrors() {
    Memory memory{0u};
    auto ram=std::make_shared<LinearMemoryDevice>(0x1000000u);
    for(unsigned mirror=0;mirror<4;++mirror)
        memory.map_region("mirror"+std::to_string(mirror),0x0C000000u+mirror*0x1000000u,ram);
    memory.bind_direct_linear_alias_window(0x0C000000u,0x4000000u,*ram);
    memory.set_lookup_mode(MemoryLookupMode::Indexed);
    const std::array<NativePortImmutableRange,1> ranges{{{0x0C000800u,16u,
        native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)}}};
    NativePortImmutableWriteGuard guard{ranges};
    memory.set_guest_write_observer([&](const GuestWriteEvent& e) noexcept {guard.observe_write(e);},
        GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
    memory.set_guest_write_batch_observer({&guard,
        [](void*,std::span<const GuestWriteEvent>) noexcept{return true;},
        [](void* p,std::span<const GuestWriteEvent> events) noexcept {
            for(const auto& e:events)static_cast<NativePortImmutableWriteGuard*>(p)->observe_write(e);
        }});
    require(!sonic::scalar_writes::View(memory,&guard).available(),"unregistered observer admitted");
    sonic::scalar_writes::bind(memory,guard,memory.guest_write_observer_generation());
    const sonic::scalar_writes::View view(memory,&guard);
    require(view.available(),"native RAM view unavailable");
    for(auto segment:{0x80000000u,0xA0000000u})for(unsigned mirror=0;mirror<4;++mirror) {
        const auto base=segment|0x0C000000u|mirror*0x1000000u;
        require(view.try_write(base+0x100u,std::uint32_t{mirror+1}),"data mirror not written");
        require(ram->read_u32(0x100u)==mirror+1,"data mirror resolves to wrong backing");
        require(!view.try_write(base+0x800u,std::uint32_t{0xAABBCCDD}),"executable mirror bypassed guard");
    }
    guard.reserve_additional_runtime_executable_ranges(1);
    guard.add_runtime_executable_range(0x0C000100u,4);
    require(!view.try_write(0xAF000100u,std::uint32_t{0}),"new runtime code bypassed guard");
    guard.remove_runtime_executable_range(0x0C000100u,4);
    require(view.try_write(0xAF000100u,std::uint32_t{0}),"retired runtime range not released");
    sonic::diagnostics::internal_runtime_enabled=true;
    require(!sonic::scalar_writes::View(memory,&guard).available(),"diagnostic capture not disabled");
    sonic::diagnostics::internal_runtime_enabled=false;
    memory.clear_guest_write_observer();
    require(!view.try_write(0x8C000100u,std::uint32_t{1}),"retired observer capability stayed valid");
    sonic::scalar_writes::unbind(&memory,&guard);
}
void mutate(Owned& o,unsigned mode) {
    auto& f=o.f;
    switch(mode) {
    case 0:break;
    case 1:f.trace();break;
    case 2:f.watch(MemoryWatchpointAccess::Write);break;
    case 3:f.watch(MemoryWatchpointAccess::Read);break;
    case 4:f.sink();break;
    case 5:f.cpu.memory.set_lookup_mode(MemoryLookupMode::Reference);break;
    case 6:f.cpu.memory.clear_direct_linear_alias_window();break;
    case 7:f.cpu.memory.clear_guest_write_observer();break;
    case 8:f.cpu.memory.set_guest_write_observer([&f](const GuestWriteEvent& e){
        f.log.add({9,{e.address,e.size,static_cast<unsigned>(e.source),e.bytes_changed}});
    },GuestWriteObserverContract::StableForPrevalidatedLinearWrites);break;
    case 9:f.cpu.memory.clear_guest_write_batch_observer();break;
    case 10:o.guard.reserve_additional_runtime_executable_ranges(1);o.guard.add_runtime_executable_range(0x0C000100u,16);break;
    case 11:o.guard.reserve_additional_runtime_executable_ranges(1);o.guard.add_runtime_executable_range(0x0C000100u,16);
        o.guard.remove_runtime_executable_range(0x0C000100u,16);break;
    }
}
template<class T>
void execute(Owned& o,const sonic::scalar_writes::View* view,std::uint32_t address,T value) {
    auto& cpu=o.f.cpu;
    const auto read_guard=cpu.memory.direct_linear_memory_guard(false);
    std::uint32_t offset=0;
    const bool admitted=cpu.memory.guest_write_observer_allows_prevalidated_linear_writes() &&
        direct_linear_guard_offset(read_guard,address,sizeof(T),offset);
    ExplicitGuestInstructionAttempt attempt(cpu,source_pc,2);
    try {
        bool written=admitted && view && view->try_write(address,value);
        if(written)++fast_hits;
        if(!written && admitted) {
            if constexpr(sizeof(T)==1)written=cpu.memory.try_write_direct_linear_u8(address&0x1FFFFFFFu,value);
            if constexpr(sizeof(T)==2)written=cpu.memory.try_write_direct_linear_u16(address&0x1FFFFFFFu,value);
            if constexpr(sizeof(T)==4)written=cpu.memory.try_write_direct_linear_u32(address&0x1FFFFFFFu,value);
        }
        if(!written) {
            const GuestInstructionOrigin origin{source_pc,source_pc,true};
            if constexpr(sizeof(T)==1)guest_write_u8_at(cpu,origin,address,value,CodeWriteSource::Cpu);
            if constexpr(sizeof(T)==2)guest_write_u16_at(cpu,origin,address,value,CodeWriteSource::Cpu);
            if constexpr(sizeof(T)==4)guest_write_u32_at(cpu,origin,address,value,CodeWriteSource::Cpu);
        }
        attempt.complete();
    } catch(const MemoryAccessError& e){enter_memory_exception_with_provenance(cpu,e,source_pc,0x2242);}
}
template<class T>
void compare(unsigned mutation,bool refresh,std::uint32_t address) {
    Owned a,b;
    sonic::scalar_writes::View view(b.f.cpu.memory,&b.guard);
    require(view.available(),"initial owned view unavailable");
    require(!b.f.cpu.memory.direct_linear_memory_guard(true),"capture capability leaked into public API");
    mutate(a,mutation);mutate(b,mutation);
    if(refresh)view=sonic::scalar_writes::View(b.f.cpu.memory,&b.guard);
    for(unsigned repeat=0;repeat<2;++repeat) {
        execute<T>(a,nullptr,address,T(0x12345678));execute<T>(b,&view,address,T(0x12345678));
        require(state(a.f.cpu)==state(b.f.cpu),"CPU/cycle/fault state differs");
        require(provenance(a.f.cpu)==provenance(b.f.cpu),"fault provenance differs");
        require(counts(a.f.cpu.memory)==counts(b.f.cpu.memory),"memory counters differ");
        require(a.f.log==b.f.log&&!a.f.log.overflow,"observer events differ");
        require(std::ranges::equal(a.f.ram->bytes(),b.f.ram->bytes()),"written bytes differ");
        require(a.guard.generation()==b.guard.generation()&&a.guard.write_detected()==b.guard.write_detected()&&
            a.guard.first_write_address()==b.guard.first_write_address()&&
            a.guard.first_write_size()==b.guard.first_write_size()&&
            a.guard.first_write_kind_mask()==b.guard.first_write_kind_mask(),"immutable write result differs");
        if(a.f.cpu.trap_pending)break;
    }
    ++cases;
}
}
int main() {
    try {
        for(unsigned mutation=0;mutation<12;++mutation)for(bool refresh:{false,true})
            for(auto address:{0x8C000100u,0xAC000100u,0x8C010100u,0x8C000101u,0x8C00FFFEu,
                              0x8C020000u,0x0C000100u,0xFFFFFFFFu,0x8C000800u,0xAC000800u,0x8C000A00u}){
                compare<std::uint8_t>(mutation,refresh,address);
                compare<std::uint16_t>(mutation,refresh,address);
                compare<std::uint32_t>(mutation,refresh,address);
            }
        require(fast_hits>0,"no actual fast stores tested");
        real_ram_mirrors();
        require(!sonic::scalar_writes::requested_capture,"capture scope leaked");
        for(const auto& binding:sonic::scalar_writes::bindings)require(!binding.memory,"binding lifetime leaked");
        std::cout<<"SONIC_SCALAR_WRITES_OK cases="<<cases<<" fast_hits="<<fast_hits
            <<" bytes=exact counters=exact observers=exact faults=exact immutable=exact\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
