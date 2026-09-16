#include "sonic_read_test_fixture.hpp"
#include "sonic_fpu_register_cache.hpp"
#include "katana/runtime/native_aot_state.hpp"
#include <xmmintrin.h>

namespace {
using Registers=NativeAotRegisterFile<0xFFFFu,0x3Fu>;
std::uint64_t cases=0,retained=0,released=0;
std::uint32_t rng=0x514FA183u;
std::uint32_t random_bits(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}

template<bool Candidate>
void instruction(CpuState& cpu,Registers& registers,unsigned operation,
                 unsigned source,unsigned destination,bool delay,bool single_only) {
    const bool keep=Candidate && registers.owns_registers() && sonic::fpu_register_cache::admitted(cpu);
    if constexpr(Candidate) {if(keep)++retained;else ++released;}
    if(!keep)registers.flush_release();
    ExplicitGuestInstructionAttempt attempt(cpu,source_pc,2u);
    const auto owner=delay?std::optional<std::uint32_t>(source_pc-2u):std::nullopt;
    if((cpu.sr&sr_fd_mask)!=0u){raise_fpu_disabled(cpu,source_pc,owner);return;}
    if((single_only && (cpu.fpscr&fpscr_pr_mask)!=0u) || (cpu.fpscr&fpscr_rounding_mode_mask)>1u){
        raise_illegal_instruction(cpu,source_pc,owner);return;
    }
    bool trap;
    if(operation<4)trap=fpu_binary(cpu,FpuBinaryOperation(operation),source,destination,owner);
    else if(operation==4)trap=fpu_square_root(cpu,destination,owner);
    else trap=fpu_reciprocal_square_root(cpu,destination,owner);
    if(trap){require(!keep,"admitted FPU raised an exception with unpublished registers");return;}
    attempt.complete();
    if(!keep)registers.reload_acquire();
}

void dirty(Registers& r) {
    for(unsigned i=0;i<16;++i)r[i]=0x8C001000u+i*16;
    r.t()=true;r.pr()=0x8C019876;r.gbr()=0x8C004321;
    r.mach()=0xFF123456;r.macl()=0x19EF1234;r.fpul()=0x41234567;
}

template<bool Candidate>
void run(Fixture& f,unsigned operation,unsigned source,unsigned destination,bool delay,
         bool single_only,bool owned,bool callback) {
    Registers r(f.cpu);dirty(r);
    if(!owned) {
        r.flush_release();
        // A preceding external boundary changed the public register bank.
        f.cpu.r[4]=0x8C004444;f.cpu.r[15]=0x8C005555;f.cpu.pr=0x8C006666;f.cpu.t=false;
    }
    instruction<Candidate>(f.cpu,r,operation,source,destination,delay,single_only);
    if(callback) {
        r.flush_release();
        f.services.on_flush=[&f] {
            require(f.cpu.r[15]==0x8C0010F0,"callback saw stale stack register");
            require(f.cpu.pr==0x8C019876 && f.cpu.t,"callback saw stale scalar registers");
            f.cpu.r[15]=0x8C006000;f.cpu.pr=0x8C007000;
            f.cpu.fpscr &= ~fpscr_dn_mask;
            f.cpu.fr[2]=1;f.cpu.fr[3]=0x3F800000;
        };
        flush_pending_guest_cycles(f.cpu,f.services);
        r.reload_acquire();
        // Must recheck admission after the callback changed DN. Cause.E must
        // observe the newly published stack, PR and exception owner.
        instruction<Candidate>(f.cpu,r,0,2,3,true,true);
    }
    r.flush();
}

void compare(unsigned operation,unsigned source,unsigned destination,std::uint32_t n,std::uint32_t m,
             std::uint32_t mode,bool fd,bool delay,bool single_only,bool owned,bool callback=false) {
    Fixture a,b;
    for(auto* f:{&a,&b}) {
        f->cpu.fpscr=mode;
        if(fd)f->cpu.write_sr(f->cpu.read_sr()|sr_fd_mask);
        f->cpu.fr[source]=m;f->cpu.fr[destination]=n;
        f->cpu.fr[(source+1)&15]=m^0xA13523;f->cpu.fr[(destination+1)&15]=n^0xEF4312;
    }
    const auto ambient=0x1F80u|((n&3u)<<13u)|0x8040u|0x25u;
    _mm_setcsr(ambient);
    run<false>(a,operation,source,destination,delay,single_only,owned,callback);
    const auto reference_host=_mm_getcsr();
    _mm_setcsr(ambient);
    run<true>(b,operation,source,destination,delay,single_only,owned,callback);
    require(_mm_getcsr()==reference_host,"host FPU state differs");
    if(state(a.cpu)!=state(b.cpu)) {
        std::cerr<<"op="<<operation<<" mode="<<std::hex<<mode<<" n="<<n<<" m="<<m
                 <<" fr="<<a.cpu.fr[destination]<<'/'<<b.cpu.fr[destination]<<std::dec<<'\n';
        throw std::runtime_error("CPU/register/exception/accounting mismatch");
    }
    require(a.log==b.log && counts(a.cpu.memory)==counts(b.cpu.memory),"observer or memory accounting mismatch");
    require(std::equal(a.ram->bytes().begin(),a.ram->bytes().end(),b.ram->bytes().begin()),"exception memory mismatch");
    ++cases;
}
}

int main(){try {
    const auto ambient=_mm_getcsr();
    constexpr std::array<std::uint32_t,18> values{0,0x80000000,1,0x007FFFFF,0x00800000,0x00800001,
        0x3F000000,0x3F800000,0x3F800001,0xBF800000,0x4B800001,0x7F7FFFFF,
        0x7F800000,0xFF800000,0x7FC12345,0x7F812345,0xFFC12345,0x80800001};
    for(unsigned op=0;op<6;++op)for(unsigned rm=0;rm<2;++rm)
        for(auto n:values)for(auto m:values)for(bool delay:{false,true})
            compare(op,2,4,n,m,fpscr_dn_mask|rm|fpscr_cause_mask|fpscr_flag_mask,false,delay,true,true);
    for(unsigned i=0;i<4096;++i)
        compare(i%6,2,4,random_bits(),random_bits(),fpscr_dn_mask|(i&1),false,(i&2)!=0,true,true);
    // Rejected precision, rounding, unmasked and unmaskable exceptions, disabled
    // FPU, and an already released cache use the complete original envelope.
    for(unsigned op=0;op<6;++op)for(auto mode:{0u,2u,3u,fpscr_dn_mask|2u,
        fpscr_dn_mask|fpscr_pr_mask,fpscr_dn_mask|fpscr_exception_enable_mask})
        for(auto n:values)for(bool delay:{false,true})for(bool fd:{false,true})
            compare(op,2,4,n,1,mode,fd,delay,op==5,true);
    for(unsigned i=0;i<128;++i)
        compare(i%6,2,4,values[i%values.size()],values[(i+3)%values.size()],fpscr_dn_mask|(i&1),false,false,true,false);
    for(unsigned i=0;i<16;++i)
        compare(0,2,4,0x3F800000,0x40000000,fpscr_dn_mask|(i&1),false,false,true,true,true);
    _mm_setcsr(ambient);
    std::cout<<"SONIC_FPU_REGISTER_CACHE_OK cases="<<cases<<" retained="<<retained<<" fallback="<<released
        <<" state=exact exceptions=ordered callbacks=published\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
