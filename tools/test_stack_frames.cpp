#include "sonic_read_test_fixture.hpp"
#include "sonic_stack_frames.hpp"

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
        for(std::uint32_t i=0;i<0x10000;i+=4)f.ram->write_u32(i,i^0xC04D359Eu);
        for(unsigned i=0;i<15;++i)f.cpu.r[i]=0x81234567u+i*0x01030507u;
        f.cpu.pr=0xBAFFE123;
    }
    ~Owned(){sonic::scalar_writes::unbind(&f.cpu.memory,&guard);}
};
unsigned cases=0,push_hits=0,pop_hits=0;

void mutate(Owned& o,unsigned mode) {
    auto& f=o.f;
    switch(mode) {
    case 1:(void)f.cpu.memory.add_watchpoint(0x0C000000,0x10000,MemoryWatchpointAccess::Write,
        [&f](const auto& e){f.access(9,e);});break;
    case 2:(void)f.cpu.memory.add_watchpoint(0x0C000000,0x10000,MemoryWatchpointAccess::Read,
        [&f](const auto& e){f.access(9,e);});break;
    case 3:f.cpu.memory.set_trace_handler([&f](const auto& e){f.access(9,e);});break;
    case 4:f.cpu.memory.clear_guest_write_observer();break;
    case 5:o.guard.reserve_additional_runtime_executable_ranges(1);
        o.guard.add_runtime_executable_range(0x0C000FEC,4);break;
    case 6:f.cpu.memory.clear_direct_linear_alias_window();break;
    case 7:f.cpu.write_sr(0);break;
    case 8:f.cpu.mmucr=1;break;
    case 9:f.cpu.active_block_size=0;break;
    case 10:f.cpu.pending_guest_cycles=UINT64_MAX-3;
        f.cpu.attempted_guest_instructions=UINT64_MAX-2;
        f.cpu.retired_guest_instructions=UINT64_MAX-2;break;
    case 11:
        (void)f.cpu.memory.add_watchpoint(0x0C000000,0x10000,MemoryWatchpointAccess::Read,
            [&f](const auto& e){f.access(9,e);});
        f.services.on_flush=[&f] {
            f.cpu.r[14]=0xACCE5514u;f.cpu.pr=0xACCE5516u;
            f.cpu.r[15]=0x8C002040u;
        };
        break;
    case 12:f.sink();break;
    }
}

// Independent instruction oracle, including the original function-entry guard
// and pre/post-instruction scheduler boundaries. Operands are reloaded after a
// flush because a scheduler callback can change CPU state.
template<bool Push,std::size_t N>
void original(Owned& o,const std::array<unsigned,N>& indices,const DirectLinearMemoryGuard& direct) {
    auto& cpu=o.f.cpu;
    for(std::size_t i=0;i<N;++i) {
        const auto pc=source_pc+std::uint32_t(i)*2;
        const auto index=indices[i];
        const auto preflight_address=cpu.r[15]-(Push?4u:0u);
        std::uint32_t preflight_direct=0,offset=0;
        const bool guarded=(!Push || cpu.memory.guest_write_observer_allows_prevalidated_linear_writes()) &&
            sonic::stack_frames::translate(cpu,preflight_address,preflight_direct) &&
            direct_linear_guard_offset(direct,preflight_direct,4,offset);
        if(!guarded)flush_pending_guest_cycles(cpu,o.f.services);
        const auto epoch=guarded?0:cpu.memory.mmio_boundary_epoch();
        auto& value=index==16?cpu.pr:cpu.r[index];
        const auto address=cpu.r[15]-(Push?4u:0u);
        const auto segment=address>>29u;
        bool linear=false;
        std::uint32_t translated=0;
        if(segment==4 || segment==5) {linear=cpu.privileged_mode_inline();translated=address;}
        else if(!(cpu.mmucr&1u) && segment<7 && (cpu.privileged_mode_inline() || address<0x80000000u)) {
            linear=true;translated=canonical_physical_address_inline(address)|0x80000000u;
        }
        ExplicitGuestInstructionAttempt attempt(cpu,pc,2);
        try {
            const GuestInstructionOrigin origin{pc,pc,true};
            if constexpr(Push) {
                const bool written=linear && cpu.memory.guest_write_observer_allows_prevalidated_linear_writes() &&
                    cpu.memory.try_write_direct_linear_u32(translated&0x1FFFFFFFu,value,CodeWriteSource::Cpu);
                if(!written)guest_write_u32_at(cpu,origin,address,value,CodeWriteSource::Cpu);
                cpu.r[15]=address;
            } else {
                std::uint32_t loaded=0;
                if(!linear || !direct_linear_guard_read_u32(direct,translated,loaded))
                    loaded=guest_read_u32_at(cpu,origin,address);
                value=loaded;cpu.r[15]=address+4;
            }
            attempt.complete();
        } catch(const MemoryAccessError& e) {
            const auto op=Push?(index==16?0x4F22u:0x2F06u|(index<<4)):
                (index==16?0x4F26u:0x60F6u|(index<<8));
            enter_memory_exception_with_provenance(cpu,e,pc,op);return;
        }
        if(o.guard.write_detected()) {cpu.pc=pc+2;return;}
        if(!guarded && cpu.memory.mmio_boundary_epoch()!=epoch) {
            cpu.pc=pc+2;
            if(finalize_guest_block(cpu,o.f.services,1024u,pc,0u,false,false,false).interrupt)return;
        }
    }
}

