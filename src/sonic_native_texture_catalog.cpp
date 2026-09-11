#include "sonic_native_texture_catalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <ranges>
#include <span>

namespace sonic_native {
namespace {

struct SonicNativeTextureCatalogEntry final {
    std::string_view name;
    std::uint32_t global_index = 0u;
    bool has_global_index = false;
    std::uint8_t pixel_format = 0u;
    std::uint8_t data_format = 0u;
    std::uint16_t width = 0u;
    std::uint16_t height = 0u;
    std::string_view pvrt_byte_identity;
};

struct SonicNativeTextureArchiveCatalog final {
    std::string_view filename;
    std::string_view content_path;
    std::string_view byte_identity;
    std::uint64_t byte_size = 0u;
    std::size_t first_entry = 0u;
    std::size_t entry_count = 0u;
    bool standalone_pvr = false;
};

#define SONIC_NATIVE_TEXTURE_CATALOG_ENTRIES
// The generated PAL catalog contains many thousands of entries.  Keep its
// element type explicit: std::array CTAD otherwise builds one recursive
// same-type constraint per initializer and exceeds clang-cl's template-depth
// limit despite every initializer already targeting this aggregate.
constexpr SonicNativeTextureCatalogEntry texture_entry_storage[]{
#include "sonic-native-texture-catalog.inc"
};
constexpr std::span<const SonicNativeTextureCatalogEntry> texture_entries{
    texture_entry_storage};
#undef SONIC_NATIVE_TEXTURE_CATALOG_ENTRIES

#define SONIC_NATIVE_TEXTURE_CATALOG_ARCHIVES
constexpr SonicNativeTextureArchiveCatalog texture_archive_storage[]{
#include "sonic-native-texture-catalog.inc"
};
constexpr std::span<const SonicNativeTextureArchiveCatalog> texture_archives{
    texture_archive_storage};
#undef SONIC_NATIVE_TEXTURE_CATALOG_ARCHIVES

[[nodiscard]] bool ascii_equal(std::string_view left,
                               std::string_view right) noexcept;

[[nodiscard]] bool matches(
    const SonicNativeTextureArchiveCatalog& archive,
    const std::span<const TextureObservation> observations) noexcept {
    if (archive.entry_count != observations.size() ||
        archive.first_entry > texture_entries.size() ||
        archive.entry_count > texture_entries.size() - archive.first_entry)
        return false;
    for (std::size_t index = 0u; index < observations.size(); ++index) {
        const auto& expected = texture_entries[archive.first_entry + index];
        const auto& observed = observations[index];
        // Standalone PVRs are opened through the Dreamcast filesystem.  The
        // PAL code supplies lower-case NINJA names (for example
        // mission_s_box_e), while the same identity-bound disc files and the
        // generated catalog use upper-case ISO names.  Preserve exact PVM
        // entry-name matching, but compare standalone file identities with
        // the filesystem's case-insensitive contract.  unique_archive_match
        // still rejects any case-folded ambiguity.
        if (archive.standalone_pvr
                ? !ascii_equal(observed.name, expected.name)
                : observed.name != expected.name)
            return false;
        if (observed.global_index.has_value() &&
            (!expected.has_global_index ||
             *observed.global_index != expected.global_index))
            return false;
    }
    return true;
}

[[nodiscard]] constexpr std::uint8_t normalize_data_format(
    const std::uint8_t format) noexcept {
    using F = katana::runtime::NativePortTextureAssetDataFormat;
    if (format == static_cast<std::uint8_t>(F::SmallVectorQuantized))
        return static_cast<std::uint8_t>(F::VectorQuantized);
    if (format ==
        static_cast<std::uint8_t>(F::SmallVectorQuantizedMipmaps))
        return static_cast<std::uint8_t>(F::VectorQuantizedMipmaps);
    return format;
}

[[nodiscard]] bool matches_layout(
    const SonicNativeTextureCatalogEntry& expected,
    const TextureLayoutObservation& observed) noexcept {
    return expected.pixel_format ==
               static_cast<std::uint8_t>(observed.pixel_format) &&
           normalize_data_format(expected.data_format) ==
               normalize_data_format(
                   static_cast<std::uint8_t>(observed.data_format)) &&
           expected.width == observed.extent.width &&
           expected.height == observed.extent.height;
}

[[nodiscard]] bool matches_layouts(
    const SonicNativeTextureArchiveCatalog& archive,
    const std::span<const TextureLayoutObservation> observations) noexcept {
    if (archive.entry_count != observations.size() ||
        archive.first_entry > texture_entries.size() ||
        archive.entry_count > texture_entries.size() - archive.first_entry)
        return false;
    for (std::size_t index = 0u; index < observations.size(); ++index) {
        const auto& expected = texture_entries[archive.first_entry + index];
        const auto& observed = observations[index];
        if (!matches_layout(expected, observed))
            return false;
    }
    return true;
}

template <typename Matcher>
[[nodiscard]] std::optional<TextureArchiveBinding> unique_archive_match(
    Matcher&& matcher) noexcept {
    std::optional<std::size_t> match;
    for (std::size_t index = 0u; index < texture_archives.size(); ++index) {
        if (!matcher(texture_archives[index])) continue;
        if (match.has_value()) return std::nullopt;
        match = index;
    }
    if (!match.has_value()) return std::nullopt;
    const auto& archive = texture_archives[*match];
    return TextureArchiveBinding{*match,
                                 archive.filename,
                                 archive.content_path,
                                 archive.byte_identity,
                                 archive.byte_size,
                                 archive.entry_count,
                                 archive.standalone_pvr};
}

[[nodiscard]] constexpr char ascii_upper(const char value) noexcept {
    return value >= 'a' && value <= 'z'
               ? static_cast<char>(value - ('a' - 'A'))
               : value;
}

[[nodiscard]] bool ascii_equal(const std::string_view left,
                               const std::string_view right) noexcept {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0u; index < left.size(); ++index) {
        if (ascii_upper(left[index]) != ascii_upper(right[index]))
            return false;
    }
    return true;
}

[[nodiscard]] bool logical_archive_name_matches(
    const SonicNativeTextureArchiveCatalog& archive,
    const std::string_view logical_id,
    const bool standalone_pvr) noexcept {
    if (archive.standalone_pvr != standalone_pvr) return false;
    constexpr std::string_view prs_suffix = ".PRS";
    constexpr std::string_view pvr_suffix = ".PVR";
    const auto suffix = standalone_pvr ? pvr_suffix : prs_suffix;
    if (archive.filename.size() != logical_id.size() + suffix.size())
        return false;
    return ascii_equal(archive.filename.substr(0u, logical_id.size()), logical_id) &&
           ascii_equal(archive.filename.substr(logical_id.size()), suffix);
}

} // namespace

