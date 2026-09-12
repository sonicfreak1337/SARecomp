#include "sonic_texture_sentinel.hpp"
#include "sonic_native_sdk_texture_release_plan.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

using namespace sonic::texture;
namespace sdk = sonic_native_sdk_release;
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Memory {
    std::uint32_t base = 0x8C7608B4u, count = 2048u;
    std::vector<std::uint32_t> rows = std::vector<std::uint32_t>(count * 17u);
    std::map<std::uint32_t, std::uint32_t> globals;
    Memory() {
        globals = {{sdk::registry_slot, base}, {sdk::registry_count_slot, count},
            {sdk::mask_slot, 0u}, {sdk::highwater_slot, 300u},
            {sdk::active_key_slot, 1000u}, {sdk::last_error_index_slot, 0u},
            {sdk::release_error_slot, 0u}};
    }
    bool range(std::uint32_t address, std::size_t bytes) const {
        address &= 0x1FFFFFFFu;
        return address >= (base & 0x1FFFFFFFu) &&
            std::uint64_t(address) + bytes <= (base & 0x1FFFFFFFu) + rows.size() * 4u;
    }
    bool u32(std::uint32_t address, std::uint32_t& value) const {
        address &= 0x1FFFFFFFu;
        if (address % 4u) return false;
        if (const auto at = globals.find(address); at != globals.end()) { value = at->second; return true; }
        if (!range(address, 4u)) return false;
        value = rows[(address - (base & 0x1FFFFFFFu)) / 4u]; return true;
    }
    void put(std::uint32_t address, std::uint32_t value) {
        address &= 0x1FFFFFFFu;
        if (range(address, 4u)) rows[(address - (base & 0x1FFFFFFFu)) / 4u] = value;
        else globals.at(address) = value;
    }
    void row(std::uint32_t address, const Descriptor& words) {
        for (std::uint32_t i = 0u; i < words.size(); ++i) put(address + i * 4u, words[i]);
    }
};
struct Owners {
    bool pvm = true, texture_set = true, native_handle = false;
    unsigned pvm_checks = 0u, set_checks = 0u, resolves = 0u;
    auto resolve(const Memory& memory, std::uint32_t address) {
        return sentinel(memory, address, {sdk::registry_slot, sdk::registry_count_slot, 4096u},
            [&]() { ++pvm_checks; return pvm; },
            [&]() { ++set_checks; return texture_set; },
            [&]() { ++resolves; return native_handle; });
    }
};
}
int main() try {
    // Complete selected descriptor from session 1789238274496 / PID 3840,
    // frame 16056, Tails Casinopolis. Owner header and current PVM both match;
    // the native TextureSet has one lease but no live SDK texture to resolve.
    constexpr std::uint32_t address = 0x8C765028u;
    const Descriptor crash{0xFFFFFFFFu, 0xFFFFFFFFu, 0u, 0x08000000u};
    Memory memory; memory.row(address, crash);
    const auto initial_rows = memory.rows;
    const auto initial_globals = memory.globals;
    Owners overlap;
    require(overlap.resolve(memory, address) == Resolution::UnboundSentinel, "capsule regression: released PVM aborts");
    require(overlap.resolves == 0u && overlap.set_checks == 0u, "freed PVM fell through to native cache");
    overlap.native_handle = true;
    require(overlap.resolve(memory, address) == Resolution::UnboundSentinel, "freed PVM resurrected cached texture");
    require(memory.rows == initial_rows && memory.globals == initial_globals, "resolution changed guest memory");

    // The same released resource shape occurs throughout SDK release families.
    // Run the production SDK plan, then the production draw-state classifier.
    unsigned release_cases = 0u;
    for (auto bookkeeping : {0u, 0xA55A0000u}) for (auto tsp : {0x08000000u, 0x88000000u}) {
        Descriptor live{1000u, 0xFFFFFFFFu, 27u, tsp, 2u};
        live[16] = bookkeeping | 1u;
        memory.row(address, live);
        Owners ordinary;
        require(!ordinary.resolve(memory, address), "live native descriptor mistaken for sentinel");
        require(ordinary.pvm_checks == 0u && ordinary.set_checks == 0u, "normal draw rescanned owners");
        const auto plan = sdk::make_plan(std::array{address}, memory.base, memory.count,
            [&](auto at, auto& word) { return memory.u32(at, word); },
            [&](auto at, const auto&) { return at == (address & 0x1FFFFFFFu); });
        require(plan.freed_rows.size() == 1u && plan.result == 1u, "release fixture did not free row");
        for (const auto& word : plan.writes) memory.put(word.address, word.after);
        std::uint32_t actual = 0u;
        require(memory.u32(address + 12u, actual) && actual == tsp, "SDK release lost TSP bookkeeping");
        require(memory.u32(address + 64u, actual) && actual == bookkeeping, "SDK release lost upper halfword");
        Owners freed;
        require(freed.resolve(memory, address) == Resolution::UnboundSentinel, "released SDK row aborts draw");
        require(freed.resolve(memory, address ^ 0x20000000u) == Resolution::UnboundSentinel, "P1/P2 alias differs");
        ++release_cases;
    }
    memory.row(address, crash);
    Owners static_owner; static_owner.pvm = false; static_owner.native_handle = true;
    require(static_owner.resolve(memory, 0u) == Resolution::Bound, "descriptorless native TextureSet lost");
    require(static_owner.resolve(memory, address) == Resolution::Bound, "native-only TextureSet precedence changed");
    Owners unowned; unowned.pvm = unowned.texture_set = false;
    require(unowned.resolve(memory, address) == Resolution::UnboundSentinel, "unowned released row rejected");
    for (auto pointer : {0u, 0xFFFFFFFFu}) {
        Owners pvm;
        require(pvm.resolve(memory, pointer) == Resolution::UnboundSentinel, "PVM acquisition/release sentinel rejected");
    }
    auto damaged = crash; damaged[16] = 1u; memory.row(address, damaged);
    require(overlap.resolve(memory, address) == Resolution::Malformed, "live references accepted as released");
    damaged = crash; damaged[4] = 2u; memory.row(address, damaged);
    require(overlap.resolve(memory, address) == Resolution::Malformed, "live resource accepted as released");
    memory.row(address, crash); memory.globals[sdk::registry_count_slot] = 269u;
    require(overlap.resolve(memory, address) == Resolution::Malformed, "one-past-end registry row accepted");
    memory.globals[sdk::registry_count_slot] = memory.count;
    memory.row(address + 4u, crash);
    require(overlap.resolve(memory, address + 4u) == Resolution::Malformed, "unaligned registry row accepted");
    require(overlap.resolves == 0u, "malformed SDK resource was rescued by catalog");
    std::cout << "TEXTURE_SENTINEL_OK capsule=3840 frame=16056 sdk_release_cases=" << release_cases
              << " ownership=shared-pvm-native no_resurrection=1 malformed_rejected=1\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "TEXTURE_SENTINEL_FAIL " << error.what() << '\n'; return 1;
}
