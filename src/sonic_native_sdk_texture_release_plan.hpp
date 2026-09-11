#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace sonic_native_sdk_release {

// Private, source-bound unloaded branch and loaded Type0 subset. Retail witnesses:
// 6087FC..60884C: ordered TEXNAME+8 releases, aggregate -1 and last bad index.
// 64DD0A,64DDCE..64DDEC: unloaded registered/free/error state split.
// 64DD26..64DD5E: low16 refcount, resource free, active key and flag clearing.
// 64E660..64E6E2: exact row clearing, highwater collapse and unsigned mask rule.
// No Type1 path, guest VRAM allocator, guessed GBIX lookup or guest writes here.
inline constexpr std::uint32_t registry_slot = 0x0C6733C4u;
inline constexpr std::uint32_t registry_count_slot = 0x0C6733C8u;
inline constexpr std::uint32_t active_key_slot = 0x0C6733CCu;
inline constexpr std::uint32_t highwater_slot = 0x0C88F5E0u;
inline constexpr std::uint32_t mask_slot = 0x0C673C58u;
inline constexpr std::uint32_t last_error_index_slot = 0x0C88F6E8u;
inline constexpr std::uint32_t release_error_slot = 0x0C88F6F0u;
inline constexpr std::uint32_t descriptor_stride = 68u;
using Words = std::array<std::uint32_t, 17u>;

enum class Failure { Address, Read, Registry, UnboundRow, UnsupportedType, UnsupportedState };
class Error final : public std::runtime_error {
public:
    Failure reason;
    std::uint32_t address;
    Error(Failure reason_, std::uint32_t address_)
        : std::runtime_error("native-sdk-texture-release-plan"), reason(reason_), address(address_) {}
};
struct Write { std::uint32_t address, before, after; };
struct FreedRow { std::uint32_t address; Words before; };
struct Plan {
    std::vector<Write> writes;
    std::vector<FreedRow> freed_rows;
    std::uint32_t result = 1u;
};

inline std::uint32_t physical(std::uint32_t address) {
    const auto segment = address >> 29u;
    if (segment != 0u && segment != 4u && segment != 5u)
        throw Error(Failure::Address, address);
    return address & 0x1FFFFFFFu;
}

enum class AllocationStatus : std::uint32_t {
    Ready, Read, Registry, Cursor, Highwater, Full
};
struct AllocationInspection {
    AllocationStatus status = AllocationStatus::Read;
    std::uint32_t registry = 0u, count = 0u, cursor = 0u, highwater = 0u;
    std::uint32_t selected = 0xFFFFFFFFu, failed_address = 0u, valid = 0u;
};

// Read-only preflight for retail 64E55E. Its unsuccessful full scan falls
// through to a one-past-end descriptor write. Admit the original body only
// when its exact cursor-first / first-key-FFFFFFFF selection is in bounds.
// No host lease is released or inferred here; even dirty free rows retain the
// retail key-only selection rule. A highwater gap does not change selection.
template<class Reader, class Range>
AllocationInspection inspect_allocation(Reader&& read, Range&& range,
                                       const std::uint32_t maximum_count) {
    AllocationInspection result;
    const auto get = [&](const std::uint32_t address, std::uint32_t& word) {
        if (read(address, word)) return true;
        result.status = AllocationStatus::Read;
        result.failed_address = address;
        return false;
    };
    const std::array addresses{registry_slot, registry_count_slot, mask_slot, highwater_slot};
    const std::array fields{&result.registry, &result.count, &result.cursor, &result.highwater};
    for (std::size_t i = 0u; i < addresses.size(); ++i) {
        if (!get(addresses[i], *fields[i])) return result;
        result.valid |= 1u << i;
    }
    const auto segment = result.registry >> 29u;
    const auto base = result.registry & 0x1FFFFFFFu;
    const auto bytes = std::uint64_t(result.count) * descriptor_stride;
    if ((segment != 0u && segment != 4u && segment != 5u) ||
        !base || base % 4u || !result.count || result.count > maximum_count ||
        bytes > 0x20000000ull - base || !range(base, bytes)) {
        result.status = AllocationStatus::Registry;
        result.failed_address = registry_slot;
        return result;
    }
    for (const auto address : {registry_slot, registry_count_slot, active_key_slot,
                              highwater_slot, mask_slot, last_error_index_slot, release_error_slot}) {
        if (address < std::uint64_t(base) + bytes && std::uint64_t(address) + 4u > base) {
            result.status = AllocationStatus::Registry;
            result.failed_address = address;
            return result;
        }
    }
    if (result.cursor >= result.count) {
        result.status = AllocationStatus::Cursor;
        result.failed_address = mask_slot;
        return result;
    }
    if (result.highwater > result.count) {
        result.status = AllocationStatus::Highwater;
        result.failed_address = highwater_slot;
        return result;
    }
    std::uint32_t key = 0u;
    result.selected = result.cursor;
    if (!get(base + result.selected * descriptor_stride, key)) return result;
    if (key != 0xFFFFFFFFu) {
        for (result.selected = 0u; result.selected < result.count; ++result.selected) {
            if (!get(base + result.selected * descriptor_stride, key)) return result;
            if (key == 0xFFFFFFFFu) break;
        }
    }
    result.status = result.selected == result.count
        ? AllocationStatus::Full : AllocationStatus::Ready;
    return result;
}

