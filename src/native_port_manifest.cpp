#include "katana/analysis/native_sdk_provider_analysis.hpp"
#include "katana/io/executable_image.hpp"
#include "katana/io/input_provenance.hpp"
#include "katana/runtime/native_port_artifact.hpp"
#include "katana/runtime/native_port_content.hpp"
#include "katana/runtime/runtime.hpp"

#include "postpal_bootstrap_state.hpp"
#include "native_latent_texture_dispatch_provider_identity.hpp"
#include "native_provider_identity.hpp"
#include "native_spg_status_provider_identity.hpp"

#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {

// This value is intentionally reviewed, not generated.  CMake exposes the
// actual source identity separately and this assertion forces an explicit
// contract re-proof whenever the provider implementation changes.
// r305: each successful native drain retains its exact RAM tail (642916),
// invoking the admitted 656420 helper once before the next scene. Pure UI
// Type-2 TR lists share autosort under the existing per-producer authority.
// Sprite wrappers preserve all nine SDK snapshot/commit words, including
// the five physical list cursors and the flags-dependent permutation. Header
// and clipped vertex counts advance only the selected, prevalidated cursor.
// Its target can be a currently mapped main-RAM list buffer or the TA aperture;
// 6063F0 derives the region from that target, preserving RAM alias prefixes.
// culled route sprites retain the raw transaction before texture selection.
// Shader, material, host geometry and blend contracts remain unchanged. See
// private/diagnostics/r303-sdk-sprite-cursor-review for source evidence and
// the component build. The r302 canonical archive-stem resolver and the r301
// live-registry SDK-free/resident texture ownership boundaries remain intact.
// The three hardware-state providers retain their existing instruction effects.
// r310 only extends the bounded active-gameplay timing probe with p95/p99
// frame intervals. Its fixed storage is sorted after the measured window;
// guest execution, provider calls, resource ownership and save data are unchanged.
// r312 separates exact module-overwrite cache retirement from guest SDK release.
// The sole historical Named lease is retired only after owner/generation/token
// validation; guest registry words and cached GPU resources are not changed.
// Direct SDK release keeps its ABI and now records the failing owner header.
// r313 discards ownerless renderer lookup metadata before an explicit archive
// or texture-set acquire. Every publication/lease/module field must be empty,
// with no reverse set claim; the normal exact SDK transaction then starts fresh.
// Guest RAM, live owners, resident references and cached GPU assets stay intact.
// Named PVM loads use the archive's N allocations and the caller's independent
// M-entry TEXLIST copy, including the persistent staging tail. Split SDK rows
// and mutable views retain exact native Type0 acquire/release semantics; the
// schema-4 development state serializes those allocations and retokenizes their
// host resources on restore. Existing normal texture-set leases remain intact.
// r316 retains Main's exclusive publication of the default TEXLIST/loading
// globals: retail 098EEE only changes the Character reference counter, while
// 09901E owns those globals. Movie failures add bounded source/phase evidence;
// the existing playback and return contracts are unchanged. The opt-in text
// observer only reads the existing queue before/after 055B32 and never changes
// message timers, uploads or guest control flow.
// r317 replaces the packet-count stop with an allocation-capacity budget,
// including retained buffers and reallocation peaks. Active packet order and
// draw semantics are unchanged. Reviewed exact SDK tail transfers in Primary
// and STG04 authenticate their real transfer/delay bytes and active AOT owner;
// descriptor renewal, release and executable-generation predicates stay intact.
// r318 seals an adapter-owned open GPU frame before the existing movie owner
// takes over. Draw order and leases use the normal retirement path; the
// handoff adds no simulation tick, guest timer update or title audio service.
// Mismatched frame owners and orphaned draw queues still fail closed.
// Sprite diagnostics label a texture Bound only after a Bound resolution;
// the optional trace no longer reports an unbound sentinel as a second success.
// r319 adds one source-verified Sky Chase diagnostic selector. It uses the
// existing state-12 owner and leaves the Action Stage and execution contracts
// unchanged; no extra provider call, guest entry or story flag is introduced.
// Empty oversized geometry buffers are released before reuse; completed draws
// recycle in stable order. The host queue budget is 1 GiB, allocated on demand,
// with failure-only allocation details. Active draw semantics are unchanged.
// r320 recognizes SDK-registered unloaded foreign rows as displaced host
// claims, preserving their contents and acquiring a separate free slot.
// The common event/recap text release executes the existing compiled SDK
// row/VRAM release after exact caller/context/surface validation; actual row
// retirement invalidates native views while retaining pending draw handles.
// r321 retains all six possible outputs of a clipped four-point contour.
// Texture reserves the resulting twelve strip-derived list vertices; source
// order, winding and TextureH triangle bounds remain unchanged. Failed vertex
// submissions retain the source branch, contour counts and camera depths in
// the crash capsule without adding work to successful draw submissions.
// r322 validates SDK-named texture carriers by their publication authority
// and exact guest-RAM spans. Heap TEXLIST headers need not lie inside the
// executable image. Full overwrites retire only the verified host lease;
// partial overwrites keep the existing live SDK release transaction.
// r328 permits only authority-ordered descriptor ABA supersession and exact
// SDK-renewable Main-set/named dual aliases. Current owners and balanced leases
// remain protected. Content failures retain their phase and source identity.
// r339: original FTRV XYZ/near projection ignores its discarded fourth output;
// the native normal/clip matrices share that affine contract. Invalid raster
// triangles are culled, with valid unrepresentable geometry still fail-closed.
// SDK release preserves the separate unloaded-row/error paths and validates
// current PVM-view pointers independently of retired host allocation aliases.
// Source evidence and component checks: r339-user-story-crash-batch-20260910a.
// Failure-only FR/XF/release records and 64 additive-material witnesses do not
// modify CPU, texture, material, or control-flow semantics.
// r340 source preparation: failure-only raw release globals; bounded SDK
// owner/mesh witnesses before culling with checked file output. No change to
// rendering or release semantics. Evidence: r340-user-story-crash-batch-20260910a.
// r341: complete Texture flag product; normal import only for actual lighting/
// environment consumers; 6214FA SDK light preparation and float vertex colors.
// Source evidence and nine numeric checks: r341-light-dash-material-20260910a.
// r342: exact carrier release, successful SDK selector observation, SDK-owned
// packet/ARC1-cache publication. SATP is failure-only, no draw suppression.
// Evidence: r342-dash-followup-20260910a; runtime confirmation remains pending.
inline constexpr std::string_view
    sonic_native_title_adapter_provider_implementation_identity{
        // r343: original indexed material-table reads for all Basic owners;
        // generic 6087FC list release calls the compiled 64DD00 leaf in order.
        // Source fixtures, Flycast TA conversion and release-loop components:
        // private/diagnostics/r343-dash-color-20260910a.
        // r344: byte-proved BIOS null-release behavior, ordered SDK leaf calls,
        // and bounded incomplete-list diagnostics; retained unchanged in r345.
        // r346: provenance aliases and saved-state identity include the physical
        // SDK row. Separate publications keep separate pins; conflicting payloads
        // on the same live row remain rejected. No new AOT target or state ABI.
        // Evidence: r346-super-sonic-20260910a/texture-review.
        // r347: source-authored alias TEXLISTs resolve their exact current PVM
        // descriptor before catalog fallback; unchanged owner/payload guards.
        // Failure-only Basic selection/row evidence now reaches crash capsules.
        // r348: exact active TextureSet ownership survives named acquisition
        // through a module-resident TEXLIST, including repeat/release/renewal.
        // Failure-only SAOW evidence separates raw carrier and resource owner.
        // r352: source-bound standalone SDK PVR import, exact custom keys and
        // existing SDK allocation/release transactions; module fonts unchanged.
        // Both ADVERTISE Trial builders reject their unpopulated seventh row.
        // Evidence: r352-postcomplete-menu-font-20260910a.
        // r354: SDK block-range reads, create-only VMU game writes and real
        // completion results; separate persistent Chao file and original
        // eight-byte dates. SDK638 owns its ARC1/TSP and unlit packed colors;
        // the monitor's authored off material now reaches texture modulation.
        // Source review and components: r354-save-sdk-20260911a and
        // r354-graphics-20260911a. Hardware-provider semantics are unchanged.
        // Sonic-only Hor+: an opt-in host presentation transform, explicit
        // world/UI/fade provenance and source-bound HUD anchors. Original
        // mode leaves packets unchanged; only host copies of horizontal
        // render-cull bounds expand. Guest projection/timing/save semantics
        // and every existing function/source binding remain unchanged.
        // Host dialogs use independent physical input polls. Rendering keeps
        // TitleBasic's original 038F10 UV factor separate from SDK 1/256;
        // typed full-width color planes cover the selected display aspect.
        // Host quit/movie input follows configured physical bindings; movie
        // Master gain preserves decoder timestamps, stream ends and counts.
        // Settings cap internal rendering at 100 percent. No AOT regeneration.
        // Render interpolation is compiled out; retired INI keys are ignored.
        // Private hidden gameplay trace observes the two existing periodic
        // samples, elapsed result and wait argument. CPU/RAM/timer effects,
        // source bindings and the retained AOT pack remain unchanged.
        "sha256:37f2495a2ec1be6d1ad3aa7c16840740ba4412e2f1b62f5f471df1cf20c6841a"};
static_assert(sonic_native_title_adapter_source_identity ==
              sonic_native_title_adapter_provider_implementation_identity);
static_assert(
    sonic_native_latent_texture_dispatch_dependency_source_identity ==
    sonic_native_title_adapter_provider_implementation_identity);

[[nodiscard]] std::vector<std::uint8_t> read_verified_boot_image(
    const std::filesystem::path& path,
    const std::size_t expected_size,
    const std::string_view expected_sha256) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("native-manifest-boot-open");
    const std::vector<char> raw(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>{});
    const std::vector<std::uint8_t> bytes(raw.begin(), raw.end());
    if (input.bad() || bytes.size() != expected_size)
        throw std::runtime_error("native-manifest-boot-size");
    const auto* data = reinterpret_cast<const char*>(bytes.data());
    if (katana::io::sha256_bytes(
            std::string_view(data, bytes.size())) != expected_sha256)
        throw std::runtime_error("native-manifest-boot-identity");
    return bytes;
}

[[nodiscard]] const katana::analysis::NativeSdkProviderCandidate&
require_unique_provider(
    const std::span<const katana::analysis::NativeSdkProviderCandidate>
        candidates,
    const katana::analysis::NativeSdkProviderFamily family) {
    const auto first = std::ranges::find_if(
        candidates, [&](const auto& candidate) {
            return candidate.family == family;
        });
    if (first == candidates.end() ||
        std::find_if(std::next(first), candidates.end(),
                     [&](const auto& candidate) {
                         return candidate.family == family;
                     }) != candidates.end())
        throw std::runtime_error("native-manifest-sdk-provider-ambiguity");
    return *first;
}

void require_guest_texture_descriptor_contract(
    const katana::analysis::NativeSdkProviderCandidate& candidate) {
    if (!candidate.resource_reference.has_value())
        throw std::runtime_error(
            "native-manifest-sdk-resource-reference-missing");
    const auto& resource = *candidate.resource_reference;
    if (resource.kind != katana::analysis::
                             NativeSdkResourceReferenceKind::
                                 GuestDescriptorPointer ||
        resource.owner_record_stride != 12u ||
        resource.reference_field_offset != 8u ||
        resource.descriptor_stride != 68u ||
        resource.minimum_descriptor_bytes == 0u ||
        resource.minimum_descriptor_bytes > resource.descriptor_stride ||
        !resource.observed_write_offsets.empty())
        throw std::runtime_error(
            "native-manifest-sdk-resource-reference-invalid");

    constexpr std::array<std::uint32_t, 4u> supported_read_offsets{
        0u, 32u, 36u, 40u};
    if (resource.observed_read_offsets.empty() ||
        !std::ranges::all_of(
            resource.observed_read_offsets,
            [&](const std::uint32_t offset) {
                return std::ranges::find(supported_read_offsets, offset) !=
                       supported_read_offsets.end();
            }))
        throw std::runtime_error(
            "native-manifest-sdk-resource-field-unsupported");
}

} // namespace

