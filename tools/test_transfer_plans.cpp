#include "sonic_prepared_transfers.hpp"
#include <cstdio>
#include <cstdlib>
using namespace sonic::dispatch;
static void require(bool ok) { if (!ok) std::abort(); }
int main() {
    PreparedTransfers<unsigned, 4> cache;
    TransferEpoch epoch{};
    TransferKey key{0x8c010000, 0x8c010008, 0x8c020000, 0x8c01000c, true};
    require(!cache.find(epoch, key));
    cache.remember(epoch, key, 42);
    require(cache.find(epoch, key) && *cache.find(epoch, key) == 42);
    for (unsigned field = 0; field < 5; ++field) {
        auto other = key;
        switch (field) {
        case 0: other.source ^= 0x20000000; break;
        case 1: other.callsite += 2; break;
        case 2: other.target ^= 0x20000000; break;
        case 3: other.continuation += 2; break;
        case 4: other.call = false; break;
        }
        require(!cache.find(epoch, other));
    }
    for (std::size_t i = 0; i < epoch.generations.size(); ++i) {
        cache.remember(epoch, key, 42);
        ++epoch.generations[i];
        require(!cache.find(epoch, key));
    }
    for (std::size_t i = 0; i < epoch.owners.size(); ++i) {
        cache.remember(epoch, key, 42);
        epoch.owners[i] = &cache;
        require(!cache.find(epoch, key));
    }
    cache.remember(epoch, key, 42);
    epoch.static_ready = !epoch.static_ready;
    require(!cache.find(epoch, key));
    cache.remember(epoch, key, 42);
    cache.reset();
    require(!cache.find(epoch, key));
    // More distinct targets than capacity must evict, never alias another plan.
    for (unsigned i = 0; i < 20; ++i) {
        auto next = key; next.target += i * 2;
        cache.remember(epoch, next, i);
    }
    for (unsigned i = 0; i < 20; ++i) {
        auto next = key; next.target += i * 2;
        if (const auto* plan = cache.find(epoch, next)) require(*plan == i);
    }
    std::puts("SONIC_TRANSFER_PLANS_TESTS_OK raw_keys=5 epoch_fields=20 reset=1 collisions=1");
}
