#include "sonic_fpu_scratch.hpp"
#include "katana/runtime/fpu.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <tuple>

static unsigned page_allocations=0;
void* operator new(std::size_t size) {
    // The CRT's aligned-vector allocation adds a small header to the table.
    if(size>=262144u)++page_allocations;
    if(auto* memory=std::malloc(size?size:1u))return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept {std::free(memory);}
void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
using namespace katana::runtime;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static auto result(const CpuState& cpu) {
    return std::tuple{cpu.fr,cpu.xf,cpu.r,cpu.fpscr,cpu.fpul,cpu.sr,cpu.pc,
        cpu.ssr,cpu.spc,cpu.sgr,cpu.expevt,cpu.trap_pending,cpu.exception_generation,
        cpu.last_exception_cause,cpu.last_exception_instruction_pc,
        cpu.last_exception_generation,cpu.sleeping};
}
static void compute(CpuState& cpu,std::uint32_t seed) {
    const HostFpuExecutionEpoch epoch(cpu);
    constexpr std::array words{0u,0x80000000u,1u,0x007fffffu,0x00800000u,
        0x3f000000u,0xbf800000u,0x3f800000u,0x7f7fffffu,0x7f800000u,
        0xff800000u,0x7fc12345u,0x7f812345u};
    for(unsigned i=0;i<16;++i) {
        cpu.fr[i]=words[(seed+i)%words.size()];
        cpu.xf[i]=i%5==0?0x3f800000u:0u;
    }
    fpu_transform_vector(cpu,0u);
    for(auto op:{FpuBinaryOperation::Multiply,FpuBinaryOperation::Add,
                 FpuBinaryOperation::Subtract,FpuBinaryOperation::Divide})
        fpu_binary(cpu,op,1u,0u);
}
int main() {
    try {
        sonic::FpuScratch slot;
        {sonic::FpuScratch::Lease warm(slot,0u);}
        require(page_allocations==1,"unexpected empty-memory allocation size");
        unsigned cases=0;
        for(auto fpscr:{0u,1u,0x40000u,0x40001u,0x100000u,0x80000u,0x003fffu}) {
            for(unsigned seed=0;seed<65;++seed) {
                CpuState fresh{.memory=Memory{0u,MemoryAlignmentPolicy::Permissive}};
                fresh.fpscr=fpscr;
                const auto before=page_allocations;
                sonic::FpuScratch::Lease reused(slot,fpscr);
                require(result(reused.get())==result(fresh),"scratch retained CPU state across models");
                compute(fresh,seed);compute(reused.get(),seed);
                require(result(reused.get())==result(fresh),"scratch changed FPU result or exception state");
                require(page_allocations==before,"warm scratch allocated a page lookup table");
                reused.get().r.fill(0xdeadbeefu);reused.get().fpul=0x55u;
                reused.get().pc=0x1234u;reused.get().sleeping=true;
                ++cases;
            }
        }
        {
            sonic::FpuScratch::Lease parent(slot,0x40000u);
            parent.get().fr[6]=0x12345678u;
            {sonic::FpuScratch::Lease nested(slot,0u);
             require(&nested.get()!=&parent.get(),"reentrant draw reused active parent");
             nested.get().fr[6]=0x99u;}
            require(parent.get().fr[6]==0x12345678u,"reentrant draw corrupted parent");
        }
        std::cout<<"FPU_SCRATCH_PASS cases="<<cases<<" nested=1 warm_page_allocations=0\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