std::optional<TextureArchiveBinding> match_texture_archive(
    const std::span<const TextureObservation> observations) noexcept {
    if (observations.empty() ||
        std::ranges::any_of(observations,
                            [](const auto& observed) {
                                return observed.name.empty();
                            }))
        return std::nullopt;

    return unique_archive_match([&](const auto& archive) {
        return matches(archive, observations);
    });
}

std::optional<TextureArchiveBinding> match_texture_archive_layouts(
    const std::span<const TextureLayoutObservation> observations) noexcept {
    if (observations.empty()) return std::nullopt;
    return unique_archive_match([&](const auto& archive) {
        return matches_layouts(archive, observations);
    });
}

std::optional<PartialTextureArchiveMatch> match_texture_archive_partial(
    const std::size_t archive_entry_count,
    const std::span<const IndexedTextureObservation> observations) noexcept {
    if (archive_entry_count == 0u || observations.size() < 3u)
        return std::nullopt;

    std::size_t identity_witnesses = 0u;
    std::size_t layout_witnesses = 0u;
    for (std::size_t index = 0u; index < observations.size(); ++index) {
        const auto& observed = observations[index];
        if (observed.archive_ordinal >= archive_entry_count)
            return std::nullopt;
        for (std::size_t previous = 0u; previous < index; ++previous) {
            if (observations[previous].archive_ordinal ==
                observed.archive_ordinal)
                return std::nullopt;
        }
        if (!observed.name.empty() || observed.global_index.has_value())
            ++identity_witnesses;
        if (observed.layout.has_value()) ++layout_witnesses;
        if (observed.name.empty() && !observed.global_index.has_value() &&
            !observed.layout.has_value())
            return std::nullopt;
    }
    if ((identity_witnesses < 2u || observations.size() < 3u) &&
        layout_witnesses < 8u)
        return std::nullopt;

    std::optional<std::size_t> match;
    for (std::size_t archive_index = 0u;
         archive_index < texture_archives.size(); ++archive_index) {
        const auto& archive = texture_archives[archive_index];
        if (archive.entry_count != archive_entry_count ||
            archive.first_entry > texture_entries.size() ||
            archive.entry_count > texture_entries.size() - archive.first_entry)
            continue;

        bool matches_candidate = true;
        for (const auto& observed : observations) {
            const auto& expected =
                texture_entries[archive.first_entry + observed.archive_ordinal];
            if ((!observed.name.empty() &&
                 (archive.standalone_pvr
                      ? !ascii_equal(observed.name, expected.name)
                      : observed.name != expected.name)) ||
                (observed.global_index.has_value() &&
                 (!expected.has_global_index ||
                  *observed.global_index != expected.global_index)) ||
                (observed.layout.has_value() &&
                 !matches_layout(expected, *observed.layout))) {
                matches_candidate = false;
                break;
            }
        }
        if (!matches_candidate) continue;
        if (match.has_value()) return std::nullopt;
        match = archive_index;
    }
    if (!match.has_value()) return std::nullopt;
    const auto& archive = texture_archives[*match];
    return PartialTextureArchiveMatch{
        TextureArchiveBinding{*match,
                              archive.filename,
                              archive.content_path,
                              archive.byte_identity,
                              archive.byte_size,
                              archive.entry_count,
                              archive.standalone_pvr},
        identity_witnesses,
        layout_witnesses};
}

