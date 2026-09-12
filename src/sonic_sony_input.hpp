#pragma once
#include "katana/runtime/native_port_platform.hpp"
#include <memory>
#include <span>

namespace sonic::sony {
namespace detail { struct SdlApi; }
struct Sample {
    std::uint64_t identity = 0; // Complete HID path, separate from SDK backend domain.
    bool dualsense = false;
    katana::runtime::NativePortGamepadState state{};
};

// Owns Sony HID input and output together. In particular, Bluetooth rumble may
// change the report format; WinMM must not remain that device's input owner.
class Backend {
public:
    explicit Backend(const detail::SdlApi* test_api = nullptr);
    ~Backend();
    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;
    bool initialize(); // Platform/main thread. Hidden runs never initialize HID.
    bool active() const noexcept;
    void update(); // Platform owner; also used while the native menu is open.
    std::span<const Sample> samples() const noexcept;
    int endpoint(std::uint64_t identity) const noexcept;
    // Worker-safe, gain already applied. Endpoint tokens are never reused.
    bool rumble(unsigned endpoint, std::uint16_t low, std::uint16_t high) noexcept;
    void shutdown(); // Join the external rumble worker before calling this.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
