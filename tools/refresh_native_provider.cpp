#include "katana/codegen/native_disc_analysis_artifact.hpp"
#include "katana/codegen/project.hpp"
#include "katana/io/input_provenance.hpp"
#include "katana/runtime/native_port_artifact.hpp"
#include "katana/runtime/native_port_identity.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <limits>
#include <ranges>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::uintmax_t maximum_refresh_source_bytes =
    64u * 1024u * 1024u;

[[noreturn]] void fail(const std::string_view reason) {
    throw std::runtime_error(std::string(reason));
}

[[nodiscard]] std::string read_regular_file(
    const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error || !std::filesystem::is_regular_file(status) ||
        std::filesystem::is_symlink(status))
        fail("provider-refresh-source-invalid");
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > maximum_refresh_source_bytes)
        fail("provider-refresh-source-invalid");
    std::ifstream input(path, std::ios::binary);
    if (!input) fail("provider-refresh-source-open");
    std::string result(static_cast<std::size_t>(size), '\0');
    if (!result.empty())
        input.read(result.data(), static_cast<std::streamsize>(result.size()));
    if (!input || input.gcount() !=
                      static_cast<std::streamsize>(result.size()))
        fail("provider-refresh-source-read");
    return result;
}

struct QuotedToken final {
    std::size_t begin = 0u;
    std::size_t end = 0u;
    std::string value;
};

[[nodiscard]] std::vector<QuotedToken> quoted_tokens(
    const std::string_view text,
    const std::size_t begin,
    const std::size_t end) {
    if (begin > end || end > text.size()) fail("provider-refresh-string-range");
    std::vector<QuotedToken> result;
    for (std::size_t index = begin; index < end; ++index) {
        if (text[index] != '"') continue;
        ++index;
        const auto token_begin = index;
        std::string value;
        bool closed = false;
        while (index < end) {
            const auto character = text[index++];
            if (character == '\\') {
                if (index >= end) fail("provider-refresh-unterminated-string");
                value.push_back(text[index++]);
            } else if (character == '"') {
                closed = true;
                break;
            } else {
                value.push_back(character);
            }
        }
        if (!closed) fail("provider-refresh-unterminated-string");
        const auto token_end = index - 1u;
        result.push_back({token_begin, token_end, std::move(value)});
        --index;
    }
    return result;
}

struct TextReplacement final {
    std::size_t begin = 0u;
    std::size_t end = 0u;
    std::string value;
};

void apply_replacements(std::string& text,
                        std::vector<TextReplacement> replacements) {
    std::sort(replacements.begin(), replacements.end(),
              [](const auto& left, const auto& right) {
                  return left.begin > right.begin;
              });
    for (std::size_t index = 1u; index < replacements.size(); ++index) {
        if (replacements[index - 1u].begin == replacements[index].begin)
            fail("provider-refresh-overlapping-replacement");
    }
    for (const auto& replacement : replacements) {
        if (replacement.begin > replacement.end ||
            replacement.end > text.size())
            fail("provider-refresh-replacement-range");
        text.replace(replacement.begin,
                     replacement.end - replacement.begin,
                     replacement.value);
    }
}

[[nodiscard]] std::size_t unique_marker(
    const std::string_view text,
    const std::string_view marker) {
    const auto first = text.find(marker);
    if (first == std::string_view::npos ||
        text.find(marker, first + marker.size()) != std::string_view::npos)
        fail("provider-refresh-structure-marker");
    return first;
}

struct ArrayRegion final {
    std::size_t begin = 0u;
    std::size_t end = 0u;
    std::vector<std::pair<std::size_t, std::size_t>> entries;
};

[[nodiscard]] ArrayRegion array_region(const std::string_view text,
                                       const std::string_view marker) {
    const auto marker_offset = unique_marker(text, marker);
    const auto body_begin = marker_offset + marker.size();
    const auto body_end = text.find("}};", body_begin);
    if (body_end == std::string_view::npos || body_end <= body_begin)
        fail("provider-refresh-array-boundary");

    ArrayRegion result{body_begin, body_end, {}};
    int depth = 0;
    std::size_t entry_begin = std::string_view::npos;
    for (std::size_t index = body_begin; index < body_end; ++index) {
        if (text[index] == '"') {
            ++index;
            bool closed = false;
            while (index < body_end) {
                if (text[index] == '\\') {
                    if (++index >= body_end)
                        fail("provider-refresh-array-string");
                } else if (text[index++] == '"') {
                    closed = true;
                    break;
                }
            }
            if (!closed) fail("provider-refresh-array-string");
            --index;
            continue;
        }
        if (text[index] == '{') {
            if (depth == 0) entry_begin = index;
            ++depth;
        } else if (text[index] == '}') {
            if (depth == 0) fail("provider-refresh-array-depth");
            --depth;
            if (depth == 0) {
                if (entry_begin == std::string_view::npos)
                    fail("provider-refresh-array-entry");
                result.entries.emplace_back(entry_begin, index + 1u);
                entry_begin = std::string_view::npos;
            }
        }
    }
    if (depth != 0 || entry_begin != std::string_view::npos ||
        result.entries.empty())
        fail("provider-refresh-array-depth");
    return result;
}

[[nodiscard]] std::string hex_u32(const std::uint32_t value) {
    std::ostringstream output;
    output << std::uppercase << std::hex << value;
    return output.str();
}

[[nodiscard]] std::string hook_kind_name(
    const katana::runtime::NativePortHookKind kind) {
    return kind == katana::runtime::NativePortHookKind::FunctionEntry
               ? "FunctionEntry"
               : "Instruction";
}

[[nodiscard]] std::string hook_requirement_name(
    const katana::runtime::NativePortHookRequirement requirement) {
    switch (requirement) {
    case katana::runtime::NativePortHookRequirement::Required:
        return "Required";
    case katana::runtime::NativePortHookRequirement::BringUpProbe:
        return "BringUpProbe";
    case katana::runtime::NativePortHookRequirement::DiagnosticOnly:
        return "DiagnosticOnly";
    }
    fail("provider-refresh-hook-requirement");
}

[[nodiscard]] std::string hook_policy_name(
    const katana::runtime::NativePortHookOriginalPolicy policy) {
    return policy == katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal
               ? "MayContinueOriginal"
               : "ReplacesOriginal";
}

[[nodiscard]] std::string hook_source_name(
    const katana::runtime::NativePortHookCodeSource source) {
    return source == katana::runtime::NativePortHookCodeSource::LatentAotModule
               ? "LatentAotModule"
               : "StaticImage";
}

[[nodiscard]] std::string hook_prefix(
    const katana::runtime::NativePortHookBinding& hook) {
    std::ostringstream output;
    output << "{0x" << hex_u32(hook.guest_address) << "u, "
           << hook.covered_size << "u, katana::runtime::NativePortHookKind::"
           << hook_kind_name(hook.kind)
           << ", katana::runtime::NativePortHookRequirement::"
           << hook_requirement_name(hook.requirement)
           << ", katana::runtime::NativePortHookOriginalPolicy::"
           << hook_policy_name(hook.original_policy) << ", \""
           << hook.symbol << "\", \"" << hook.code_identity << "\", ";
    return output.str();
}

