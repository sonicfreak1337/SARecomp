#pragma once

#include "katana/runtime/native_port_texture_asset.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace sonic_native {

struct TextureObservation final {
    std::string name;
    std::optional<std::uint32_t> global_index;
};

// A live NINJA descriptor retains this complete SDK-facing storage shape even
// after the title discards the source name and GBIX.  Matching the complete
// ordered archive shape is therefore a content identity witness, not a PVR
// device fallback or an address-specific title hook.
struct TextureLayoutObservation final {
    katana::runtime::NativePortTextureAssetPixelFormat pixel_format =
        katana::runtime::NativePortTextureAssetPixelFormat::Rgb565;
    katana::runtime::NativePortTextureAssetDataFormat data_format =
        katana::runtime::NativePortTextureAssetDataFormat::Rectangle;
    katana::runtime::NativePortExtent extent;
};

// A partially materialized NJS_TEXLIST can retain only selected names and
// live descriptors.  Preserve their original archive ordinals so several
// independent identity/layout witnesses can bind the complete source archive
// without treating the compacted observation order as an archive order.
struct IndexedTextureObservation final {
    std::size_t archive_ordinal = 0u;
    std::string name;
    std::optional<std::uint32_t> global_index;
    std::optional<TextureLayoutObservation> layout;
};

struct TextureArchiveBinding final {
    std::size_t catalog_index = 0u;
    std::string_view logical_id;
    std::string_view content_relative_path;
    std::string_view byte_identity;
    std::uint64_t byte_size = 0u;
    std::size_t entry_count = 0u;
    bool standalone_pvr = false;
};

struct PartialTextureArchiveMatch final {
    TextureArchiveBinding binding;
    std::size_t identity_witnesses = 0u;
    std::size_t layout_witnesses = 0u;
};

// Returns one exact archive only when the complete ordered NJS_TEXLIST name
// sequence and every available GBIX witness identify it uniquely.
[[nodiscard]] std::optional<TextureArchiveBinding> match_texture_archive(
    std::span<const TextureObservation> observations) noexcept;

// Returns one exact archive only when every ordered live descriptor layout
// identifies it uniquely. SmallVQ is normalized to the ordinary VQ class
// because the live TCW does not preserve the source codebook-size variant.
[[nodiscard]] std::optional<TextureArchiveBinding>
match_texture_archive_layouts(
    std::span<const TextureLayoutObservation> observations) noexcept;

// Binds one archive only when the TEXLIST entry count matches, every available
// indexed witness agrees and the candidate is unique.  At least two identity
// plus three total witnesses, or eight layout witnesses, are required.  This
// deliberately refuses sparse/ambiguous lists instead of guessing a title
// archive from one convenient texture.
[[nodiscard]] std::optional<PartialTextureArchiveMatch>
match_texture_archive_partial(
    std::size_t archive_entry_count,
    std::span<const IndexedTextureObservation> observations) noexcept;

[[nodiscard]] std::optional<TextureArchiveBinding> find_texture_archive(
    std::string_view content_relative_path) noexcept;

// Resolves the title/SDK-facing archive stem without importing a private
// filesystem path into the hook contract.  Matching is ASCII case-insensitive
// because NINJA texture names are case-insensitive on the original content
// filesystem.  The source class remains explicit so a standalone .PVR can
// never alias a same-named .PRS/PVM archive.
[[nodiscard]] std::optional<TextureArchiveBinding>
find_texture_archive_logical_id(
    std::string_view logical_id,
    bool standalone_pvr) noexcept;

[[nodiscard]] std::optional<TextureArchiveBinding> texture_archive_binding(
    std::size_t catalog_index) noexcept;

// Exact complete PVRT chunk identity, independent of archive ordinal/name/GBIX.
// It binds the format header, codebook and every mip payload. The caller must
// separately prove the live SDK key/bank and resource lifetime before aliasing.
[[nodiscard]] std::optional<std::string_view> texture_archive_entry_pvrt_identity(
    const TextureArchiveBinding& binding, std::size_t archive_ordinal) noexcept;

// Exact source GBIX for rejecting a recycled bootstrap descriptor before
// associating its unchanged allocation/layout with checkpoint texture bytes.
[[nodiscard]] std::optional<std::uint32_t> texture_archive_entry_global_index(
    const TextureArchiveBinding& binding, std::size_t archive_ordinal) noexcept;

[[nodiscard]] bool texture_archive_matches(
    const TextureArchiveBinding& binding,
    std::span<const TextureObservation> observations) noexcept;

[[nodiscard]] bool texture_archive_layouts_match(
    const TextureArchiveBinding& binding,
    std::span<const TextureLayoutObservation> observations) noexcept;

// Verifies that runtime decoding produced the exact catalog order, names and
// GBIX values selected by match_texture_archive().
[[nodiscard]] bool validate_materialized_texture_archive(
    const TextureArchiveBinding& binding,
    const katana::runtime::NativePortMaterializedTextureArchive& archive) noexcept;

} // namespace sonic_native