// Reader: bool(uint32_t physical_address, uint32_t& word), read-only.
// Bound: bool(uint32_t physical_row, const Words& original_words), read-only.
// Bound must authenticate each first-seen row against the caller's exact current
// release authority, including free rows. For the retail outer PVM release this
// is the current validated TEXLIST/TEXNAME pointer plus registry membership; a
// host content alias is not required. It must never infer authority from a
// matching GBIX alone. Subsequent duplicates use planned state.
// Caller bounds pointer count and registry capacity, revalidates preimages, then
// applies writes atomically with rollback before consuming freed_rows on host.
// A thrown Error returns no partial plan and has made no mutations/callback frees.
template<class Reader, class Bound>
Plan make_plan(std::span<const std::uint32_t> pointers,
               std::uint32_t expected_registry, std::uint32_t expected_count,
               Reader&& read, Bound&& bound) {
    Plan plan;
    if (pointers.empty()) return plan; // Retail zero-count result is 1.
    const auto registry = physical(expected_registry);
    if (!registry || registry % 4u || !expected_count ||
        expected_count > (0x20000000ull - registry) / descriptor_stride)
        throw Error(Failure::Registry, registry);
    for (const auto address : {registry_slot, registry_count_slot, active_key_slot,
                              highwater_slot, mask_slot, last_error_index_slot, release_error_slot})
        if (address >= registry && address < registry + std::uint64_t(expected_count) * descriptor_stride)
            throw Error(Failure::Registry, address);
    const auto current = [&](std::uint32_t address) {
        for (const auto& word : plan.writes)
            if (word.address == address) return word.after;
        std::uint32_t value = 0u;
        if (address % 4u || !read(address, value)) throw Error(Failure::Read, address);
        return value;
    };
    const auto write = [&](std::uint32_t address, std::uint32_t value) {
        for (auto& word : plan.writes) {
            if (word.address == address) { word.after = value; return; }
        }
        const auto before = current(address);
        if (before != value) plan.writes.push_back({address, before, value});
    };
    const auto clear_row = [&](const std::uint32_t row, const Words& words,
                               const bool clear_active_key) {
        plan.freed_rows.push_back({row, words});
        // The loaded Type0 path clears the active key at 64DD4A..64DD54.
        // The registered-but-unloaded path jumps directly to 64E660 and does
        // not touch that separate global.
        if (clear_active_key && current(active_key_slot) == words[0])
            write(active_key_slot, 0xFFFFFFFFu);
        write(row, 0xFFFFFFFFu);
        write(row + 4u, 0xFFFFFFFFu);
        write(row + 16u, 0u);
        for (std::uint32_t offset = 20u; offset <= 52u; offset += 4u)
            write(row + offset, 0u);
        // 64E682 is MOV.W: clear only the SDK reference-count halfword and
        // retain the unrelated bookkeeping halfword at +66.
        write(row + 64u, words[16] & 0xFFFF0000u);
        auto highwater = current(highwater_slot);
        if (highwater > expected_count)
            throw Error(Failure::Registry, highwater_slot);
        if (highwater != 0u &&
            (row - registry) / descriptor_stride == highwater - 1u) {
            while (highwater != 0u &&
                   current(registry + (highwater - 1u) * descriptor_stride) ==
                       0xFFFFFFFFu)
                --highwater;
            write(highwater_slot, highwater);
        }
        // 64E6D2 is unsigned CMP/HI; highwater==0 makes limit FFFFFFFF,
        // so the mask becomes zero, not FFFFFFFF.
        const auto limit = highwater - 1u;
        write(mask_slot, current(mask_slot) > limit ? limit : 0u);
    };
    if (physical(current(registry_slot)) != registry || current(registry_count_slot) != expected_count)
        throw Error(Failure::Registry, registry_slot);
    std::vector<std::uint32_t> authenticated;
    for (std::size_t index = 0u; index < pointers.size(); ++index) {
        if (index > std::numeric_limits<std::uint32_t>::max())
            throw Error(Failure::Address, last_error_index_slot);
        const auto row = physical(pointers[index]);
        if (row < registry || row >= registry + std::uint64_t(expected_count) * descriptor_stride ||
            (row - registry) % descriptor_stride)
            throw Error(Failure::Address, row);
        Words words{};
        for (std::uint32_t i = 0u; i < words.size(); ++i) words[i] = current(row + i * 4u);
        bool seen = false;
        for (const auto address : authenticated) if (address == row) seen = true;
        if (!seen) {
            if (!bound(row, words)) throw Error(Failure::UnboundRow, row);
            authenticated.push_back(row);
        }
        // 64DD0A branches on the loaded bit before it inspects the texture
        // type. An unloaded registered row is released directly by 64E660;
        // an error-state or already-free row reports the ordered outer-list
        // failure without requiring a canonical reset image.
        if ((words[4] & 2u) == 0u) {
            if ((words[4] & 1u) != 0u || words[0] == 0xFFFFFFFFu) {
                plan.result = 0xFFFFFFFFu;
                write(release_error_slot, 1u); // 64DDE4..64DDEA.
                write(last_error_index_slot, static_cast<std::uint32_t>(index));
                continue;
            }
            clear_row(row, words, false);
            continue;
        }
        // The private host path has only proved the ordinary loaded Type0
        // publication. Combined loaded flags and Type1 retain their fail-closed
        // boundary until their separate resource lifetime is represented.
        if (words[6] != 0u) throw Error(Failure::UnsupportedType, row);
        if (words[4] != 2u || words[0] == 0xFFFFFFFFu)
            throw Error(Failure::UnsupportedState, row);
        const auto count = words[16] & 0xFFFFu;
        const auto next_count = count == 0u ? 0u : count - 1u;
        write(row + 64u, (words[16] & 0xFFFF0000u) | next_count);
        if (next_count != 0u) continue;
        clear_row(row, words, true);
    }
    return plan;
}

} // namespace sonic_native_sdk_release

