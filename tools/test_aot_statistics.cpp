#include "sonic_read_test_fixture.hpp"
#include "sonic_aot_statistics.hpp"

namespace {
unsigned cases=0;
template<class Attempt>
void instruction(Fixture& f,unsigned operation) {
    auto& c=f.cpu;
    Attempt attempt(c,f.runtime_pc,2);
    switch(operation) {
    case 0: c.r[4]=c.r[3]+7;break;
    case 1: ++c.exception_generation;break;
    case 2: raise_fpu_disabled(c,f.runtime_pc);break;
    case 3: try {c.fr[4]=guest_read_u32_at(c,{source_pc,f.runtime_pc,true},0x8C020000u);}
        catch(const MemoryAccessError& e) {enter_memory_exception_with_provenance(c,e,f.runtime_pc,0xF438);return;}break;
    case 4: throw std::runtime_error("unwind");
    }
    require(attempt.exception_generation_on_entry()==2,"entry generation");
    attempt.complete();attempt.complete();
}
void compare_instruction(bool enabled,unsigned operation,unsigned mapping) {
    sonic::diagnostics::internal_runtime_enabled=enabled;
#if defined(SARECOMP_AOT_STATISTICS_OMIT)
    enabled=false;
#endif
    Fixture original,candidate;
    for(auto* f:{&original,&candidate}) {
        if(mapping==1) {f->runtime_pc=0xAC02947C;f->cpu.active_block_virtual_start=0xAC029400;}
        if(mapping==2) f->cpu.active_block_size=0;
        if(mapping==3) {f->runtime_pc=0x8C099100;f->cpu.active_block_physical_start=0x0C029500;}
    }
    try {instruction<ExplicitGuestInstructionAttempt>(original,operation);} catch(const std::runtime_error& e) {require(operation==4,"unexpected original unwind");}
    try {instruction<sonic::statistics::InstructionAttempt>(candidate,operation);} catch(const std::runtime_error& e) {require(operation==4,"unexpected candidate unwind");}
    if(!enabled) {
        require(candidate.cpu.attempted_guest_instructions==5 && candidate.cpu.retired_guest_instructions==3,"disabled statistics changed");
        // These two observational totals are deliberately unavailable when off.
        original.cpu.attempted_guest_instructions=5;original.cpu.retired_guest_instructions=3;
    }
    require(state(original.cpu)==state(candidate.cpu),"instruction state/cycles/fault PC differ");
    require(provenance(original.cpu)==provenance(candidate.cpu),"fault provenance differs");
    require(counts(original.cpu.memory)==counts(candidate.cpu.memory),"fallback accounting differs");
    ++cases;
}
template<class T>
bool load(bool candidate,const DirectLinearMemoryGuard& guard,std::uint32_t address,T& value) {
    if constexpr(sizeof(T)==1) return candidate?sonic::statistics::read_u8(guard,address,value):direct_linear_guard_read_u8(guard,address,value);
    if constexpr(sizeof(T)==2) return candidate?sonic::statistics::read_u16(guard,address,value):direct_linear_guard_read_u16(guard,address,value);
    if constexpr(sizeof(T)==4) return candidate?sonic::statistics::read_u32(guard,address,value):direct_linear_guard_read_u32(guard,address,value);
}
template<class T>
void compare_read(bool enabled,std::uint32_t address,unsigned mode) {
    sonic::diagnostics::internal_runtime_enabled=enabled;
#if defined(SARECOMP_AOT_STATISTICS_OMIT)
    enabled=false;
#endif
    Fixture original,candidate;
    auto a=original.cpu.memory.direct_linear_memory_guard(false),b=candidate.cpu.memory.direct_linear_memory_guard(false);
    for(auto* f:{&original,&candidate}) {
        if(mode==1) f->trace();
        if(mode==2) f->watch(MemoryWatchpointAccess::Read);
        if(mode==3) f->cpu.memory.set_lookup_mode(MemoryLookupMode::Reference);
        if(mode==4) f->sink();
        f->cpu.memory.reset_performance_counters();
    }
    T x=T(sentinel),y=T(sentinel);
    const bool accepted=load(false,a,address,x);
    require(load(true,b,address,y)==accepted && x==y,"read admission/value differs");
    if(enabled) require(counts(original.cpu.memory)==counts(candidate.cpu.memory),"enabled access counters differ");
    else require(counts(candidate.cpu.memory)==Counts{},"disabled read counted");
    require(state(original.cpu)==state(candidate.cpu) && original.log==candidate.log,"read changed state/observers");
    ++cases;
}
}
int main() {
    try {
        for(bool enabled:{false,true}) {
            for(unsigned operation=0;operation<5;++operation)for(unsigned mapping=0;mapping<4;++mapping)
                compare_instruction(enabled,operation,mapping);
            for(unsigned mode=0;mode<5;++mode)
                for(auto address:{0x8C000100u,0xAC000100u,0x8C010100u,0x8C000101u,0x8C00FFFEu,0x8C020000u,0xFFFFFFFFu,0x0C000100u}) {
                    compare_read<std::uint8_t>(enabled,address,mode);
                    compare_read<std::uint16_t>(enabled,address,mode);
                    compare_read<std::uint32_t>(enabled,address,mode);
                }
        }
        std::cout<<"SONIC_AOT_STATISTICS_OK cases="<<cases
#if defined(SARECOMP_AOT_STATISTICS_OMIT)
                 <<" mode=omit counters=disabled"
#else
                 <<" mode=runtime on=exact off=statistics-only"
#endif
                 <<" cycles=exact faults=exact guards=exact\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
