#pragma once
#include <cstdlib>
#include <cstring>

namespace sonic::input {
// Private measurement fixture only. Sample once per platform construction;
// never turn a normal controller session into a synthetic input session.
inline bool isolated_gameplay_input() noexcept {
    const auto equal=[](const char* name,const char* expected) {
        const auto* value=std::getenv(name);
        return value && std::strcmp(value,expected)==0;
    };
    return equal("SARECOMP_BENCHMARK_ISOLATED_INPUT","1") &&
        equal("KATANA_PORT_BACKGROUND_TEST","1") &&
        equal("KATANA_SONIC_GAMEPLAY_PROBE","1") &&
        equal("KATANA_SONIC_GAMEPLAY_INPUT_PROFILE","3");
}
}