namespace sonic_native_sdk_texture_list_release {

enum class Failure { Count, Texnames, Pointer, Row, Index };

class Error final : public std::runtime_error {
public:
    Failure reason;
    std::uint32_t index;
    std::uint32_t descriptor;

    Error(const Failure reason_, const std::uint32_t index_,
          const std::uint32_t descriptor_ = 0u)
        : std::runtime_error("native-sdk-texture-list-release"),
          reason(reason_), index(index_), descriptor(descriptor_) {}
};

struct Result final {
    std::uint32_t value = 1u;
    std::uint32_t final_count = 0u;
    std::uint32_t calls = 0u;
};

struct Control final {
    std::uint32_t texnames = 0u;
    std::uint32_t count = 0u;
};

[[nodiscard]] constexpr bool loaded_type0_can_reach_free(
    const std::uint16_t references,
    const std::uint32_t ordered_calls) noexcept {
    return ordered_calls != 0u &&
           (references == 0u || ordered_calls >= references);
}

[[nodiscard]] constexpr bool loaded_type0_backing_was_released(
    const std::uint32_t before_flags,
    const std::uint32_t after_flags) noexcept {
    return (before_flags & 2u) != 0u && (after_flags & 2u) == 0u;
}

template <class Handle, class Pending, class AlreadyRetired>
void plan_retired_handle(std::vector<Handle>& planned,
                         const Handle handle,
                         Pending&& pending,
                         AlreadyRetired&& already_retired) {
    if (!static_cast<bool>(handle) || !pending(handle) ||
        already_retired(handle) ||
        std::ranges::find(planned, handle) != planned.end())
        return;
    planned.push_back(handle);
}

[[nodiscard]] constexpr bool retirement_budget_accepts(
    const std::size_t current,
    const std::size_t planned,
    const std::size_t maximum) noexcept {
    return current <= maximum && planned <= maximum - current;
}

// Native preflight enumerates the initially reachable rows before the first
// destructive leaf call. The adapter separately proves that TEXLIST/TEXNAME
// control storage cannot alias the SDK rows mutated by that leaf.
template <class ReadCount, class ReadTexnames, class ReadPointer,
          class ValidateRow>
Control preflight(const std::uint32_t maximum_count,
                  ReadCount&& read_count,
                  ReadTexnames&& read_texnames,
                  ReadPointer&& read_pointer,
                  ValidateRow&& validate_row) {
    Control result;
    if (!read_count(result.count) || result.count > maximum_count)
        throw Error(Failure::Count, 0u);
    if (result.count == 0u) return result;
    if (!read_texnames(result.texnames))
        throw Error(Failure::Texnames, 0u);
    for (std::uint32_t index = 0u; index < result.count; ++index) {
        std::uint32_t descriptor = 0u;
        if (!read_pointer(result.texnames, index, descriptor))
            throw Error(Failure::Pointer, index);
        // A failed retail per-entry load leaves TEXNAME+8 null. PAL 64DD00
        // then observes the bound Dreamcast boot-ROM words and returns 1
        // without freeing a resource; the native adapter reproduces that
        // leaf outcome without issuing the ignored ROM write. Nonzero
        // descriptors still require exact current registry-row authority.
        if (descriptor != 0u && !validate_row(descriptor, index))
            throw Error(Failure::Row, index, descriptor);
    }
    return result;
}

// Source-bound outer loop for PAL 6087FC..60884A. Count is reloaded at every
// loop boundary and TEXNAME is reloaded for every call. The leaf owns the
// descriptor/type-specific release; this helper owns only ordering, aggregate
// failure and the last-failed-index side effect. The maximum is a native safety
// fence, not an SDK capacity or a license to truncate a changing list.
template <class ReadCount, class ReadTexnames, class ReadPointer,
          class InvokeLeaf, class RecordFailure, class AfterLeaf>
Result run(const std::uint32_t maximum_count,
           ReadCount&& read_count,
           ReadTexnames&& read_texnames,
           ReadPointer&& read_pointer,
           InvokeLeaf&& invoke_leaf,
           RecordFailure&& record_failure,
           AfterLeaf&& after_leaf) {
    Result result;
    std::uint32_t index = 0u;
    for (;;) {
        if (!read_count(result.final_count) ||
            result.final_count > maximum_count)
            throw Error(Failure::Count, index);
        if (index >= result.final_count) return result;

        std::uint32_t texnames = 0u;
        if (!read_texnames(texnames))
            throw Error(Failure::Texnames, index);
        std::uint32_t descriptor = 0u;
        if (!read_pointer(texnames, index, descriptor))
            throw Error(Failure::Pointer, index);

        const auto leaf_result = invoke_leaf(descriptor);
        if (static_cast<std::int32_t>(leaf_result) < 0) {
            result.value = std::numeric_limits<std::uint32_t>::max();
            record_failure(index);
        }
        after_leaf(descriptor, leaf_result);
        ++result.calls;
        if (index == std::numeric_limits<std::uint32_t>::max())
            throw Error(Failure::Index, index);
        ++index;
    }
}

} // namespace sonic_native_sdk_texture_list_release
