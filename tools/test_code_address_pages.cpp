#include "sonic_code_address_pages.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <thread>
#include <time.h>
#include <vector>

using namespace katana::runtime;
namespace pages = sonic::code_address_pages;
namespace {
std::uint64_t checks = 0;
void require(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAILED: %s\n", message); std::abort(); }
}
struct Entry {
    CodeAddressMapping mapping;
    std::unique_ptr<ScopedCodeAddressMapping> scope;
};
std::uint32_t oracle(const std::vector<Entry>& stack, std::uint32_t address, bool reverse) {
    for (auto it=stack.rbegin(); it!=stack.rend(); ++it) {
        if (!it->scope) continue;
        const auto& m=it->mapping;
        const auto start=reverse?m.runtime_start:m.source_start;
        if (std::uint64_t(address)>=start && std::uint64_t(address)<std::uint64_t(start)+m.extent)
            return address-start+(reverse?m.source_start:m.runtime_start);
    }
    return address;
}
void compare(const std::vector<Entry>& stack,std::uint32_t address) {
    const auto expected=oracle(stack,address,false), reversed=oracle(stack,address,true);
    require(pages::relocate(address)==expected,"forward page translation differs");
    require(pages::unrelocate(address)==reversed,"reverse page translation differs");
    require(relocate_code_address_inline(address)==expected,"original forward differs");
    require(unrelocate_code_address_inline(address)==reversed,"original reverse differs");
    ++checks;
}
void inspect(const std::vector<Entry>& stack,std::mt19937& random) {
    // Both ends of every touched page, mapping boundaries, P1/P2 aliases and
    // the full 32-bit boundary are checked after each arbitrary scope mutation.
    for(auto address:{0u,0xFFFFFFFFu,0x8C000000u,0xAC000000u,0x8C040784u,0x8C057FEAu})compare(stack,address);
    for(const auto& e:stack)for(auto start:{e.mapping.source_start,e.mapping.runtime_start}) {
        for(auto address:{start,start-1u,start+e.mapping.extent,start+e.mapping.extent-1u})compare(stack,address);
        const auto first=std::uint64_t(start)>>16u;
        const auto last=(std::uint64_t(start)+e.mapping.extent-1u)>>16u;
        for(auto page=first;page<=last;++page) {
            compare(stack,std::uint32_t(page<<16u));
            compare(stack,std::uint32_t((page<<16u)+0xFFFFu));
        }
    }
    for(unsigned i=0;i<128;++i)compare(stack,random());
}
void correctness() {
    std::mt19937 random(0x53415243u);
    std::vector<Entry> stack;
    inspect(stack,random);
    const std::array<CodeAddressMapping,10> special{{
        {0x8C040000u,0x8D140000u,0x50000u},
        {0x8C051234u,0x8F008765u,0x19876u},
        {0x8C070000u,0xAC070000u,0x10000u},
        {0xAC070123u,0x8C081234u,0x30000u},
        {0x8D140000u,0x8C040000u,0x20000u},
        {0x20000u,0x1FFFFu,0x40000u}, // delta -1 uses the exact fallback
        {0xFFFFFFFFu,0u,1u},
        {0u,0xFFFF0000u,0x10000u},
        {0xFFFE0000u,0xFFFF0000u,0x10000u},
        {0xFFF00000u,0xFFE00000u,0x100000u}}};
    for(auto m:special) {stack.push_back({m,std::make_unique<ScopedCodeAddressMapping>(m)});inspect(stack,random);}
    // Non-LIFO retirement beneath live, partly overlapping mappings.
    for(auto i:{0u,4u,2u,7u,5u,1u,9u,3u,6u,8u}) {stack[i].scope.reset();inspect(stack,random);}
    stack.clear();
    for(unsigned step=0;step<500;++step) {
        if(stack.empty() || (stack.size()<24 && (random()%3u))) {
            const auto source=std::uint32_t(0x8C000000u+(random()%0x100000u));
            const auto target=std::uint32_t(0x8D000000u+(random()%0x100000u));
            const CodeAddressMapping m{source,target,std::uint32_t(1u+random()%0x30000u)};
            stack.push_back({m,std::make_unique<ScopedCodeAddressMapping>(m)});
        } else {
            const auto index=random()%stack.size();
            stack.erase(stack.begin()+index);
        }
        inspect(stack,random);
    }
    stack.clear();inspect(stack,random);
    for(auto bad:std::array<CodeAddressMapping,3>{{{0,0,0},{0xFFFFFFFFu,0,2},{0,0xFFFFFFFFu,2}}}) {
        bool rejected=false;
        try {ScopedCodeAddressMapping invalid(bad);}catch(const std::exception&){rejected=true;}
        require(rejected,"invalid scope accepted");inspect(stack,random);
    }
    {ScopedCodeAddressMapping outer({0x8C000000u,0x8D000000u,0x100000u});
        std::thread worker([]{
            require(pages::relocate(0x8C040784u)==0x8C040784u,"mapping leaked between threads");
            ScopedCodeAddressMapping own({0x8C000000u,0x8E000000u,0x100000u});
            require(pages::relocate(0x8C040784u)==0x8E040784u,"worker map missing");
        });worker.join();
        require(pages::relocate(0x8C040784u)==0x8D040784u,"worker changed parent map");
    }
}
std::uint64_t ns() {timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return std::uint64_t(t.tv_sec)*1000000000u+t.tv_nsec;}
template<bool Page> __attribute__((noinline)) std::uint64_t work(unsigned count) {
    std::uint64_t result=0;
    for(unsigned i=0;i<count;++i) {
        // Literal PCs model generated owner code; volatile iteration prevents
        // the compiler from replacing the representative workload with a sum.
        asm volatile("" ::: "memory");
        if constexpr(Page) {
            result+=pages::relocate(0x8C040784u);result+=pages::relocate(0x8C057FEAu);
            result+=pages::relocate(0x8C036BC0u);result+=pages::relocate(0x82F484A4u);
        } else {
            result+=relocate_code_address_inline(0x8C040784u);result+=relocate_code_address_inline(0x8C057FEAu);
            result+=relocate_code_address_inline(0x8C036BC0u);result+=relocate_code_address_inline(0x82F484A4u);
        }
    }
    return result;
}
void measure(const char* state) {
    const auto start=ns();const auto old=work<false>(1000000);const auto middle=ns();
    const auto fast=work<true>(1000000);const auto end=ns();
    require(old==fast,"benchmark checksum differs");
    std::printf("CODE_PAGES_COST state=%s original_ns=%llu pages_ns=%llu checksum=%llu\n",state,
        (unsigned long long)(middle-start),(unsigned long long)(end-middle),(unsigned long long)old);
}
}
int main(){
    correctness();
    std::printf("SONIC_CODE_PAGES_OK checks=%llu mappings=exact overlaps=exact aliases=exact threads=exact\n",(unsigned long long)checks);
    measure("identity");
    ScopedCodeAddressMapping module({0x8C000000u,0x8D000000u,0x100000u});measure("module");
    ScopedCodeAddressMapping nested({0x8C050000u,0x8E070000u,0x10000u});measure("nested");
}
