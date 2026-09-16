#include "sonic_procedure_registers.hpp"
#include "katana/runtime/native_aot_state.hpp"
#include "katana/runtime/code_address_inline.hpp"
#include <array>
#include <cstdio>
#include <ctime>
#include <stdexcept>
#include <vector>

using namespace katana::runtime;
using sonic::procedure_registers::Bank;
namespace {
using OriginalRegisters = NativeAotRegisterFile<0xFFFFu,0x3Fu>;
unsigned cases = 0;
void require(bool yes, const char* message) {
    if (!yes) throw std::runtime_error(message);
}
struct Snapshot {
    std::array<std::uint32_t,16> r, fr, xf;
    std::array<std::uint32_t,8> bank;
    std::array<std::uint64_t,32> state;
    bool operator==(const Snapshot&) const = default;
};
Snapshot snapshot(const CpuState& c) {
    return {c.r,c.fr,c.xf,c.r_bank,{
        c.pc,c.pr,c.gbr,c.vbr,c.read_sr(),c.ssr,c.spc,c.sgr,c.mach,c.macl,
        c.fpul,c.fpscr,c.exception_generation,c.trap_pending,
        static_cast<std::uint64_t>(c.last_exception_cause),c.expevt,
        c.last_exception_instruction_pc,c.last_exception_instruction_physical_pc,
        c.last_exception_owner_pc,c.exception_in_delay_slot,
        c.attempted_guest_instructions,c.retired_guest_instructions,
        c.pending_guest_cycles,c.total_guest_cycles,c.active_instruction_pc,
        c.active_instruction_physical_pc,c.active_block_virtual_start,
        c.active_block_physical_start,c.active_block_size}};
}
void initialize(CpuState& c,unsigned seed) {
    c.write_sr(sr_md_mask);
    for(unsigned i=0;i<16;++i) {
        c.r[i]=seed*0x1234567u+i*0x01298765u;
        c.fr[i]=0x3F800000u+i*4096u;
        c.xf[i]=0xBF000000u+i*8192u;
    }
    for(unsigned i=0;i<8;++i)c.r_bank[i]=seed*127u+i;
    c.pc=0x8C010000u;c.pr=0x8C020000u;c.gbr=0x8C030000u;
    c.mach=seed;c.macl=seed+1;c.fpul=seed+2;c.fpscr=fpscr_dn_mask;
    c.vbr=0x8C000000u;c.exception_generation=seed;
    c.trap_pending=(seed%7u)==0u; // generation, not level, detects a new exception
}
template<class Registers>
void arithmetic(Registers& r,CpuState& c,unsigned depth) {
    r[0]+=r[5]^(0x9E3779B9u+depth);
    r[5]=(r[5]<<3u)|(r[5]>>29u);
    r[8]^=r[0];r[15]-=4u;
    r.pr()=0x8C100000u+depth*4u;
    r.gbr()+=4u;r.mach()^=r[8];r.macl()+=r[5];r.fpul()=r[0];
    r.t()=(r[0]&1u)!=0u;
    ++c.attempted_guest_instructions;++c.retired_guest_instructions;
    c.pending_guest_cycles+=7u;
}
using Observations=std::vector<Snapshot>;
void original(CpuState&,unsigned,unsigned,Observations*);
template<bool Count> void prepared(Bank<Count>&,unsigned,unsigned,Observations*);

void reenter(CpuState& cpu,bool private_abi) {
    if(private_abi) {
        Bank bank(cpu);
        prepared(bank,2,0,nullptr);
    } else original(cpu,2,0,nullptr);
}

// Actual public ABI operations: inspect published registers, mutate register
// banks, enter a real SH-4 exception, or call a public AOT root reentrantly.
void external(CpuState& c,unsigned mode,bool private_abi,Observations* log) {
    if(log)log->push_back(snapshot(c));
    c.r[4]^=c.r[0];c.pr+=2;c.fpul+=19;c.t=!c.t;
    switch(mode) {
    case 1: break;
    case 2: c.write_sr(c.read_sr()^sr_rb_mask);break;
    case 3: c.toggle_fpu_register_bank();break;
    case 4: raise_illegal_instruction(c,0x8C012346u);break;
    case 5: throw std::runtime_error("host callback failure");
    case 6: reenter(c,private_abi);break;
    case 7: original(c,2,0,nullptr);break; // unchanged AOT callee
    case 8: {
        CpuState other{.memory=Memory{0u}};initialize(other,91);
        const auto before=snapshot(c);
        reenter(other,private_abi);
        require(snapshot(c)==before,"another CPU changed caller registers");
        c.r[1]^=other.r[0];break;
    }
    default:throw std::runtime_error("unknown external mode");
    }
    if(log)log->push_back(snapshot(c));
}

[[gnu::noinline]] void original(CpuState& cpu,unsigned depth,unsigned mode,Observations* log) {
    OriginalRegisters r(cpu);
    const auto generation=cpu.exception_generation;
    arithmetic(r,cpu,depth);
    if(depth) {
        r.flush_release();
        original(cpu,depth-1,mode,log);
        if(cpu.exception_generation!=generation)return;
        r.reload_acquire();
    } else if(mode==9) {
        throw std::runtime_error("private body failure");
    } else if(mode) {
        r.flush_release();
        external(cpu,mode,false,log);
        if(cpu.exception_generation!=generation)return;
        r.reload_acquire();
    }
    r[2]^=r[0];r[15]+=4u;r.macl()+=3u;
}
template<bool Count>
[[gnu::noinline]] void prepared(Bank<Count>& r,unsigned depth,unsigned mode,Observations* log) {
    auto& cpu=r.cpu();
    const auto generation=cpu.exception_generation;
    arithmetic(r,cpu,depth);
    if(depth) {
        prepared(r,depth-1,mode,log);
        if(cpu.exception_generation!=generation)return;
    } else if(mode==9) {
        throw std::runtime_error("private body failure");
    } else if(mode) {
        typename Bank<Count>::PublicBoundary boundary(r);
        external(cpu,mode,true,log);
        if(!boundary.resume_if_no_new_exception())return;
    }
    r[2]^=r[0];r[15]+=4u;r.macl()+=3u;
}
void differential() {
    for(unsigned seed=1;seed<=64;++seed)for(unsigned depth=0;depth<8;++depth)
    for(unsigned mode=0;mode<=9;++mode) {
        CpuState a{.memory=Memory{0u}},b{.memory=Memory{0u}};
        initialize(a,seed);initialize(b,seed);
        Observations x,y;bool threw_a=false,threw_b=false;
        try {original(a,depth,mode,&x);}catch(const std::runtime_error&){threw_a=true;}
        try {Bank bank(b);prepared(bank,depth,mode,&y);}
        catch(const std::runtime_error&){threw_b=true;}
        require(threw_a==threw_b,"host exception outcome differs");
        require(threw_a==(mode==5 || mode==9),"unexpected component failure");
        require(x==y,"public-boundary observation differs");
        require(snapshot(a)==snapshot(b),"CPU/exception/accounting result differs");
        ++cases;
    }
}
void transfer_proof() {
    CpuState cpu{.memory=Memory{0u}};initialize(cpu,17);
    Bank<true> bank(cpu);
    prepared(bank,31,0,nullptr);
    require(bank.transfers().imports==1 && bank.transfers().publications==0,
            "a private callee copied registers");
    bank.publish_release();bank.publish_release();
    require(bank.transfers().publications==1,"release duplicated publication");
    {
        Bank<true>::PublicBoundary boundary(bank);
        cpu.r[0]=12345;
        require(boundary.resume_if_no_new_exception(),"normal public resume rejected");
        require(bank[0]==12345 && bank.transfers().imports==2,"public changes not imported");
    }
    {
        Bank<true>::PublicBoundary boundary(bank);
        raise_illegal_instruction(cpu,0x8C012346u);
        const auto handler=snapshot(cpu);
        require(!boundary.resume_if_no_new_exception(),"exception resumed stale bank");
        bank.publish_release();
        require(snapshot(cpu)==handler && !bank.owned(),"handler state overwritten");
    }
    ++cases;
}

#include "procedure_register_fixture.inc"
void actual_body() {
    for(unsigned seed=0;seed<65536;++seed) {
        CpuState a{.memory=Memory{0u}},b{.memory=Memory{0u}};
        initialize(a,seed);initialize(b,seed);
        const std::uint32_t x=seed*0x98761u,y=seed*0x53u+0xABCD;
        {
            NativeAotRegisterFile<0x00FFu,0x03u> caller(a);
            caller[4]=x;caller[5]=y;caller.pr()=0xAC123456u;
            for(unsigned n=0;n<3;++n) {
                a.pc=seed&1u?0x8C055C8Eu:0xAC055C8Eu;
                caller.flush_release();
                retained_angle(a);
                caller.reload_acquire();
                caller[2]^=caller[0];caller[5]+=17u;
            }
        }
        {
            Bank<true> caller(b);
            caller[4]=x;caller[5]=y;caller.pr()=0xAC123456u;
            for(unsigned n=0;n<3;++n) {
                b.pc=seed&1u?0x8C055C8Eu:0xAC055C8Eu;
                private_angle(b,caller);
                caller[2]^=caller[0];caller[5]+=17u;
            }
            require(caller.transfers().imports==1 && caller.transfers().publications==0,
                    "real private body copied caller registers");
        }
        require(snapshot(a)==snapshot(b),"real partial-register AOT body differs");
        ++cases;
    }
}
double thread_ms() {
    timespec ts{};require(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&ts)==0,"CPU clock failed");
    return ts.tv_sec*1000.0+ts.tv_nsec/1e6;
}
void microbenchmark() {
    CpuState a{.memory=Memory{0u}},b{.memory=Memory{0u}};
    initialize(a,5);initialize(b,5);
    constexpr unsigned n=200000;
    const auto t0=thread_ms();
    for(unsigned i=0;i<n;++i)original(a,4,0,nullptr);
    const auto t1=thread_ms();
    for(unsigned i=0;i<n;++i){Bank bank(b);prepared(bank,4,0,nullptr);}
    const auto t2=thread_ms();
    require(snapshot(a)==snapshot(b),"microbenchmark result differs");
    std::printf("SONIC_PROCEDURE_REGISTER_MICRO roots=%u depth=5 original_ms=%.6f private_ms=%.6f checksum=%08x synthetic_only=1\n",
                n,t1-t0,t2-t1,a.r[0]);
}
}
int main() {
    try {
        differential();transfer_proof();actual_body();microbenchmark();
        std::printf("SONIC_PROCEDURE_REGISTERS_PASS cases=%u game_integration=0\n",cases);
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"SONIC_PROCEDURE_REGISTERS_FAIL %s cases=%u\n",e.what(),cases);
        return 1;
    }
}