std::optional<TextureArchiveBinding> find_texture_archive(
    const std::string_view content_relative_path) noexcept {
    for (std::size_t index = 0u; index < texture_archives.size(); ++index) {
        const auto& archive = texture_archives[index];
        if (archive.content_path != content_relative_path &&
            archive.filename != content_relative_path)
            continue;
        return TextureArchiveBinding{index,
                                     archive.filename,
                                     archive.content_path,
                                     archive.byte_identity,
                                     archive.byte_size,
                                     archive.entry_count,
                                     archive.standalone_pvr};
    }
    return std::nullopt;
}

std::optional<TextureArchiveBinding> find_texture_archive_logical_id(
    const std::string_view logical_id,
    const bool standalone_pvr) noexcept {
    if (logical_id.empty()) return std::nullopt;
    return unique_archive_match([&](const auto& archive) {
        return logical_archive_name_matches(archive, logical_id,
                                            standalone_pvr);
    });
}

std::optional<TextureArchiveBinding> texture_archive_binding(
    const std::size_t catalog_index) noexcept {
    if (catalog_index >= texture_archives.size()) return std::nullopt;
    const auto& archive = texture_archives[catalog_index];
    return TextureArchiveBinding{catalog_index,
                                 archive.filename,
                                 archive.content_path,
                                 archive.byte_identity,
                                 archive.byte_size,
                                 archive.entry_count,
                                 archive.standalone_pvr};
}

