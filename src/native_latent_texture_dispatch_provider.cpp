#include "katana/runtime/native_port.hpp"
#include "katana/runtime/runtime.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

extern "C" katana::runtime::NativePortHookResult
sonic_native_named_texture_archive_load(
    katana::runtime::NativePortContext& context) noexcept;

namespace {

constexpr std::uint32_t latent_texture_dispatch_error_context = 0x53415630u;
constexpr std::uint32_t latent_texture_dispatch_error_contract = 0x53415631u;

class GuestReader final {
  public:
    explicit GuestReader(const katana::runtime::CpuState& cpu) noexcept
        : guard_(cpu.memory.direct_linear_memory_guard(false)) {}

    [[nodiscard]] bool u8(const std::uint32_t address,
                          std::uint8_t& value) const noexcept {
        std::uint32_t offset = 0u;
        if (!offset_of(address, sizeof(value), offset)) return false;
        value = guard_.read_bytes[offset];
        return true;
    }

    [[nodiscard]] bool u32(const std::uint32_t address,
                           std::uint32_t& value) const noexcept {
        std::uint32_t offset = 0u;
        if (!offset_of(address, sizeof(value), offset)) return false;
        if constexpr (std::endian::native == std::endian::little) {
            std::memcpy(&value, guard_.read_bytes + offset, sizeof(value));
        } else {
            value = static_cast<std::uint32_t>(guard_.read_bytes[offset]) |
                    static_cast<std::uint32_t>(guard_.read_bytes[offset + 1u])
                        << 8u |
                    static_cast<std::uint32_t>(guard_.read_bytes[offset + 2u])
                        << 16u |
                    static_cast<std::uint32_t>(guard_.read_bytes[offset + 3u])
                        << 24u;
        }
        return true;
    }

  private:
    [[nodiscard]] bool offset_of(const std::uint32_t address,
                                 const std::size_t bytes,
                                 std::uint32_t& offset) const noexcept {
        if (!guard_ || bytes == 0u) return false;
        const auto segment = address & 0xC0000000u;
        if (segment != 0u && segment != 0x80000000u) return false;
        const auto physical =
            katana::runtime::canonical_physical_address(address);
        if (physical < guard_.physical_base) return false;
        const auto relative = physical - guard_.physical_base;
        if (relative >= guard_.physical_span ||
            bytes > guard_.physical_span - relative)
            return false;
        offset = relative & guard_.backing_mask;
        return bytes <=
               static_cast<std::size_t>(guard_.backing_mask) + 1u - offset;
    }

    katana::runtime::DirectLinearMemoryGuard guard_;
};

[[nodiscard]] bool checked_add(const std::uint32_t base,
                               const std::uint32_t displacement,
                               std::uint32_t& result) noexcept {
    const auto value = static_cast<std::uint64_t>(base) + displacement;
    if (value > std::numeric_limits<std::uint32_t>::max()) return false;
    result = static_cast<std::uint32_t>(value);
    return true;
}

} // namespace

extern "C" katana::runtime::NativePortHookResult
sonic_native_latent_texture_archive_dispatch_809961ae(
    katana::runtime::NativePortContext& context) noexcept {
    if (context.cpu == nullptr)
        return {katana::runtime::NativePortHookAction::Abort, 0u,
                latent_texture_dispatch_error_context};
    try {
        // Exact PAL latent-module ABI. The displaced 66-byte owner calls
        // 0x8C090500, stores its bounded byte selector into *(r4 + 0x20) + 1,
        // selects one of three immutable archive-name/TEXLIST pairs and then
        // composes the already proven 0x8C099690 native SDK provider.
        GuestReader reader(*context.cpu);
        std::uint32_t selector_owner = 0u;
        std::uint32_t result_state_address = 0u;
        std::uint32_t result_selector_address = 0u;
        std::uint8_t selector = 0u;
        if (!reader.u32(0x8C754B30u, selector_owner) ||
            !checked_add(context.cpu->r[4], 0x20u,
                         result_state_address) ||
            !reader.u32(result_state_address, result_selector_address) ||
            !checked_add(result_selector_address, 1u,
                         result_selector_address) ||
            (selector_owner != 0u &&
             !reader.u8(selector_owner, selector)))
            throw std::runtime_error("latent-texture-dispatch-contract");

        katana::runtime::guest_write_u8(
            *context.cpu, result_selector_address, selector,
            katana::runtime::CodeWriteSource::Copy);
        switch (selector) {
        case 0u:
            context.cpu->r[4] = 0x0CB81098u;
            context.cpu->r[5] = 0x0CB929A4u;
            break;
        case 1u:
            context.cpu->r[4] = 0x0CB810ACu;
            context.cpu->r[5] = 0x0CB95A84u;
            break;
        case 2u:
            context.cpu->r[4] = 0x0CB810C0u;
            context.cpu->r[5] = 0x0CB98B64u;
            break;
        default:
            // 0x8C090500 leaves the dereferenced selector owner in r4. The
            // latent default edge skips 0x8C099690 and returns its signed-byte
            // selector unchanged; the final failed compare leaves T clear.
            context.cpu->r[4] = selector_owner;
            const auto signed_selector =
                std::bit_cast<std::int8_t>(selector);
            context.cpu->r[0] = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(signed_selector));
            context.cpu->t = false;
            return {katana::runtime::NativePortHookAction::Return, 0u, 0u};
        }
        // The identity-bound AL_GARDEN02.PRS module stores nbTexture=5 in
        // each of these three NJS_TEXLISTs. 0x8C099690 reloads that field at
        // +4 and its final CMP/GE leaves T set exactly when the signed count
        // is positive.
        // The composed native loader intentionally owns no SH-4 flags, so
        // preserve the original callee's observable T result here.
        const auto texlist_address = context.cpu->r[5];
        std::uint32_t texture_count = 0u;
        std::uint32_t texture_count_address = 0u;
        if (!checked_add(texlist_address, 4u, texture_count_address) ||
            !reader.u32(texture_count_address, texture_count))
            throw std::runtime_error("latent-texture-texlist-contract");
        const auto result = sonic_native_named_texture_archive_load(context);
        if (result.action == katana::runtime::NativePortHookAction::Return)
            context.cpu->t = std::bit_cast<std::int32_t>(texture_count) > 0;
        return result;
    } catch (const std::exception& error) {
        std::cerr << "SONIC_NATIVE_NAMED_TEXTURE failure=latent-dispatch "
                     "detail="
                  << error.what() << '\n';
    } catch (...) {
    }
    return {katana::runtime::NativePortHookAction::Abort, 0u,
            latent_texture_dispatch_error_contract};
}
