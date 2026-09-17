#pragma once
#include "katana/runtime/native_port_sound_bank.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace sonic::audio {
inline bool deferred_notes_enabled() noexcept {
    static const bool value=[] { const auto* p=std::getenv("SARECOMP_DEFERRED_MIDI_NOTES");
        return p && std::strcmp(p,"1")==0; }();
    return value;
}
inline bool metadata_cache_enabled() noexcept {
    static const bool value=[] { const auto* p=std::getenv("SARECOMP_SOUND_METADATA_CACHE");
        return p && std::strcmp(p,"1")==0; }();
    return value;
}
inline bool metadata_verify_enabled() noexcept {
    static const bool value=[] { const auto* p=std::getenv("SARECOMP_SOUND_METADATA_VERIFY");
        return p && std::strcmp(p,"1")==0; }();
    return value;
}
struct SoundCommandCounts {std::uint64_t metadata_hits=0,metadata_misses=0,metadata_verified=0,deferred_notes=0;};
inline thread_local SoundCommandCounts sound_command_counts;

// A bounded producer-only cache of immutable, authenticated collection facts.
// Its owner must invalidate it before load/unload/reset/restore, including a
// restore which deliberately reuses the same handle generation.
class SoundMetadataCache {
    struct Key {std::uint32_t slot,generation;std::uint16_t item;std::uint8_t bank,kind;
        bool operator==(const Key&) const=default;};
    struct Entry {Key key{};bool value=false,valid=false;};
    std::array<Entry,512> entries_{};
public:
    void clear() noexcept {for(auto& entry:entries_)entry.valid=false;}
    template<class Query> bool read(katana::runtime::NativePortSoundCollectionHandle collection,
            std::uint8_t kind,std::uint8_t bank,std::uint16_t item,Query&& query) {
        const Key key{collection.slot,collection.generation,item,bank,kind};
        const auto hash=key.slot*0x9E3779B9u ^ key.generation*0x85EBCA6Bu ^
            std::uint32_t(key.item)*0xC2B2AE35u ^ std::uint32_t(key.bank)*131u ^ key.kind;
        auto& entry=entries_[(hash^(hash>>16u))&(entries_.size()-1u)];
        if(entry.valid && entry.key==key) {
            ++sound_command_counts.metadata_hits;
            if(metadata_verify_enabled()) {
                if(query()!=entry.value)throw std::runtime_error("sound-metadata-cache-mismatch");
                ++sound_command_counts.metadata_verified;
            }
            return entry.value;
        }
        ++sound_command_counts.metadata_misses;
        const bool answer=query(); // Exceptions are never cached.
        entry={key,answer,true};return answer;
    }
};

namespace detail {
inline thread_local const katana::runtime::NativePortSoundBankEngine* deferred_note_owner=nullptr;
struct DeferredNoteRequest {
    const katana::runtime::NativePortSoundBankEngine* previous;
    explicit DeferredNoteRequest(const katana::runtime::NativePortSoundBankEngine& engine)
        :previous(deferred_note_owner){deferred_note_owner=&engine;}
    ~DeferredNoteRequest(){deferred_note_owner=previous;}
    DeferredNoteRequest(const DeferredNoteRequest&)=delete;
};
}
inline bool deferred_note_requested(const katana::runtime::NativePortSoundBankEngine* engine) noexcept {
    return deferred_notes_enabled() && detail::deferred_note_owner==engine;
}
// Only the title's two no-result callers use this scope. The ordinary SDK API
// continues returning real VoiceHandles to all other callers. Serial-reference
// mode remains synchronous inside the prepared implementation.
inline void start_note_without_handle(katana::runtime::NativePortSoundBankEngine& engine,
        katana::runtime::NativePortSoundMidiPortHandle port,std::uint8_t note,std::uint8_t velocity) {
    detail::DeferredNoteRequest request(engine);
    static_cast<void>(engine.midi_note_on(port,note,velocity));
}
}