std::optional<std::string_view> texture_archive_entry_pvrt_identity(
    const TextureArchiveBinding& binding, const std::size_t archive_ordinal) noexcept {
    if (binding.catalog_index >= texture_archives.size()) return std::nullopt;
    const auto& archive = texture_archives[binding.catalog_index];
    if (archive.filename != binding.logical_id ||
        archive.content_path != binding.content_relative_path ||
        archive.byte_identity != binding.byte_identity ||
        archive.byte_size != binding.byte_size ||
        archive.entry_count != binding.entry_count ||
        archive.standalone_pvr != binding.standalone_pvr ||
        archive.first_entry > texture_entries.size() ||
        archive.entry_count > texture_entries.size() - archive.first_entry ||
        archive_ordinal >= archive.entry_count) return std::nullopt;
    const auto identity = texture_entries[archive.first_entry + archive_ordinal].pvrt_byte_identity;
    if (identity.size() != 71u || !identity.starts_with("sha256:")) return std::nullopt;
    for (const char digit : identity.substr(7u))
        if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f')))
            return std::nullopt;
    return identity;
}

std::optional<std::uint32_t> texture_archive_entry_global_index(
    const TextureArchiveBinding& binding, const std::size_t archive_ordinal) noexcept {
    // Share the existing complete binding/ordinal validation.
    if (!texture_archive_entry_pvrt_identity(binding, archive_ordinal))
        return std::nullopt;
    const auto& archive = texture_archives[binding.catalog_index];
    const auto& entry = texture_entries[archive.first_entry + archive_ordinal];
    return entry.has_global_index ? std::optional{entry.global_index} : std::nullopt;
}

bool texture_archive_matches(
    const TextureArchiveBinding& binding,
    const std::span<const TextureObservation> observations) noexcept {
    return binding.catalog_index < texture_archives.size() &&
           texture_archives[binding.catalog_index].filename ==
               binding.logical_id &&
           texture_archives[binding.catalog_index].byte_identity ==
               binding.byte_identity &&
           matches(texture_archives[binding.catalog_index], observations);
}

bool texture_archive_layouts_match(
    const TextureArchiveBinding& binding,
    const std::span<const TextureLayoutObservation> observations) noexcept {
    return binding.catalog_index < texture_archives.size() &&
           texture_archives[binding.catalog_index].filename ==
               binding.logical_id &&
           texture_archives[binding.catalog_index].byte_identity ==
               binding.byte_identity &&
           matches_layouts(texture_archives[binding.catalog_index],
                           observations);
}

bool validate_materialized_texture_archive(
    const TextureArchiveBinding& binding,
    const katana::runtime::NativePortMaterializedTextureArchive& materialized) noexcept {
    if (binding.catalog_index >= texture_archives.size()) return false;
    const auto& archive = texture_archives[binding.catalog_index];
    if (archive.filename != binding.logical_id ||
        archive.content_path != binding.content_relative_path ||
        archive.byte_identity != binding.byte_identity ||
        archive.byte_size != binding.byte_size ||
        archive.entry_count != binding.entry_count ||
        materialized.entries.size() != archive.entry_count ||
        archive.first_entry > texture_entries.size() ||
        archive.entry_count > texture_entries.size() - archive.first_entry)
        return false;
    for (std::size_t index = 0u; index < archive.entry_count; ++index) {
        const auto& expected = texture_entries[archive.first_entry + index];
        const auto& actual = materialized.entries[index];
        // A standalone PVRT object has no embedded logical texture name.  Its
        // filename stem is private catalog metadata used to bind the content
        // request, while the byte identity and GBIX bind the decoded surface.
        // PVM entries do carry names and must continue matching them exactly.
        if (actual.archive_ordinal != index ||
            (!archive.standalone_pvr && actual.name != expected.name) ||
            actual.global_index.has_value() != expected.has_global_index ||
            (expected.has_global_index &&
             *actual.global_index != expected.global_index) ||
            actual.guest_token == 0u || !actual.texture ||
            actual.extent.width != expected.width ||
            actual.extent.height != expected.height)
            return false;
    }
    return true;
}

} // namespace sonic_native
