#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace sonic::texture {
enum class Resolution : std::uint8_t { Bound, UnboundSentinel, Malformed };
using Descriptor = std::array<std::uint32_t, 17>;

// Retail 64E660 frees identity/resources, retaining TSP/TCW and the upper
// halfword at +66. Only the low halfword at +64 is the SDK reference count.
[[nodiscard]] constexpr bool released(const Descriptor& words) noexcept {
    return words[0] == 0xFFFFFFFFu && words[1] == 0xFFFFFFFFu &&
           words[4] == 0u && (words[16] & 0xFFFFu) == 0u;
}

struct Registry {
    std::uint32_t address_slot, count_slot, maximum_count;
};

// nullopt means an ordinary live descriptor: retain the existing resolver.
// The callbacks are lazy; normal draws do not scan ownership twice.
template<class Reader, class PvmOwner, class TextureSetOwner, class Resolve>
[[nodiscard]] std::optional<Resolution> sentinel(
    const Reader& reader, std::uint32_t descriptor, Registry registry,
    PvmOwner&& has_pvm_view, TextureSetOwner&& has_texture_set,
    Resolve&& resolve_texture) noexcept {
    const auto resolve = [&](Resolution unowned) {
        // A current SDK PVM view follows its live TEXNAME+8, including release.
        // It has the same precedence here as in the ordinary live resolver.
        // An overlapping native TextureSet must not resurrect a freed row or
        // turn its legitimate no-draw state into a failed cache lookup.
        if (has_pvm_view() || !has_texture_set()) return unowned;
        return resolve_texture() ? Resolution::Bound : Resolution::Malformed;
    };
    if (descriptor == 0u || descriptor == 0xFFFFFFFFu)
        return resolve(Resolution::UnboundSentinel);
    if (!reader.range(descriptor, 32u)) return std::nullopt;
    std::array<std::uint32_t, 8> prefix{};
    for (std::uint32_t i = 0; i < prefix.size(); ++i)
        if (!reader.u32(descriptor + i * 4u, prefix[i])) return Resolution::Malformed;
    // Native live descriptors also have word1 == -1; word0 owns the key.
    if (prefix[0] != 0xFFFFFFFFu) return std::nullopt;
    bool exact = prefix[1] == 0xFFFFFFFFu &&
        std::all_of(prefix.begin() + 2, prefix.end(), [](auto word) { return word == 0u; });
    if (!exact && prefix[1] == 0xFFFFFFFFu) {
        constexpr std::uint32_t stride = sizeof(Descriptor);
        std::uint32_t base = 0u, count = 0u;
        if (reader.u32(registry.address_slot, base) &&
            reader.u32(registry.count_slot, count) && base && count &&
            count <= registry.maximum_count && count <= 0xFFFFFFFFu / stride &&
            reader.range(base, count * stride)) {
            const auto physical_base = base & 0x1FFFFFFFu;
            const auto physical = descriptor & 0x1FFFFFFFu;
            if (physical >= physical_base && physical - physical_base < count * stride &&
                (physical - physical_base) % stride == 0u) {
                Descriptor words{};
                bool complete = true;
                for (std::uint32_t i = 0; i < words.size(); ++i)
                    if (!reader.u32(descriptor + i * 4u, words[i])) { complete = false; break; }
                exact = complete && released(words);
            }
        }
    }
    return resolve(exact ? Resolution::UnboundSentinel : Resolution::Malformed);
}
} // namespace sonic::texture
