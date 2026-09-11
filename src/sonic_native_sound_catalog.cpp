#include "sonic_native_sound_catalog.hpp"

#include <algorithm>
#include <array>

namespace sonic_native {
namespace {

struct SonicNativeSoundCollectionCatalogEntry final {
    std::string_view logical_id;
    std::string_view content_relative_path;
    std::string_view byte_identity;
    std::uint64_t byte_size = 0u;
};

constexpr std::array catalog_entries{
#include "sonic-native-sound-catalog.inc"
};

constexpr auto public_catalog = [] {
    std::array<SoundCollectionBinding, catalog_entries.size()> result{};
    for (std::size_t index = 0u; index < result.size(); ++index) {
        const auto& source = catalog_entries[index];
        result[index] = {index,
                         source.logical_id,
                         source.content_relative_path,
                         source.byte_identity,
                         source.byte_size};
    }
    return result;
}();

[[nodiscard]] constexpr char normalized_path_character(const char value) noexcept {
    if (value == '\\') return '/';
    return value >= 'a' && value <= 'z'
               ? static_cast<char>(value - ('a' - 'A'))
               : value;
}

[[nodiscard]] std::string_view trim_title_path(
    std::string_view value) noexcept {
    while (value.starts_with("./") || value.starts_with(".\\"))
        value.remove_prefix(2u);
    while (!value.empty() && (value.front() == '/' || value.front() == '\\'))
        value.remove_prefix(1u);
    return value;
}

[[nodiscard]] bool path_equal(std::string_view left,
                              std::string_view right) noexcept {
    left = trim_title_path(left);
    right = trim_title_path(right);
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0u; index < left.size(); ++index)
        if (normalized_path_character(left[index]) !=
            normalized_path_character(right[index]))
            return false;
    return true;
}

[[nodiscard]] std::string_view path_basename(std::string_view value) noexcept {
    value = trim_title_path(value);
    const auto separator = value.find_last_of("/\\");
    return separator == std::string_view::npos ? value
                                                : value.substr(separator + 1u);
}

} // namespace

std::optional<SoundCollectionBinding> find_sound_collection_by_identity(
    const std::string_view byte_identity,
    const std::uint64_t byte_size) noexcept {
    const auto match = std::ranges::find_if(
        public_catalog, [&](const auto& candidate) {
            return candidate.byte_size == byte_size &&
                   candidate.byte_identity == byte_identity;
        });
    if (match == public_catalog.end()) return std::nullopt;
    return *match;
}

std::optional<SoundCollectionBinding> find_sound_collection_by_title_path(
    const std::string_view title_path) noexcept {
    if (title_path.empty()) return std::nullopt;
    for (const auto& candidate : public_catalog)
        if (path_equal(candidate.content_relative_path, title_path))
            return candidate;

    const auto basename = path_basename(title_path);
    std::optional<SoundCollectionBinding> result;
    for (const auto& candidate : public_catalog) {
        // The stable logical id intentionally contains no retail filename.
        // Title calls, however, commonly pass only an MLT basename. Resolve
        // that basename against the identity-bound catalog path and retain
        // fail-closed ambiguity handling across the complete collection set.
        if (!path_equal(candidate.logical_id, basename) &&
            !path_equal(path_basename(candidate.content_relative_path),
                        basename))
            continue;
        if (result.has_value()) return std::nullopt;
        result = candidate;
    }
    return result;
}

std::span<const SoundCollectionBinding> sound_collection_catalog() noexcept {
    return public_catalog;
}

} // namespace sonic_native
