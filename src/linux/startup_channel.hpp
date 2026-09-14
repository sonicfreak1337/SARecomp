#pragma once
#include <array>
#include <cstdint>
namespace sonic::startup {
// Fixed, bounded messages over a private Unix socket inherited by the helper.
// No shared files, environment strings or global socket names contain status.
struct ProgressMessage {
    std::uint32_t version=1,label_size=0;
    std::uint64_t completed=0,total=0;
    std::array<char,256> label{};
};
}
