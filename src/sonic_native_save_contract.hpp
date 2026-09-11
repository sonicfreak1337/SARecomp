#pragma once

#include "katana/runtime/native_port_save.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace sonic::native_save_sdk {

using namespace katana::runtime;
inline constexpr std::uint32_t block_bytes = 512u;
inline constexpr std::uint32_t total_blocks = 200u;
inline constexpr std::uint32_t game_blocks = 128u;
inline constexpr std::uint32_t metadata_tag = 0x53410000u;
inline constexpr std::uint32_t verify_flag = 0x80000000u;
inline constexpr std::string_view date_prefix = "sdk-vmu-date-v1:";

[[nodiscard]] inline std::string encode_date(const std::array<std::uint8_t, 8u>& date) {
    constexpr std::string_view hex = "0123456789abcdef";
    std::string text(date_prefix);
    for (const auto byte : date) {
        text += hex[byte >> 4u];
        text += hex[byte & 15u];
    }
    return text;
}

[[nodiscard]] inline std::optional<std::array<std::uint8_t, 8u>>
decode_date(const NativePortSaveDirectoryEntry& entry) {
    if ((entry.user_flags & 0x7FFF0000u) != metadata_tag ||
        !entry.description.starts_with(date_prefix) ||
        entry.description.size() != date_prefix.size() + 16u)
        return std::nullopt;
    constexpr std::string_view hex = "0123456789abcdef";
    std::array<std::uint8_t, 8u> date{};
    for (std::size_t i = 0u; i < date.size(); ++i) {
        const auto high = hex.find(entry.description[date_prefix.size() + 2u * i]);
        const auto low = hex.find(entry.description[date_prefix.size() + 2u * i + 1u]);
        if (high == hex.npos || low == hex.npos) return std::nullopt;
        date[i] = static_cast<std::uint8_t>((high << 4u) | low);
    }
    return date;
}

[[nodiscard]] inline bool is_game(const NativePortSaveDirectoryEntry& entry) noexcept {
    return (entry.user_flags & 0x7FFFFF00u) == (metadata_tag | 0x200u);
}

// File-based native allocation: games occupy the low 128-block domain,
// ordinary files the high end of the shared 200 blocks. The native volume
// can compact files; no FAT/device or physical-block identity is emulated.
[[nodiscard]] inline std::uint32_t free_game_blocks(
    const NativePortSaveListResult& directory) noexcept {
    std::uint64_t used = 0u;
    for (const auto& entry : directory.entries)
        if (is_game(entry)) used += entry.allocated_blocks;
    return std::min(directory.status.free_blocks,
        used < game_blocks ? game_blocks - static_cast<std::uint32_t>(used) : 0u);
}

struct ReadResult {
    NativePortSaveCompletion completion;
    std::vector<std::byte> payload;
};

// SDK 60355C/64BAF2: r7 is the first block, stack[0] the block count.
// Count zero reads from that first block to EOF. Directory and read share
// one generation, and nothing is published to guest RAM until all succeeds.
[[nodiscard]] inline ReadResult read(
    NativePortSaveProvider& provider, const NativePortSaveEndpoint endpoint,
    const std::string_view file_id, const std::uint32_t first_block,
    const std::uint32_t count) {
    ReadResult out;
    const auto directory = provider.list({endpoint});
    out.completion = directory.completion;
    out.completion.operation = NativePortSaveOperation::Read;
    if (out.completion.error != NativePortSaveError::None) return out;
    const auto found = std::ranges::find_if(directory.entries,
        [&](const auto& entry) { return entry.file_id == file_id; });
    if (found == directory.entries.end()) {
        out.completion.error = NativePortSaveError::NotFound;
        return out;
    }
    const auto offset = static_cast<std::uint64_t>(first_block) * block_bytes;
    const auto requested = static_cast<std::uint64_t>(count) * block_bytes;
    if (offset > found->byte_size || requested > found->byte_size - offset ||
        found->byte_size > static_cast<std::uint64_t>(total_blocks) * block_bytes ||
        found->byte_size % block_bytes != 0u) {
        out.completion.error = NativePortSaveError::InvalidArgument;
        return out;
    }
    const auto bytes = count == 0u ? found->byte_size - offset : requested;
    out.payload.resize(static_cast<std::size_t>(bytes));
    out.completion = provider.read({endpoint, file_id, out.payload, offset,
        bytes, directory.completion.generation}).completion;
    if (out.completion.error != NativePortSaveError::None) out.payload.clear();
    return out;
}

struct WriteResult {
    NativePortSaveCompletion completion;
    bool verification_failed = false;
};

[[nodiscard]] inline WriteResult write(
    NativePortSaveProvider& provider, const NativePortSaveEndpoint endpoint,
    const std::string_view file_id, const std::span<const std::byte> payload,
    const std::array<std::uint8_t, 8u>& date, const std::uint32_t flags,
    const bool game) {
    WriteResult out;
    const auto directory = provider.list({endpoint});
    out.completion = directory.completion;
    out.completion.operation = NativePortSaveOperation::Write;
    if (out.completion.error != NativePortSaveError::None) return out;
    const auto found = std::ranges::find_if(directory.entries,
        [&](const auto& entry) { return entry.file_id == file_id; });
    if (game && found != directory.entries.end()) {
        out.completion.error = NativePortSaveError::AlreadyExists;
        return out;
    }
    if (payload.empty() || payload.size() % block_bytes != 0u ||
        payload.size() / block_bytes > total_blocks) {
        out.completion.error = NativePortSaveError::InvalidArgument;
        return out;
    }
    if (game && payload.size() / block_bytes > free_game_blocks(directory)) {
        out.completion.error = NativePortSaveError::InsufficientBlocks;
        return out;
    }
    const auto description = encode_date(date);
    const NativePortSaveFileMetadata metadata{file_id, "SONICAD", "SONIC ADVENTURE",
        description, metadata_tag | (game ? 0x200u : 0x100u) |
            (flags & (verify_flag | 0xFFu))};
    out.completion = provider.write({endpoint, metadata, payload,
        directory.completion.generation, !game}).completion;
    if (out.completion.error != NativePortSaveError::None ||
        (flags & verify_flag) == 0u) return out;
    // Both retail write variants have a verify-after-write branch. Reopen
    // through the real provider, validating the atomically committed volume.
    std::vector<std::byte> verify(payload.size());
    const auto checked = provider.read({endpoint, file_id, verify, 0u,
        payload.size(), out.completion.generation});
    if (checked.completion.error != NativePortSaveError::None) {
        out.completion = checked.completion;
    } else {
        out.verification_failed = checked.completion.transferred_bytes != payload.size() ||
            !std::equal(payload.begin(), payload.end(), verify.begin());
    }
    return out;
}

// Original worker result codes. Host corruption, identity conflicts and I/O
// exceptions have no invented SDK success/format result: keep them typed.
[[nodiscard]] inline std::optional<std::uint32_t> completion_code(
    const NativePortSaveError error) noexcept {
    switch (error) {
    case NativePortSaveError::None: return 0u;
    case NativePortSaveError::Absent: return 0xFFFFFF01u;
    case NativePortSaveError::NotFound: return 0xFFFFFF05u;
    case NativePortSaveError::AlreadyExists: return 0xFFFFFF06u;
    case NativePortSaveError::InsufficientBlocks: return 0xFFFFFF04u;
    case NativePortSaveError::DirectoryFull: return 0xFFFFFF08u;
    case NativePortSaveError::InvalidArgument:
    case NativePortSaveError::BufferTooSmall: return 0xFFFF0000u;
    default: return std::nullopt;
    }
}

} // namespace sonic::native_save_sdk