int main(const int argc, char* argv[]) {
    try {
        if (argc != 3)
            throw std::invalid_argument(
                "usage: sonic-native-port-manifest <output-artifact> "
                "<verified-boot-image>");

        constexpr std::array images{
            katana::runtime::NativePortImageBinding{
                "sa-pal-v1003-ip",
                "ip.bin",
                "sha256:0b257318c1273095d4236ce2df84e2d673a89c8a7a0059b763f181bc8a793265",
                0u,
                0xAC008000u,
                32'768u,
                true},
            katana::runtime::NativePortImageBinding{
                "sa-pal-v1003-boot",
                "boot.bin",
                "sha256:b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af",
                0u,
                0x8C010000u,
                6'735'296u,
                true}};
        constexpr std::array static_hooks{
            katana::runtime::NativePortHookBinding{
                0x8C6042B0u, 6u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_rumble_capability",
                "sha256:2eb2196012d5e864de7c33573a13e8f3179d01a955e1d5994c123eac1314593c",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6042B6u, 0x64u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_rumble_configure",
                "sha256:882a220b86201c6e457a2b995de1087fb3a4b709e12e54a42a84969c8691fa2c",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C60431Au, 0x2Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_rumble_request",
                "sha256:aee276d94ff40aff8c76eec88d446d6cbaf705f5dd380762e9dcba37fe97bfba",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C604348u, 0x3Cu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_rumble_stop",
                "sha256:aa649b8b79189f8ddce7fc3b813fe6391e5be8d3a9878be5113991c409200c10",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x808929BEu, 0x38u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_options_legacy_display",
                "sha256:634269bce4e226bfd5c6296653363581c690e2f55c38810eb90348c78dcbefdf",
                sonic_native_title_adapter_provider_implementation_identity,
                katana::runtime::NativePortHookCodeSource::LatentAotModule,
                "sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5"},
            katana::runtime::NativePortHookBinding{
                0x8089928Eu, 0xD8u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_legacy_video_mode_disabled",
                "sha256:2eea7fcacf69722f68fb85461b4a455b2ae301aa89b6785df124ad3a95a32bb8",
                sonic_native_title_adapter_provider_implementation_identity,
                katana::runtime::NativePortHookCodeSource::LatentAotModule,
                "sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5"},
            katana::runtime::NativePortHookBinding{
                0x8C0884A0u, 0xA8u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_language_save",
                "sha256:bc707d8f911b559cb66eb1c91d169519fe462a9cc3d6adabe0bb031013499fa2",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C019F4Au, 0x158u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_recompiled_camera_original_step",
                "sha256:ed23827fa453d89252cda31480e5ae1854976d41680f155904e9f525eaf7186e",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C01A100u, 0xACu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_recompiled_camera_publish",
                "sha256:f88a14755daffb71dc3b9490f35660e768605e121e8f83e5df2dd2a8f541d002",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C0885C0u, 0x74u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_language_loaded",
                "sha256:77a9ef8bf117b7ba4048071acecd6d6705ec1e334f30ba4154eb46c37cda3446",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C0544E2u, 0x11Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_language_initial",
                "sha256:b0cd74b8c534c9e18ff099e03ff17e1db8616a5bf35a950726dda6153aa26552",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C08A4B2u, 6u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_language_subtitles_loaded",
                "sha256:1c9358b9d3149b6cd8d3d5fec7dd9b76a182525e015b858b985720eed6ee6098",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8088CA94u, 0x28u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_language_subtitles",
                "sha256:4e3875cafc64b9e68ffcfe9f991da55405e9ca14632293ffb923234e959c1410",
                sonic_native_title_adapter_provider_implementation_identity,
                katana::runtime::NativePortHookCodeSource::LatentAotModule,
                "sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5"},
            katana::runtime::NativePortHookBinding{
                0x8088CAF8u, 0x50u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_language_voice",
                "sha256:d3c2ba0cf8c234b9d1bfdb967d78242e328b23d71111869aecb33746a32d2eeb",
                sonic_native_title_adapter_provider_implementation_identity,
                katana::runtime::NativePortHookCodeSource::LatentAotModule,
                "sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5"},
            katana::runtime::NativePortHookBinding{
                0x8088CBC0u, 0x44u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_language_text",
                "sha256:356d2b2811becea7dc9372eaaed12bee6dd766a160b11004425a5bb01468d3a1",
                sonic_native_title_adapter_provider_implementation_identity,
                katana::runtime::NativePortHookCodeSource::LatentAotModule,
                "sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5"},
            katana::runtime::NativePortHookBinding{
                0x8088C060u, 126u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_trial_character_list_guard",
                "sha256:ff8599701e1fba1577bef717fa9cc7fa811245727c8a8d5b3b8686c8a0483165",
                sonic_native_title_adapter_provider_implementation_identity,
                katana::runtime::NativePortHookCodeSource::LatentAotModule,
                "sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5"},
            katana::runtime::NativePortHookBinding{
                0x8088C0DEu, 126u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_trial_minigame_list_guard",
                "sha256:2afe4f46ae16d6838ff71ea746562542b86f19fbaa83eab0ac4035951b3318a4",
                sonic_native_title_adapter_provider_implementation_identity,
                katana::runtime::NativePortHookCodeSource::LatentAotModule,
                "sha256:6e8a5806f1f32e6c17c70c30c953600f16fcdb4959b8cd91094c4b32062793d5"},
            katana::runtime::NativePortHookBinding{
                0x8C608BC2u, 0x4Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_standalone_texture_list_load",
                "sha256:7857f904bc38ccaaab02ea247a1d307ab6be52d65696dee28a044a78b30790a6",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x809961AEu,
                0x42u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_latent_texture_archive_dispatch_809961ae",
                "sha256:907d46242287b63b992c9c30c79924dcdeb7d76d6d11f754667366bb5051adc7",
                sonic_native_latent_texture_dispatch_provider_source_identity,
                katana::runtime::NativePortHookCodeSource::LatentAotModule,
                "sha256:f9e729e86d885d6644286022bd1a36f8082de60774160eae15c2a0b57aa88ac9"},
            katana::runtime::NativePortHookBinding{
                0x8297C3E0u,
                0x1Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_spg_status_boundary_wait",
                "sha256:7896e9c455055d132dd6ed717384af114713109f333d971252bb4495c20615f5",
                sonic_native_spg_status_provider_source_identity,
                katana::runtime::NativePortHookCodeSource::LatentAotModule,
                "sha256:823f58ab9b914226a99226263357883de4e0e2cd4daac0c750cd5c0382a6b7c7"},
            katana::runtime::NativePortHookBinding{
                0x840D03E0u,
                0x1Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_spg_status_boundary_wait_840d03e0",
                "sha256:7896e9c455055d132dd6ed717384af114713109f333d971252bb4495c20615f5",
                sonic_native_spg_status_provider_source_identity,
                katana::runtime::NativePortHookCodeSource::LatentAotModule,
                "sha256:717601f4ab2c76e4f9ea8b568c2c9c1d20876493e81d401802d7180f6c9f174c"},
            katana::runtime::NativePortHookBinding{
                0x8C10B780u,
                0x66u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_play_movie",
                "sha256:b23b3fe011cbe682bb91fff4e2a8ad1868dc8dd6189c38d5fd9f42dda7fe27a7"},
            katana::runtime::NativePortHookBinding{
                0x8C10D866u,
                12u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_cable_bits",
                "sha256:7aa8ce5ffd7494f38fac86da9d9797cd93fbd3bcd6c0a2a0f17646aa42987205"},
            katana::runtime::NativePortHookBinding{
                0x8C10ED60u,
                52u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transfer_resume_all",
                "sha256:030c1eb98dd48aed5a385b617c8ed1633683e8015f0d6dc92de18f7c6e1051d0"},
            katana::runtime::NativePortHookBinding{
                0x8C10EDDCu,
                52u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transfer_suspend_all",
                "sha256:d548aa206d282a08cc9ba1617af6a40787fed6597ce2bb98b7d8fa0f783b712c"},
            katana::runtime::NativePortHookBinding{
                0x8C6044E8u,
                12u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transfer_fifo_ready_raw",
                "sha256:0e5047def861d958b331f252bd671a719c83dfc02d5c63ee9823c3ed0c961189"},
            katana::runtime::NativePortHookBinding{
                0x8C10F8E8u,
                14u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transfer_audio_fifo_ready",
                "sha256:a12fc9a8213e95795a395d85b19d9801e9c4520b34a90857f097be298e363440"},
            katana::runtime::NativePortHookBinding{
                0x8C10FA88u,
                14u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transfer_fifo_ready",
                "sha256:152993394503801324548cb45376c8372fe572301c49d3da4efaa662ea856310"},
            katana::runtime::NativePortHookBinding{
                0x8C5FC018u,
                0x25Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_operand_cache_invalidate_range",
                "sha256:326995f3c306c8765bbcbae2ef0cef5bf6afa7dcf7b9dcbc21e2e57f8ec3a2bb"},
            katana::runtime::NativePortHookBinding{
                0x8C5FC276u,
                0x254u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_operand_cache_purge_range",
                "sha256:1e56e4b6a41ee69ccf93928a759194070b0de468c61ec751b58fd34462edc6f9"},
            katana::runtime::NativePortHookBinding{
                0x8C5FC4CAu,
                0x292u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_operand_cache_writeback_range",
                "sha256:13a67f00ed7ed48d20954620956f703bdc78023284d1fce1b2f2cadcd814f48e"},
            katana::runtime::NativePortHookBinding{
                0x8C5FC75Cu,
                0xB0u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_operand_cache_invalidate_all",
                "sha256:2c46f62cf4ee6c84c8542fd91e7aa2bf4fd33f842fae9406a0bfa3eb042524dd"},
            katana::runtime::NativePortHookBinding{
                0x8C6044F4u,
                268u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_audio_control_transaction",
                "sha256:4eec990489beb8b50d76933a185c85cc4cae4c5bda316955a5ebe250b1341823"},
            katana::runtime::NativePortHookBinding{
                0x8C604600u,
                66u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_audio_output_control",
                "sha256:0ade153a1fe031ab8819f2fd2e99c31830a86ba8e833004c39dbaef39bad05f0"},
            katana::runtime::NativePortHookBinding{
                0x8C65A140u,
                104u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_audio_processor_stop",
                "sha256:5bd9cf59ca5e733b6937295b55acc139c0b30be15c4af48793498040f0b2fb17"},
            katana::runtime::NativePortHookBinding{
                0x8C65A1BCu,
                106u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_audio_processor_start",
                "sha256:673625fc894ba5787d1df718f95b5828fe0e7982d90bc447dd0c6fa803e90a8a"},
            // The retail state-12 selector omits STG12 although Amy, Gamma
            // and Big each have a real Hot Shelter route.  Preserve the
            // complete original transition owner and replace only the
            // (level, act) arguments at its exact SetLevelAndAct boundary.
            katana::runtime::NativePortHookBinding{
                0x8C04F76Cu,
                0x1Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_private_stage_tuple_override",
                "sha256:f136afd5dc49328b910d2b0bd247d0e9ad0e3781b00d38ff5dca6e5183dd0d00",
                sonic_native_title_adapter_provider_implementation_identity},
            // Exact collision-work allocation failure branch. The original
            // code is a non-terminating diagnostic loop, not a hardware wait:
            // preserve the successful AOT owner and fail typed at the first
            // instruction reached after its 28-byte allocation returned zero.
            katana::runtime::NativePortHookBinding{
                0x8C02F4DEu,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_collision_work_allocation_failure",
                "sha256:6d1bf6692081a332027423dee1cbe1901dbd988c00a66f6aa2b6f16a7de5b202"},
            // Bounded input-consumer identities. These two exact loads live
            // inside owners which ordinary static reachability does not
            // materialize. Keep them non-executable until that reachability
            // exists: a diagnostic must not manufacture an AOT root merely
            // to observe the title's projected pressed word.
            katana::runtime::NativePortHookBinding{
                0x8C04D5F0u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::DiagnosticOnly,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_input_consumer_8c04d5f0",
                "sha256:5b19c951ebe8f742e862b55f328711a6095b19a859e0e610f24b2e8257637f62",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C04E19Eu,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::DiagnosticOnly,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_input_consumer_8c04e19e",
                "sha256:5b19c951ebe8f742e862b55f328711a6095b19a859e0e610f24b2e8257637f62",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C04EBF2u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_input_consumer_8c04ebf2",
                "sha256:6c1b26d38383e98492adf014fe3a53713ae11e1ebd5959f73627dfdb1fe2a358",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C04EC20u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_input_consumer_8c04ec20",
                "sha256:81cbee436d3fdbd9130be691f489f0eb2b62fd348bf85ca753fd75ef96c6282a",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C0A1258u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_input_consumer_8c0a1258",
                "sha256:6bf39e21d5d4b856134d89ddb6b3b59a66183523287e9ec20d30e9809db84652",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C0A6264u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_input_consumer_8c0a6264",
                "sha256:763d264401b67ec0ba5e96d1a30e950b047d00b223fa6a6288a483abd002ff1b",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C0A6388u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_input_consumer_8c0a6388",
                "sha256:ef014e50b98cd58bf2c9b6a966fcd15d41fd0b332048f7785a228962c33ce34a",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6024B0u,
                0x28u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_platform_interrupt_state_owner_8c6024b0",
                "sha256:a1a7903832e600b1d2a2d5a428836ec0c76d6b10535a3ab2afd4043f5a564fcc"},
            // Exact isolated platform instructions. Each binding validates
            // the audited address/value transport and resumes the unchanged
            // static owner; none constructs a guest-visible register bank.
            katana::runtime::NativePortHookBinding{
                0x8C6047BAu,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_pvr_asic_interrupt_normal_acknowledge_8_8c6047ba",
                "sha256:0a8835e2d14ceb47875f235735a97ff5e093696ea9004296e96534b194e00ff2",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C604852u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_system_bus_revision_read_8c604852",
                "sha256:3876a4ca0daecc1f999b11b21ae371ebb2febd73e6daf2904841da65497fef90",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C646750u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_platform_interrupt_iml4_nrm_read_8c646750",
                "sha256:b2239ae33d9bc5614aeb2e439e5032b1fe77c32f7080d3211c1d03f1b17ffcf6",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C646754u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_platform_interrupt_iml4_nrm_write_8c646754",
                "sha256:bad986587868fccde7544c3016b5ac957ca658434435dcae55fadf51eb5bf5e4",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C65F56Cu,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_system_bus_revision_read_8c65f56c",
                "sha256:f5fcb5a1e3534de6007b6c49b3a5f4c545edb7c0e0608a30b20e1695db3e43b2",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6656B8u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_system_bus_revision_read_8c6656b8",
                "sha256:b5196483f90f6525ff241a0480b1fac712a8eaf1793e0f964e3552f9d137c30f",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C653580u,
                76u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_platform_interrupt_state_owner_8c653580",
                "sha256:0b3aec9e6d9b7211298d0b696ab20936f88771670ef069be83cb9b55a74b4ba3"},
            katana::runtime::NativePortHookBinding{
                0x8C64588Cu,
                0x1AEu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_platform_interrupt_register",
                "sha256:b889d8c51420138cc64132c1fdb7396c986de2194d8178db87c4027ea719a103"},
            katana::runtime::NativePortHookBinding{
                0x8C645B94u,
                0x16Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_platform_interrupt_release",
                "sha256:e0806cb781db9571fe2b111d64f19bafd2a8caac085353a6332def2f4873925e"},
            katana::runtime::NativePortHookBinding{
                0x8C645ACEu,
                0xAAu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                // The host registration mirror is complete, but no
                // identity-bound producer for the four ASIC status words is
                // present yet. Keep this dispatcher probe fail-closed until
                // that status/completion owner is proven as well.
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_platform_interrupt_state_owner_8c645ace",
                "sha256:daa1f6de62e8bb260baf7ced260c2e32eba36e76dc55b36b33f9c45b7af259b7"},
            // Complete ISTNRM initialization leaf. The provider preserves
            // every ordered ordinary-RAM effect and publishes only the exact
            // final mask as bounded host state after runtime literal checks.
            katana::runtime::NativePortHookBinding{
                0x8C6460C4u,
                0x8u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_platform_interrupt_normal_mask_seed",
                "sha256:cfe87802c6ec4643393a21fcfcfb95392d7bf1379826ee2ab6e8cb83b525eeca",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6460CCu,
                0x22u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_platform_interrupt_normal_mask_initialize",
                "sha256:d3d1e9401268f893dbc461eaefe22964be9a9a6abe53bff332e5defcc1e3d93f",
                sonic_native_title_adapter_provider_implementation_identity},
            // Four leaf variants form the complete PAL NINJA transform-stream
            // family reached by owner 0x8C611384. The native implementations
            // preserve the owner's descriptor/status contract and write the
            // 32-byte ordinary-RAM stream directly; QACR, store queues, OCBI
            // and PREF are displaced rather than represented as devices.
            katana::runtime::NativePortHookBinding{
                0x8C610FA0u,
                0xBEu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transform_stream_aux_unclipped",
                "sha256:142c3357ed70097d999ed17e86d1fcfbfe1884009eeca0ddcb709d84bd68330a"},
            katana::runtime::NativePortHookBinding{
                0x8C611062u,
                0xB0u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transform_stream_unclipped",
                "sha256:6520686d5f9d500b79a467cfe71411a67d5bfd64db7c19c9d18d16ef3db876cb"},
            katana::runtime::NativePortHookBinding{
                0x8C611116u,
                0x108u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transform_stream_aux_clipped",
                "sha256:5dd94abb4dd04f1be202293a0cc2882e02c8456175ac2d284122a240f46de9ba"},
            katana::runtime::NativePortHookBinding{
                0x8C611222u,
                0xFCu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transform_stream_clipped",
                "sha256:65649cb73906c5e49a88c35119498940c42f304de81a18a2959871c4f122a34b"},
            katana::runtime::NativePortHookBinding{
                0x8C6509E0u,
                0x70u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                // Exact PAL initializer: the verified owner publishes seven
                // globals and two banks of eight descriptor statuses, then
                // returns with the audited GPR/T state. Its displaced DMAC,
                // System-Bus and QACR writes are retained as bounded private
                // native state. Submit/drain remain independent probes until
                // their TA consumer and completion contract is proven.
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transfer_ring_initialize",
                "sha256:39989baf1c213cd3d31e922951a5745c39f0a6fe86578663bf48065f41a7a573",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C650AA0u,
                0x68u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                // The reset provider closes the complete identity-bound
                // owner: it publishes the six ordinary-RAM globals and all
                // sixteen descriptor status words, returns r0=0 and projects
                // the original CHCR2 clear-bit-0 result into the native ring
                // state.  Submit/drain remain probes until their consumer is
                // independently identity-bound.
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transfer_ring_reset",
                "sha256:64392a299a96c48c6b9622aa5f6f7746e1dbdcbd0600bcae5f0269e7cba29c13"},
            katana::runtime::NativePortHookBinding{
                0x8C650B08u,
                0x220u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transfer_ring_submit",
                "sha256:8f1f84968b79f39aa23663ea3d8c9fecd9d6889ac4c6f5262d4653969371d642"},
            katana::runtime::NativePortHookBinding{
                0x8C650D80u,
                0x138u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_transfer_ring_drain",
                "sha256:f665836dc872ef85da4d7ad42ee18a942e80d3b53ffea37828758e3fce5e304e"},
            // PAL-verified PVR/System-ASIC owners. Their displaced P4/ASIC
            // writes are represented by the bounded native transaction
            // provider; unresolved status/Manatee completion remains a
            // BringUpProbe and therefore does not discharge hardware closure.
            katana::runtime::NativePortHookBinding{
                0x8C6515E0u,
                0x2AAu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_pvr_asic_channel_setup",
                "sha256:3b1ec9eb9d10ebf85be325990a5a8ac456cce4caee0868d4adf45bc363d2e73e"},
            katana::runtime::NativePortHookBinding{
                0x8C65188Au,
                0x216u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_pvr_asic_service",
                "sha256:424c677f0f3e7348971bb7eeb17a0b04c3d5909388b02ee3f821042e3f1f46a5"},
            katana::runtime::NativePortHookBinding{
                0x8C651B00u,
                0x35Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_pvr_asic_scheduler",
                "sha256:95a42b86594af4b7573d3e29f0b1bbe0c294dc7c6e56479fb09bf5ff6522223f"},
            // Two exact ISTERR stores precede owner-local RAM/call effects.
            // Replace only the isolated hardware instruction so the complex
            // owners and their optional callee remain statically recompiled.
            katana::runtime::NativePortHookBinding{
                0x8C6520DCu,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_pvr_asic_interrupt_error_acknowledge_4",
                "sha256:85d35af01a555c7ef4431e4d492ab1e63c969acc925cf5113ef848e46e814a9a",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C652110u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_pvr_asic_interrupt_error_acknowledge_8",
                "sha256:85d35af01a555c7ef4431e4d492ab1e63c969acc925cf5113ef848e46e814a9a",
                sonic_native_title_adapter_provider_implementation_identity},
            // Complete ISTERR acknowledgement leaf.  The provider keeps the
            // hardware publication as bounded native state and reproduces
            // the two path-sensitive ordinary-RAM effects and CPU clobbers.
            katana::runtime::NativePortHookBinding{
                0x8C652132u,
                0x1Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_pvr_asic_error_acknowledge",
                "sha256:9cec7b1b230eb9462bc8a3b32445b0b483f73e8bc899647125b0faeb11602978",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C652150u,
                0x2C2u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_pvr_asic_dispatch",
                "sha256:786b52482aa5174699087e380ee1b769f4d442b0384688834ea44a9d6f3555fc"},
            // NativeBringup Candidate for the sole replay-witnessed member
            // of this raw store family.  The provider additionally requires
            // the exact owner PC, A05F8008 target and value 1 at runtime; all
            // other 0x2452 stores remain unbound and fail closed.
            katana::runtime::NativePortHookBinding{
                0x8C653388u,
                2u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::BringUpProbe,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_pvr_softreset_write_1_8c653388",
                "sha256:0d69dfb69160a379cb74bc622b3350b314963ca89a944734a470f2f8b3a70ffb",
                sonic_native_title_adapter_provider_implementation_identity},
            // Complete G2 transfer-bus configuration leaf.  Native audio and
            // content services own the transfers themselves; this provider
            // retains the exact encoded range, title-RAM mirror and ABI
            // clobbers without constructing a G2 register device.
            katana::runtime::NativePortHookBinding{
                0x8C10EDA4u,
                0x28u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_g2_bus_configure",
                "sha256:cec7d30708f26fd7ffdae0a111e36d26291d3098b89ccae4f8d8ae13c1cafabc"},
            katana::runtime::NativePortHookBinding{
                0x8C092F28u,
                132u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_sound_output_mode_initialize",
                "sha256:181ab47081755b765749ed972654b10836cb3c0d49cdf49092a0c948b723ff39"},
            katana::runtime::NativePortHookBinding{
                0x8C092FACu,
                40u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_sound_port_table_initialize",
                "sha256:0ca258f84bc274ff8028e8fc36baccfb8463d343a44b1aebf1271db6bf0ca0e0"},
            katana::runtime::NativePortHookBinding{
                0x8C093DA0u,
                162u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_sound_driver_initialize",
                "sha256:fb898aaeea4ebeace1e3aefd65a57f990192ff2267c56dfd66a6204212e0f551"},
            katana::runtime::NativePortHookBinding{
                0x8C643730u,
                104u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_audio_defaults",
                "sha256:7750e55db494e3f3246184addd786448b328127155cbe1b1f87b50893c8a3ba2"},
            katana::runtime::NativePortHookBinding{
                0x8C643ED8u,
                88u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_audio_service",
                "sha256:3ed359a735a1a957cf57dde063ba668b01e2c38777868d1111071749d865f241"},
            katana::runtime::NativePortHookBinding{
                0x8C643F62u,
                114u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_audio_foundation_initialize",
                "sha256:9c578279c139a29d17f8c7ca65cff890d427ad8fdb7c49c15cbe09a9c62bc04d"},
            katana::runtime::NativePortHookBinding{
                0x8C643FDCu,
                72u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                // The shutdown owner is identity-bound for lifecycle
                // completeness, but it is not reachable from the current
                // post-PAL product closure. Do not turn provider metadata into
                // an artificial AOT root; promote it to Required when ordinary
                // analysis first reaches this exact entry.
                katana::runtime::NativePortHookRequirement::DiagnosticOnly,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_audio_foundation_shutdown",
                "sha256:6db1376342055b0c88dbc9fb87a258bf541e8c8a4cfe117c91343bc3cba22a69"},
            katana::runtime::NativePortHookBinding{
                0x8C644232u,
                0xAAu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_adxt_destroy",
                "sha256:b7ba7e42c718ae1d82b0c0a1616612f7aece5dd9eafb7164e48c11bf7861b355"},
            katana::runtime::NativePortHookBinding{
                0x8C6443A8u,
                0x98u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_adxt_stop",
                "sha256:d13b43675bca3624ecb5fc2f45fd2bec1faa7cb9e17a9adf766d240c5a591185"},
            katana::runtime::NativePortHookBinding{
                0x8C644440u,
                0x04u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_adxt_status",
                "sha256:c8f46a214c1c2903d4ad5d6e0c93bbc3b9c6a5d020569e6b4b5822c521c47374"},
            katana::runtime::NativePortHookBinding{
                0x8C644444u,
                0x6Cu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_adxt_time",
                "sha256:46dee8493a7d302563db86965719bcec8f706f91b103a00bd922b5daa05eb5e4"},
            katana::runtime::NativePortHookBinding{
                0x8C64452Au,
                0x12u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_adxt_output_volume",
                "sha256:1c72f99dcd54e0357ede2f0fb8cf489de07bb3b1abe88a2620b1c0dfd8d050fc"},
            katana::runtime::NativePortHookBinding{
                0x8C644844u,
                0x64u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_adxt_start_afs",
                "sha256:a62c86514a6153c70db49c3e267b53ba0ae49660f945103d0cbba86bd2a81200"},
            katana::runtime::NativePortHookBinding{
                0x8C6448A8u,
                0x92u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_adxt_start_file",
                "sha256:c5f38524d2b416a69edff51c4cb773e423d69b17525e2c4e26a1847db7c3d38b"},
            katana::runtime::NativePortHookBinding{
                0x8C0938F0u,
                0x42u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_sound_collection_load",
                "sha256:457529f3f48cdccc8d8f1a3df16e7421c2d6cb020743f1a3b5af84afa85e81da"},
            katana::runtime::NativePortHookBinding{
                0x8C093080u,
                0x100u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_midi_initialize",
                "sha256:5f58e0f45762f02cab7252e4895db5c2a2134fe93489729bda913e23eabd10d1"},
            katana::runtime::NativePortHookBinding{
                0x8C093180u,
                0x4Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_midi_volume",
                "sha256:52f94ade5f466749a65ac5892e4f52595fec0434a59cae13d3ccc61134ee71f5"},
            katana::runtime::NativePortHookBinding{
                0x8C0931CEu,
                0x52u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_midi_pan",
                "sha256:c938bb274d164e2fec9329a20032cb7f6e0200f96e688287d7f24b46ed942ca6"},
            katana::runtime::NativePortHookBinding{
                0x8C093220u,
                0x5Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_midi_pitch",
                "sha256:5f1aae6111110ef4561e610e7f0e1e89e11a9c690277fd2ef929eeed2595ea3e"},
            katana::runtime::NativePortHookBinding{
                0x8C0932A0u,
                0x5Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_midi_note_on",
                "sha256:f63c1b8615dc42a995fcadb141d55b6c063263fcb7de4b1ab687ebf78ac68adb"},
            katana::runtime::NativePortHookBinding{
                0x8C0932FAu,
                0x52u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_midi_note_off",
                "sha256:31512683c29f47ff9b08d145f71548c111fbb5c788a9b620358b20530945d84b"},
            katana::runtime::NativePortHookBinding{
                0x8C093A6Au,
                0x46u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_sound_play",
                "sha256:42e0c312853ffe898db457ee57eeffffc31cbcc772d562fb052ceae284f712c4"},
            katana::runtime::NativePortHookBinding{
                0x8C093AB0u,
                0x20u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_sound_stop",
                "sha256:a5d4a8432ed5f48076cedd38e27f60936e021b3e32cedc9db81338c32cd636a1"},
            katana::runtime::NativePortHookBinding{
                0x8C093B20u,
                0x52u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_sound_volume",
                "sha256:c5e18c6148d54f71f2a29a01d2bb7594630c0b47d60cdf1621a99173cf5b9f98"},
            katana::runtime::NativePortHookBinding{
                0x8C093B72u,
                0x22u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                // Identity-bound API coverage without an artificial root. The
                // current product closure has no caller for the pan/pitch pair;
                // ordinary reachability promotes either binding to Required.
                katana::runtime::NativePortHookRequirement::DiagnosticOnly,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_sound_pan",
                "sha256:30069effc2800f24884c0b8c772db9dd02dcc8ff866191d79202b4898051b0a8"},
            katana::runtime::NativePortHookBinding{
                0x8C093B94u,
                0x1Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::DiagnosticOnly,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_sound_pitch",
                "sha256:eb91250d400c1b794048fd46fd91a67ee108325bccb71e4bdba33dc389b68688"},
            katana::runtime::NativePortHookBinding{
                0x8C093BB4u,
                0x30u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_sound_close_sequence_ports",
                "sha256:54aa1aad646053b325b890208e569fb7dd81b882d23a2ca8d666e0c701043abf"},
            katana::runtime::NativePortHookBinding{
                0x8C658220u,
                620u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_mode_apply",
                "sha256:234d0135feaf6f1217f928e97085d42b591e0be11514b4122a86b31421f24b28"},
            katana::runtime::NativePortHookBinding{
                0x8C658500u,
                580u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_mode_60hz",
                "sha256:a5613165673007f31e20cdc5bab384c02cbd3f9cabf7efb4b5d8e0edb055d212"},
            katana::runtime::NativePortHookBinding{
                0x8C652500u,
                92u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_output_commit",
                "sha256:da83a69cdbba4dec3d4cf97df9603dc082b4dc8f762f8c63303be2acc398a9e7"},
            // The PAL60 owner publishes its already-derived read/write
            // framebuffer addresses through one generic PVR register-write
            // leaf. Native graphics owns scanout buffers, so bind only the
            // seven exact call+delay-slot sites and retain every surrounding
            // title-visible RAM and control-flow effect in AOT.
            katana::runtime::NativePortHookBinding{
                0x8C641C2Au,
                4u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_scanout_r_sof1_a",
                "sha256:368cfdef8c9a4fc41221ce5bad63aae06dc9735fe073dd00d52a486ef3bbce43"},
            katana::runtime::NativePortHookBinding{
                0x8C641C32u,
                4u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_scanout_r_sof2_a",
                "sha256:022c26f506e3d003f7009ed727f9439ab31c0122a0f73369cf2434c23ebcfa3b"},
            katana::runtime::NativePortHookBinding{
                0x8C641C3Au,
                4u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_scanout_w_sof1",
                "sha256:8a5bcd2f2446251371b32c282ffb48e0dc1bf25ec2718bbb590247f3b3122301"},
            katana::runtime::NativePortHookBinding{
                0x8C641C42u,
                4u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_scanout_w_sof2",
                "sha256:ff635044945b608b22e1e1f22266f9b20b1b84cd8dff3af7e49d5dd714638556"},
            katana::runtime::NativePortHookBinding{
                0x8C641C68u,
                4u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_scanout_r_sof1_b",
                "sha256:368cfdef8c9a4fc41221ce5bad63aae06dc9735fe073dd00d52a486ef3bbce43"},
            katana::runtime::NativePortHookBinding{
                0x8C641C76u,
                4u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_scanout_r_sof1_c",
                "sha256:368cfdef8c9a4fc41221ce5bad63aae06dc9735fe073dd00d52a486ef3bbce43"},
            katana::runtime::NativePortHookBinding{
                0x8C641C7Eu,
                4u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_scanout_r_sof2_b",
                "sha256:022c26f506e3d003f7009ed727f9439ab31c0122a0f73369cf2434c23ebcfa3b"},
            // The arena initializer has two mode branches and two allocation
            // phases. Each phase performs the same isolated legacy SPG timing
            // reset. Bind every path while retaining the surrounding native-
            // safe allocator and linked-list effects in AOT.
            katana::runtime::NativePortHookBinding{
                0x8C6630CCu,
                4u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_timing_reset_a",
                "sha256:b70557d62b5a38cc74771adee33b3fab21495912ab8db3068124e18bace28252"},
            katana::runtime::NativePortHookBinding{
                0x8C6630E8u,
                4u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_timing_reset_b",
                "sha256:e380291781ca8c931fd146cea9c6de7d71afba07c6d57e251b71bccc0d5762d6"},
            katana::runtime::NativePortHookBinding{
                0x8C663184u,
                4u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_timing_reset_c",
                "sha256:b70557d62b5a38cc74771adee33b3fab21495912ab8db3068124e18bace28252"},
            katana::runtime::NativePortHookBinding{
                0x8C6631A2u,
                4u,
                katana::runtime::NativePortHookKind::Instruction,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_video_timing_reset_d",
                "sha256:e380291781ca8c931fd146cea9c6de7d71afba07c6d57e251b71bccc0d5762d6"},
            katana::runtime::NativePortHookBinding{
                0x8C665D7Cu,
                0x106u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_render_command_arena_configure",
                "sha256:82ca06e338f8f599df8bc8016adb25c0716f77df3c51db1675ce31e8bcf25d2d"},
            katana::runtime::NativePortHookBinding{
                0x8C0141E0u,
                0x62u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_lighting_content_load",
                "sha256:5b8d26e89a7fe6edbaa1025f77997a80f85dcfd8f966d41e3b1865b086122c30"},
            katana::runtime::NativePortHookBinding{
                0x8C10C06Eu,
                0x8Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_content_range_read",
                "sha256:35f9ba958af8a581fbc651c8e83253b1c81ec1083a6a75a07c8cc7a1b87348cb"},
            katana::runtime::NativePortHookBinding{
                0x8C09CBC0u,
                100u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_load_movie_overlay",
                "sha256:75ed7ecb701f21ff4b0ebac11fce2b2ce4b34fe33cbbb918ae1f141b45a76ec7"},
            katana::runtime::NativePortHookBinding{
                0x8C09901Eu,
                0x106u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_main_texture_set_load",
                "sha256:295e1fdb10011f3c9e9b5b4a1732d38739402222efb7275d962e79dc188ea2e7"},
            katana::runtime::NativePortHookBinding{
                0x8C098EEEu,
                0x4Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_character_texture_set_load",
                "sha256:4eddab747d18d16b5a39d7166eb2176ed503945bcb1da97388f558f5bd36d887"},
            katana::runtime::NativePortHookBinding{
                0x8C098F3Cu,
                0x4Cu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_character_texture_set_release",
                "sha256:1b702ca5b283ed4d749c4cc117f7f0c25159b475b30b0838001bcda010af4d39"},
            katana::runtime::NativePortHookBinding{
                0x8C098FC0u,
                0x5Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_character_texture_sets_release_all",
                "sha256:170b95ef566a1beaba06ba0562364878c6b91aa7a779c8cfba29b56eb11ff4c8"},
            // Guard the shared retail allocator before its unchecked full-table
            // fallthrough. A free slot continues the unchanged AOT body.
            katana::runtime::NativePortHookBinding{
                0x8C64E55Eu,
                0x102u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_sdk_texture_allocation_guard",
                "sha256:33531da599a55802f16a3881de8aa311be34b6d0f7bf79f8bc8a4e1ac40e58f2",
                sonic_native_title_adapter_provider_implementation_identity},
            // Event-family resource retirement precedes the original arena
            // clear. The full loader body is hash-bound; original AOT code runs.
            katana::runtime::NativePortHookBinding{
                0x8C049882u,
                0x82u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_event_arena_prepare",
                "sha256:95808a03dfb33ef3f0126cf9cb0003b34041cff9e2f546b79dfbf314a51bb40c",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C04C5E0u,
                0x98u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_prs_transform_guard",
                "sha256:712feef2faa16af74be8dcee29e925ff5cdda767bbbcb4419fd9f5b88869c877"},
            katana::runtime::NativePortHookBinding{
                0x8C6044D4u,
                20u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_frame_boundary_wait",
                "sha256:69f29563d05b2bd2fb1698f5e69c588eda2f65fe7fd902f6bf49d45a901f33b0"},
            katana::runtime::NativePortHookBinding{
                0x8C604FF0u,
                182u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_frame_turnover",
                "sha256:16caccb947f72d8af006ff65c9f654cf995aec199b2c8664044f5ce7d3b17392"},
            katana::runtime::NativePortHookBinding{
                0x8C641E40u,
                726u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_scene_begin",
                "sha256:1ed342cc4ea9a45c88388c9469da6c18fb61955c1431322d596483dc926ad2b5"},
            katana::runtime::NativePortHookBinding{
                0x8C051790u,
                102u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_frame_begin",
                "sha256:e29b6430317bf2e88b7666a4a2c25ce8bec1a6e9f269acbbdbb3a66b80aa6829"},
            katana::runtime::NativePortHookBinding{
                0x8C0517F6u,
                22u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_frame_producer_wait",
                "sha256:56bdcec9b72c7f7a84d5e708c645a9e7886c665918884490f31a1786e7801262"},
            katana::runtime::NativePortHookBinding{
                0x8C051810u,
                110u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_frame_producer_complete",
                "sha256:07e85607c0805fde4be7c47e2373794dc88d5d8fe5bd3390b526f84f3213897f"},
            katana::runtime::NativePortHookBinding{
                0x8C642380u,
                1472u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_kamui_present_drain",
                "sha256:8d6a9549ead292205fb2e9a0a98c2af0337cd7fea344b017e8aa0ab7a8b4a101"},
            katana::runtime::NativePortHookBinding{
                0x8C605B50u,
                0x18u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_border_color",
                "sha256:68238676ba7162fce0f29607033631f36f481ee9a2e8016078a05088c4078047"},
            katana::runtime::NativePortHookBinding{
                0x8C63FD60u,
                0x16u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_fog_table_color",
                "sha256:78aa1162b7d48f6fcbcdd05d49157bfbf445f1eccca1eef100a647b4cffd2591"},
            katana::runtime::NativePortHookBinding{
                0x8C63FD7Cu,
                0x06u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_fog_density",
                "sha256:41d4ed16fec6de860b1863e0dbde26f1440473f57cbd176b7c66c2208433861c"},
            katana::runtime::NativePortHookBinding{
                0x8C63FD88u,
                0x06u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_fog_table",
                "sha256:41d4ed16fec6de860b1863e0dbde26f1440473f57cbd176b7c66c2208433861c"},
            katana::runtime::NativePortHookBinding{
                0x8C652CAEu,
                0x10u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_fog_table_color_direct",
                "sha256:4b62b16c2703e53dc239d5e4fdf254a627a0c7c8668f728a04378144060d3341"},
            katana::runtime::NativePortHookBinding{
                0x8C652D10u,
                0x68u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_fog_table_direct",
                "sha256:4cd4ec32931b05e26d11bcec47393f15e8708de6865bffc24c32b054fed1506e"},
            katana::runtime::NativePortHookBinding{
                0x8C652D78u,
                0x18u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_fog_density_direct",
                "sha256:a57a9576fff7b0b184bda6c74b902f601c8d447de58d72f6896c5ff29f5e9acb"},
            katana::runtime::NativePortHookBinding{
                0x8C652C7Au,
                0x1Cu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_cull_value",
                "sha256:5c6bac9d08d2c8a5caf3ca64b40cc26223498a3f51ce58d2ab6bbeb5a15c92a7"},
            katana::runtime::NativePortHookBinding{
                0x8C6533BCu,
                0x16u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_texture_backing_word_copy",
                "sha256:ff52b710fa899f8ecbf921ed2a3a3bac01b0f055b9f20f31a88dac6853862b75"},
            katana::runtime::NativePortHookBinding{
                0x8C653420u,
                0xE2u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_texture_backing_upload",
                "sha256:df7fabe9515585901dda3aaf2f43fe905e17121ed04c5f894bc16939e5a7a2c1"},
            katana::runtime::NativePortHookBinding{
                0x8C653320u,
                0x24u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_texture_backing_completion",
                "sha256:982bd9cfcc9fe0f233c9a67377d84d94732b3db135f7e919ce53e63175e180ab"},
            katana::runtime::NativePortHookBinding{
                0x8C65387Au,
                0x62u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_font_surface_upload",
                "sha256:ca0e6e20ff476f4eef3c53750d312b8b708c228ea6d3b5bf29a4c9c3181fdb6e"},
            katana::runtime::NativePortHookBinding{
                0x8C608C0Cu,
                18u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_texture_list_bind",
                "sha256:7f87f37b7718960fc8fa61145e98d42134de1d97c1e53df617c821d3efdc6049"},
            katana::runtime::NativePortHookBinding{
                0x8C608C54u,
                0x74u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_texture_index_failure_observe",
                "sha256:ac02159a2a1ca84edfaa1d0d71d7bc38fb44d7d812a9d089256b501187f16624"},
            katana::runtime::NativePortHookBinding{
                0x8C6069C8u,
                88u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_texture_number_observe",
                "sha256:6fb9dfa405a952edbeef64e9c43e0b2244f8bdeaee9f0a7c1ac15ecb6a6abd3e"},
            katana::runtime::NativePortHookBinding{
                0x8C03718Cu, 0x108u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_widescreen_model_cull",
                "sha256:df39afabfbfdce25d7c3bd0cd59008959ec7e365320dd19a9603857401e67137",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C038D00u, 0xA0u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::MayContinueOriginal,
                "sonic_native_widescreen_draw_sphere_cull",
                "sha256:1f573f535bbc2d5e67ba50eca018c42cab9736a60bc89ec7542df1a88e10d551",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C037294u,
                0xA4u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_model_transform",
                "sha256:5708882ebd4bed824d368eb010a269879fe0ab80f627c6dc234cea957d34a572",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C037460u,
                0x48u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C037460",
                "sha256:660b0fdf9f7eb2b94cc40049523021c00add4ce1fbd5c5eb2afba2b027903e74",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C0374D4u,
                0x40u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C0374D4",
                "sha256:30dbbe2d0f174d1ab96cadf1a5191b066a50ab6c9fb1d7b957e172083735ac35",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C037538u,
                0x4Cu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C037538",
                "sha256:c5c6c9c552d028dd9cb0a472123bd98c44fc9da293d8ca2b073f27e51f2c2d96",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C0375B0u,
                0x44u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C0375B0",
                "sha256:7d3fed39cc73efdb174f080ea7088e9737fe9bbd38657a68d22a21f326dcf5f6",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C605DA8u,
                0x4Cu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C605DA8",
                "sha256:e2d64720048274dc107fd3389b33a18acdb7f3fa657e4fc5d44324edeb16c71a",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C605E7Cu,
                0x44u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C605E7C",
                "sha256:f7afe6081bae146a8630604343e66543ac7f1863410272a9925bcdb082119ac5",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C605F0Au,
                0x50u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C605F0A",
                "sha256:c9f1ece5bc1b262b9ef4b08406d8325a2cdcdfe288205e34d4c6519c5bcfe33d",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C605FBAu,
                0x48u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C605FBA",
                "sha256:ae826aede82c822f9f15c44fe47e8451b770c45891cd1eaf27a45782c921314c",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6061ECu,
                0x66u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C6061EC",
                "sha256:73fc82709bce88bb3d47a7814374a512bd490570e632b0a7118eb3c504cdfe5a",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6062DAu,
                0x5Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C6062DA",
                "sha256:c8ee6ad25d8fd30ad5757df81223306275183c27dd147d4c2447c4512e97b394",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6063F0u,
                0x68u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C6063F0",
                "sha256:ff63031510d324455b5643fe1666ac28103330ce122861e6fba753cb2eda069e",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6064F0u,
                0x60u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "katana_native_hook_8C6064F0",
                "sha256:39141cdc9d6aec3982922d4b73c322bf5933161468b471dadb0ded5d6aa710b7",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C0376D0u,
                0x1A2u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_model_draw",
                "sha256:8cdbb7248de9e28b488192cb0994c386900b0f22e399b56f95096adbffd0f935"},
            katana::runtime::NativePortHookBinding{
                0x8C6107F0u,
                0x448u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_resident_model_draw_8c6107f0",
                "sha256:0961dd9635752da41ea0ddefef812330c43fa8b76cc3600d5d240e14f3d8cf16",
                sonic_native_title_adapter_provider_implementation_identity},
            // The complete local Boot disassembly contains exactly two
            // instances of this 28-byte BasicAttach wrapper shape.  Each
            // preserves r8, forwards the unchanged r4=NJS_MODEL argument to
            // its family renderer and returns that renderer's result.  Bind
            // both exact owners to the same high-level composite rather than
            // hooking their frame-sharing internal worker entries.
            katana::runtime::NativePortHookBinding{
                0x8C6214FAu,
                0x1Cu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_basic_model_draw_8c6214fa",
                "sha256:93d9360d285d9476fbea2320879b891edf152ac09d4a01af8fcd50aa4899668a",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C638D72u,
                0x1Cu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_ninja_basic_model_draw_8c638d72",
                "sha256:248da77be1d725311c581175119c9b250232e0ceb9899c56765053a7a3f41285",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C63CD64u,
                0x5E6u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_polygon",
                "sha256:06528129ef9bb3cd70f24247c9921464e6c54a51bf7a539d54f487b50fdc823d"},
            katana::runtime::NativePortHookBinding{
                0x8C63D354u,
                0x246u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_texture",
                "sha256:5de63c8654b8069b1814cf643eba6d0d3877fe39326872165cb80726487fde83"},
            katana::runtime::NativePortHookBinding{
                0x8C63D5ACu,
                0x2FCu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_textureh",
                "sha256:ec3e6af1a66ea9fbbc63bed63b8f51fd159a9dd3281a72d5e37c7ebf6a2f49f6"},
            katana::runtime::NativePortHookBinding{
                0x8C63E114u,
                0x66u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_pretransformed_line_dispatch",
                "sha256:7174727e659fbb10f975cbc0b87601141398e6b66e76311bd6a524090f148ebc",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C63EB08u,
                100u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_pretransformed_dispatch",
                "sha256:5573d1f441710d204dc86751a6266fd68832215bfacad3bb0d807d283d7e746c"},
            katana::runtime::NativePortHookBinding{
                0x8C63EB98u,
                0xA0u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_pretransformed_colored",
                "sha256:39ee8c03fdf11a8417273e4a2d9e23c55475417b780bfc9215789f95c53c0d64"},
            katana::runtime::NativePortHookBinding{
                0x8C63EC48u,
                0xC4u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_pretransformed_textured",
                "sha256:bddd9257e8449897e26515277cf8bcd789068a665c04317a99c10077649710bc"},
            katana::runtime::NativePortHookBinding{
                0x8C63EDF4u,
                156u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_sprite_2d",
                "sha256:1c9114fbc596d3cb340f338bef0d01fd7f857a14f99c4043838a76fbafeb7878"},
            katana::runtime::NativePortHookBinding{
                0x8C63EEB4u,
                156u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_sprite_3d",
                "sha256:8364b2df2f5119ccbc358b9eb819acb45126a8b6b34ee4e24e77d5a9e52f1e82"},
            katana::runtime::NativePortHookBinding{
                0x8C09E6B8u,
                0xE8u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_route_sprite_3d",
                "sha256:1225a5e03d3243ef02b53dd40000b6d1ffb3265886a66698311e9c9752ed8a34"},
            // Complete resident save/status glyph family, including the
            // foreground text variant used after Chaos0. Exact SDK owners
            // share the native atlas/GPU path; no QACR device is installed.
            katana::runtime::NativePortHookBinding{
                0x8C042298u,
                0x17Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_resident_glyph_text",
                "sha256:84620eb8611286e2049fc186391b1be274b84aa4a8857fed16b3cf5b60843118"},
            katana::runtime::NativePortHookBinding{
                0x8C042416u,
                0x118u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_resident_glyph_foreground",
                "sha256:de18d7badb24d56798e9e71e29e6f8ea8a68dfb659889d583535840287c75eaa"},
            katana::runtime::NativePortHookBinding{
                0x8C0425A0u,
                0x290u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_resident_glyph_decimal",
                "sha256:8fab141d4300d0e97d979337114074bf6b841e0fc916ffdea212d86750618c8c"},
            katana::runtime::NativePortHookBinding{
                0x8C042880u,
                0x10Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_resident_glyph_binary",
                "sha256:b2b6318696bce99ccc615393e22a84c61c4bbcfae0f81c41d466c04daec228d1"},
            katana::runtime::NativePortHookBinding{
                0x8C04298Eu,
                0x194u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_resident_glyph_nibbles",
                "sha256:a4e6c15d7f6f32f9b74a4b9d07bc4ea86d7bef0e98b785d3858f78bca5330b35"},
            katana::runtime::NativePortHookBinding{
                0x8C042B22u,
                0x218u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_resident_glyph_float",
                "sha256:b71163eac72d473110ad4cfc7fd1b80e4b79ab8fa6b1af5d4413da58567a29ac"},
            katana::runtime::NativePortHookBinding{
                0x8C640862u,
                260u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_glyph_text",
                "sha256:b7717ad10c5a10176bce16efa7c4b95ca3ab850281c1923f6ee9782bce610dea"},
            katana::runtime::NativePortHookBinding{
                0x8C6409C0u,
                0x23Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_glyph_decimal",
                "sha256:39cb44e91bd36db939fd1c2f251fab5bfa510c9fcaa49a1045e473c7ef5676a5"},
            katana::runtime::NativePortHookBinding{
                0x8C640D22u,
                0x110u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_draw_glyph_nibbles",
                "sha256:fbab5cae552ac0145c9a3b3a506233d304e919435ac649ffd1436e9c2e5428c7"},
            katana::runtime::NativePortHookBinding{
                0x8C010FCCu,
                0x76u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_file_exists",
                "sha256:bf1f5eb3b7a50d1428c03cdfe6836794192ff056f5d3145179485b1cd19784a5",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C011060u,
                0x1D8u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_highest_slot",
                "sha256:3357c20370fcf1be590c392765b66f71355021aedb31dae4e281c0a20b12ea25",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C602D32u,
                0x6u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_unit_ready",
                "sha256:735d197e05f63c6a40c2917de9a0244b3198ffa9cd99d342d12273a4ab7792c4",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6032C4u,
                0x2Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_sdk_file_exists",
                "sha256:88718f703ba7ce2ad5ceed638f7f98437f49aaf9d1df03e1df643bb6dfa0575f",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C602E9Cu,
                0x2Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_unit_query",
                "sha256:a4943cd981ddac47275c890cf03625b69ac2de551a175c53c81f03d0b930fecf",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C602ECAu,
                0x2Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_free_blocks",
                "sha256:cc4b2c203901d2a8ffa90367e5597a531cb13d3ea2b679a43a6d28fe3475d2eb",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C603324u,
                0x1Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_status",
                "sha256:a8d2002b280f44a1cf80255abee16bc39e0bd515c928f49e87b7445b540ae2ed",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6033B4u,
                0x2Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_file_blocks",
                "sha256:844b9a45db6ddae41f63b8639254685a5516471f9e22d60c7b463e36eb526e45",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C603448u,
                0x58u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_read",
                "sha256:f9b3258d46581219f9d79309cdf94c643ae2b7715adebb54f4f3ea073b40c355",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6034A0u,
                0x5Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_write",
                "sha256:9ed7f47f8559ace8833618b22adb65fe1ed68d90cce48419da1b3f831dd8b3de",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C6034FEu,
                0x5Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_write_game",
                "sha256:e5568b9f7cc9463ddde2c5fd23b7d7cd5f5cf50f09296af3d9f4772bbe510eb1",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C60355Cu,
                0x56u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_save_read_blocks",
                "sha256:044cb0a4d221443728402744af6718148758828615c86cf2c781f10188f33f1f",
                sonic_native_title_adapter_provider_implementation_identity},
            katana::runtime::NativePortHookBinding{
                0x8C602528u,
                0x26u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_timer_initialize",
                "sha256:5747fc7b81840d0b83f87e3e02ae2e1c4c5d032ec8875adf1684e7d5af9812f4"},
            katana::runtime::NativePortHookBinding{
                0x8C60254Eu,
                0x0Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_timer_count",
                "sha256:90629f29891a28af2a6ceea556b4ad0f4f0b12740d809a305a874eaaeaa973bd"},
            katana::runtime::NativePortHookBinding{
                0x8C06C000u,
                0x6Eu,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_host_timing_owner_8c06c000",
                "sha256:4c612cefe622d1996996746e895a8ad562ba5d5b7ef59211e53a82a8061b7b7c"},
            katana::runtime::NativePortHookBinding{
                0x8C06C09Au,
                0x28u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_periodic_delta",
                "sha256:2912507a87a814c58b9731735532e4cec0c3641d09ae536dc29d1583615e663e"},
            katana::runtime::NativePortHookBinding{
                0x8C06C0C2u,
                0x0Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_periodic_sample",
                "sha256:42db69f46902e1521610f9ae277c23fde959c04241d2256ff919a6a6dfa42bd6"},
            katana::runtime::NativePortHookBinding{
                0x8C06C0F2u,
                0x26u,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_periodic_elapsed",
                "sha256:869e8f449c32c59b2f654ca208ec142e27d907a89ae2a7e4c2ae7fd7455076d2"},
            katana::runtime::NativePortHookBinding{
                0x8C07EEAEu,
                0x1Au,
                katana::runtime::NativePortHookKind::FunctionEntry,
                katana::runtime::NativePortHookRequirement::Required,
                katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
                "sonic_native_registered_texture_key_select",
                "sha256:3b2c5879528fcb64867be79f5b9ba212a4197c2bf9932ed0f95dc92a5d53cdbe"}};
        auto boot_bytes = read_verified_boot_image(
            std::filesystem::path(argv[2]), images[1].byte_size,
            images[1].byte_identity.substr(
                std::string_view("sha256:").size()));
        katana::io::ExecutableImage sdk_image(argv[2]);
        katana::io::ImageSegment sdk_segment{
            "verified-native-sdk-image",
            images[1].guest_address,
            0u,
            boot_bytes.size(),
            katana::io::SegmentKind::Mixed,
            {true, false, true},
            std::move(boot_bytes)};
        sdk_segment.source_kind = katana::io::ImageSourceKind::DiscBootFile;
        sdk_image.add_segment(std::move(sdk_segment));
        sdk_image.set_address_model(
            katana::io::ImageAddressModel::Sh4DirectMapped);
        const auto provider_candidates =
            katana::analysis::discover_native_sdk_provider_candidates(
                sdk_image);
        for (const auto& candidate : provider_candidates) {
            std::cerr << "KATANA_NATIVE_SDK_PROVIDER family="
                      << katana::analysis::native_sdk_provider_family_name(
                             candidate.family)
                      << " entry=0x" << std::hex << candidate.entry_address
                      << " size=0x" << candidate.covered_size << std::dec
                      << " identity=" << candidate.code_identity << '\n';
        }
        const auto& named_texture_load = require_unique_provider(
            provider_candidates,
            katana::analysis::NativeSdkProviderFamily::
                NamedTextureArchiveLoad);
        const auto& texture_release = require_unique_provider(
            provider_candidates,
            katana::analysis::NativeSdkProviderFamily::
                TextureArchiveRelease);
        const auto& sound_bank_chunk_registration = require_unique_provider(
            provider_candidates,
            katana::analysis::NativeSdkProviderFamily::
                SoundBankChunkRegistration);
        const auto& sound_frame_service = require_unique_provider(
            provider_candidates,
            katana::analysis::NativeSdkProviderFamily::SoundFrameService);
        require_guest_texture_descriptor_contract(named_texture_load);
        require_guest_texture_descriptor_contract(texture_release);
        if (named_texture_load.resource_reference !=
            texture_release.resource_reference)
            throw std::runtime_error(
                "native-manifest-sdk-resource-reference-divergence");
        const auto& texture_resource =
            *named_texture_load.resource_reference;
        std::cerr << "KATANA_NATIVE_SDK_RESOURCE kind="
                  << katana::analysis::native_sdk_resource_reference_kind_name(
                         texture_resource.kind)
                  << " owner-stride="
                  << texture_resource.owner_record_stride
                  << " field=" << texture_resource.reference_field_offset
                  << " descriptor-stride="
                  << texture_resource.descriptor_stride
                  << " minimum-bytes="
                  << texture_resource.minimum_descriptor_bytes
                  << " reads=";
        for (std::size_t index = 0u;
             index < texture_resource.observed_read_offsets.size(); ++index) {
            if (index != 0u) std::cerr << ',';
            std::cerr << texture_resource.observed_read_offsets[index];
        }
        std::cerr << " sites=";
        for (std::size_t index = 0u;
             index < texture_resource.evidence_sites.size(); ++index) {
            if (index != 0u) std::cerr << ',';
            std::cerr << "0x" << std::hex
                      << texture_resource.evidence_sites[index] << std::dec;
        }
        std::cerr << '\n';
        std::vector<katana::runtime::NativePortHookBinding> hooks(
            static_hooks.begin(), static_hooks.end());
        hooks.push_back(
            {named_texture_load.entry_address,
             named_texture_load.covered_size,
             katana::runtime::NativePortHookKind::FunctionEntry,
             katana::runtime::NativePortHookRequirement::Required,
             katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
             "sonic_native_named_texture_archive_load",
             named_texture_load.code_identity});
        hooks.push_back(
            {texture_release.entry_address,
             texture_release.covered_size,
             katana::runtime::NativePortHookKind::FunctionEntry,
             katana::runtime::NativePortHookRequirement::Required,
             katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
             "sonic_native_named_texture_archive_release",
             texture_release.code_identity});
        hooks.push_back(
            {sound_bank_chunk_registration.entry_address,
             sound_bank_chunk_registration.covered_size,
             katana::runtime::NativePortHookKind::FunctionEntry,
             katana::runtime::NativePortHookRequirement::Required,
             katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
             "sonic_native_sound_bank_chunk_register",
             sound_bank_chunk_registration.code_identity});
        hooks.push_back(
            {sound_frame_service.entry_address,
             sound_frame_service.covered_size,
             katana::runtime::NativePortHookKind::FunctionEntry,
             katana::runtime::NativePortHookRequirement::Required,
             katana::runtime::NativePortHookOriginalPolicy::ReplacesOriginal,
             "sonic_native_sound_frame_service",
             sound_frame_service.code_identity});
        std::ranges::sort(hooks, {},
                          &katana::runtime::NativePortHookBinding::guest_address);
        constexpr std::array hardware_resolutions{
            katana::runtime::NativePortHardwareResolution{
                0x8297C3E4u, 0x8297C3E0u},
            katana::runtime::NativePortHardwareResolution{
                0x8297C3EEu, 0x8297C3E0u},
            katana::runtime::NativePortHardwareResolution{
                0x8C03747Au, 0x8C037460u},
            katana::runtime::NativePortHardwareResolution{
                0x8C03747Eu, 0x8C037460u},
            katana::runtime::NativePortHardwareResolution{
                0x8C0374A0u, 0x8C037460u},
            katana::runtime::NativePortHardwareResolution{
                0x8C0374EEu, 0x8C0374D4u},
            katana::runtime::NativePortHardwareResolution{
                0x8C0374F2u, 0x8C0374D4u},
            katana::runtime::NativePortHardwareResolution{
                0x8C03750Cu, 0x8C0374D4u},
            katana::runtime::NativePortHardwareResolution{
                0x8C037552u, 0x8C037538u},
            katana::runtime::NativePortHardwareResolution{
                0x8C0375CAu, 0x8C0375B0u},
            katana::runtime::NativePortHardwareResolution{
                0x8C0375CEu, 0x8C0375B0u},
            katana::runtime::NativePortHardwareResolution{
                0x8C0375ECu, 0x8C0375B0u},
            katana::runtime::NativePortHardwareResolution{
                0x8C6047BAu, 0x8C6047BAu},
            katana::runtime::NativePortHardwareResolution{
                0x8C604852u, 0x8C604852u},
            katana::runtime::NativePortHardwareResolution{
                0x8C605DC6u, 0x8C605DA8u},
            katana::runtime::NativePortHardwareResolution{
                0x8C605DC8u, 0x8C605DA8u},
            katana::runtime::NativePortHardwareResolution{
                0x8C605DE4u, 0x8C605DA8u},
            katana::runtime::NativePortHardwareResolution{
                0x8C605EA0u, 0x8C605E7Cu},
            katana::runtime::NativePortHardwareResolution{
                0x8C605EA2u, 0x8C605E7Cu},
            katana::runtime::NativePortHardwareResolution{
                0x8C605EB0u, 0x8C605E7Cu},
            katana::runtime::NativePortHardwareResolution{
                0x8C605F32u, 0x8C605F0Au},
            katana::runtime::NativePortHardwareResolution{
                0x8C605FDEu, 0x8C605FBAu},
            katana::runtime::NativePortHardwareResolution{
                0x8C605FE0u, 0x8C605FBAu},
            katana::runtime::NativePortHardwareResolution{
                0x8C605FF2u, 0x8C605FBAu},
            katana::runtime::NativePortHardwareResolution{
                0x8C606212u, 0x8C6061ECu},
            katana::runtime::NativePortHardwareResolution{
                0x8C606214u, 0x8C6061ECu},
            katana::runtime::NativePortHardwareResolution{
                0x8C606242u, 0x8C6061ECu},
            katana::runtime::NativePortHardwareResolution{
                0x8C606300u, 0x8C6062DAu},
            katana::runtime::NativePortHardwareResolution{
                0x8C606302u, 0x8C6062DAu},
            katana::runtime::NativePortHardwareResolution{
                0x8C606328u, 0x8C6062DAu},
            katana::runtime::NativePortHardwareResolution{
                0x8C606448u, 0x8C6063F0u},
            katana::runtime::NativePortHardwareResolution{
                0x8C606516u, 0x8C6064F0u},
            katana::runtime::NativePortHardwareResolution{
                0x8C606518u, 0x8C6064F0u},
            katana::runtime::NativePortHardwareResolution{
                0x8C606540u, 0x8C6064F0u},
            katana::runtime::NativePortHardwareResolution{
                0x8C6460CAu, 0x8C6460C4u},
            katana::runtime::NativePortHardwareResolution{
                0x8C6460ECu, 0x8C6460CCu},
            katana::runtime::NativePortHardwareResolution{
                0x8C646750u, 0x8C646750u},
            katana::runtime::NativePortHardwareResolution{
                0x8C646754u, 0x8C646754u},
            katana::runtime::NativePortHardwareResolution{
                0x8C6509E6u, 0x8C6509E0u},
            katana::runtime::NativePortHardwareResolution{
                0x8C650ABAu, 0x8C650AA0u},
            katana::runtime::NativePortHardwareResolution{
                0x8C6520DCu, 0x8C6520DCu},
            katana::runtime::NativePortHardwareResolution{
                0x8C652110u, 0x8C652110u},
            katana::runtime::NativePortHardwareResolution{
                0x8C65359Eu, 0x8C653580u},
            katana::runtime::NativePortHardwareResolution{
                0x8C6535A2u, 0x8C653580u},
            katana::runtime::NativePortHardwareResolution{
                0x8C6535A8u, 0x8C653580u},
            katana::runtime::NativePortHardwareResolution{
                0x8C6535ACu, 0x8C653580u},
            katana::runtime::NativePortHardwareResolution{
                0x8C6535B0u, 0x8C653580u},
            katana::runtime::NativePortHardwareResolution{
                0x8C6535B4u, 0x8C653580u},
            katana::runtime::NativePortHardwareResolution{
                0x8C65F56Cu, 0x8C65F56Cu},
            katana::runtime::NativePortHardwareResolution{
                0x8C6656B8u, 0x8C6656B8u}};
        constexpr std::array<std::string_view, 1u>
            checkpoint_runtime_image_ids{
                "sa-pal-v1003-runtime-handle-v1"};
        constexpr std::array bootstrap_writes{
            katana::runtime::NativePortBootstrapWriteBinding{
                0x8C000000u,
                16'777'216u,
                "sha256:e5bca2122bbe72e5a4a0445dac0a6e0f30d656d2dd94b4a4497e20297dc6c12b",
                // The checkpoint stores save_busy=1. The native VMU
                // bootstrap publishes the ready state (four zero bytes at
                // 0x8C161BEC) before bootstrap validation, so bind the exact
                // resulting 16-MiB image rather than the source file digest.
                "sha256:b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c",
                katana::runtime::NativePortBootstrapWritePolicy::
                    IdentityBoundImmutableMaterialization}};
        katana::runtime::CpuState post_bootstrap_cpu;
        katana::runtime::reset_cpu(
            post_bootstrap_cpu,
            {0xAC008300u,
             0x8C00F3A4u,
             0x8C000000u,
             0x60000001u,
             0x00040001u});
        sonic_native::apply_postpal_cpu_state(post_bootstrap_cpu);
        const auto post_bootstrap_cpu_identity =
            katana::runtime::native_port_cpu_state_identity(
                post_bootstrap_cpu);
        // The checkpoint resumes inside the title's state dispatcher while a
        // still-live caller frame returns into the outer game loop. Keep the
        // two true function owners separate from their externally reachable
        // resume blocks; Katana validates every pair against its own CFG.
        constexpr std::array post_aot_roots{
            0x8C053940u,
            0x8C053B60u,
            // Exact PAL-disassembly owners for the externally resumed case
            // and loop blocks declared below.  Current native-port contract
            // validation requires every continuation owner to be an explicit
            // AOT root; the resume labels remain typed interior entries.
            0x8C04BF00u,
            0x8C04C878u,
            // A mutable post-checkpoint record selects this exact resident
            // object-cleanup callback. Its GameProject boundary includes
            // both closed exits through their delay slots, but excludes the
            // following padding and literal pool from static authority.
            0x8C05AC66u,
            0x8C099D50u,
            // This is a trace-derived owner, not a graphics-hook root. Its
            // identity-bound direct calls reach texture-list binding and the
            // 2D sprite family from the post-PAL state.
            0x8C08F0E0u,
            // Exact resident entries observed at the two debug-launch
            // boundaries. Both are current FunctionMap owners with complete
            // PAL byte identities in the GameProject; neither declaration
            // widens a loaded overlay or manufactures an interior ABI.
            0x8C085A26u,
            0x8C0CF9AEu,
            // Required by the complete native replacement of the exact
            // frame wrapper. This is a title service reached through the AOT
            // callback bridge, not a graphics-hook root. The former second
            // service was the complete SDK peripheral updater; native input
            // now owns it directly and its Maple/device subtree is no longer
            // revived as a product root.
            0x8C055B32u,
            // Native frame-producer completion is an externally registered
            // callback and therefore remains an explicit dispatch root. Its
            // exact owner is replaced as a whole: only the three bounded
            // title/NINJA state callbacks remain AOT, while Kamui, Maple and
            // guest interrupt tails are not product roots. The second root is
            // the exact hardware-free tail service of the replaced wait owner.
            0x8C051810u,
            0x8C0947E0u,
            // Periodic title notification retained by the native host-time
            // provider. The former TMU interrupt tail is intentionally not a
            // product root: native time never re-enters a guest scheduler.
            0x8C604486u,
            // The ADXT time query is reached through the Manatee handle API
            // rather than a statically recoverable direct call.  Its exact
            // PAL owner boundary, byte identity and r4/r5/r6 ABI are proven
            // by the released disassembly and the required whole-owner hook.
            0x8C644444u,
            // Runtime-selected NINJA callback stored in the title's mutable
            // callback slot. The callsite is post-root reachable, but its
            // selected value is established after the checkpoint; bind the
            // complete callback owner and let normal CFG discovery retain its
            // full transitive AOT closure.
            0x8C64C2B4u,
            // A mutable callback record selects this exact ten-byte PAL
            // state-publication leaf. Its private GameProject boundary and
            // byte identity make only the owner static authority; the
            // runtime-selected record remains non-authoritative.
            0x8C0B22ACu,
            // A mutable callback record selects this exact resident PAL
            // state fan-out leaf. The private GameProject binds its complete
            // 0x5e-byte owner and byte identity, without treating the record
            // itself as immutable dispatch authority.
            0x8C0B27D0u,
            // A second mutable callback record selects this complete PAL
            // object-state fan-out leaf. Its private 0x6e-byte identity is
            // static authority for the owner only, never for the record.
            0x8C0BE0A0u,
            // A mutable callback record selects this exact fourteen-byte PAL
            // state-link publication leaf. Its private GameProject boundary
            // and byte identity authenticate only the complete owner; the
            // runtime record and adjacent literal data remain non-authority.
            0x8C0BF334u,
            // Runtime object records select this exact resident state-copy
            // callback at the post-PAL object dispatcher.  Its bounded
            // GameProject identity makes the complete function dispatchable;
            // no mutable vtable contents are promoted to static authority.
            0x8C0C719Eu,
            // A second resident state-copy callback has an independently
            // byte-closed GameProject identity. Runtime records may select
            // this exact owner without promoting their mutable table.
            0x8C0DD3C8u,
            // A separate runtime callback slot selects this exact eight-byte
            // resident state-publication leaf. Its GameProject boundary and
            // byte identity end at the independently owned 0x8C0DD0AE entry.
            0x8C0DD0A6u,
            // A mutable callback record selects this exact ten-byte PAL
            // state-publication leaf. The private GameProject authenticates
            // only its complete owner; the runtime record and adjacent
            // literal data remain non-authority.
            0x8C0E5934u,
            // Runtime callback records also select the exact resident PAL
            // RTS+NOP no-op. Its four-byte GameProject identity excludes the
            // following data and makes the observed target dispatchable
            // without widening the mutable callback table.
            0x8C0C74ECu,
            // Hardware-free title render-context state helper retained by
            // the native alternate-frame provider after its PVR writes are
            // removed.
            0x8C656420u};
        constexpr std::array post_aot_continuations{
            katana::runtime::NativePortAotContinuationBinding{
                0x8C053B60u, 0x8C053CA2u},
            katana::runtime::NativePortAotContinuationBinding{
                0x8C053940u, 0x8C053980u},
            // Cross-image code pointers select these case/loop blocks. Their
            // exact enclosing owners are declared in the GameProject; they
            // are dispatchable continuations, never second function ABIs.
            katana::runtime::NativePortAotContinuationBinding{
                0x8C04BF00u, 0x8C04C01Au},
            katana::runtime::NativePortAotContinuationBinding{
                0x8C04BF00u, 0x8C04C100u},
            katana::runtime::NativePortAotContinuationBinding{
                0x8C04C878u, 0x8C04C8C2u},
            katana::runtime::NativePortAotContinuationBinding{
                0x8C099D50u, 0x8C099E2Cu}};
        static constexpr std::array<
            katana::runtime::NativePortProviderGuard, 0u>
            interrupt_normal_mask_seed_guards{};
        static constexpr std::array interrupt_normal_mask_seed_effects{
            katana::runtime::NativePortProviderEffect{
                0u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                true,
                0x8C6461E4u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "guest:0x8C6461E4",
                "",
                "gpr:r3",
                "block:0x8C6460C4"},
            katana::runtime::NativePortProviderEffect{
                1u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                2u,
                true,
                0x8C6461D4u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "guest:0x8C6461D4",
                "",
                "gpr:r0",
                "block:0x8C6460C4"},
            katana::runtime::NativePortProviderEffect{
                2u,
                katana::runtime::NativePortProviderOperation::Write,
                katana::runtime::NativePortProviderResourceKind::
                    HardwareRegister,
                4u,
                true,
                0x005F6900u,
                0xFFFFFFFFu,
                0u,
                "system_asic",
                "ISTNRM",
                "ISTNRM",
                "canonical:0x005F6900",
                "gpr:r0",
                "",
                "block:0x8C6460C4"}};
        static constexpr std::array<
            katana::runtime::NativePortProviderGuard, 0u>
            interrupt_normal_mask_guards{};
        static constexpr std::array interrupt_normal_mask_effects{
            katana::runtime::NativePortProviderEffect{
                0u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                true,
                0x8C6461E8u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "guest:0x8C6461E8",
                "",
                "gpr:r4",
                "block:0x8C6460CC"},
            katana::runtime::NativePortProviderEffect{
                1u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                true,
                0x8C6461ECu,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "guest:0x8C6461EC",
                "",
                "gpr:r1",
                "block:0x8C6460CC"},
            katana::runtime::NativePortProviderEffect{
                2u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                false,
                0u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "gpr:r4+disp:24",
                "",
                "gpr:r3",
                "block:0x8C6460CC"},
            katana::runtime::NativePortProviderEffect{
                3u,
                katana::runtime::NativePortProviderOperation::Write,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                false,
                0u,
                0xFFFFFFFFu,
                0u,
                "guest-memory",
                "",
                "memory",
                "gpr:r4+disp:24",
                "gpr:r3",
                "",
                "block:0x8C6460CC"},
            katana::runtime::NativePortProviderEffect{
                4u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                false,
                0u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "gpr:r1",
                "",
                "gpr:r2",
                "block:0x8C6460CC"},
            katana::runtime::NativePortProviderEffect{
                5u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                2u,
                true,
                0x8C6461D6u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "guest:0x8C6461D6",
                "",
                "gpr:r3",
                "block:0x8C6460CC"},
            katana::runtime::NativePortProviderEffect{
                6u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                2u,
                false,
                0u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "gpr:r2",
                "",
                "gpr:r0",
                "block:0x8C6460CC"},
            katana::runtime::NativePortProviderEffect{
                7u,
                katana::runtime::NativePortProviderOperation::Write,
                katana::runtime::NativePortProviderResourceKind::Memory,
                2u,
                false,
                0u,
                0xFFFFu,
                0u,
                "guest-memory",
                "",
                "memory",
                "gpr:r2",
                "gpr:r0",
                "",
                "block:0x8C6460CC"},
            katana::runtime::NativePortProviderEffect{
                8u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                true,
                0x8C6461E4u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "guest:0x8C6461E4",
                "",
                "gpr:r2",
                "block:0x8C6460CC"},
            katana::runtime::NativePortProviderEffect{
                9u,
                katana::runtime::NativePortProviderOperation::Write,
                katana::runtime::NativePortProviderResourceKind::
                    HardwareRegister,
                4u,
                true,
                0x005F6900u,
                0xFFFFFFFFu,
                0u,
                "system_asic",
                "ISTNRM",
                "ISTNRM",
                "canonical:0x005F6900",
                "gpr:r3",
                "",
                "block:0x8C6460CC"}};
        static constexpr std::array error_acknowledge_guards{
            katana::runtime::NativePortProviderGuard{
                0u, "successor:0x8C652142", "block:0x8C652132"},
            katana::runtime::NativePortProviderGuard{
                1u, "successor:0x8C65214A", "block:0x8C652132"}};
        static constexpr std::array error_acknowledge_effects{
            katana::runtime::NativePortProviderEffect{
                0u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                true,
                0x8C652204u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "guest:0x8C652204",
                "",
                "gpr:r2",
                "block:0x8C652132"},
            katana::runtime::NativePortProviderEffect{
                1u,
                katana::runtime::NativePortProviderOperation::Write,
                katana::runtime::NativePortProviderResourceKind::
                    HardwareRegister,
                4u,
                true,
                0x005F6908u,
                0xFFFFFFFFu,
                0u,
                "system_asic",
                "ISTERR",
                "ISTERR",
                "canonical:0x005F6908",
                "gpr:r3",
                "",
                "block:0x8C652132"},
            katana::runtime::NativePortProviderEffect{
                2u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                true,
                0x8C652214u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "guest:0x8C652214",
                "",
                "gpr:r5",
                "block:0x8C652132"},
            katana::runtime::NativePortProviderEffect{
                3u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                false,
                0u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "gpr:r5",
                "",
                "gpr:r3",
                "block:0x8C652132"},
            katana::runtime::NativePortProviderEffect{
                4u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                true,
                0x8C652218u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "guest:0x8C652218",
                "",
                "gpr:r3",
                "block:0x8C652142"},
            katana::runtime::NativePortProviderEffect{
                5u,
                katana::runtime::NativePortProviderOperation::Read,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                false,
                0u,
                0u,
                0u,
                "guest-memory",
                "",
                "memory",
                "gpr:r3",
                "",
                "gpr:r0",
                "block:0x8C652142"},
            katana::runtime::NativePortProviderEffect{
                6u,
                katana::runtime::NativePortProviderOperation::Write,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                false,
                0u,
                0xFFFFFFFFu,
                0u,
                "guest-memory",
                "",
                "memory",
                "gpr:r3",
                "gpr:r0",
                "",
                "block:0x8C652142"},
            katana::runtime::NativePortProviderEffect{
                7u,
                katana::runtime::NativePortProviderOperation::Write,
                katana::runtime::NativePortProviderResourceKind::Memory,
                4u,
                false,
                0u,
                0xFFFFFFFFu,
                0u,
                "guest-memory",
                "",
                "memory",
                "gpr:r5",
                "gpr:r4",
                "",
                "block:0x8C65214A"}};
        static constexpr std::array native_provider_semantic_contracts{
            katana::runtime::NativePortProviderSemanticContract{
                katana::runtime::
                    native_port_provider_semantics_contract_version,
                0x8C6460C4u,
                true,
                "sonic_native_platform_interrupt_normal_mask_seed",
                "sha256:867524f362d114146502640062ec3e48e5530eafdfd918c5fe3b64ca75ac8d39",
                "sha256:c1372c29d79006f565c4f2ff03ffcc467f844cb71be12f59773d88bc274b2a0d",
                sonic_native_title_adapter_provider_implementation_identity,
                interrupt_normal_mask_seed_guards,
                interrupt_normal_mask_seed_effects,
                katana::runtime::NativePortProviderResultProjection{
                    katana::runtime::NativePortProviderReturnAction::Return,
                    0x9u,
                    0u,
                    0u,
                    "",
                    "",
                    "owner-state-v1:sha256:cfe87802c6ec4643393a21fcfcfb95392d7bf1379826ee2ab6e8cb83b525eeca",
                    ""}},
            katana::runtime::NativePortProviderSemanticContract{
                katana::runtime::
                    native_port_provider_semantics_contract_version,
                0x8C6460CCu,
                true,
                "sonic_native_platform_interrupt_normal_mask_initialize",
                "sha256:a311d851694cc7d2b99b6d72af756100f9b3da5296d8b1614c5337b62f275a32",
                "sha256:97dcdb509de49995f60b1d01a345896dd8dfc0247bbb790343e3843c82bb5f2b",
                sonic_native_title_adapter_provider_implementation_identity,
                interrupt_normal_mask_guards,
                interrupt_normal_mask_effects,
                katana::runtime::NativePortProviderResultProjection{
                    katana::runtime::NativePortProviderReturnAction::Return,
                    0x1Fu,
                    0u,
                    1u,
                    "",
                    "",
                    "owner-state-v1:sha256:d3d1e9401268f893dbc461eaefe22964be9a9a6abe53bff332e5defcc1e3d93f",
                    ""}},
            katana::runtime::NativePortProviderSemanticContract{
                katana::runtime::
                    native_port_provider_semantics_contract_version,
                0x8C652132u,
                true,
                "sonic_native_pvr_asic_error_acknowledge",
                "sha256:1a66d8a02a0ce2db2580534606f7aa3d4a2447591223e97f2cdc8b33d4f859bf",
                "sha256:0c75b2c527ad219e1b9321e8a2807767bf1d1cde380198677d1fc3cb210ba233",
                sonic_native_title_adapter_provider_implementation_identity,
                error_acknowledge_guards,
                error_acknowledge_effects,
                katana::runtime::NativePortProviderResultProjection{
                    katana::runtime::NativePortProviderReturnAction::Return,
                    0x3Du,
                    0u,
                    1u,
                    "",
                    "",
                     "owner-state-v1:sha256:9cec7b1b230eb9462bc8a3b32445b0b483f73e8bc899647125b0faeb11602978",
                     ""}}};
        const katana::runtime::NativePortDefinition definition{
            katana::runtime::native_port_definition_contract_version,
            "sonic-adventure-pal-v1003",
            "native-source-generated-hardware-provider-foundations",
            {"73dd6546704fb1ac491fc44672442610d40ddf9bf34876a596c8c4f0739410e2",
             "1ST_READ.BIN",
             "sha256:b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af"},
            {0xAC008300u,
             0x8C00F3A4u,
             0x8C000000u,
             0x60000001u,
             0x00040001u,
             // BIOS-established persistent CCR state after self-clearing
             // ICI/OCI commands. Katana's native CPU-control provider keeps
             // this guest-visible configuration without constructing caches.
             0x00000121u,
             post_bootstrap_cpu.pc,
             post_aot_roots,
             post_aot_continuations,
             katana::runtime::NativePortBootstrapTimePolicy::
                 NativeHostEpoch,
             "sonic_native_bootstrap",
             post_bootstrap_cpu_identity,
             bootstrap_writes},
            {"FirstVisibleGameFrame", 0x8C604FF0u},
            checkpoint_runtime_image_ids,
            images,
            hooks,
            hardware_resolutions,
            // Sonic's simulation cadence stays title-authored. Dev products
            // start at the user-selected 144-Hz presentation rate; the host
            // may repeat completed frames without accelerating game time.
            {30u, 144u, 144u},
            native_provider_semantic_contracts,
            katana::runtime::NativePortProviderSemanticCoverage::
                DeclaredOnly,
            katana::runtime::NativePortInputOwnership::
                NativeTitleProjection};
        const auto artifact =
            katana::runtime::NativePortArtifact::write(
                std::filesystem::path(argv[1]), definition);
        std::cout << "KATANA_NATIVE_PORT_ARTIFACT_OK identity="
                  << artifact->artifact_identity() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "KATANA_NATIVE_PORT_ARTIFACT_ERROR "
                  << error.what() << '\n';
        return 1;
    }
}