template<bool Push,unsigned... Indices>
void compare(std::uint32_t sp,unsigned mode,bool stale) {
    Owned a,b;
    a.f.cpu.r[15]=b.f.cpu.r[15]=sp;
    auto control_read=a.f.cpu.memory.direct_linear_memory_guard(false);
    auto read=b.f.cpu.memory.direct_linear_memory_guard(false);
    mutate(a,mode);mutate(b,mode);
    if(!stale) {
        control_read=a.f.cpu.memory.direct_linear_memory_guard(false);
        read=b.f.cpu.memory.direct_linear_memory_guard(false);
    }
    const auto before=state(b.f.cpu);
    const auto before_counts=counts(b.f.cpu.memory);
    bool fast=false;
    {
        NativeAotRegisterFile<0xFFFFu,0x02u> registers(b.f.cpu);
        constexpr auto last=source_pc+(sizeof...(Indices)-1)*2;
        if constexpr(Push)fast=sonic::stack_frames::push<Indices...>(b.f.cpu,registers,read,&b.guard,last);
        else fast=sonic::stack_frames::pop<Indices...>(b.f.cpu,registers,read,last);
    }
    if(!fast) {
        require(before==state(b.f.cpu),"rejected group changed CPU state");
        require(before_counts==counts(b.f.cpu.memory),"rejected group changed counters");
        require(std::ranges::equal(a.f.ram->bytes(),b.f.ram->bytes()),"rejected group wrote bytes");
        original<Push>(b,std::array<unsigned,sizeof...(Indices)>{Indices...},read);
    } else {if constexpr(Push)++push_hits;else ++pop_hits;}
    original<Push>(a,std::array<unsigned,sizeof...(Indices)>{Indices...},control_read);
    require(state(a.f.cpu)==state(b.f.cpu),"architectural state differs");
    require(provenance(a.f.cpu)==provenance(b.f.cpu),"fault provenance differs");
    require(counts(a.f.cpu.memory)==counts(b.f.cpu.memory),"memory accounting differs");
    require(a.f.log==b.f.log&&!a.f.log.overflow,"observations differ");
    require(std::ranges::equal(a.f.ram->bytes(),b.f.ram->bytes()),"RAM differs");
    require(a.guard.generation()==b.guard.generation() && a.guard.write_detected()==b.guard.write_detected(),
        "immutable ownership differs");
    if(mode==11) {
        require(!fast,"scheduler-sensitive group admitted");
        require(!a.f.services.on_flush && !b.f.services.on_flush,"scheduler callback not executed");
        require(a.f.log.entries[0].kind==1 && a.f.log.entries[0].data[0]==7,"initial cycles not flushed");
        require(a.f.cpu.r[15]==0x8C002040u+(Push?-int(sizeof...(Indices)*4):int(sizeof...(Indices)*4)),
            "scheduler-modified stack pointer not used");
    }
    ++cases;
}
}
int main() {
    try {
        require(sonic::scalar_writes::stack_frames_enabled(),"set SARECOMP_STACK_FRAMES=1 for this test");
        for(unsigned mode=0;mode<=12;++mode)for(bool stale:{false,true})
            for(auto sp:{0x8C001000u,0xAC001000u,0x0C001000u,0x8C011000u,0x8C00000Cu,
                         0x8C01000Cu,0x8C000810u,0x8C000A10u,0x8C001001u,0xE0000000u,
                         0x8C020000u,0xFFFFFFFFu}) {
                compare<true,14,13,12,11,10,9,8,16>(sp,mode,stale);
                compare<false,16,8,9,10,11,12,13,14>(sp,mode,stale);
                compare<true,14,16>(sp,mode,stale);
                compare<false,16,14>(sp,mode,stale);
                compare<true,14,13,16>(sp,mode,stale);
                compare<false,16,13,14>(sp,mode,stale);
            }
        compare<true,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,16>(0x8C001044,0,false);
        compare<false,16,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14>(0x8C001004,0,false);
        sonic::diagnostics::internal_runtime_enabled=true;
        const auto previous_push=push_hits,previous_pop=pop_hits;
        compare<true,14,16>(0x8C001000,0,false);
        compare<false,16,14>(0x8C001000,0,false);
        require(push_hits==previous_push && pop_hits==previous_pop,"diagnostics did not disable groups");
        sonic::diagnostics::internal_runtime_enabled=false;
        require(push_hits&&pop_hits,"no fast groups executed");
        require(!sonic::scalar_writes::requested_capture,"capture scope leaked");
        for(const auto& b:sonic::scalar_writes::bindings)require(!b.memory,"binding leaked");
        std::cout<<"SONIC_STACK_FRAMES_OK cases="<<cases<<" push_hits="<<push_hits<<" pop_hits="<<pop_hits
            <<" registers=exact ram=exact counters=exact faults=exact observers=exact scheduler=exact\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"case="<<cases<<" "<<e.what()<<'\n';return 1;}
}
