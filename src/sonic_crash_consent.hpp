#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <string_view>

namespace sonic::diagnostics {
// The fault owner serializes into reserved memory. No file, directory, stderr
// redirect or upload receives this data before the user selects Export.
class PendingCrash final {
    std::array<char,262144> bytes_{};
    std::size_t size_=0;
    bool started_=false;
    std::atomic<bool> ready_{false};
public:
    void begin() noexcept {size_=0;started_=true;ready_.store(false,std::memory_order_relaxed);}
    void append(std::string_view text) noexcept {
        if(!started_ || ready_.load(std::memory_order_relaxed))return;
        const auto count=std::min(text.size(),bytes_.size()-size_);
        std::memcpy(bytes_.data()+size_,text.data(),count);size_+=count;
    }
    void finish() noexcept {ready_.store(true,std::memory_order_release);}
    [[nodiscard]] std::string_view view() const noexcept {
        return ready_.load(std::memory_order_acquire)?std::string_view(bytes_.data(),size_):std::string_view{};
    }
};
inline PendingCrash pending_crash;
}
