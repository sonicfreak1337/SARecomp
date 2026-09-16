#include "sonic_read_test_fixture.hpp"
#include "sonic_fpu_region.hpp"
#include <chrono>
#include <xmmintrin.h>

using Op=FpuBinaryOperation;
using Region=sonic::fpu_region::BinaryRegion;
namespace {
std::uint64_t cases=0,accepted=0,rejected=0;
std::uint32_t rng=0x7B364C21;
std::uint32_t random_bits() {rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
struct Restore {unsigned value=_mm_getcsr();~Restore(){_mm_setcsr(value);}};
template<Op O>
bool candidate(CpuState& cpu,Region& region) {return region.binary<O,2,3>();}
bool candidate(CpuState& cpu,Region& region,Op op) {
    switch(op) {
    case Op::Add:return candidate<Op::Add>(cpu,region);
    case Op::Subtract:return candidate<Op::Subtract>(cpu,region);
    case Op::Multiply:return candidate<Op::Multiply>(cpu,region);
    case Op::Divide:return candidate<Op::Divide>(cpu,region);
    }
    throw std::runtime_error("bad operation");
}
void compare(Fixture& original,Fixture& changed,Op op,unsigned rm,std::uint32_t n,std::uint32_t m) {
    for(auto* f:{&original,&changed}) {
        f->cpu.fpscr=fpscr_dn_mask|rm|fpscr_cause_mask|((n^m)&fpscr_flag_mask);
        f->cpu.fr[2]=m;f->cpu.fr[3]=n;
    }
    const auto ambient=0x1F80u|((n&3u)<<13u)|0x8040u|0x25u;
    _mm_setcsr(ambient);
    {HostFpuExecutionEpoch epoch(original.cpu);fpu_binary(original.cpu,op,2,3);}
    require(_mm_getcsr()==ambient,"reference host boundary changed");
    {
        HostFpuExecutionEpoch epoch(changed.cpu);Region region(changed.cpu,epoch);
        if(candidate(changed.cpu,region,op)) ++accepted;else ++rejected;
    }
    require(_mm_getcsr()==ambient,"candidate host boundary changed");
    if(state(original.cpu)!=state(changed.cpu) || original.log!=changed.log) {
        std::cerr<<"op="<<unsigned(op)<<" rm="<<rm<<" n="<<std::hex<<n<<" m="<<m
                 <<" ref="<<original.cpu.fr[3]<<" candidate="<<changed.cpu.fr[3]
                 <<" flags="<<original.cpu.fpscr<<'/'<<changed.cpu.fpscr<<std::dec<<'\n';
        throw std::runtime_error("architectural state differs");
    }
    ++cases;
}
void sequences() {
    // Actual dependent arithmetic, mixed with retained operations/fallbacks.
    // Check every architectural step; only the enclosing host boundary is public.
    for(unsigned rm=0;rm<2;++rm)for(unsigned seed=0;seed<128;++seed) {
        Fixture a,b;
        for(auto* f:{&a,&b}) {
            f->cpu.fpscr=fpscr_dn_mask|rm;
            f->cpu.fr[3]=0x3F810001;f->cpu.fr[2]=0x3F700003;
        }
        using State=decltype(state(a.cpu));
        std::vector<State> states;
        const unsigned ambient=0x1FA5u|((seed&3u)<<13u)|0x8040u;
        const auto step=[seed](CpuState& cpu,unsigned i,Region* region){
            if(i%8==0) {cpu.fr[3]=0x3F010001u+seed;cpu.fr[2]=0x3F710003u+seed;}
            if(i!=0 && i%11==0) cpu.fr[2]=(i&1)?0x00000001u:0x7FC12345u;
            else if(i%7==0) cpu.fr[2]=0x3F800000u|((seed+i)*0x103u);
            if(i%5==0) fpu_square_root(cpu,3);
            else if(i%5==1) fpu_compare_greater(cpu,2,3);
            else if(i%5==2) fpu_multiply_accumulate(cpu,2,3);
            else if(region) candidate(cpu,*region,Op(i&3));
            else fpu_binary(cpu,Op(i&3),2,3);
        };
        _mm_setcsr(ambient);
        {HostFpuExecutionEpoch epoch(a.cpu);for(unsigned i=0;i<64;++i){step(a.cpu,i,nullptr);states.push_back(state(a.cpu));}}
        require(_mm_getcsr()==ambient,"reference sequence host state");
        {HostFpuExecutionEpoch epoch(b.cpu);Region region(b.cpu,epoch);
            for(unsigned i=0;i<64;++i){step(b.cpu,i,&region);require(state(b.cpu)==states[i],"dependent sequence differs");++cases;}}
        require(_mm_getcsr()==ambient,"candidate sequence host state");
    }
}
struct Input {std::uint32_t n,m;};
std::array<Input,1024> inputs;
volatile std::uint64_t sink=0;
template<Op O,bool Changed>
double measure(unsigned rm,unsigned group) {
    CpuState cpu{.memory=Memory{0u}};cpu.fpscr=fpscr_dn_mask|rm;
    const auto start=std::chrono::steady_clock::now();
    std::uint64_t fingerprint=0;
    for(unsigned block=0;block<262144/group;++block) {
        HostFpuExecutionEpoch epoch(cpu);Region region(cpu,epoch);
        for(unsigned i=0;i<group;++i) {
            const auto& in=inputs[(block*group+i)&1023];cpu.fr[3]=in.n;cpu.fr[2]=in.m;
            if constexpr(Changed) region.binary<O,2,3>();else fpu_binary(cpu,O,2,3);
            fingerprint+=cpu.fr[3];fingerprint^=cpu.fpscr;
        }
    }
    sink=fingerprint;
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
template<Op O>
void bench() {
    for(unsigned group:{2u,8u,32u})for(unsigned rm=0;rm<2;++rm) {
        std::array<double,5> a{},b{};
        for(unsigned round=0;round<5;++round) {
            std::uint64_t x,y;
            if(round&1) {b[round]=measure<O,true>(rm,group);y=sink;a[round]=measure<O,false>(rm,group);x=sink;}
            else {a[round]=measure<O,false>(rm,group);x=sink;b[round]=measure<O,true>(rm,group);y=sink;}
            require(x==y,"benchmark fingerprint differs");
        }
        std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());
        std::cout<<"FPU_REGION_KERNEL op="<<unsigned(O)<<" rm="<<rm<<" group="<<group
                 <<" reference_ms="<<a[2]<<" candidate_ms="<<b[2]<<" ratio="<<a[2]/b[2]<<'\n';
    }
}
}
int main(int argc,char** argv) {
    Restore restore;
    try {
        if(argc==2 && std::string(argv[1])=="--benchmark") {
            for(auto& in:inputs){auto n=random_bits(),m=random_bits();in={(n&0x807FFFFFu)|((120+n%15)<<23),(m&0x807FFFFFu)|((120+m%15)<<23)};}
            bench<Op::Add>();bench<Op::Subtract>();bench<Op::Multiply>();bench<Op::Divide>();return 0;
        }
        constexpr std::array values={0u,0x80000000u,1u,0x007FFFFFu,0x00800000u,0x00800001u,
            0x00FFFFFFu,0x01000000u,0x33800000u,0x33000000u,0x32800000u,0x24000000u,
            0x3EAAAAABu,0x3F000000u,0x3F7FFFFFu,0x3F800000u,0x3F800001u,0x40000000u,
            0x4B7FFFFFu,0x4B800000u,0x7E7FFFFFu,0x7F000000u,0x7F7FFFFFu,0x7F800000u,
            0xFF800000u,0x7F800001u,0x7FC12345u,0xFFC12345u};
        Fixture a,b;
        for(auto op:{Op::Add,Op::Subtract,Op::Multiply,Op::Divide})for(unsigned rm=0;rm<2;++rm) {
            for(auto n:values)for(auto m:values)for(unsigned signs=0;signs<4;++signs)
                compare(a,b,op,rm,n^((signs&1)?0x80000000u:0),m^((signs&2)?0x80000000u:0));
            for(unsigned i=0;i<32768;++i){const auto n=random_bits(),m=random_bits();compare(a,b,op,rm,n,m);}
        }
        sequences();
        std::cout<<"FPU_REGION_TEST_OK cases="<<cases<<" hardware="<<accepted<<" fallback="<<rejected
                 <<" cpu=exact host_boundaries=exact\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<"FPU_REGION_TEST_FAIL "<<e.what()<<'\n';return 1;}
}