[[nodiscard]] std::string hook_suffix(
    const katana::runtime::NativePortHookBinding& hook) {
    std::ostringstream output;
    output << ", katana::runtime::NativePortHookCodeSource::"
           << hook_source_name(hook.code_source) << ", \""
           << hook.code_source_identity << "\"}";
    return output.str();
}

[[nodiscard]] std::string semantic_prefix(
    const katana::runtime::NativePortProviderSemanticContract& contract) {
    std::ostringstream output;
    output << "{" << contract.contract_version << "u, 0x"
           << hex_u32(contract.hook_guest_address)
           << "u, " << (contract.authoritative ? "true" : "false")
           << ", \"" << contract.provider_symbol << "\", ";
    return output.str();
}

[[nodiscard]] bool valid_raw_sha256(const std::string_view value) noexcept {
    return value.size() == 64u &&
           std::all_of(value.begin(), value.end(), [](const char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}

struct NormalizedDefinition final {
    katana::runtime::NativePortDefinition definition;
    std::vector<katana::runtime::NativePortHookBinding> hooks;
    std::vector<katana::runtime::NativePortProviderSemanticContract>
        semantic_contracts;
};

[[nodiscard]] NormalizedDefinition normalize_provider_identity(
    const katana::runtime::NativePortDefinition& source) {
    constexpr std::string_view neutral_provider_identity{
        "sha256:0000000000000000000000000000000000000000000000000000000000000000"};
    NormalizedDefinition result;
    result.definition = source;
    result.hooks.assign(source.hooks.begin(), source.hooks.end());
    for (auto& hook : result.hooks) {
        if (!hook.provider_implementation_identity.empty())
            hook.provider_implementation_identity = neutral_provider_identity;
    }
    result.semantic_contracts.assign(
        source.provider_semantic_contracts.begin(),
        source.provider_semantic_contracts.end());
    for (auto& contract : result.semantic_contracts) {
        if (!contract.provider_implementation_identity.empty())
            contract.provider_implementation_identity =
                neutral_provider_identity;
    }
    result.definition.hooks = result.hooks;
    result.definition.provider_semantic_contracts =
        result.semantic_contracts;
    return result;
}

struct TemporaryArtifacts final {
    std::vector<std::filesystem::path> paths;
    ~TemporaryArtifacts() {
        for (const auto& path : paths) {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    }
};

// Sonic-local extensions: rendering, language/save and native rumble boundaries.
// Admit only reviewed byte-bound hooks; continuing hooks need retained AOT.
// This is not a general structural refresh or permission to change the frozen pack.
struct RenderHookExtension {
    katana::runtime::NativePortDefinition before;
    std::vector<katana::runtime::NativePortHookBinding> hooks;
    std::vector<katana::runtime::NativePortHookBinding> added;
};
RenderHookExtension render_hook_extension(
    const katana::runtime::NativePortDefinition& before,
    const katana::runtime::NativePortDefinition& after) {
    using namespace katana::runtime;
    RenderHookExtension result{before, {}, {}};
    std::size_t old = 0;
    unsigned candidates_added=0;
    unsigned rendering_added=0,language_added=0,camera_added=0,legacy_video_added=0,options_display_added=0,rumble_added=0,cadence_added=0,palette_added=0,normals_added=0,matrix_stack_added=0,collision_added=0,inverse_added=0,contacts_added=0,atan_added=0,amy_effect_added=0,matrix_vectors_added=0;
    struct ReviewedRumble {std::uint32_t address,size;std::string_view symbol,sha;};
    constexpr std::array rumble_hooks{
        ReviewedRumble{0x8C6042B0u,6u,"sonic_native_rumble_capability","2eb2196012d5e864de7c33573a13e8f3179d01a955e1d5994c123eac1314593c"},
        ReviewedRumble{0x8C6042B6u,0x64u,"sonic_native_rumble_configure","882a220b86201c6e457a2b995de1087fb3a4b709e12e54a42a84969c8691fa2c"},
        ReviewedRumble{0x8C60431Au,0x2Eu,"sonic_native_rumble_request","aee276d94ff40aff8c76eec88d446d6cbaf705f5dd380762e9dcba37fe97bfba"},
        ReviewedRumble{0x8C604348u,0x3Cu,"sonic_native_rumble_stop","aa649b8b79189f8ddce7fc3b813fe6391e5be8d3a9878be5113991c409200c10"}};
    struct ReviewedLanguage {std::uint32_t address,size;std::string_view symbol,sha;bool latent;};
    constexpr std::array languages{
        ReviewedLanguage{0x8C0884A0u,0xA8u,"sonic_language_save","bc707d8f911b559cb66eb1c91d169519fe462a9cc3d6adabe0bb031013499fa2",false},
        ReviewedLanguage{0x8C0885C0u,0x74u,"sonic_language_loaded","77a9ef8bf117b7ba4048071acecd6d6705ec1e334f30ba4154eb46c37cda3446",false},
        ReviewedLanguage{0x8C0544E2u,0x11Au,"sonic_language_initial","b0cd74b8c534c9e18ff099e03ff17e1db8616a5bf35a950726dda6153aa26552",false},
        ReviewedLanguage{0x8C08A4B2u,6u,"sonic_language_subtitles_loaded","1c9358b9d3149b6cd8d3d5fec7dd9b76a182525e015b858b985720eed6ee6098",false},
        ReviewedLanguage{0x8088CA94u,0x28u,"sonic_language_subtitles","4e3875cafc64b9e68ffcfe9f991da55405e9ca14632293ffb923234e959c1410",true},
        ReviewedLanguage{0x8088CAF8u,0x50u,"sonic_language_voice","d3c2ba0cf8c234b9d1bfdb967d78242e328b23d71111869aecb33746a32d2eeb",true},
        ReviewedLanguage{0x8088CBC0u,0x44u,"sonic_language_text","356d2b2811becea7dc9372eaaed12bee6dd766a160b11004425a5bb01468d3a1",true}};
    for (const auto& hook : after.hooks) {
        if (old < before.hooks.size() &&
            before.hooks[old].guest_address == hook.guest_address) {
            result.hooks.push_back(before.hooks[old++]);
            continue;
        }
        const bool model = hook.guest_address == 0x8C03718Cu &&
            hook.covered_size == 0x108u &&
            hook.symbol == "sonic_native_widescreen_model_cull" &&
            hook.code_identity == "sha256:df39afabfbfdce25d7c3bd0cd59008959ec7e365320dd19a9603857401e67137";
        const bool sphere = hook.guest_address == 0x8C038D00u &&
            hook.covered_size == 0xA0u &&
            hook.symbol == "sonic_native_widescreen_draw_sphere_cull" &&
            hook.code_identity == "sha256:1f573f535bbc2d5e67ba50eca018c42cab9736a60bc89ec7542df1a88e10d551";
        const bool camera=(hook.guest_address==0x8C01A100u && hook.covered_size==0xACu &&
            hook.symbol=="sonic_recompiled_camera_publish" &&
            hook.code_identity=="sha256:f88a14755daffb71dc3b9490f35660e768605e121e8f83e5df2dd2a8f541d002") ||
            (hook.guest_address==0x8C019F4Au && hook.covered_size==0x158u &&
            hook.symbol=="sonic_recompiled_camera_original_step" &&
            hook.code_identity=="sha256:ed23827fa453d89252cda31480e5ae1854976d41680f155904e9f525eaf7186e");
        const bool language=std::ranges::any_of(languages,[&](const auto& row) {
            return hook.guest_address==row.address && hook.covered_size==row.size &&
                hook.symbol==row.symbol && hook.code_identity=="sha256:"+std::string(row.sha) &&
                hook.code_source==(row.latent?NativePortHookCodeSource::LatentAotModule:NativePortHookCodeSource::StaticImage) &&
                hook.code_source_identity==(row.latent?"sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5":"");
        });
        const bool legacy_video=hook.guest_address==0x8089928Eu && hook.covered_size==0xD8u &&
            hook.symbol=="sonic_legacy_video_mode_disabled" &&
            hook.code_identity=="sha256:2eea7fcacf69722f68fb85461b4a455b2ae301aa89b6785df124ad3a95a32bb8" &&
            hook.code_source==NativePortHookCodeSource::LatentAotModule &&
            hook.code_source_identity=="sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5";
        const bool options_display=hook.guest_address==0x808929BEu && hook.covered_size==0x38u &&
            hook.symbol=="sonic_options_legacy_display" &&
            hook.code_identity=="sha256:634269bce4e226bfd5c6296653363581c690e2f55c38810eb90348c78dcbefdf" &&
            hook.code_source==NativePortHookCodeSource::LatentAotModule &&
            hook.code_source_identity=="sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5";
        const bool rumble=std::ranges::any_of(rumble_hooks,[&](const auto& row) {
            return hook.guest_address==row.address && hook.covered_size==row.size &&
                hook.symbol==row.symbol && hook.code_identity=="sha256:"+std::string(row.sha);
        });
        const bool cadence=hook.guest_address==0x8C051760u && hook.covered_size==0x30u &&
            hook.symbol=="sonic_native_sixty_frame_cadence" &&
            hook.code_identity=="sha256:320d63bbff3edb9d77d47a64e1e0214bf8d83b3522736b7c06445d03c85bc3e5";
        const bool amy_effect=hook.guest_address==0x8C0DF84Cu && hook.covered_size==0xECu &&
            hook.symbol=="sonic_native_amy_hammer_effect" &&
            hook.code_identity=="sha256:6848e8ae03d8ca34e4d1a6b3cb274912ba92e25b4bc3d6245f6dd557f5f97315";
        const bool palette=hook.guest_address==0x8C037350u && hook.covered_size==0x110u &&
            hook.symbol=="sonic_native_palette_lighting" &&
            hook.code_identity=="sha256:6033d3d4b0c9821d221d54c2bc3e78477df900a59c56208fc0a8bddfc518084c";
        const bool normals=hook.guest_address==0x8C0563ACu && hook.covered_size==0x2A6u &&
            hook.symbol=="sonic_native_vertex_normals" &&
            hook.code_identity=="sha256:bffbfdedd2721c7829b7cc35e82bc34190040b4703b131fafcdbf06df802907e";
        const bool matrix_stack=(
            (hook.symbol=="sonic_native_matrix_stack_pop" && hook.guest_address==0x8C639AD8u && hook.covered_size==0x40u &&
             hook.code_identity=="sha256:a3ff7b35d7be9f1ac1dce0209af71beca602344d478d8cec77901ccf7598cc62") ||
            (hook.symbol=="sonic_native_matrix_stack_push" && hook.guest_address==0x8C639BB0u && hook.covered_size==0x80u &&
             hook.code_identity=="sha256:b1a24af68d7a51cc4ffebc58b54082563beb4add63eb4eeef54663967a0ffb10"));
        const bool collision=(
            (hook.symbol=="sonic_native_collision_cross" && hook.guest_address==0x8C027360u && hook.covered_size==0x42u &&
             hook.code_identity=="sha256:ab64ed43a74ef8ae8bc802e9a03ac1ff70c19370a74186906ecf5878a3149dd8") ||
            (hook.symbol=="sonic_native_collision_length" && hook.guest_address==0x8C63A69Cu && hook.covered_size==0x10u &&
             hook.code_identity=="sha256:184ec57b105022cf5a5df589f52fed8dc017109b7c0ff5626bfaf6c31c5a39dd") ||
            (hook.symbol=="sonic_native_collision_normalize" && hook.guest_address==0x8C63A88Cu && hook.covered_size==0x20u &&
             hook.code_identity=="sha256:91bc28ff6fe7b04c8d5178dd3b7e8ee411224556da83d770225895326e61376b"));
        const bool inverse=(
            (hook.symbol=="sonic_native_matrix_inverse" && hook.guest_address==0x8C638FF0u && hook.covered_size==0x804u &&
             hook.code_identity=="sha256:ff02ae8352528051e7806b0d08f449d086f052891e499156aaf49b7e76a4a996") ||
            (hook.symbol=="sonic_native_matrix_determinant" && hook.guest_address==0x8C64F32Cu && hook.covered_size==0x158u &&
             hook.code_identity=="sha256:f237439dce9e4b3ab4b359ce5fce9bb37a82328916e1809955650de95bf5f28c"));
        const bool matrix_vectors=(
            (hook.symbol=="sonic_native_matrix_vector_point" && hook.guest_address==0x8C638E0Cu && hook.covered_size==0x58u &&
             hook.code_identity=="sha256:dc20bddcbd5938d708a7669e769cf3b4b7209f04667e3777c0d6be57370740b8") ||
            (hook.symbol=="sonic_native_matrix_vector_direction" && hook.guest_address==0x8C638E68u && hook.covered_size==0x68u &&
             hook.code_identity=="sha256:fe97aaf272e15ba9a42a46ccf5f7af51d6ab68112410e02ec79e581dbab839fc") ||
            (hook.symbol=="sonic_native_matrix_vector_store" && hook.guest_address==0x8C638ED4u && hook.covered_size==0x2Cu &&
             hook.code_identity=="sha256:229d431e2775b03ed38ef67c482c4e9bea39bac410c897ceb752f01e7e4395d7") ||
            (hook.symbol=="sonic_native_matrix_vector_translation" && hook.guest_address==0x8C638F00u && hook.covered_size==0x20u &&
             hook.code_identity=="sha256:3f723ba70a79ca1afb5fd248ef863a512bbba44e1239bbf35245320e28e78997"));
        const bool contacts=hook.symbol=="sonic_native_triangle_contacts" &&
            hook.guest_address==0x8C029400u && hook.covered_size==0x6F4u &&
            hook.code_identity=="sha256:fbff84a132a49217c521c601ae85e5c6b14d7eee1a177db8f861942de67fb23b";
        const bool candidates=hook.symbol=="sonic_native_collision_candidates" &&
            hook.guest_address==0x8C029B00u && hook.covered_size==0xB6Cu &&
            hook.code_identity=="sha256:744ca095c47e07d9b87ed43cf54e013c45349cd472852cf44cee85dd7b946832";
        const bool atan=(
            (hook.symbol=="sonic_native_atan" && hook.guest_address==0x8C10EEC4u && hook.covered_size==0x1E0u &&
             hook.code_identity=="sha256:1361220e5d950f6c9548df0303e16156c0aceb2c3f19753d7329dc28070d6496") ||
            (hook.symbol=="sonic_native_atan_quotient" && hook.guest_address==0x8C10FAF8u && hook.covered_size==0x104u &&
             hook.code_identity=="sha256:8edb1eea052f1622840e3f6fa67dd7aa2efccfcb8e8e030d3935ea5b5a826fb4") ||
            (hook.symbol=="sonic_native_atan_polynomial" && hook.guest_address==0x8C10FAD4u && hook.covered_size==0x24u &&
             hook.code_identity=="sha256:4c9efceb0a2491382e2251fb758565cb4073f1292ea079692f68e79e22246b82") ||
            (hook.symbol=="sonic_native_atan_scale" && hook.guest_address==0x8C10E6F8u && hook.covered_size==0xC0u &&
             hook.code_identity=="sha256:316c8b53e094bc27f5d85d3be392105d732e2aae3609409e41b862ce1dddb4ca"));
        if ((!model && !sphere && !language && !camera && !legacy_video && !options_display && !rumble && !cadence && !palette && !normals && !matrix_stack && !collision && !inverse && !contacts && !candidates && !atan && !amy_effect && !matrix_vectors) ||
            hook.kind != NativePortHookKind::FunctionEntry ||
            hook.requirement != NativePortHookRequirement::Required ||
            hook.original_policy != ((rumble||amy_effect)?NativePortHookOriginalPolicy::ReplacesOriginal:
                NativePortHookOriginalPolicy::MayContinueOriginal) ||
            (!language && !legacy_video && !options_display && (hook.code_source != NativePortHookCodeSource::StaticImage ||
                !hook.code_source_identity.empty())) ||
            !valid_native_port_sha256_identity(hook.provider_implementation_identity) ||
            std::ranges::any_of(before.hooks, [&](const auto& h) {
                return h.guest_address == hook.guest_address; }) ||
            std::ranges::any_of(result.added, [&](const auto& h) {
                return h.guest_address == hook.guest_address; })) {
            std::cerr<<"SONIC_PROVIDER_STRUCTURE_MISMATCH next=0x"<<std::hex<<hook.guest_address
                <<" prior=0x"<<(old<before.hooks.size()?before.hooks[old].guest_address:0u)
                <<std::dec<<" symbol="<<hook.symbol<<" before_count="<<before.hooks.size()
                <<" after_count="<<after.hooks.size()<<'\n';
            fail("sonic-render-hook-unreviewed-structural-delta");
        }
        result.hooks.push_back(hook);
        result.added.push_back(hook);
        if(candidates) ++candidates_added;else if(language) ++language_added;else if(camera) ++camera_added;
        else if(legacy_video) ++legacy_video_added;else if(options_display) ++options_display_added;
        else if(rumble) ++rumble_added;else if(cadence) ++cadence_added;else if(palette) ++palette_added;else if(normals) ++normals_added;else if(matrix_stack) ++matrix_stack_added;else if(collision) ++collision_added;else if(inverse) ++inverse_added;else if(matrix_vectors) ++matrix_vectors_added;else if(contacts) ++contacts_added;else if(atan) ++atan_added;else if(amy_effect) ++amy_effect_added;else ++rendering_added;
    }
    if (old != before.hooks.size() || candidates_added>1u ||
        (rendering_added!=0u && rendering_added!=2u) ||
        (language_added!=0u && language_added!=7u) || camera_added>2u || legacy_video_added>1u || options_display_added>1u ||
        (rumble_added!=0u && rumble_added!=4u) || cadence_added>1u || palette_added>1u || normals_added>1u ||
        (matrix_stack_added!=0u && matrix_stack_added!=2u) || (collision_added!=0u && collision_added!=3u) ||
        (inverse_added!=0u && inverse_added!=2u) || contacts_added>1u || (matrix_vectors_added!=0u && matrix_vectors_added!=4u) || (atan_added!=0u && atan_added!=4u) || amy_effect_added>1u)
        fail("sonic-render-hook-incomplete-extension");
    result.before.hooks = result.hooks;
    return result;
}

void insert_render_hooks(std::string& dispatch, std::string& audit,
                         const RenderHookExtension& extension,
                         const std::filesystem::path& generated_root) {
    if (extension.added.empty()) return;
    const auto region = array_region(dispatch, "native_hooks{{");
    // Permit transactional replay after a prior successful refresh.
    if (region.entries.size() == extension.hooks.size()) return;
    if (region.entries.size() + extension.added.size() != extension.hooks.size())
        fail("sonic-render-hook-old-cardinality");
    const std::string newline = dispatch.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::string body = newline;
    std::size_t old = 0;
    for (const auto& hook : extension.hooks) {
        const bool added = std::ranges::any_of(extension.added, [&](const auto& h) {
            return h.guest_address == hook.guest_address; });
        if (added) {
            body += "    " + hook_prefix(hook) + "\"" +
                std::string(hook.provider_implementation_identity) + "\"" +
                hook_suffix(hook) + "," + newline;
        } else {
            const auto [begin, end] = region.entries.at(old++);
            body += "    " + dispatch.substr(begin, end-begin) + "," + newline;
        }
    }
    dispatch.replace(region.begin, region.end-region.begin, body);
    const auto change = [](std::string& text, const std::string& from, const std::string& to) {
        const auto at = unique_marker(text, from);
        text.replace(at, from.size(), to);
    };
    const std::string type = "constexpr std::array<katana::runtime::NativePortHookBinding, ";
    change(dispatch, type + std::to_string(old) + "u> native_hooks{{",
        type + std::to_string(extension.hooks.size()) + "u> native_hooks{{");
    std::string declarations, cases, tokens;
    for (const auto& hook : extension.added) {
        const auto hex = hex_u32(hook.guest_address);
        // Continuing hooks require an exact block witness in the frozen pack.
        // The reviewed rumble stop leaf has no retained entry, but is replaced
        // completely: its byte-bound provider cannot continue into original AOT.
        const bool replaced_rumble_stop = hook.guest_address == 0x8C604348u &&
            hook.original_policy == katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal &&
            hook.symbol == "sonic_native_rumble_stop" && hook.covered_size == 0x3Cu &&
            hook.code_identity == "sha256:aa649b8b79189f8ddce7fc3b813fe6391e5be8d3a9878be5113991c409200c10";
        // This whole Amy callback was absent from the frozen pack. Its native
        // body restores the original code; none of its branches can fall back.
        const bool replaced_amy_effect = hook.guest_address == 0x8C0DF84Cu &&
            hook.original_policy == katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal &&
            hook.symbol == "sonic_native_amy_hammer_effect" && hook.covered_size == 0xECu &&
            hook.code_identity == "sha256:6848e8ae03d8ca34e4d1a6b3cb274912ba92e25b4bc3d6245f6dd557f5f97315";
        const auto witness = "{{0x" + hex + "u, 0x" +
            hex_u32(hook.guest_address & 0x1fffffffu) + "u}, ";
        // Physical addresses in the emitter have a leading zero.
        const auto padded = "{{0x" + hex + "u, 0x0" +
            hex_u32(hook.guest_address & 0x1fffffffu) + "u}, ";
        if(hook.code_source==katana::runtime::NativePortHookCodeSource::LatentAotModule) {
            const auto shard=read_regular_file(generated_root/((hook.guest_address==0x8089928Eu||hook.guest_address==0x808929BEu)?
                "code/native-port-dispatch-shard-98441.cpp":"code/native-port-dispatch-shard-98440.cpp"));
            if(shard.find("{0x"+hex+"u, &fn_"+hex+"_runtime_entry, false, false}")==std::string::npos)
                fail("sonic-language-hook-missing-frozen-entry");
        } else if (!replaced_rumble_stop && !replaced_amy_effect && dispatch.find(witness) == std::string::npos && dispatch.find(padded) == std::string::npos)
            fail("sonic-render-hook-missing-frozen-block");
        declarations += "extern \"C\" katana::runtime::NativePortHookResult " +
            std::string(hook.symbol) + "(katana::runtime::NativePortContext&) noexcept;" + newline;
        cases += "        case 0x" + hex + "u: return HookDispatch{true, HookKind::FunctionEntry, HookRequirement::Required, katana::runtime::NativePortHookOriginalPolicy::" +
            hook_policy_name(hook.original_policy) + ", &" +
            std::string(hook.symbol) + ", 0x" + hex + "u, " +
            std::to_string(hook.covered_size) + "u};" + newline;
        tokens += "    std::string_view{\"" + std::string(hook.symbol) + "\"}," + newline;
    }
    const std::string declaration_marker = "extern \"C\" katana::runtime::NativePortHookResult sonic_native_ninja_model_transform";
    change(dispatch, declaration_marker, declarations + declaration_marker);
    const std::string switch_marker = "        case 0x8C037294u: return HookDispatch{";
    change(dispatch, switch_marker, cases + switch_marker);
    // Direct AOT calls consult this chain query. Intercept the added entries
    // before the immutable shard's pre-enhancement chainability index.
    const std::string chain_marker = "        if (static_chainable_source_address(source)) return true;";
    std::string condition;
    for(const auto& hook:extension.added) {
        if(!condition.empty()) condition+=" || ";
        condition+="(source | 0x20000000u) == 0x"+hex_u32(hook.guest_address|0x20000000u)+"u";
    }
    const std::string gate="        if ("+condition+") return false;"+newline;
    change(dispatch, chain_marker, gate + chain_marker);
    const std::string hook_marker = "bool native_hook_source_address(std::uint32_t source) noexcept {";
    change(dispatch, hook_marker, hook_marker + newline +
        "    if ("+condition+") return true;");
    const std::string audit_marker = "    std::string_view{\"sonic_native_ninja_model_transform\"},";
    change(audit, audit_marker, tokens + audit_marker);
    const std::string audit_type="constexpr std::array<std::string_view, ";
    const auto count_end=unique_marker(audit,"> required_tokens{");
    const auto count_begin=audit.rfind(audit_type,count_end)+audit_type.size();
    unsigned count=0;
    const auto parsed=std::from_chars(audit.data()+count_begin,audit.data()+count_end,count);
    if(parsed.ec!=std::errc{} || parsed.ptr!=audit.data()+count_end)
        fail("sonic-hook-audit-cardinality");
    audit.replace(count_begin,count_end-count_begin,std::to_string(count+extension.added.size()));
}

[[nodiscard]] std::string neutral_definition_identity(
    const katana::runtime::NativePortDefinition& definition,
    const std::filesystem::path& directory,
    const std::string_view label,
    TemporaryArtifacts& temporary) {
    const auto normalized = normalize_provider_identity(definition);
    const auto nonce = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = directory /
        (".katana-provider-refresh-" + std::string(label) + "-" +
         std::to_string(nonce) + ".katana-native-port");
    temporary.paths.push_back(path);
    return katana::runtime::NativePortArtifact::write(
               path, normalized.definition)
        ->artifact_identity();
}

void add_provider_mapping(
    std::map<std::string, std::string>& mappings,
    const std::string_view before,
    const std::string_view after) {
    if (before == after) return;
    if (before.empty() || after.empty() ||
        !katana::runtime::valid_native_port_sha256_identity(before) ||
        !katana::runtime::valid_native_port_sha256_identity(after))
        fail("provider-refresh-identity-shape");
    const auto [entry, inserted] = mappings.emplace(before, after);
    if (!inserted && entry->second != after)
        fail("provider-refresh-nonfunctional-delta");
}

[[nodiscard]] std::map<std::string, std::string> provider_mappings(
    const katana::runtime::NativePortDefinition& before,
    const katana::runtime::NativePortDefinition& after) {
    if (before.hooks.size() != after.hooks.size() ||
        before.provider_semantic_contracts.size() !=
            after.provider_semantic_contracts.size())
        fail("provider-refresh-structural-delta");
    std::map<std::string, std::string> result;
    for (std::size_t index = 0u; index < before.hooks.size(); ++index)
        add_provider_mapping(
            result,
            before.hooks[index].provider_implementation_identity,
            after.hooks[index].provider_implementation_identity);
    for (std::size_t index = 0u;
         index < before.provider_semantic_contracts.size(); ++index)
        add_provider_mapping(
            result,
            before.provider_semantic_contracts[index]
                .provider_implementation_identity,
            after.provider_semantic_contracts[index]
                .provider_implementation_identity);
    if (result.empty()) fail("provider-refresh-empty-delta");
    std::set<std::string> targets;
    for (const auto& [source, target] : result) {
        if (!targets.insert(target).second || result.contains(target))
            fail("provider-refresh-ambiguous-delta");
        static_cast<void>(source);
    }
    return result;
}

struct RuntimeBindingParse final {
    std::vector<QuotedToken> tokens;
    std::array<std::uint64_t, 6u> numbers{};
    katana::codegen::NativeDiscAnalysisArtifactIdentity identity;
};

[[nodiscard]] std::array<std::uint64_t, 6u> trailing_numbers(
    const std::string_view text,
    const std::size_t begin,
    const std::size_t end) {
    std::array<std::uint64_t, 6u> result{};
    std::size_t count = 0u;
    std::size_t index = begin;
    while (index < end) {
        if (text[index] < '0' || text[index] > '9') {
            ++index;
            continue;
        }
        const auto number_begin = index;
        while (index < end && text[index] >= '0' && text[index] <= '9')
            ++index;
        std::uint64_t value = 0u;
        const auto [parsed_end, error] = std::from_chars(
            text.data() + number_begin, text.data() + index, value, 10);
        if (error != std::errc{} || parsed_end != text.data() + index ||
            index >= end || text[index] != 'u' || count == result.size())
            fail("provider-refresh-runtime-frontier-numbers");
        result[count++] = value;
        ++index;
    }
    if (count != result.size())
        fail("provider-refresh-runtime-frontier-numbers");
    return result;
}

[[nodiscard]] RuntimeBindingParse parse_runtime_binding(
    const std::string_view text) {
    constexpr std::string_view marker{"runtime_frontier_binding{"};
    const auto marker_offset = unique_marker(text, marker);
    const auto body_begin = marker_offset + marker.size();
    const auto body_end = text.find("};", body_begin);
    if (body_end == std::string_view::npos)
        fail("provider-refresh-runtime-frontier-boundary");
    RuntimeBindingParse result;
    result.tokens = quoted_tokens(text, body_begin, body_end);
    if (result.tokens.size() != 13u)
        fail("provider-refresh-runtime-frontier-shape");
    result.numbers = trailing_numbers(
        text, result.tokens.back().end, body_end);
    result.identity.key = result.tokens[0].value;
    result.identity.content_identity = result.tokens[1].value;
    result.identity.boot_byte_identity = result.tokens[2].value;
    result.identity.project_identity = result.tokens[3].value;
    result.identity.analysis_contract_identity = result.tokens[4].value;
    result.identity.image_analysis_key = result.tokens[5].value;
    result.identity.game_project_identity = result.tokens[6].value;
    result.identity.native_port_identity = result.tokens[7].value;
    result.identity.native_port_artifact_identity = result.tokens[8].value;
    result.identity.analysis_implementation_identity = result.tokens[9].value;
    result.identity.analysis_cache_implementation_identity =
        result.tokens[10].value;
    result.identity.ir_product_implementation_identity = result.tokens[11].value;
    result.identity.codegen_implementation_identity = result.tokens[12].value;
    if (result.numbers[0] > UINT32_MAX || result.numbers[1] > UINT32_MAX ||
        result.numbers[2] > UINT32_MAX || result.numbers[3] > UINT32_MAX ||
        result.numbers[4] > UINT32_MAX)
        fail("provider-refresh-runtime-frontier-number-range");
    result.identity.analyzer_abi = static_cast<std::uint32_t>(result.numbers[0]);
    result.identity.backend_abi = static_cast<std::uint32_t>(result.numbers[1]);
    result.identity.analysis_mode = static_cast<std::uint32_t>(result.numbers[2]);
    result.identity.disc_volume_start_lba =
        static_cast<std::uint32_t>(result.numbers[3]);
    result.identity.disc_extent_lba_bias =
        static_cast<std::uint32_t>(result.numbers[4]);
    return result;
}

void validate_runtime_identity(
    const katana::codegen::NativeDiscAnalysisArtifactIdentity& identity) {
    if (!valid_raw_sha256(identity.key) ||
        identity.key != katana::codegen::native_disc_analysis_artifact_identity_key(
                             identity))
        fail("provider-refresh-runtime-frontier-key");
}

[[nodiscard]] std::uint64_t resident_generation(
    const katana::codegen::NativeDiscAnalysisArtifactIdentity& identity) {
    std::ostringstream seed;
    seed << "native-resident-primary-generation-v1\n"
         << identity.key << '\n'
         << identity.content_identity << '\n'
         << identity.boot_byte_identity << '\n'
         << identity.image_analysis_key << '\n'
         << identity.codegen_implementation_identity;
    const auto digest = katana::io::sha256_bytes(seed.str());
    std::uint64_t result = 0u;
    const auto [end, error] = std::from_chars(
        digest.data(), digest.data() + 16u, result, 16);
    if (error != std::errc{} || end != digest.data() + 16u)
        fail("provider-refresh-resident-generation");
    return result == 0u ? 1u : result;
}

enum class CanonicalState : std::uint8_t { Before, After };

[[nodiscard]] CanonicalState rewrite_runtime_binding(
    std::string& dispatch,
    const std::string_view before_native_identity,
    const std::string_view before_artifact_identity,
    const std::string_view after_native_identity,
    const std::string_view after_artifact_identity,
    std::string& old_key,
    std::string& new_key) {
    auto parsed = parse_runtime_binding(dispatch);
    validate_runtime_identity(parsed.identity);
    const auto expected_before_identity = parsed.identity;
    const bool is_before =
        parsed.identity.native_port_identity == before_native_identity &&
        parsed.identity.native_port_artifact_identity == before_artifact_identity;
    const bool is_after =
        parsed.identity.native_port_identity == after_native_identity &&
        parsed.identity.native_port_artifact_identity == after_artifact_identity;
    if (is_before == is_after) fail("provider-refresh-runtime-frontier-identity");

    if (is_before) {
        auto expected_after_identity = parsed.identity;
        expected_after_identity.native_port_identity =
            std::string(after_native_identity);
        expected_after_identity.native_port_artifact_identity =
            std::string(after_artifact_identity);
        old_key = katana::codegen::native_disc_analysis_artifact_identity_key(
            expected_before_identity);
        new_key = katana::codegen::native_disc_analysis_artifact_identity_key(
            expected_after_identity);
        if (parsed.identity.key != old_key)
            fail("provider-refresh-runtime-frontier-old-key");
        apply_replacements(
            dispatch,
            {{parsed.tokens[0].begin, parsed.tokens[0].end, new_key},
             {parsed.tokens[7].begin,
              parsed.tokens[7].end,
              std::string(after_native_identity)},
             {parsed.tokens[8].begin,
              parsed.tokens[8].end,
              std::string(after_artifact_identity)}});
        return CanonicalState::Before;
    }

    auto expected_after_identity = parsed.identity;
    expected_after_identity.native_port_identity =
        std::string(after_native_identity);
    expected_after_identity.native_port_artifact_identity =
        std::string(after_artifact_identity);
    new_key = katana::codegen::native_disc_analysis_artifact_identity_key(
        expected_after_identity);
    auto expected_old_identity = parsed.identity;
    expected_old_identity.native_port_identity =
        std::string(before_native_identity);
    expected_old_identity.native_port_artifact_identity =
        std::string(before_artifact_identity);
    old_key = katana::codegen::native_disc_analysis_artifact_identity_key(
        expected_old_identity);
    if (parsed.identity.key != new_key)
        fail("provider-refresh-runtime-frontier-new-key");
    return CanonicalState::After;
}

void rewrite_pack_analysis_identity(std::string& dispatch,
                                    const std::string_view old_key,
                                    const std::string_view new_key) {
    constexpr std::string_view marker{"native_bringup_dispatch_pack{"};
    const auto marker_offset = unique_marker(dispatch, marker);
    const auto body_begin = marker_offset + marker.size();
    const auto body_end = dispatch.find("};", body_begin);
    if (body_end == std::string_view::npos)
        fail("provider-refresh-pack-boundary");
    auto tokens = quoted_tokens(dispatch, body_begin, body_end);
    if (tokens.size() != 5u)
        fail("provider-refresh-pack-shape");
    const auto expected_old = "sha256:" + std::string(old_key);
    const auto expected_new = "sha256:" + std::string(new_key);
    if (tokens[3].value == expected_old) {
        apply_replacements(dispatch,
                           {{tokens[3].begin, tokens[3].end, expected_new}});
    } else if (tokens[3].value != expected_new) {
        fail("provider-refresh-pack-analysis-identity");
    }
}

void rewrite_resident_generation(std::string& dispatch,
                                 const std::uint64_t old_generation,
                                 const std::uint64_t new_generation) {
    constexpr std::string_view marker{"resident_primary_generation = "};
    const auto marker_offset = unique_marker(dispatch, marker);
    auto begin = marker_offset + marker.size();
    auto end = begin;
    while (end < dispatch.size() && dispatch[end] >= '0' &&
           dispatch[end] <= '9')
        ++end;
    if (begin == end || dispatch.substr(end, 3u) != "ull")
        fail("provider-refresh-resident-generation-shape");
    std::uint64_t current = 0u;
    const auto [parsed_end, error] = std::from_chars(
        dispatch.data() + begin, dispatch.data() + end, current, 10);
    if (error != std::errc{} || parsed_end != dispatch.data() + end)
        fail("provider-refresh-resident-generation-value");
    if (current == old_generation) {
        apply_replacements(dispatch,
                           {{begin, end, std::to_string(new_generation)}});
    } else if (current != new_generation) {
        fail("provider-refresh-resident-generation-mismatch");
    }
}

void rewrite_dispatch_build_marker(std::string& dispatch,
                                   const std::string_view old_artifact,
                                   const std::string_view new_artifact) {
    constexpr std::string_view marker{
        "native_port_build_identity_marker = \""};
    const auto marker_offset = unique_marker(dispatch, marker);
    const auto value_begin = marker_offset + marker.size();
    const auto value_end = dispatch.find('"', value_begin);
    if (value_end == std::string_view::npos)
        fail("provider-refresh-dispatch-marker-shape");
    const auto old_value =
        "KATANA_NATIVE_PORT_BUILD_IDENTITY_V1:" + std::string(old_artifact);
    const auto new_value =
        "KATANA_NATIVE_PORT_BUILD_IDENTITY_V1:" + std::string(new_artifact);
    const auto current = dispatch.substr(value_begin, value_end - value_begin);
    if (current == old_value) {
        apply_replacements(dispatch,
                           {{value_begin, value_end, new_value}});
    } else if (current != new_value) {
        fail("provider-refresh-dispatch-marker-value");
    }
}

std::size_t rewrite_provider_fields(
    std::string& dispatch,
    const katana::runtime::NativePortDefinition& before,
    const katana::runtime::NativePortDefinition& after) {
    const auto hooks_region = array_region(dispatch, "native_hooks{{");
    const auto semantic_region = array_region(
        dispatch, "native_provider_semantic_contracts{{");
    if (hooks_region.entries.size() != before.hooks.size() ||
        hooks_region.entries.size() != after.hooks.size() ||
        semantic_region.entries.size() !=
            before.provider_semantic_contracts.size() ||
        semantic_region.entries.size() !=
            after.provider_semantic_contracts.size())
        fail("provider-refresh-dispatch-array-cardinality");

    std::vector<TextReplacement> replacements;
    std::size_t changed = 0u;
    for (std::size_t index = 0u; index < hooks_region.entries.size(); ++index) {
        const auto [begin, end] = hooks_region.entries[index];
        const auto entry = std::string_view(dispatch).substr(begin, end - begin);
        const auto tokens = quoted_tokens(entry, 0u, entry.size());
        const auto suffix = hook_suffix(before.hooks[index]);
        if (tokens.size() != 4u ||
            !entry.starts_with(hook_prefix(before.hooks[index])) ||
            entry.size() < suffix.size() ||
            entry.substr(entry.size() - suffix.size()) != suffix)
            fail("provider-refresh-dispatch-hook-structure");
        const auto old_identity =
            before.hooks[index].provider_implementation_identity;
        const auto new_identity =
            after.hooks[index].provider_implementation_identity;
        if (tokens[2].value == old_identity) {
            if (old_identity != new_identity) {
                replacements.push_back({begin + tokens[2].begin,
                                        begin + tokens[2].end,
                                        std::string(new_identity)});
                ++changed;
            }
        } else if (tokens[2].value != new_identity) {
            fail("provider-refresh-dispatch-hook-provider");
        }
    }
    for (std::size_t index = 0u;
         index < semantic_region.entries.size(); ++index) {
        const auto [begin, end] = semantic_region.entries[index];
        const auto entry = std::string_view(dispatch).substr(begin, end - begin);
        const auto tokens = quoted_tokens(entry, 0u, entry.size());
        const auto& before_contract =
            before.provider_semantic_contracts[index];
        const auto& after_contract = after.provider_semantic_contracts[index];
        if (tokens.size() != 8u ||
            !entry.starts_with(semantic_prefix(before_contract)) ||
            tokens[0].value != before_contract.provider_symbol ||
            tokens[1].value != before_contract.semantic_identity ||
            tokens[2].value != before_contract.expected_owner_semantic_identity ||
            tokens[4].value != before_contract.result.target_expression ||
            tokens[5].value != before_contract.result.error_expression ||
            tokens[6].value != before_contract.result.cpu_state_expression ||
            tokens[7].value != before_contract.result.title_state_expression)
            fail("provider-refresh-dispatch-semantic-structure");
        const auto old_identity =
            before_contract.provider_implementation_identity;
        const auto new_identity =
            after_contract.provider_implementation_identity;
        if (tokens[3].value == old_identity) {
            if (old_identity != new_identity) {
                replacements.push_back({begin + tokens[3].begin,
                                        begin + tokens[3].end,
                                        std::string(new_identity)});
                ++changed;
            }
        } else if (tokens[3].value != new_identity) {
            fail("provider-refresh-dispatch-semantic-provider");
        }
    }
    apply_replacements(dispatch, std::move(replacements));
    return changed;
}

[[nodiscard]] std::size_t count_audit_marker(
    const std::string_view audit,
    const std::string_view value) {
    const auto tokens = quoted_tokens(audit, 0u, audit.size());
    return static_cast<std::size_t>(std::ranges::count_if(
        tokens, [&](const auto& token) { return token.value == value; }));
}

void rewrite_audit_marker(std::string& audit,
                          const std::string_view old_artifact,
                          const std::string_view new_artifact) {
    const auto old_value =
        "katana_native_port_build_identity_v1:" + std::string(old_artifact);
    const auto new_value =
        "katana_native_port_build_identity_v1:" + std::string(new_artifact);
    auto tokens = quoted_tokens(audit, 0u, audit.size());
    const auto old_count = count_audit_marker(audit, old_value);
    const auto new_count = count_audit_marker(audit, new_value);
    if (old_count == 1u && new_count == 0u) {
        for (const auto& token : tokens) {
            if (token.value == old_value) {
                apply_replacements(audit,
                                   {{token.begin, token.end, new_value}});
                return;
            }
        }
    }
    if (old_count == 0u && new_count == 1u) return;
    fail("provider-refresh-audit-marker");
}

void validate_parser_self_check() {
    std::string binding =
        "runtime_frontier_binding{\n"
        "\"0000000000000000000000000000000000000000000000000000000000000000\",\n"
        "\"content\",\n"
        "\"sha256:boot\",\n"
        "\"project\",\n"
        "\"analysis\",\n"
        "\"image\",\n"
        "\"sha256:game\",\n"
        "\"native-before\",\n"
        "\"sha256:artifact-before\",\n"
        "\"implementation\",\n"
        "\"cache\",\n"
        "\"ir\",\n"
        "\"codegen\",\n"
        "72u, 26u, 1u, 45000u, 0u, 0u};";
    const auto parsed = parse_runtime_binding(binding);
    apply_replacements(
        binding,
        {{parsed.tokens[0].begin,
          parsed.tokens[0].end,
          std::string(64u, 'f')},
         {parsed.tokens[7].begin,
          parsed.tokens[7].end,
          "native-after"},
         {parsed.tokens[8].begin,
          parsed.tokens[8].end,
          "sha256:artifact-after"}});
    const auto reparsed = parse_runtime_binding(binding);
    if (reparsed.tokens[0].value != std::string(64u, 'f') ||
        reparsed.tokens[7].value != "native-after" ||
        reparsed.tokens[8].value != "sha256:artifact-after")
        fail("provider-refresh-self-check-token-range");

    const std::string array_fixture =
        "native_hooks{{\n"
        "    {\"alpha\", \"\"},\n"
        "    {\"beta\", \"tail\"},\n"
        "}};";
    const auto region = array_region(array_fixture, "native_hooks{{");
    if (region.entries.size() != 2u)
        fail("provider-refresh-self-check-array-depth");
}

} // namespace

int main(const int argc, char* argv[]) {
    try {
        validate_parser_self_check();
        if (argc == 2 && std::string_view(argv[1]) == "--self-test") {
            std::cout << "KATANA_NATIVE_PROVIDER_REFRESH_SELF_TEST_OK\n";
            return 0;
        }
        if (argc != 4) {
            std::cerr
                << "usage: sonic-native-provider-refresh <generated-root> "
                   "<before.katana-native-port> <after.katana-native-port>\n";
            return 2;
        }
        const auto generated_root =
            std::filesystem::absolute(argv[1]).lexically_normal();
        const auto before =
            katana::runtime::NativePortArtifact::load(argv[2]);
        const auto after =
            katana::runtime::NativePortArtifact::load(argv[3]);

        const auto render_extension = render_hook_extension(before->definition(), after->definition());
        TemporaryArtifacts temporary;
        const auto scratch = generated_root.parent_path();
        const auto before_neutral = neutral_definition_identity(
            render_extension.before, scratch, "before", temporary);
        const auto after_neutral = neutral_definition_identity(
            after->definition(), scratch, "after", temporary);
        if (before_neutral != after_neutral)
            fail("provider-refresh-non-provider-delta");
        const auto mappings = provider_mappings(
            render_extension.before, after->definition());

        const auto before_native_identity =
            katana::runtime::native_port_definition_export_identity(
                before->definition());
        const auto after_native_identity =
            katana::runtime::native_port_definition_export_identity(
                after->definition());
        if (!valid_raw_sha256(before_native_identity) ||
            !valid_raw_sha256(after_native_identity))
            fail("provider-refresh-native-port-identity");
        const auto before_artifact_identity = before->artifact_identity();
        const auto after_artifact_identity = after->artifact_identity();
        if (before_artifact_identity == after_artifact_identity ||
            before_native_identity == after_native_identity)
            fail("provider-refresh-empty-native-delta");

        const auto dispatch_path =
            generated_root / "code" / "native-port-dispatch.cpp";
        const auto audit_path =
            generated_root / "tools" / "native-port-link-audit.cpp";
        auto dispatch = read_regular_file(dispatch_path);
        auto audit = read_regular_file(audit_path);
        const auto dispatch_before_sha =
            "sha256:" + katana::io::sha256_bytes(dispatch);
        const auto audit_before_sha =
            "sha256:" + katana::io::sha256_bytes(audit);
        insert_render_hooks(dispatch, audit, render_extension, generated_root);

        std::string old_key;
        std::string new_key;
        const auto binding_state = rewrite_runtime_binding(
            dispatch,
            before_native_identity,
            before_artifact_identity,
            after_native_identity,
            after_artifact_identity,
            old_key,
            new_key);
        if (binding_state == CanonicalState::Before && old_key == new_key)
            fail("provider-refresh-runtime-frontier-key-delta");
        rewrite_dispatch_build_marker(
            dispatch, before_artifact_identity, after_artifact_identity);
        rewrite_pack_analysis_identity(dispatch, old_key, new_key);
        auto parsed_after_binding = parse_runtime_binding(dispatch);
        auto old_generation_identity = parsed_after_binding.identity;
        old_generation_identity.native_port_identity = before_native_identity;
        old_generation_identity.native_port_artifact_identity =
            before_artifact_identity;
        old_generation_identity.key = old_key;
        auto new_generation_identity = parsed_after_binding.identity;
        new_generation_identity.native_port_identity = after_native_identity;
        new_generation_identity.native_port_artifact_identity =
            after_artifact_identity;
        new_generation_identity.key = new_key;
        rewrite_resident_generation(
            dispatch,
            resident_generation(old_generation_identity),
            resident_generation(new_generation_identity));
        const auto provider_references = rewrite_provider_fields(
            dispatch, render_extension.before, after->definition());
        rewrite_audit_marker(
            audit, before_artifact_identity, after_artifact_identity);

        const auto canonical = parse_runtime_binding(dispatch);
        validate_runtime_identity(canonical.identity);
        if (canonical.identity.key != new_key ||
            canonical.identity.native_port_identity != after_native_identity ||
            canonical.identity.native_port_artifact_identity !=
                after_artifact_identity)
            fail("provider-refresh-canonical-runtime-binding");
        const auto after_generation = resident_generation(canonical.identity);
        const auto generation_marker = dispatch.find(
            "resident_primary_generation = ");
        if (generation_marker == std::string::npos)
            fail("provider-refresh-canonical-generation");
        auto generation_begin = generation_marker +
                                std::string_view(
                                    "resident_primary_generation = ").size();
        auto generation_end = generation_begin;
        while (generation_end < dispatch.size() &&
               dispatch[generation_end] >= '0' &&
               dispatch[generation_end] <= '9')
            ++generation_end;
        std::uint64_t observed_generation = 0u;
        const auto [parsed_generation_end, generation_error] = std::from_chars(
            dispatch.data() + generation_begin,
            dispatch.data() + generation_end,
            observed_generation,
            10);
        if (generation_error != std::errc{} ||
            parsed_generation_end != dispatch.data() + generation_end ||
            observed_generation != after_generation)
            fail("provider-refresh-canonical-generation");
        const auto canonical_provider_references = rewrite_provider_fields(
            dispatch, after->definition(), after->definition());
        if (canonical_provider_references != 0u)
            fail("provider-refresh-canonical-provider-fields");
        const auto new_audit_marker =
            "katana_native_port_build_identity_v1:" + after_artifact_identity;
        if (count_audit_marker(audit, new_audit_marker) != 1u)
            fail("provider-refresh-canonical-audit");

        const bool dispatch_changed =
            dispatch_before_sha != "sha256:" + katana::io::sha256_bytes(dispatch);
        const bool audit_changed =
            audit_before_sha != "sha256:" + katana::io::sha256_bytes(audit);
        katana::codegen::ProjectWriteResult result;
        if (dispatch_changed || audit_changed) {
            const std::vector<katana::codegen::ProjectArtifactReplacement>
                replacements{
                    {"code/native-port-dispatch.cpp",
                     dispatch_before_sha,
                     std::move(dispatch)},
                    {"tools/native-port-link-audit.cpp",
                     audit_before_sha,
                     std::move(audit)}};
            result = katana::codegen::rewrite_codegen_project_artifacts(
                generated_root, replacements);
        }
        std::cout
            << "KATANA_NATIVE_PROVIDER_REFRESH_OK old_artifact="
            << before_artifact_identity << " new_artifact="
            << after_artifact_identity << " provider_mappings="
            << mappings.size() << " provider_references="
            << provider_references << " idempotent="
            << ((!dispatch_changed && !audit_changed) ? "true" : "false")
            << " written=" << result.written_files.size() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "KATANA_NATIVE_PROVIDER_REFRESH_FAILED detail="
                  << error.what() << '\n';
        return 1;
    }
}
