#include "sonic_read_test_fixture.hpp"
#include "sonic_ram_regions.hpp"
#include "sonic_stack_frames.hpp"
#include <xmmintrin.h>
#include <ctime>

namespace {
struct Owned {
    Fixture f;
    std::array<NativePortImmutableRange,2> ranges{{
        {0x0C000800u,16u,native_port_immutable_range_mask(NativePortImmutableRangeKind::Executable)},
        {0x0C000A00u,16u,native_port_immutable_range_mask(NativePortImmutableRangeKind::ReadOnlyImage)}}};
    NativePortImmutableWriteGuard guard{ranges};
    explicit Owned(std::size_t size=0x10000u):f(size) {
        f.cpu.memory.set_guest_write_observer([this](const GuestWriteEvent& e) noexcept {guard.observe_write(e);},
            GuestWriteObserverContract::StableForPrevalidatedLinearWrites);
        f.cpu.memory.set_guest_write_batch_observer({&guard,
            [](void*,std::span<const GuestWriteEvent>) noexcept{return true;},
            [](void* p,std::span<const GuestWriteEvent> events) noexcept {
                for(const auto& e:events)static_cast<NativePortImmutableWriteGuard*>(p)->observe_write(e);
            }});
        sonic::scalar_writes::bind(f.cpu.memory,guard,f.cpu.memory.guest_write_observer_generation());
        for(unsigned i=0;i<0x10000;i+=4) f.ram->write_u32(i,0xEFAB8912u^i);
        f.cpu.r[1]=0x8C003000;
    }
    ~Owned(){sonic::scalar_writes::unbind(&f.cpu.memory,&guard);}
};
#include "ram_region_aot_fixture.inc"
#include "ram_region_extended_fixture.inc"
constexpr unsigned length=13;
constexpr bool alu(unsigned i){return i==0||i==4||i==10||i==12;}
constexpr bool store(unsigned i){return i==2||i==5||i==9;}
constexpr unsigned width(unsigned i){return i==2||i==6?16:i==5||i==11?8:32;}
constexpr unsigned reg(unsigned i){return i==1?3:i==3?4:i==6?5:6;}
unsigned cases=0,complete_hits=0,partial_hits=0;
std::uint32_t address(const CpuState& c,unsigned i) {
    return i==7?c.r[1]:c.r[0]+(i==1?0:i==3?0x10008:i==5||i==11?9:i==9?12:8);
}
void arithmetic(CpuState& c,unsigned i) {
    if(i==0)c.r[2]=c.r[0];
    else if(i==4)c.r[4]^=0xABCD;
    else if(i==10)c.r[2]+=16;
    else if(i==12)c.r[2]-=4;
    ++c.attempted_guest_instructions;++c.retired_guest_instructions;++c.pending_guest_cycles;
}

// Original scalar instruction semantics, with the preflight scheduler, exact
// fault attempt, write-invalidation exit, and post-MMIO scheduler boundary.
void original(Owned& o,const DirectLinearMemoryGuard& entry,unsigned start=0) {
    auto& c=o.f.cpu;
    for(unsigned i=start;i<length;++i) {
        if(alu(i)){arithmetic(c,i);continue;}
        const auto pc=source_pc+i*2;
        bool guarded=true;
        std::uint32_t direct=0,offset=0;
        if(i!=8) guarded=(!store(i)||c.memory.guest_write_observer_allows_prevalidated_linear_writes())&&
            sonic::stack_frames::translate(c,address(c,i),direct)&&
            direct_linear_guard_offset(entry,direct,width(i)/8,offset);
        if(!guarded)flush_pending_guest_cycles(c,o.f.services);
        const auto epoch=guarded?0:c.memory.mmio_boundary_epoch();
        ExplicitGuestInstructionAttempt attempt(c,pc,2);
        if(i==7||i==8||i==9) {
            if(c.sr&sr_fd_mask){raise_fpu_disabled(c,pc);return;}
        }
        try {
            const GuestInstructionOrigin origin{pc,pc,true};
            if(i==8)c.fr[1]=c.fr[0];
            else {
                const auto a=address(c,i);
                const bool linear=sonic::stack_frames::translate(c,a,direct);
                if(store(i)) {
                    const auto value=i==2?c.r[3]:i==5?c.r[4]:c.fr[1];
                    bool done=false;
                    if(linear&&c.memory.guest_write_observer_allows_prevalidated_linear_writes()) {
                        if(width(i)==8)done=c.memory.try_write_direct_linear_u8(direct&0x1FFFFFFF,static_cast<std::uint8_t>(value),CodeWriteSource::Cpu);
                        if(width(i)==16)done=c.memory.try_write_direct_linear_u16(direct&0x1FFFFFFF,static_cast<std::uint16_t>(value),CodeWriteSource::Cpu);
                        if(width(i)==32)done=c.memory.try_write_direct_linear_u32(direct&0x1FFFFFFF,value,CodeWriteSource::Fpu);
                    }
                    if(!done) {
                        if(width(i)==8)guest_write_u8_at(c,origin,a,static_cast<std::uint8_t>(value),CodeWriteSource::Cpu);
                        if(width(i)==16)guest_write_u16_at(c,origin,a,static_cast<std::uint16_t>(value),CodeWriteSource::Cpu);
                        if(width(i)==32)guest_write_u32_at(c,origin,a,value,CodeWriteSource::Fpu);
                    }
                } else {
                    std::uint32_t value=0;
                    if(width(i)==8) {
                        std::uint8_t v=0;
                        if(!linear||!direct_linear_guard_read_u8(entry,direct,v))v=guest_read_u8_at(c,origin,a);
                        value=v&0x80u?0xFFFFFF00u|v:v;
                    } else if(width(i)==16) {
                        std::uint16_t v=0;
                        if(!linear||!direct_linear_guard_read_u16(entry,direct,v))v=guest_read_u16_at(c,origin,a);
                        value=v&0x8000u?0xFFFF0000u|v:v;
                    } else if(!linear||!direct_linear_guard_read_u32(entry,direct,value))value=guest_read_u32_at(c,origin,a);
                    if(i==7)c.fr[0]=value;else c.r[reg(i)]=value;
                }
            }
            attempt.complete();
        } catch(const MemoryAccessError& e){enter_memory_exception_with_provenance(c,e,pc,0x6002u);return;}
        if(store(i)&&o.guard.write_detected()){c.pc=pc+2;return;}
        if(!guarded&&c.memory.mmio_boundary_epoch()!=epoch) {
            c.pc=pc+2;
            if(finalize_guest_block(c,o.f.services,1024u,pc,0u,false,false,false).interrupt)return;
        }
    }
}

unsigned prefix(Owned& o,const DirectLinearMemoryGuard& entry,sonic::ram_regions::PreparedWrites* prepared=nullptr) {
    auto& c=o.f.cpu;
    if(!sonic::scalar_writes::ram_regions_enabled()||c.trap_pending||(c.sr&sr_fd_mask)||(c.fpscr&fpscr_sz_mask))return 0;
    sonic::ram_regions::Access access(c,entry,&o.guard,prepared);
    auto r=c.r;auto f=c.fr;
    unsigned i=0,cycles=0,last=0;
    for(;i<length;++i) {
        bool success=true;
        if(i==0)r[2]=r[0];
        if(i==1)success=access.read<32>(r[0],r[3]);
        if(i==2)success=access.write<16>(r[0]+8,r[3]);
        if(i==3)success=access.read<32>(r[0]+0x10008,r[4]);
        if(i==4)r[4]^=0xABCD;
        if(i==5)success=access.write<8>(r[0]+9,r[4]);
        if(i==6)success=access.read<16,true>(r[0]+8,r[5]);
        if(i==7)success=access.read<32>(r[1],f[0]);
        if(i==8)f[1]=f[0];
        if(i==9)success=access.write<32>(r[0]+12,f[1]);
        if(i==10)r[2]+=16;
        if(i==11)success=access.read<8,true>(r[0]+9,r[6]);
        if(i==12)r[2]-=4;
        if(!success)break;
        cycles+=alu(i)?1:2;
        if(!alu(i))last=source_pc+i*2;
    }
    c.r=r;c.fr=f;
    sonic::ram_regions::complete(c,i,cycles,last);
    return i;
}

void mutate(Owned& o,unsigned mode) {
    auto& f=o.f;
    switch(mode) {
    case 1:f.trace();break;
    case 2:f.watch(MemoryWatchpointAccess::Read,0x0C000100);break;
    case 3:f.watch(MemoryWatchpointAccess::Write,0x0C000108);break;
    case 4:f.sink();break;
    case 5:f.cpu.memory.clear_guest_write_observer();break;
    case 6:o.guard.reserve_additional_runtime_executable_ranges(1);o.guard.add_runtime_executable_range(0x0C00010C,4);break;
    case 7:f.cpu.memory.clear_direct_linear_alias_window();break;
    case 8:f.cpu.write_sr(0);break;
    case 9:f.cpu.mmucr=1;break;
    case 10:f.cpu.pending_guest_cycles=UINT64_MAX-3;f.cpu.attempted_guest_instructions=UINT64_MAX-2;f.cpu.retired_guest_instructions=UINT64_MAX-2;break;
    case 11:f.watch(MemoryWatchpointAccess::Read,0x0C000100);f.services.on_flush=[&f]{f.cpu.r[0]=0x8C002000;f.cpu.r[1]=0x8C003000;};break;
    case 12:f.cpu.active_block_size=0;break;
    case 13:f.cpu.r[1]=0xFFFFFFFF;break;
    case 14:f.mmio();f.cpu.r[1]=0xB0000000;break;
    case 15:f.mmio(true);f.cpu.r[1]=0xB0000000;break;
    case 16:f.cpu.write_sr(sr_md_mask|sr_fd_mask);break;
    case 17:f.cpu.fpscr|=fpscr_pr_mask;break;
    case 18:f.cpu.memory.set_guest_write_observer([&f](const GuestWriteEvent& e) noexcept {
        f.log.add({10,{e.address,e.size,f.cpu.r[2],f.cpu.attempted_guest_instructions,f.cpu.retired_guest_instructions}});
        },GuestWriteObserverContract::StableForPrevalidatedLinearWrites);break;
    }
}

void compare(std::uint32_t base,unsigned mode,bool stale) {
    Owned a,b;
    a.f.cpu.r[0]=b.f.cpu.r[0]=base;
    auto ga=a.f.cpu.memory.direct_linear_memory_guard(false),gb=b.f.cpu.memory.direct_linear_memory_guard(false);
    mutate(a,mode);mutate(b,mode);
    if(!stale){ga=a.f.cpu.memory.direct_linear_memory_guard(false);gb=b.f.cpu.memory.direct_linear_memory_guard(false);}
    auto stop=prefix(b,gb);
    if(stop==length)++complete_hits;else if(stop>2)++partial_hits;
    original(b,gb,stop);original(a,ga);
    require(state(a.f.cpu)==state(b.f.cpu),"CPU prefix/fallback state differs");
    require(provenance(a.f.cpu)==provenance(b.f.cpu),"fault provenance differs");
    require(counts(a.f.cpu.memory)==counts(b.f.cpu.memory),"memory accounting differs");
    require(a.f.log==b.f.log&&!a.f.log.overflow,"observer/scheduler log differs");
    require(std::ranges::equal(a.f.ram->bytes(),b.f.ram->bytes()),"alias/partial RAM differs");
    require(a.guard.generation()==b.guard.generation()&&a.guard.write_detected()==b.guard.write_detected(),"immutable state differs");
    ++cases;
}

void generated_compare(unsigned mode,std::uint32_t resume) {
    Owned a(0x100000),b(0x100000);
    for(auto* o:{&a,&b}) {
        auto& ram=*o->f.ram;
        ram.write_u32(0xCC200,0x8C002000);ram.write_u32(0xCC204,0x8C001000);
        ram.write_u32(0xCC208,0xFFFFFF70);ram.write_u32(0xCC20C,0x42);
        for(unsigned i=0;i<4;++i)ram.write_u32(0xCC210+4*i,0x8C003000+0x1000*i);
        ram.write_u32(0xCC220,0x8C007000);
        o->f.cpu.r[15]=0x8C008000;ram.write_u32(0x8018,0x8C009000);
        if(mode==1)ram.write_u32(0xCC214,0xFFFFFFFF);
        if(mode==2) {
            ram.write_u32(0xCC214,0xFFFFFFFF);
            o->f.services.on_flush=[o]{o->f.cpu.r[3]=0x8C00A000;};
        }
        if(mode==3)ram.write_u32(0xCC210,0x8C0007F0);
        if(mode==4)o->f.trace();
        if(mode==5)o->f.sink();
        if(mode==6)o->f.cpu.pending_guest_cycles=UINT64_MAX-4;
    }
    const auto ga=a.f.cpu.memory.direct_linear_memory_guard(false),gb=b.f.cpu.memory.direct_linear_memory_guard(false);
    retained_region(a,ga,resume);transformed_region(b,gb,resume);
    require(state(a.f.cpu)==state(b.f.cpu),"generated prefix CPU/labels differ");
    require(provenance(a.f.cpu)==provenance(b.f.cpu),"generated prefix fault differs");
    require(counts(a.f.cpu.memory)==counts(b.f.cpu.memory),"generated prefix counters differ");
    require(a.f.log==b.f.log&&!a.f.log.overflow,"generated prefix callbacks differ");
    require(std::ranges::equal(a.f.ram->bytes(),b.f.ram->bytes()),"generated prefix stores differ");
    ++cases;
}

void generated_mixed_compare(unsigned mode,std::uint32_t bits,std::uint32_t fpscr,std::uint32_t resume) {
    Owned a(0x100000),b(0x100000);
    for(auto* o:{&a,&b}) {
        auto& ram=*o->f.ram;
        auto& c=o->f.cpu;
        c.fpscr=fpscr;
        c.gbr=0x8C00B000u;
        c.fr[0]=bits;c.fr[4]=0x3FC00000u;
        c.r[1]=0x8C004000u;c.r[2]=0x8C003000u;
        ram.write_u32(0x36FB4,0x8C004000);
        for(unsigned i=0;i<6;++i) {
            ram.write_u32(0x36FB8+4*i,0x8C003000+4*i);
            ram.write_u32(0x3000+4*i,i==2?0x3FC00000u:bits);
        }
        ram.write_u32(0x36FD0,0x8C005000);
        if(mode==1)ram.write_u32(0x36FB4,0x8C1FFFEC); // fault after the first multiply
        if(mode==2) {
            o->guard.reserve_additional_runtime_executable_ranges(1);
            o->guard.add_runtime_executable_range(0x0C004014,4);
        }
        if(mode==3)o->f.trace();
        if(mode==4)o->f.sink();
        if(mode==5)c.write_sr(sr_md_mask|sr_fd_mask);
        if(mode==6) {
            ram.write_u32(0x36FCC,0xFFFFFFFF);
            o->f.services.on_flush=[o]{o->f.cpu.r[1]=0x8C006000;};
        }
        if(mode==7)c.pending_guest_cycles=UINT64_MAX-10;
    }
    const auto ga=a.f.cpu.memory.direct_linear_memory_guard(false),gb=b.f.cpu.memory.direct_linear_memory_guard(false);
    const auto host=_mm_getcsr();
    _mm_setcsr((host|0x1FA1u)&~0x6000u);
    const auto expected_host=_mm_getcsr();
    retained_mixed_region(a,ga,resume);
    const auto retained_host=_mm_getcsr();
    _mm_setcsr(expected_host);
    transformed_mixed_region(b,gb,resume);
    const auto transformed_host=_mm_getcsr();
    _mm_setcsr(host);
    require(retained_host==expected_host&&transformed_host==expected_host,"mixed prefix host FP state differs");
    require(state(a.f.cpu)==state(b.f.cpu),"mixed prefix CPU/FPSCR differs");
    require(provenance(a.f.cpu)==provenance(b.f.cpu),"mixed prefix fault differs");
    require(counts(a.f.cpu.memory)==counts(b.f.cpu.memory),"mixed prefix counters differ");
    require(a.f.log==b.f.log&&!a.f.log.overflow,"mixed prefix callbacks differ");
    require(std::ranges::equal(a.f.ram->bytes(),b.f.ram->bytes()),"mixed prefix stores differ");
    require(a.guard.generation()==b.guard.generation()&&a.guard.write_detected()==b.guard.write_detected(),"mixed prefix immutable state differs");
    ++cases;
}

void generated_vector_compare(unsigned mode,bool gbr) {
    Owned a(0x100000),b(0x100000);
    for(auto* o:{&a,&b}) {
        auto& ram=*o->f.ram;auto& c=o->f.cpu;
        c.fpscr=fpscr_dn_mask;
        c.gbr=0x8C001000;c.r[4]=0x8C003000;c.r[15]=0x8C009000;
        c.r[0]=8;c.r[2]=0x8C006000;c.r[5]=0x8C004000;
        ram.write_u32(0x9004,0x8C007000);
        ram.write_u32(0x373B0,0x8C008000);ram.write_u32(0x373B4,0x8C004000);
        for(auto offset:{0x6008u,0x7008u,0x3020u,0x4000u,0x4004u,0x4008u})ram.write_u32(offset,0x3F800001u);
        if(mode==1) {
            c.r[5]=0x8C1FFFFC;
            ram.write_u32(0x373B4,0x8C1FFFFC);
        }
        if(mode==2)o->f.watch(MemoryWatchpointAccess::Read,0x0C004004);
        if(mode==3)o->f.trace();
        if(mode==4)o->f.sink();
        if(mode==5)c.fpscr|=fpscr_pr_mask;
        if(mode==6)c.fpscr|=fpscr_sz_mask;
        if(mode==7)c.write_sr(sr_md_mask|sr_fd_mask);
        if(mode==8){c.r[4]=0xAC003000;ram.write_u32(0x373B4,0xAC004000);}
        if(mode==9)c.pending_guest_cycles=UINT64_MAX-9;
    }
    const auto ga=a.f.cpu.memory.direct_linear_memory_guard(false),gb=b.f.cpu.memory.direct_linear_memory_guard(false);
    if(gbr){retained_gbr_region(a,ga);transformed_gbr_region(b,gb);}
    else {retained_vector_region(a,ga);transformed_vector_region(b,gb);}
    require(state(a.f.cpu)==state(b.f.cpu),"vector prefix CPU differs");
    require(provenance(a.f.cpu)==provenance(b.f.cpu),"vector prefix fault differs");
    require(counts(a.f.cpu.memory)==counts(b.f.cpu.memory),"vector prefix counters differ");
    require(a.f.log==b.f.log&&!a.f.log.overflow,"vector prefix callbacks differ");
    require(std::ranges::equal(a.f.ram->bytes(),b.f.ram->bytes()),"vector prefix stores differ");
    ++cases;
}

template<class Access>
std::array<std::uint32_t,20> page_sequence(Owned& o,const DirectLinearMemoryGuard& entry,std::uint32_t base,
        sonic::ram_regions::PreparedWrites* prepared=nullptr) {
    Access access=[&] {
        if constexpr(std::is_same_v<Access,sonic::ram_regions::Access>) return Access(o.f.cpu,entry,&o.guard,prepared);
        else return Access(o.f.cpu,entry,&o.guard);
    }();
    std::array<std::uint32_t,20> result{};
    result[0]=access.template read<32>(base,result[1]);
    result[2]=access.template write<32>(base+4,result[1]);
    result[3]=access.template read<32>(base+4,result[4]);
    result[5]=access.template write<8>(base+7,0xAB);
    result[6]=access.template read<16,true>(base+6,result[7]);
    std::array<std::uint32_t,4> words{sentinel,sentinel,sentinel,sentinel};
    result[8]=access.template read_group<4>(base+8,words);
    std::copy(words.begin(),words.end(),result.begin()+9);
    result[13]=access.template write<16>(base+24,0xEDCB);
    result[14]=access.template read<32>(base+24,result[15]);
    result[16]=access.template read<8,true>(base+7,result[17]);
    result[18]=access.template write<32>(base+0x10000,0x12345678);
    result[19]=access.template read<32>(base,result[1]);
    return result;
}

void page_compare(std::uint32_t base,unsigned mode,bool stale) {
    Owned a,b;
    auto ga=a.f.cpu.memory.direct_linear_memory_guard(false),gb=b.f.cpu.memory.direct_linear_memory_guard(false);
    mutate(a,mode);mutate(b,mode);
    if(!stale){ga=a.f.cpu.memory.direct_linear_memory_guard(false);gb=b.f.cpu.memory.direct_linear_memory_guard(false);}
    const auto expected=page_sequence<sonic::ram_regions::CheckedAccess>(a,ga,base);
    const auto actual=page_sequence<sonic::ram_regions::Access>(b,gb,base);
    require(expected==actual,"page proof access/group result differs");
    require(counts(a.f.cpu.memory)==counts(b.f.cpu.memory),"page proof memory accounting differs");
    require(std::ranges::equal(a.f.ram->bytes(),b.f.ram->bytes()),"page proof alias/partial store differs");
    require(a.f.log==b.f.log,"page proof observer log differs");
    // A second region must acquire new proofs after a classification change.
    for(auto* o:{&a,&b}) {
        o->guard.reserve_additional_runtime_executable_ranges(1);
        o->guard.add_runtime_executable_range(0x0C002000,256);
    }
    require(page_sequence<sonic::ram_regions::CheckedAccess>(a,ga,0x8C002000)==
            page_sequence<sonic::ram_regions::Access>(b,gb,0x8C002000),"page proof survived a region boundary");
    require(counts(a.f.cpu.memory)==counts(b.f.cpu.memory),"second region counters differ");
    require(std::ranges::equal(a.f.ram->bytes(),b.f.ram->bytes()),"second region stores differ");
    ++cases;
}

void extended_compare(unsigned index,unsigned mode,std::uint32_t base,std::uint32_t resume,bool stale) {
    Owned a(0x100000),b(0x100000);
    for(auto* o:{&a,&b}) {
        auto& c=o->f.cpu;auto& ram=*o->f.ram;
        c.r[15]=base;c.r[12]=0x8C004300;c.r[13]=0x8C005000;c.r[14]=0x8C005100;
        c.t=true;c.pr=0xCAFEBABEu;c.fpscr=fpscr_dn_mask;
        ram.write_u32(0x19A68,0x8C004000);ram.write_u32(0x4000,mode==20?0:7);
        ram.write_u32(0x19A74,0x8C004100);ram.write_u32(0x19A78,0x8C004104);
        ram.write_u32(0x19A7C,0x8C004108);ram.write_u32(0x4100,0x8C004200);
        ram.write_u32(0x4104,0x80000002);ram.write_u32(0x4200,4);
        ram.write_u32(0x4300,0x8C004400);ram.write_u8(0x4408,0x80);
        ram.write_u32(0x5100,0x8C005200);ram.write_u32(0x522C,0x8C005300);
        if(index>=4) {
            c.r[4]=0x8C007000;
            ram.write_u32(0x1A440,0x8C007100);ram.write_u32(0x1A640,0x8C007100);
            ram.write_u32(0x7100,0x8C007200);ram.write_u32(0x1A644,0x3F000000);
            constexpr std::uint32_t payloads[]{0u,0x80000000u,1u,0x7FC12345u,
                0x7FA12345u,0x3F800001u,0xBF800001u,0x7F800000u};
            for(unsigned i=0;i<16;++i) {
                c.fr[i]=payloads[i%8];c.xf[i]=payloads[(i+3)%8];
                ram.write_u32(0x7000+4*i,mode==27?payloads[i%8]:0x3F800001u+i);
                ram.write_u32(0x7200+4*i,mode==27?payloads[(i+2)%8]:0x40000001u+i);
            }
        }
    }
    auto ga=a.f.cpu.memory.direct_linear_memory_guard(false),gb=b.f.cpu.memory.direct_linear_memory_guard(false);
    for(auto* o:{&a,&b}) {
        mutate(*o,mode);
        if(mode==19) {
            o->f.watch(MemoryWatchpointAccess::Read,0x0C004000);
            o->f.services.on_flush=[o]{o->f.cpu.r[15]=0x8C009000;o->f.cpu.pr=0xDEADBEEFu;o->f.cpu.t=false;};
        }
        if(mode==21) {
            o->guard.reserve_additional_runtime_executable_ranges(1);
            o->guard.add_runtime_executable_range(0x0C00501C,4);
        }
        if(mode==22)o->f.ram->write_u32(0x4300,0xFFFFFFFFu);
        if(mode==23)o->f.cpu.fpscr|=fpscr_sz_mask;
        if(mode==24)o->f.cpu.fpscr|=fpscr_sz_mask|fpscr_pr_mask;
        if(mode==25)o->f.cpu.fpscr|=fpscr_fr_mask;
        if(mode==26)o->f.cpu.fpscr|=fpscr_exception_enable_mask|0x80000000u;
        if(mode==27)o->f.cpu.fpscr&=~fpscr_dn_mask;
        if(mode==28) {
            o->f.watch(MemoryWatchpointAccess::Write,(base-8u)&0x1FFFFFFFu);
            o->f.services.on_flush=[o]{o->f.cpu.r[15]=0x8C009000;o->f.cpu.fr[15]=0xCAFEBABEu;};
        }
        if(mode==29) {
            o->guard.reserve_additional_runtime_executable_ranges(1);
            o->guard.add_runtime_executable_range(0x0C005294,4);
        }
    }
    if(!stale){ga=a.f.cpu.memory.direct_linear_memory_guard(false);gb=b.f.cpu.memory.direct_linear_memory_guard(false);}
    extended_witnesses[index].original(a,ga,resume);
    extended_witnesses[index].extended(b,gb,resume);
    require(state(a.f.cpu)==state(b.f.cpu),"extended prefix CPU/PR/T/resume differs");
    require(provenance(a.f.cpu)==provenance(b.f.cpu),"extended prefix fault differs");
    require(counts(a.f.cpu.memory)==counts(b.f.cpu.memory),"extended prefix counters differ");
    require(a.f.log==b.f.log&&!a.f.log.overflow,"extended prefix callbacks differ");
    require(std::ranges::equal(a.f.ram->bytes(),b.f.ram->bytes()),"extended prefix RAM differs");
    require(a.guard.generation()==b.guard.generation()&&a.guard.write_detected()==b.guard.write_detected(),"extended prefix immutable state differs");
    ++cases;
}

// The reusable capability must survive ordinary callbacks only as a hint.
// Warm it, cross a real scheduler boundary, mutate admission, then compare the
// next prefix plus original fallback with the independent scalar instruction oracle.
void prepared_compare(unsigned mode,bool stale,std::uint32_t base) {
    Owned a,b;
    sonic::ram_regions::PreparedWrites prepared(true);
    a.f.cpu.r[0]=b.f.cpu.r[0]=0x8C000100u;
    auto ga=a.f.cpu.memory.direct_linear_memory_guard(false),gb=b.f.cpu.memory.direct_linear_memory_guard(false);
    for(unsigned i=0;i<2;++i) {
        original(a,ga);
        const auto stop=prefix(b,gb,&prepared);
        require(stop==length,"prepared warmup did not execute native prefix");
    }
    require(counts(a.f.cpu.memory)==counts(b.f.cpu.memory),"prepared counters not published at boundary");
    for(auto* o:{&a,&b}) {
        o->f.cpu.r[0]=base;
        o->f.services.on_flush=[o,mode] {
            if(mode<=18)mutate(*o,mode);
            if(mode==19)sonic::scalar_writes::unbind(&o->f.cpu.memory,&o->guard);
            if(mode==20)o->f.cpu.memory.clear_guest_write_batch_observer();
            if(mode==21)o->f.cpu.memory.set_lookup_mode(MemoryLookupMode::Reference);
            if(mode==22){o->f.watch(MemoryWatchpointAccess::Write);o->f.cpu.memory.clear_watchpoints();}
            if(mode==23)o->f.ram->write_u32(0x100,0x01234567u);
            if(mode==24){
                sonic::scalar_writes::unbind(&o->f.cpu.memory,&o->guard);
                sonic::scalar_writes::bind(o->f.cpu.memory,o->guard,o->f.cpu.memory.guest_write_observer_generation());
            }
            if(mode==25)o->f.cpu.memory.bind_direct_linear_alias_window(0x0C000000u,0x10000u,*o->f.ram);
            if(mode==26){
                o->guard.reserve_additional_runtime_executable_ranges(1);
                o->guard.add_runtime_executable_range(0x0C000100,256);
            }
            if(mode==27){
                o->guard.reserve_additional_runtime_executable_ranges(1);
                o->guard.add_runtime_executable_range(0x0C000100,256);
                o->guard.remove_runtime_executable_range(0x0C000100,256);
            }
        };
        flush_pending_guest_cycles(o->f.cpu,o->f.services);
    }
    if(!stale){ga=a.f.cpu.memory.direct_linear_memory_guard(false);gb=b.f.cpu.memory.direct_linear_memory_guard(false);}
    original(a,ga);
    const auto stop=prefix(b,gb,&prepared);original(b,gb,stop);
    require(state(a.f.cpu)==state(b.f.cpu),"prepared callback CPU state differs");
    require(provenance(a.f.cpu)==provenance(b.f.cpu),"prepared callback fault provenance differs");
    require(counts(a.f.cpu.memory)==counts(b.f.cpu.memory),"prepared callback memory accounting differs");
    require(a.f.log==b.f.log&&!a.f.log.overflow,"prepared callback order differs");
    require(std::ranges::equal(a.f.ram->bytes(),b.f.ram->bytes()),"prepared callback RAM differs");
    require(a.guard.generation()==b.guard.generation()&&a.guard.write_detected()==b.guard.write_detected(),
        "prepared callback code invalidation differs");
    ++cases;
}

void prepared_owners() {
    Owned a,b;
    sonic::ram_regions::PreparedWrites prepared(true),nested(true);
    const auto ga=a.f.cpu.memory.direct_linear_memory_guard(false),gb=b.f.cpu.memory.direct_linear_memory_guard(false);
    const auto ca=counts(a.f.cpu.memory),cb=counts(b.f.cpu.memory);
    // Different live Memory instances may have identical numeric generations.
    for(auto* o:{&a,&b,&a,&b}) {
        const auto& entry=o==&a?ga:gb;
        const auto& alien=o==&a?gb:ga;
        {
            sonic::ram_regions::Access access(o->f.cpu,alien,&o->guard,&prepared);
            std::uint32_t value=sentinel;
            require(!access.read<32>(0x8C000100,value)&&value==sentinel,"alien read guard accepted");
            require(!access.write<32>(0x8C000104,sentinel),"alien write guard accepted");
        }
        {
            sonic::ram_regions::Access access(o->f.cpu,entry,&o->guard,&prepared);
            require(access.write<32>(0x8C000104,0x2468ACE0u),"owner recapture failed");
        }
        {
            sonic::ram_regions::Access access(o->f.cpu,entry,&o->guard,&nested);
            std::uint32_t value=0;
            require(access.read<32>(0x8C000104,value)&&value==0x2468ACE0u,"nested owner saw stale data");
        }
    }
    require(counts(a.f.cpu.memory)[0]==ca[0]+4&&counts(b.f.cpu.memory)[0]==cb[0]+4,"owner counter attribution differs");
    require(!sonic::scalar_writes::requested_capture,"prepared capture scope leaked");
    ++cases;
}

template<class Access>
void page_benchmark(const char* name,bool prepare=false) {
    Owned o;
    const auto entry=o.f.cpu.memory.direct_linear_memory_guard(false);
    const auto begin=std::clock();
    std::uint64_t checksum=0;
    constexpr unsigned iterations=100000;
    sonic::ram_regions::PreparedWrites prepared(prepare);
    for(unsigned i=0;i<iterations;++i) {
        const auto result=page_sequence<Access>(o,entry,0x8C002000+(i&3)*64,&prepared);
        checksum+=result[1]+static_cast<std::uint64_t>(result[15])+result[7]+result[12];
    }
    const auto elapsed=1000.0*(std::clock()-begin)/CLOCKS_PER_SEC;
    std::cout<<"SONIC_RAM_PAGE_BENCH path="<<name<<" iterations="<<iterations<<" cpu_ms="<<elapsed<<" checksum="<<checksum<<'\n';
}
}
int main(int argc,char** argv) {
    try {
        require(sonic::scalar_writes::ram_regions_enabled(),"set SARECOMP_RAM_REGIONS=1");
        if(argc==2&&std::string_view(argv[1])=="--benchmark") {
            page_benchmark<sonic::ram_regions::CheckedAccess>("checked");
            page_benchmark<sonic::ram_regions::Access>("snapshot");
            page_benchmark<sonic::ram_regions::Access>("prepared",true);
            return 0;
        }
        for(unsigned mode=0;mode<=18;++mode)for(bool stale:{false,true})
            for(auto base:{0x8C000100u,0xAC000100u,0x0C000100u,0x8C010100u,0x8C000001u,
                0x8C0007F8u,0x8C0009F8u,0x8C00FFFCu,0x8C01FFFCu,0xFFFFFFFFu,0xE0000000u}) compare(base,mode,stale);
        sonic::diagnostics::internal_runtime_enabled=true;compare(0x8C000100,0,false);
        sonic::diagnostics::internal_runtime_enabled=false;
        require(complete_hits&&partial_hits,"missing successful and partial-prefix coverage");
        for(unsigned mode=0;mode<7;++mode)for(auto resume:{0u,0x8C0CC098u,0x8C0CC0A2u})generated_compare(mode,resume);
        for(unsigned mode=0;mode<8;++mode)
            for(auto bits:{0u,0x80000000u,0x3F800000u,0x3F800001u,0x7F7FFFFFu,0x00800000u,1u,0x7F800000u,0x7FC00000u,0x7FBFFFFFu})
                for(auto fpscr:{fpscr_dn_mask,fpscr_dn_mask|1u,0u,fpscr_pr_mask|fpscr_dn_mask,
                               fpscr_sz_mask|fpscr_dn_mask,2u|fpscr_dn_mask,fpscr_enable_invalid_mask|fpscr_dn_mask})
                    generated_mixed_compare(mode,bits,fpscr,0u);
        for(auto resume:{0x8C036F94u,0x8C036F98u})generated_mixed_compare(0,0x3F800001u,fpscr_dn_mask,resume);
        for(unsigned mode=0;mode<10;++mode)for(bool gbr:{false,true})generated_vector_compare(mode,gbr);
        for(unsigned mode=0;mode<=18;++mode)for(bool stale:{false,true})
            for(auto base:{0x8C000100u,0xAC000100u,0x0C000100u,0x8C0007F0u,0x8C000810u,
                0x8C0009F0u,0x8C000A10u,0x8C00FFF0u,0x8C01FFF0u,0x8C000001u,0xFFFFFFFFu})
                page_compare(base,mode,stale);
        require(!sonic::scalar_writes::requested_capture,"capture scope leaked");
        for(unsigned index=0;index<std::size(extended_witnesses);++index) {
            for(unsigned mode=0;mode<=29;++mode)for(bool stale:{false,true})
                for(auto base:{0x8C006000u,0xAC006000u,0x0C006000u,0x8C100000u,
                    0x8C000008u,0x8C000814u,0x8C1FFFFCu,0x8C006001u})
                    extended_compare(index,mode,base,0,stale);
            for(auto resume:extended_witnesses[index].resumes)if(resume)
                for(unsigned mode:{0u,1u,4u,19u})extended_compare(index,mode,0x8C006000,resume,false);
            require(extended_completed[index]!=0,"extended witness never completed natively");
            require(extended_partial[index]!=0,"extended witness never exercised partial completion");
            std::cout<<"SONIC_RAM_EXTENDED_WITNESS index="<<index<<" complete="<<extended_completed[index]
                     <<" partial="<<extended_partial[index]<<'\n';
        }
        for(unsigned mode=0;mode<=27;++mode)for(bool stale:{false,true})
            for(auto base:{0x8C000100u,0xAC000100u,0x0C000100u,0x8C010100u,0x8C000001u,0x8C00FFFCu})
                prepared_compare(mode,stale,base);
        prepared_owners();
        for(const auto& b:sonic::scalar_writes::bindings)require(!b.memory,"binding leaked");
        std::cout<<"SONIC_RAM_REGIONS_OK cases="<<cases<<" complete="<<complete_hits<<" partial="<<partial_hits
                 <<" state=exact counters=exact aliases=exact faults=exact observers=exact scheduler=exact\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<"case="<<cases<<" "<<e.what()<<'\n';return 1;}
}
