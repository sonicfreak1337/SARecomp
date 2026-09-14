#pragma once
#include "sonic_input.hpp"
#include "sonic_presentation.hpp"
#include "katana/runtime/native_port.hpp"
#include <functional>
#include <optional>
namespace sonic::menu {
struct Services {
    std::function<void()> suspend_audio;
    std::function<void(std::uint64_t)> resume; // also excludes modal time from guest clocks
    std::function<void(bool)> apply_audio;
    std::function<bool(unsigned)> original_transition;
};
void initialize(const std::filesystem::path& executable);
void request_open() noexcept;
bool available() noexcept;
int effective_text_language() noexcept;
bool observe(katana::runtime::NativePortContext&,const input::Snapshot&,bool suppressed);
bool original_options_ready(katana::runtime::NativePortContext&) noexcept;
bool pending(katana::runtime::NativePortContext&) noexcept;
bool run(katana::runtime::NativePortContext&,const Services&);
std::optional<presentation::Settings> take_restart();
int restart_language() noexcept;
const presentation::Settings& restart_baseline() noexcept;
}
