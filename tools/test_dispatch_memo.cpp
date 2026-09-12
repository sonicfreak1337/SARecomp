#include "sonic_dispatch_memo.hpp"
#include <iostream>
#include <stdexcept>

struct Entry { unsigned identity; bool primary; void (*function)(); };
static void body() {}
static void check(bool value) { if (!value) throw std::runtime_error("dispatch memo invariant"); }
int main() {
    sonic::dispatch::ImmutableSourceMemo<Entry> memo;
    constexpr Entry a{1,true,body}, b{2,false,body}, null_function{3,false,nullptr};
    constexpr unsigned address=0x8c054392u;
    unsigned collision=address+2;
    while (memo.slot_index(collision)!=memo.slot_index(address)) collision+=2;
    unsigned calls=0;
    auto lookup=[&](unsigned key)->const Entry* {
        ++calls;
        if (key==address) return &a;
        if (key==collision) return &b;
        if (key==(address^0x20000000u)) return &null_function;
        return nullptr;
    };
    check(memo.find<true>(address,lookup)==&a);
    check(memo.find<true>(address,lookup)==&a && calls==1);
    check(memo.find<true>(address,lookup)==&a && calls==1);
    check(memo.find<true>(collision,lookup)==&b);
    check(memo.find<true>(address,lookup)==&a); // conflict cannot return the other owner
    check(memo.find<true>(address^0x20000000u,lookup)==&null_function); // exact segment key
    check(memo.find<true>(address^0x20000000u,lookup)->function==nullptr);
    const auto before=calls;
    check(!memo.find<true>(1,lookup) && !memo.find<true>(1,lookup) && calls==before+2);
    const auto stats=memo.statistics;
    check(stats.hits>=3 && stats.conflicts && stats.missing==2);
    memo.reset();
    check(memo.find(address,lookup)==&a && memo.statistics.hits==0 && memo.statistics.misses==0);
    const auto prior=calls;
    memo.reset();
    check(memo.find(address,lookup)==&a && calls==prior+1);
    std::cout << "SONIC_DISPATCH_MEMO_OK exact_keys=1 conflicts=1 negative_uncached=1 null_function_preserved=1 scope_reset=1\n";
}
