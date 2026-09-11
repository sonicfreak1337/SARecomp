#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace sonic_native {

struct SoundCollectionBinding final {
    std::size_t catalog_index = 0u;
    std::string_view logical_id;
    std::string_view content_relative_path;
    std::string_view byte_identity;
    std::uint64_t byte_size = 0u;
};

// The catalog contains only private identity/path metadata. Guest MLT bytes
// must match one complete SHA-256 and exact structural extent before a native
// sound collection can be opened from the separately supplied content root.
[[nodiscard]] std::optional<SoundCollectionBinding>
find_sound_collection_by_identity(std::string_view byte_identity,
                                  std::uint64_t byte_size) noexcept;

// Guest file APIs may present either a content-relative path or a unique MLT
// basename and may use either slash style. Matching is ASCII case-insensitive,
// but the returned binding always carries the exact identity-bound catalog
// path used by the native content service.
[[nodiscard]] std::optional<SoundCollectionBinding>
find_sound_collection_by_title_path(std::string_view title_path) noexcept;

[[nodiscard]] std::span<const SoundCollectionBinding>
sound_collection_catalog() noexcept;

} // namespace sonic_native
