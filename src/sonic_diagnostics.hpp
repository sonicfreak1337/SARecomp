#pragma once
#include <atomic>
#include <cstdint>
#include <string_view>
namespace sonic::diagnostics {
enum class Failure:unsigned {None,Graphics,Contract,Runtime,Restart,Unknown};
inline std::atomic<Failure> failure{Failure::None};
inline std::atomic<std::uint32_t> code{0};
inline std::atomic<std::uint64_t> frame{0};
// Set before host threads start; identities only, never paths or file contents.
inline std::string_view source_identity="unavailable",build_profile="unavailable";
inline void record(Failure kind,std::uint32_t value=0) noexcept {code=value;failure.store(kind,std::memory_order_release);}
inline std::string_view kind() noexcept {
    switch(failure.load(std::memory_order_acquire)){
    case Failure::None:return "none";case Failure::Graphics:return "graphics";case Failure::Contract:return "contract";
    case Failure::Runtime:return "runtime";case Failure::Restart:return "restart";default:return "unknown";
    }
}
}
