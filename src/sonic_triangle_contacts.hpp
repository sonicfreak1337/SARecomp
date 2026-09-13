#pragma once
#include "katana/runtime/runtime.hpp"
#include <array>
#include <string_view>
namespace katana::runtime { class NativePortImmutableWriteGuard; }
namespace sonic::triangle_contacts {
inline constexpr std::uint32_t entry=0x8C029400u, size=0x6F4u;
inline constexpr auto source_sha256="fbff84a132a49217c521c601ae85e5c6b14d7eee1a177db8f861942de67fb23b";
struct SourceSpan { std::uint32_t address, size; std::string_view sha256; };
// Complete owner/leaf/retained-angle code and literals, then coefficient table.
inline constexpr std::array source_spans{
    SourceSpan{0x8C029400u,0x6F4u,"fbff84a132a49217c521c601ae85e5c6b14d7eee1a177db8f861942de67fb23b"},
    SourceSpan{0x8C027360u,0x42u,"ab64ed43a74ef8ae8bc802e9a03ac1ff70c19370a74186906ecf5878a3149dd8"},
    SourceSpan{0x8C63A69Cu,0x10u,"184ec57b105022cf5a5df589f52fed8dc017109b7c0ff5626bfaf6c31c5a39dd"},
    SourceSpan{0x8C63A88Cu,0x20u,"91bc28ff6fe7b04c8d5178dd3b7e8ee411224556da83d770225895326e61376b"},
    SourceSpan{0x8C10CF98u,0x50u,"96055337e822b6e0fee2a34e5d6c37e0c3e495cb49cc2bf75581e0f11ade1111"},
    SourceSpan{0x8C10D038u,0x50u,"78db9abf037229a0aebf28c5d183f0fd7adab9c5d0ab948341257ceb170805b6"},
    SourceSpan{0x8C10E4ECu,0x14Cu,"9d1f7ac6d20f75bc4a079055112b59245a91c3fc5ed2a8dbfb29defc90fc45c0"},
    SourceSpan{0x8C10E6F8u,0xC0u,"316c8b53e094bc27f5d85d3be392105d732e2aae3609409e41b862ce1dddb4ca"},
    SourceSpan{0x8C10EEC4u,0x1E0u,"1361220e5d950f6c9548df0303e16156c0aceb2c3f19753d7329dc28070d6496"},
    SourceSpan{0x8C10FAD4u,0x128u,"3356124ea2ab89b712265bb6e1c0a3ce004baa41152017bfe362d4a2d64e8b14"},
    SourceSpan{0x8C16012Cu,0x1C8u,"9efbf32ae22d23e9b42fe4bde9cd48bf4f0862478d8511084b348dfc1eaa4620"},
};
struct RetainedCallBridge {
    void* context=nullptr;
    // Only 8C10D038 / 8C10CF98. PC=target, original PR already set.
    // Execute synchronously, no unrelated guest callback/safepoint. True means
    // complete original return to PR; false is a fatal interruption, not fallback.
    bool (*invoke)(void*,katana::runtime::CpuState&,std::uint32_t target)=nullptr;
};
// False only BEFORE every mutation/bridge call. Broken post-call contracts throw;
// integration MUST NOT convert that exception into ContinueOriginal.
// P0 requires AT=0/NoMmu, P1/P2 data supported. Canonical PAL P1 entry only.
// StableForPrevalidatedLinearWrites observers only. No cross-call RAM cache.
// Instruction/cycle/provenance accounting belongs to the native-hook caller.
[[nodiscard]] bool try_execute(katana::runtime::CpuState&,
    const katana::runtime::NativePortImmutableWriteGuard*,const RetainedCallBridge&);
} // namespace sonic::triangle_contacts
