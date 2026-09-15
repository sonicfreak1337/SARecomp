#pragma once
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace sonic::diagnostics {
// Initialized once by main, before any game/renderer/audio threads start.
// No file access, environment lookup or synchronization on the hot path.
inline bool internal_runtime_enabled = false;
inline bool runtime_checks_enabled() noexcept { return internal_runtime_enabled; }
inline constexpr std::string_view internal_policy_name = ".sarecomp-diagnostics";
inline constexpr std::string_view internal_policy_on = "SARECOMP-DIAGNOSTICS-1\non\n";
inline constexpr std::string_view internal_policy_off = "SARECOMP-DIAGNOSTICS-1\noff\n";

inline bool read_internal_policy(const std::filesystem::path& program,
                                 const char* override_value) noexcept {
    if (override_value) return std::string_view(override_value) == "1";
    try {
        std::ifstream input(program.parent_path()/internal_policy_name, std::ios::binary);
        char bytes[64]{};
        input.read(bytes, sizeof(bytes));
        return std::string_view(bytes, static_cast<std::size_t>(input.gcount())) == internal_policy_on;
    } catch (...) { return false; }
}
inline void initialize_internal_policy(const std::filesystem::path& program) noexcept {
    internal_runtime_enabled = read_internal_policy(program, std::getenv("SARECOMP_INTERNAL_DIAGNOSTICS"));
}
}
