#pragma once
#include "katana/runtime/runtime.hpp"
#include <array>
#include <cstdint>
#include <type_traits>

namespace sonic::procedure_registers {

// Private ABI for explicitly prepared, authenticated procedures only.
// One explicit carrier belongs to one CpuState and one public invocation.
// A prepared closure must redirect EVERY GPR/scalar access to this carrier,
// including accesses not covered by the original NativeAotRegisterFile mask.
// FR/XF, SR, PC, exceptions and instruction/cycle accounting remain in CpuState.
template<bool CountTransfers = false> class Frame;
template<bool CountTransfers = false>
class Bank final {
public:
    struct Transfers { std::uint64_t imports = 0, publications = 0; };

    explicit Bank(katana::runtime::CpuState& cpu) noexcept : cpu_(cpu) { acquire(); }
    ~Bank() { publish_release(); }
    Bank(const Bank&) = delete;
    Bank& operator=(const Bank&) = delete;

    // These references may only be used while owned(). The authored closure
    // must end reference lifetimes before a public boundary or register-bank
    // change. There is deliberately no process-global or thread-local borrowing.
    std::uint32_t& operator[](std::size_t i) noexcept { return r_[i]; }
    bool& t() noexcept { return t_; }
    std::uint32_t& pr() noexcept { return pr_; }
    std::uint32_t& gbr() noexcept { return gbr_; }
    std::uint32_t& mach() noexcept { return mach_; }
    std::uint32_t& macl() noexcept { return macl_; }
    std::uint32_t& fpul() noexcept { return fpul_; }
    bool owned() const noexcept { return owned_; }
    katana::runtime::CpuState& cpu() noexcept { return cpu_; }
    Transfers transfers() const noexcept {
        if constexpr (CountTransfers) return transfers_;
        else return {};
    }

    // Raw generated accesses also occur inside released FPU/memory/exception
    // windows. Follow the actual owner, never reacquire merely to access them.
    std::uint32_t& raw_r(std::size_t i) noexcept { return owned_ ? r_[i] : cpu_.r[i]; }
    bool& raw_t() noexcept { return owned_ ? t_ : cpu_.t; }
    std::uint32_t& raw_pr() noexcept { return owned_ ? pr_ : cpu_.pr; }
    std::uint32_t& raw_gbr() noexcept { return owned_ ? gbr_ : cpu_.gbr; }
    std::uint32_t& raw_mach() noexcept { return owned_ ? mach_ : cpu_.mach; }
    std::uint32_t& raw_macl() noexcept { return owned_ ? macl_ : cpu_.macl; }
    std::uint32_t& raw_fpul() noexcept { return owned_ ? fpul_ : cpu_.fpul; }

    void publish_release() noexcept {
        if (!owned_) return;
        cpu_.r = r_;
        cpu_.t = t_;
        cpu_.pr = pr_;
        cpu_.gbr = gbr_;
        cpu_.mach = mach_;
        cpu_.macl = macl_;
        cpu_.fpul = fpul_;
        owned_ = false;
        if constexpr (CountTransfers) ++transfers_.publications;
    }

    // A boundary never automatically reloads on destruction. An exception or
    // aborted continuation must leave the handler's CpuState authoritative.
    // The caller must additionally retain the original PC, memory-generation,
    // scheduling, executable-owner and depth checks before resuming its body.
    class PublicBoundary final {
    public:
        explicit PublicBoundary(Bank& bank) noexcept
            : bank_(bank), exception_generation_(bank.cpu_.exception_generation) {
            bank_.publish_release();
        }
        PublicBoundary(const PublicBoundary&) = delete;
        PublicBoundary& operator=(const PublicBoundary&) = delete;
        bool resume_if_no_new_exception() noexcept {
            if (bank_.cpu_.exception_generation != exception_generation_) return false;
            bank_.acquire();
            return true;
        }
    private:
        Bank& bank_;
        std::uint64_t exception_generation_;
    };

private:
    friend class Frame<CountTransfers>;
    void acquire() noexcept {
        if (owned_) return;
        r_ = cpu_.r;
        t_ = cpu_.t;
        pr_ = cpu_.pr;
        gbr_ = cpu_.gbr;
        mach_ = cpu_.mach;
        macl_ = cpu_.macl;
        fpul_ = cpu_.fpul;
        owned_ = true;
        if constexpr (CountTransfers) ++transfers_.imports;
    }
    katana::runtime::CpuState& cpu_;
    std::array<std::uint32_t,16> r_;
    std::uint32_t pr_, gbr_, mach_, macl_, fpul_;
    bool t_, owned_ = false;
    struct NoTransfers {};
    [[no_unique_address]] std::conditional_t<CountTransfers,Transfers,NoTransfers> transfers_{};
};

// Local ownership and shared-bank validity are different. A private call gives
// up the caller's local ownership without publishing; its callee may subsequently
// release the bank at a real boundary. Original continuation guards must pass
// before reload_acquire. No destructor publishes an older caller snapshot.
template<bool CountTransfers>
class Frame final {
public:
    explicit Frame(Bank<CountTransfers>& bank) noexcept : bank_(bank) { bank_.acquire(); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    std::uint32_t& operator[](std::size_t i) noexcept { return bank_[i]; }
    bool& t() noexcept { return bank_.t(); }
    std::uint32_t& pr() noexcept { return bank_.pr(); }
    std::uint32_t& gbr() noexcept { return bank_.gbr(); }
    std::uint32_t& mach() noexcept { return bank_.mach(); }
    std::uint32_t& macl() noexcept { return bank_.macl(); }
    std::uint32_t& fpul() noexcept { return bank_.fpul(); }
    bool owns_registers() const noexcept { return owned_ && bank_.owned(); }
    void suspend_for_private_call() noexcept { owned_ = false; }
    void flush_release() noexcept { bank_.publish_release(); owned_ = false; }
    void reload_acquire() noexcept { bank_.acquire(); owned_ = true; }
private:
    Bank<CountTransfers>& bank_;
    bool owned_ = true;
};

} // namespace sonic::procedure_registers
