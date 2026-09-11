#include "katana/runtime/game_project_artifact.hpp"
#include "katana/io/input_provenance.hpp"
#include "katana/sh4/decoder.hpp"

#include "sonic-adventure-pal-v1003-legacy-exact-function-boundaries.hpp"
#include "sonic-adventure-pal-v1003-native-graphics-boundaries.hpp"
#include "sonic-adventure-pal-v1003-native-platform-boundaries.hpp"
#include "sonic_frame_completion_contract.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::array<std::uint32_t, 4815u> mapped_function_entries{
#include "sonic-adventure-pal-v1003-function-entries.inc"
};

} // namespace

int main(const int argc, char* argv[]) {
    try {
        if (argc != 5)
            throw std::invalid_argument(
                "usage: sonic-adventure-game-project <output-artifact> "
                "<output-runtime-image> <runtime-handle-window> "
                "<verified-boot-image>");

        constexpr std::array declared_functions{
            katana::runtime::GameProjectFunctionBoundary{
                0x8C010F0Eu,
                0x14u,
                "sa_runtime_only_static_target_8c010f0e"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C010F22u,
                0x18u,
                "sa_runtime_only_static_target"},
            // Replay witnesses only located these resident targets. The PAL
            // image and SH-4 disassembly independently close each callable
            // byte interval: no adjacent literal, padding or owner bytes are
            // admitted, and none of these declarations claims Strict closure
            // for the remaining runtime-only outgoing transfers.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C014936u,
                0x08u,
                "sa_replay_resident_tail_thunk_8c014936"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0F0B6Au,
                0xA4u,
                "sa_replay_resident_callback_owner_8c0f0b6a"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C64D0E6u,
                0x8C0u,
                "sa_replay_resident_event_owner_8c64d0e6"},
            // Disassembly- and cross-version-validated owner of the suspended
            // post-PAL caller frame. Mid-function return continuations remain
            // blocks of this function and are not promoted to functions.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C053940u,
                0xC0u,
                "sa_game_main"},
            // One state dispatcher with jump-table case blocks. The old
            // address map incorrectly promoted several case labels inside
            // this interval to independent functions.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C053B60u,
                0x94Eu,
                "sa_main_mode_dispatcher"},
            // Egg Carrier object records publish one resident callback from
            // two immutable initializer cells.  The caller below is the
            // complete PAL owner of the field load/null guard/indirect call;
            // its target remains a Candidate rather than a closed static
            // edge because the selected object record is mutable at runtime.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0573A0u,
                0x126u,
                "sa_runtime_object_callback_owner_8c0573a0"},
            // Exact collision-work constructor used by the ADV00 field. Its
            // normal allocator/object path remains AOT. The allocation-fail
            // branch is bound separately because the retail implementation
            // is an intentional non-terminating diagnostic loop.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C02F4B0u,
                0xD4u,
                "sa_collision_work_owner"},
            // PAL-disassembly-verified owners for externally reachable case
            // and loop continuations.  The resume addresses below are blocks
            // of these functions and must never be promoted to independent
            // SH-C ABI entries merely because a loaded module stores their
            // addresses.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C04BF00u,
                0x6E0u,
                "sa_resident_dispatch_owner_8c04bf00"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C04C878u,
                0xE8u,
                "sa_resident_loop_owner_8c04c878"},
            // A mutable runtime callback selects this complete resident PAL
            // object-cleanup owner. Both exits are closed: one returns at
            // 0x8C05ACE2 with its delay slot at 0x8C05ACE4, while the other
            // restores the same frame before a typed tail jump. Padding and
            // the PC-relative literal pool beginning at 0x8C05ACE6 remain
            // outside the callable identity.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C05AC66u,
                0x80u,
                "sa_registered_object_cleanup_8c05ac66"},
            // Common loaded-event callback registrar and consumer.  These
            // byte-closed PAL functions let the generic Latent-AOT callback
            // inventory discover complete callback families across PRS
            // modules instead of learning one runtime target per crash.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C09846Eu,
                0xBEu,
                "sa_loaded_event_callback_registrar"},
            // Complete resident owner of the runtime object-vtable consumer.
            // The indirect call at 0x8C0985B0 deliberately remains
            // RuntimeOnly: this exact boundary authenticates Candidate
            // execution safety but does not claim a complete target set.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C09859Cu,
                0x12Au,
                "sa_runtime_object_vtable_dispatch_owner_8c09859c"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0986CCu,
                0x42u,
                "sa_loaded_event_callback_consumer"},
            // A mutable runtime callback selects this complete resident PAL
            // state-publication leaf. Its ten-byte body ends at the RTS
            // delay slot at 0x8C0B22B4; the next independently owned
            // function starts exactly at 0x8C0B22B6.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0B22ACu,
                0xAu,
                "sa_registered_state_publication_8c0b22ac"},
            // A runtime callback record selects this complete resident PAL
            // state fan-out leaf. Its 0x5e-byte body ends at the RTS delay
            // slot at 0x8C0B282C; the next independently owned function
            // starts exactly at 0x8C0B282E.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0B27D0u,
                0x5Eu,
                "sa_registered_state_fanout_8c0b27d0"},
            // Another runtime callback record selects this complete resident
            // PAL object-state fan-out leaf. Its 0x6e-byte body ends at the
            // RTS delay slot at 0x8C0BE10C; the next independent owner starts
            // exactly at 0x8C0BE10E.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0BE0A0u,
                0x6Eu,
                "sa_registered_object_state_fanout_8c0be0a0"},
            // A mutable callback selects this complete resident PAL state-
            // link publication leaf. Its fourteen-byte body ends at the RTS
            // delay slot at 0x8C0BF340; the next independently owned
            // function starts exactly at 0x8C0BF342. PC-relative literal
            // data remains outside this callable identity.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0BF334u,
                0xEu,
                "sa_registered_state_link_publication_8c0bf334"},
            // Runtime object records select this resident PAL state-copy
            // callback.  The complete function is the sixteen bytes through
            // its RTS delay slot; the following owner begins at 0x8C0C71AE.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0C719Eu,
                0x10u,
                "sa_registered_object_state_copy_8c0c719e"},
            // Runtime callback records can select this resident PAL no-op.
            // The complete ABI is exactly RTS + NOP; the following zero/data
            // words are deliberately outside the function identity.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0C74ECu,
                0x4u,
                "sa_registered_noop_callback_8c0c74ec"},
            // Runtime callback records can also select this resident PAL
            // eight-byte state-publication leaf. The following owner begins
            // exactly at 0x8C0DD0AE, so no adjacent code or literal bytes are
            // admitted by this identity.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0DD0A6u,
                0x8u,
                "sa_registered_state_publication_8c0dd0a6"},
            // A resident object initializer stores the address of this exact
            // eight-byte state-copy leaf in field +0x18 before the shared
            // runtime callback consumer invokes it. The adjacent function
            // starts at 0x8C0DD3D0; no literal-pool bytes are admitted here.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0DD3C8u,
                0x8u,
                "sa_registered_object_state_copy_8c0dd3c8"},
            // A mutable runtime callback selects this complete resident PAL
            // state-publication leaf. Its ten-byte body ends at the RTS
            // delay slot at 0x8C0E593C; the next independent owner starts
            // exactly at 0x8C0E593E. PC-relative literal data remains
            // outside the callable identity.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0E5934u,
                0xAu,
                "sa_registered_state_publication_8c0e5934"},
            // A loaded, identity-bound event module calls this PAL resident
            // initializer through its immutable callback slot. The complete
            // body ends at the RTS delay slot at 0x8C0EF30C; the following
            // tail-call leaf and literal pool remain separate identities.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0EF280u,
                0x8Eu,
                "sa_loaded_event_state_initialize_8c0ef280"},
            // Complete resident BGM lifecycle. These four PAL owners register
            // the service callback, create its ADXT handles, service one
            // identity-bound catalog request and retire both handles. Mutable
            // handle/request tables remain runtime state and are not declared
            // as immutable callback targets.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C1077A4u,
                0xC6u,
                "sa_bgm_service_initialize"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C10786Au,
                0x136u,
                "sa_bgm_service_update"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C1079A0u,
                0x4Au,
                "sa_bgm_service_cleanup"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C107CC0u,
                0x2Au,
                "sa_bgm_service_register"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C099D50u,
                0x170u,
                "sa_resident_render_loop_owner_8c099d50"},
            // Private scenario-launcher event request service. The PAL boot
            // image proves the complete r4=event-id ABI, exact task/list
            // side effects, and a closed 0xCA-byte CFG. This entry is not a
            // claim that every ADV00 handler carrying the same literal is
            // naturally reachable; it is admitted solely as the exact
            // semantic provider used by the opt-in private launcher.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0903E8u,
                0xCAu,
                "sa_private_scenario_event_request"},
            // Complete SDK standalone filename-list import; unsupported modes
            // retain their original body under the conditional native hook.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C608BC2u, 0x4Au, "sa_native_standalone_texture_list_load"},
            // Exact shared Sega PRS transform. Native content providers bind
            // its complete stream/destination ABI so executable overlays are
            // materialized atomically before latent AOT dispatch.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C04C5E0u,
                0x98u,
                "sa_native_prs_transform_guard"},
            // Complete NINJA model-point transform. The SDK function used
            // QACR/store queues solely to publish its transformed cache into
            // ordinary title RAM; the native provider binds this whole owner
            // and performs one bounded RAM transaction instead.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C037294u,
                0xA4u,
                "sa_native_ninja_model_transform"},
            // Complete Basic NINJA model/meshset renderer. The native
            // provider consumes the verified SDK layouts above the former
            // polygon-header/QACR/TA family and submits host-GPU triangles.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0376D0u,
                0x1A2u,
                "sa_native_ninja_model_draw"},
            // Exact Sega/Katana cable-query leaf used by the native product
            // boundary.  The literal pool starts at 0x8C10D872 and is not
            // part of the displaced function body.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C10D866u,
                0x0Cu,
                "sa_video_cable_bits"},
            // Complete resident title movie boundary. It owns content setup,
            // overlay execution and cleanup as one synchronous operation;
            // the native product replaces it above the historical overlay,
            // YUV, VRAM, PVR and AICA protocols.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C10B780u,
                0x66u,
                "sa_native_movie_play"},
            // Exact synchronous PL_/SL_ lighting-table loader. Its two
            // callers construct identity-bound filenames and publish the
            // sector-aligned bytes into fixed ordinary-RAM tables; the
            // native provider replaces the obsolete Sega filesystem path.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0141E0u,
                0x62u,
                "sa_native_lighting_content_load"},
            // Exact synchronous SDK content-range owner.  The analyzer
            // independently recognizes its bounded submit/poll/completion
            // shape; this disassembly-validated interval supplies the strict
            // ownership proof required before replacing the whole function.
            // The executable owner ends at 0x8C10C0F8.  Its following 16-byte
            // literal pool remains immutable source data and is deliberately
            // outside the replaced function contract.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C10C06Eu,
                0x8Au,
                "sa_native_content_range_read"},
            // Exact public VMU readiness veneer and file-lookup owner. The
            // ready veneer ends after its tail-jump delay slot at 602D36;
            // the next prologue at 602D38 is a separate owner. File lookup
            // includes its return cleanup through 6032F0; the independent
            // owner at 6032F2 and all external literal pools stay outside.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C602D32u,
                0x6u,
                "sa_native_save_unit_ready"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C6032C4u,
                0x2Eu,
                "sa_native_save_sdk_file_exists"},
            // Complete resident VMU metadata-query owners.  Both functions
            // contain a tail-dispatch path followed by their ordinary return
            // cleanup; the full 0x2e-byte interval through the RTS delay slot
            // is one callable owner.  The following independent prologues at
            // 0x8C602ECA and 0x8C6033E2 remain outside these identities.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C602E9Cu,
                0x2Eu,
                "sa_native_save_unit_query"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C6033B4u,
                0x2Eu,
                "sa_native_save_file_blocks"},
            // The separate free-space query and VMU game/partial-read
            // owners are used by Chao storage. Each interval includes its
            // complete return cleanup and delay slot, excluding the next
            // prologue and the shared external literal pools.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C602ECAu,
                0x2Eu,
                "sa_native_save_free_blocks"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C6034FEu,
                0x5Eu,
                "sa_native_save_write_game"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C60355Cu,
                0x56u,
                "sa_native_save_read_blocks"},
            // Exact landing-chain owner shared by the two immutable 17-way
            // SDK dispatches at 0x8C10CC80 and 0x8C10CC8C.  The individual
            // table targets are internal fallthrough blocks, not independent
            // function owners.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C10CC96u,
                0x42u,
                "sa_table_landing_chain_8c10cc96"},
            // Exact PAL owner recovered for the singleton call at
            // 0x8C062FD0. The owner ends at the first byte after its
            // return/delay-slot pair; the literal cell remains separate.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C062F7Eu,
                0x6Eu,
                "sa_static_singleton_call_owner_8c062f7e"},
            // Exact trigonometric dispatcher reached by the ADV00 Station
            // Square path. Its bounded signed-relative table selects eight
            // adjacent ABI leaf owners; literal data beginning at 0x8C085A2A
            // remains outside every callable interval.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C085990u,
                0x6Au,
                "sa_resident_angle_dispatch_8c085990"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0859FAu,
                0x04u,
                "sa_resident_angle_case_8c0859fa"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0859FEu,
                0x06u,
                "sa_resident_angle_case_8c0859fe"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C085A04u,
                0x06u,
                "sa_resident_angle_case_8c085a04"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C085A0Au,
                0x06u,
                "sa_resident_angle_case_8c085a0a"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C085A10u,
                0x08u,
                "sa_resident_angle_case_8c085a10"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C085A18u,
                0x06u,
                "sa_resident_angle_case_8c085a18"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C085A1Eu,
                0x08u,
                "sa_resident_angle_case_8c085a1e"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C085A26u,
                0x04u,
                "sa_resident_angle_case_8c085a26"},
            // Exact resident helper reached from the loaded ADV00 overlay.
            // The current FunctionMap and PAL image agree on the complete
            // [0x8C0CF9AE, 0x8C0CFA0E) owner; binding the whole interval keeps
            // the overlay call native without manufacturing a mid-function
            // entry or widening into the following owner.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0CF9AEu,
                0x60u,
                "sa_resident_overlay_helper_8c0cf9ae"},
            // Cross-version exact SDK transfer barriers. They bracket all
            // four historical G2 channels as one semantic operation; the
            // native product binds the whole owners and never exposes the
            // per-channel register helpers as providers.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C10ED60u,
                0x34u,
                "sa_native_transfer_resume_all"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C10EDDCu,
                0x34u,
                "sa_native_transfer_suspend_all"},
            // Complete SDK FIFO-status leaf for the native audio/content
            // transfer family. The original owner sampled SB_FFST bit 0 and
            // published one boolean to title RAM; the native provider binds
            // that ABI above the synchronous host-transfer boundary.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C10F8E8u,
                0x0Eu,
                "sa_native_transfer_audio_fifo_ready"},
            // Resident TA/DMA descriptor-ring owners. Reset has a complete
            // native state/result projection; submit and drain remain bounded
            // bring-up boundaries because the displaced TA aperture has no
            // proven native consumer.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C650AA0u,
                0x68u,
                "sa_native_transfer_ring_reset"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C650B08u,
                0x220u,
                "sa_native_transfer_ring_submit"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C650D80u,
                0x138u,
                "sa_native_transfer_ring_drain"},
            // PAL-verified PVR/System-ASIC owners. They remain executable
            // bring-up boundaries until the native status/Manatee completion
            // producer is identity-bound; no P4/MMIO runtime is admitted.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C6515E0u,
                0x2AAu,
                "sa_native_pvr_asic_channel_setup"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C65188Au,
                0x216u,
                "sa_native_pvr_asic_service"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C651B00u,
                0x35Eu,
                "sa_native_pvr_asic_scheduler"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C652150u,
                0x2C2u,
                "sa_native_pvr_asic_dispatch"},
            // Complete Sega/Katana operand-cache maintenance family. Native
            // AOT code and title RAM are host-coherent, so these owners map
            // to validated host ordering barriers instead of exposing the
            // SH-4 cache-address arrays or reproducing a cache device.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C5FC018u,
                0x25Eu,
                "sa_native_operand_cache_invalidate_range"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C5FC276u,
                0x254u,
                "sa_native_operand_cache_purge_range"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C5FC4CAu,
                0x292u,
                "sa_native_operand_cache_writeback_range"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C5FC75Cu,
                0xB0u,
                "sa_native_operand_cache_invalidate_all"},
            // Native audio lifecycle owners.  These are complete SDK-level
            // control boundaries: the product preserves title-visible state
            // without exposing the displaced AICA register protocol.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C6044F4u,
                0x10Cu,
                "sa_native_audio_control_transaction"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C604600u,
                0x42u,
                "sa_native_audio_output_control"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C65A140u,
                0x68u,
                "sa_native_audio_processor_stop"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C65A1BCu,
                0x6Au,
                "sa_native_audio_processor_start"},
            // Complete title/Manatee lifecycle and service family recovered
            // from the identity-bound PAL disassembly. These are semantic
            // host-audio owners, never guest command/AICA providers.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C092F28u,
                0x84u,
                "sa_native_sound_output_mode_initialize"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C092FACu,
                0x28u,
                "sa_native_sound_port_table_initialize"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C093DA0u,
                0xA2u,
                "sa_native_sound_driver_initialize"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C643730u,
                0x68u,
                "sa_native_audio_defaults"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C643ED8u,
                0x58u,
                "sa_native_audio_service"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C643F62u,
                0x72u,
                "sa_native_audio_foundation_initialize"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C643FDCu,
                0x48u,
                "sa_native_audio_foundation_shutdown"},
            // Exact ADXT object-family entries recovered from the verified
            // PAL executable.  Katana's own CFG decoder confirms each prolog,
            // terminal transfer and delay slot; the legacy IDB is used only
            // as a cross-version semantic oracle because its coarse owner
            // intervals merge several genuine public ADXT entries.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C644232u, 0xAAu, "sa_native_adxt_destroy"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C6443A8u, 0x98u, "sa_native_adxt_stop"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C644440u, 0x04u, "sa_native_adxt_status"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C644444u, 0x6Cu, "sa_native_adxt_time"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C64452Au, 0x12u, "sa_native_adxt_output_volume"},
            // Complete ADXT_StartAfs owner.  The exact PAL ABI is
            // (wrapper, afs-id, ordinal); the owner resolves one immutable
            // AFS table member and then enters the same ADXT start/completion
            // family as StartFname.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C644844u, 0x64u, "sa_native_adxt_start_afs"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C6448A8u, 0x92u, "sa_native_adxt_start_file"},
            // Exact title/Manatee semantic boundaries.  The native provider
            // consumes identity-bound MLT content and MIDI intent above the
            // command packer, so none of these replacements exposes AICA or
            // a guest sound-driver protocol.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0938F0u, 0x42u, "sa_native_sound_collection_load"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C093080u, 0x100u, "sa_native_midi_initialize"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C093180u, 0x4Eu, "sa_native_midi_volume"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0931CEu, 0x52u, "sa_native_midi_pan"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C093220u, 0x5Eu, "sa_native_midi_pitch"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0932A0u, 0x5Au, "sa_native_midi_note_on"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0932FAu, 0x52u, "sa_native_midi_note_off"},
            // Complete title-facing sound multiplexer. Cross-version legacy
            // matching proves the nontrivial owner bounds exactly; the two
            // tiny forwarding wrappers are bounded by their terminal delay
            // slots and the following aligned owner/data interval.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C093A6Au, 0x46u, "sa_native_sound_play"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C093AB0u, 0x20u, "sa_native_sound_stop"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C093B20u, 0x52u, "sa_native_sound_volume"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C093B72u, 0x22u, "sa_native_sound_pan"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C093B94u, 0x1Au, "sa_native_sound_pitch"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C093BB4u,
                0x30u,
                "sa_native_sound_close_sequence_ports"},
            // Complete SDK video-mode application owner.  The native
            // provider retains every title-RAM mirror/derived value and
            // replaces its PVR writes plus scanline busy-waits as one unit.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C658220u,
                0x26Cu,
                "sa_native_video_mode_apply"},
            // Exact 60-Hz SDK mode constructor selected by the PAL title's
            // native product policy.  It formerly built a temporary PVR
            // register batch and called the generic mode-apply owner above.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C658500u,
                0x244u,
                "sa_native_video_mode_60hz"},
            // Final framebuffer/clip/scaler commit for the selected display
            // mode. It has no title-RAM side effects and becomes one native
            // output-layout commit rather than nine PVR register writes.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C652500u,
                0x5Cu,
                "sa_native_video_output_commit"},
            // Complete high-level NINJA fog state family.  The pure table
            // construction math remains AOT; these three owners replace only
            // the former color, density and 128-entry table publication into
            // PVR state with native renderer state.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C63FD60u,
                0x16u,
                "sa_native_ninja_fog_table_color"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C63FD7Cu,
                0x06u,
                "sa_native_ninja_fog_density"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C63FD88u,
                0x06u,
                "sa_native_ninja_fog_table"},
            // Direct low-level entries of the same NINJA fog family. The
            // public wrappers above tail-call these owners, while other SDK
            // paths may call them directly; all map to the same native
            // renderer state rather than the shared PVR write leaf.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C652CAEu,
                0x10u,
                "sa_native_ninja_fog_table_color_direct"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C652D00u,
                0x10u,
                "sa_native_ninja_fog_vertex_color_direct"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C652D10u,
                0x68u,
                "sa_native_ninja_fog_table_direct"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C652D78u,
                0x18u,
                "sa_native_ninja_fog_density_direct"},
            // Complete SDK/NINJA cull-value owner.  It accepted fr4,
            // canonicalized the magnitude and formerly published it through
            // one PVR register write; native graphics retains the semantic
            // render-state value instead.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C652C7Au,
                0x1Cu,
                "sa_native_ninja_cull_value"},
            // Exact NINJA font initializer. It remains ordinary AOT so its
            // colour, glyph-size and print-context side effects stay intact;
            // its hardware-facing surface-transfer leaf is replaced by the
            // native embedded-texture provider.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C6405ECu,
                0xA6u,
                "sa_native_ninja_font_initialize"},
            // Exact bounded VRAM32 word-copy wrapper.  The private provider
            // projects its complete register/T result into the same native
            // texture backing store used by the higher-level upload owner.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C6533BCu,
                0x16u,
                "sa_native_texture_backing_word_copy"},
            // Complete synchronous SDK texture-backing upload.  It replaces
            // the aligned DMA-ring and texture-aperture tail paths as one
            // bounded host-memory transaction; raw TA/MMIO is never exposed.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C653420u,
                0xE2u,
                "sa_native_texture_backing_upload"},
            // Paired SDK completion owner. It maps the token published by
            // 0x8C653420 to complete/busy without exposing the displaced
            // descriptor ring or its raw 0x8C650EB8 scanner.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C653320u,
                0x24u,
                "sa_native_texture_backing_completion"},
            // Exact font-surface upload leaf used by both branches of the
            // initializer. It has no generic archive/GBIX semantics.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C65387Au,
                0x62u,
                "sa_native_font_surface_upload"},
            // Complete SDK video-memory arena owner. Its ordinary title-RAM
            // allocator/list effects stay in AOT; all four control-flow
            // variants contain the same isolated SPG timing-reset call and
            // are replaced instruction-locally by the native output clock.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C663080u,
                0x18Cu,
                "sa_native_video_memory_arena_initialize"},
            // Complete high-level TA command/parameter arena configuration.
            // It only projects title-owned dimensions/buffer selection into
            // seven legacy PVR registers; native graphics owns those arenas.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C665D7Cu,
                0x106u,
                "sa_native_render_command_arena_configure"},
            // SDK frame-boundary helper: it used to busy-wait for one
            // complete transition of the PVR scanline field.  The native
            // provider binds the whole function to host simulation pacing.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C6044D4u,
                0x14u,
                "sa_native_frame_boundary_wait"},
            // Cross-version exact SDK producer/completion wait. The native
            // provider replaces this full owner only after Katana proves its
            // closed boundary and exact boot-image identity; the callback and
            // post-wait title services remain statically recompiled AOT.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0517F6u,
                0x16u,
                "sa_native_frame_producer_wait"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C051810u,
                0x6Eu,
                "sa_native_frame_producer_complete"},
            // Resident callback-list owners. Their VBR/ASIC side effects are
            // replaced by the identity-bound host registration service; the
            // exact descriptor/list RAM contract remains visible to AOT.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C64588Cu,
                0x1AEu,
                "sa_native_platform_interrupt_register"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C645B94u,
                0x16Au,
                "sa_native_platform_interrupt_release"},
            // Four exact leaf boundaries selected by the still-AOT-owned
            // NINJA transform owner 0x8C611384. Their host family preserves
            // the common descriptor ABI while replacing eight QACR sites.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C610FA0u,
                0xBEu,
                "sa_native_transform_stream_aux_unclipped"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C611062u,
                0xB0u,
                "sa_native_transform_stream_unclipped"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C611116u,
                0x108u,
                "sa_native_transform_stream_aux_clipped"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C611222u,
                0xFCu,
                "sa_native_transform_stream_clipped"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C10F9A0u,
                0x36u,
                "sa_runtime_only_dispatch_target_8c10f9a0"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C602528u,
                0x26u,
                "sa_native_timer_initialize"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C60254Eu,
                0x0Au,
                "sa_native_timer_count"},
            // Complete title timing helpers around the one-second native
            // host clock provider. Disassembly proves three independent
            // leaves: scaled delta-and-resample, counter resample, and scaled
            // elapsed phase. Their former TMU1 access is replaced at the
            // whole-function ABI boundary; no timer MMIO is exposed.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C06C09Au,
                0x28u,
                "sa_native_periodic_delta"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C06C0C2u,
                0x0Au,
                "sa_native_periodic_sample"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C06C0F2u,
                0x26u,
                "sa_native_periodic_elapsed"},
            // Exact NINJA registered-texture selector. The complete leaf is
            // TST/branch, optional global TEXLIST load, bounded 12-byte
            // NJS_TEXNAME selection and RTS+delay descriptor-key load. The
            // private provider preserves that ABI while materializing an
            // identity-bound loaded-module standalone texture before its
            // first selector read.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C07EEAEu,
                0x1Au,
                "sa_native_registered_texture_key_select"},
            // Identity-bound PAL selector owners. These exact extents bind
            // the complete guarded selector CFGs without promoting any
            // missing disassembly or additional root. Cross-owner targets
            // still require the generic unique-ownership gate. The first
            // extent includes all 15 cases, their shared return and the
            // branch-reached leaf at 0x8C079A96; ending at the table itself
            // would silently drop its direct callees.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0793B6u,
                0x784u,
                "sa_agent_table_owner_8c0793b6"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C082760u,
                0x1C6u,
                "sa_agent_table_owner_8c082760"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0B0724u,
                0x6B0u,
                "sa_agent_table_owner_8c0b0724"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0E4372u,
                0x45Au,
                "sa_agent_table_owner_8c0e4372"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C63A944u,
                0x70u,
                "sa_agent_table_owner_8c63a944"},
            // Shared PAL tail helper reached by several resident checkpoint
            // owners.  The entry starts with its own frame setup at 0x8C1001D4
            // and ends at the terminal indirect jump/delay-slot pair just
            // before the independent FunctionMap entry at 0x8C1001F2.
            // Giving the helper one exact owner removes the former
            // multi-owner static branch without claiming its three dynamic
            // outgoing transfers as statically closed.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C1001D4u,
                0x1Eu,
                "sa_egg_fleet_shared_tail_helper_8c1001d4"},
            // Historical Egg-Fleet A findings are implementation work, not
            // recurring report items. These extents are the exact PAL owner
            // bodies recovered from their prologues through the final
            // return/tail-call delay slots. The two large state dispatchers
            // deliberately subsume FunctionMap case labels; their default
            // table targets are the verified owner epilogues, not new
            // functions. The 0x8C0FFB80 owner is the reachable callee that
            // contains 0x8C0FFC0C, while 0x8C1004F8 remains its checkpoint
            // root.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0D78A0u,
                0x27A6u,
                "sa_egg_fleet_state_dispatch_owner_8c0d78a0"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0E0DB0u,
                0x29FEu,
                "sa_egg_fleet_state_dispatch_owner_8c0e0db0"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C101DA8u,
                0xB62u,
                "sa_egg_fleet_callback_owner_8c101da8"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0F084Eu,
                0xE2u,
                "sa_egg_fleet_callback_owner_8c0f084e"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C1009D6u,
                0x186u,
                "sa_egg_fleet_callback_owner_8c1009d6"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C061300u,
                0xBCu,
                "sa_egg_fleet_callback_owner_8c061300"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C062726u,
                0x34u,
                "sa_egg_fleet_callback_owner_8c062726"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C100614u,
                0x19Au,
                "sa_egg_fleet_callback_owner_8c100614"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C1004F8u,
                0x11Cu,
                "sa_egg_fleet_checkpoint_owner_8c1004f8"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0FFB80u,
                0x272u,
                "sa_egg_fleet_callback_owner_8c0ffb80"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C065740u,
                0x1AEu,
                "sa_egg_fleet_callback_owner_8c065740"},
            // Emerald Coast object-state callback family. The resident
            // updater loads object+0x60 and calls it at one shared site;
            // the two exact initializers below publish only these five
            // byte-bound targets into that field.
            katana::runtime::GameProjectFunctionBoundary{
                0x8C016076u,
                0x1B4u,
                "sa_emerald_coast_object_update_owner_8c016076"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C01622Au,
                0x30Cu,
                "sa_emerald_coast_object_update_callee_8c01622a"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C016536u,
                0x168u,
                "sa_emerald_coast_object_callback_8c016536"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C016754u,
                0x56u,
                "sa_emerald_coast_object_callback_8c016754"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C016800u,
                0xA8u,
                "sa_emerald_coast_object_callback_8c016800"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C0168A8u,
                0x1C8u,
                "sa_emerald_coast_object_callback_8c0168a8"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C016A70u,
                0x1F0u,
                "sa_emerald_coast_object_callback_8c016a70"},
            katana::runtime::GameProjectFunctionBoundary{
                0x8C6602F0u,
                0x18u,
                "sa_runtime_only_dispatch_target_8c6602f0"}};

        // Whole-game GDI audit: exact leaf returns and deterministic direct /
        // PC-relative tail thunks that are present in the verified PAL boot
        // but were never materialized by the historical partial function
        // inventory. Each range is independently byte-bound below. Keeping
        // these in one data-driven set lets every currently known scene use
        // the same strict AOT path instead of discovering the same trivial
        // executable shapes one crash at a time.
        struct WholeGameStrictEntry final {
            std::uint32_t start;
            std::uint32_t size;
            std::string_view code_identity;
        };
        constexpr std::array whole_game_strict_entries{
            WholeGameStrictEntry{0x8C0108E0u, 0x4u, "sha256:6e2abf071be3175774f79f344c52ae8ebd9f6b0d8d7608507d58f987914cf097"},
            WholeGameStrictEntry{0x8C015AD8u, 0x44u, "sha256:45469ae3c04e959a287a102952c72f99097f5616e7537bca37619a0bd074269d"},
            WholeGameStrictEntry{0x8C01818Cu, 0x4u, "sha256:7da6fbcda17a75ce183fc1991b1c56940378cf33a83cdcc2ef7305ec398ec6b5"},
            WholeGameStrictEntry{0x8C019768u, 0x4u, "sha256:bab08c7125cbb73f7a6bf05d2892d83643df15eb73adccc56011bb4204616547"},
            WholeGameStrictEntry{0x8C01C296u, 0x4u, "sha256:6c80b178c827e8ec05d461ff98566d5223015ea9817c3785723bed4c7edbce11"},
            WholeGameStrictEntry{0x8C02085Au, 0x6u, "sha256:613a57b7101d4c3086c98ebe596a6920dfb5bc55cfef53687b6fbe5f9e62639e"},
            WholeGameStrictEntry{0x8C0209A0u, 0x140u, "sha256:33624eb7126b420aeefec61779f24d9266531e45792c60e56ccf56243e313a5c"},
            WholeGameStrictEntry{0x8C020AE0u, 0xB2u, "sha256:d149b47e385741d458949f9973b83e57d689aa79fe4e19133c116f67421b9d40"},
            WholeGameStrictEntry{0x8C021284u, 0x40u, "sha256:77ec61c7a9dacb1fbc78cbeccdd523821d53eb595091a3111f7965e14fc8e3c4"},
            WholeGameStrictEntry{0x8C022580u, 0xC0u, "sha256:f8bcf8aa77f67622a8f8730ee3565984d7aaa2bb3804b6922b4729bd33e04001"},
            WholeGameStrictEntry{0x8C034DAEu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C035EC0u, 0x6Au, "sha256:6cca8df2cd9ead78b61e4154b0a009f0cadc68ab3924ed3083702ee11bac282d"},
            // Resident status-font entry family: each complete owner is an
            // independently byte-bound native SDK surface, including formatters
            // omitted by the old function inventory. Root the exact entries via
            // the existing static-entry contract; descriptive boundaries alone
            // cannot admit their whole-function native replacements. This does
            // not claim natural story reachability or close outgoing frontiers.
            WholeGameStrictEntry{0x8C042298u, 0x17Eu, "sha256:84620eb8611286e2049fc186391b1be274b84aa4a8857fed16b3cf5b60843118"},
            WholeGameStrictEntry{0x8C042416u, 0x118u, "sha256:de18d7badb24d56798e9e71e29e6f8ea8a68dfb659889d583535840287c75eaa"},
            WholeGameStrictEntry{0x8C0425A0u, 0x290u, "sha256:8fab141d4300d0e97d979337114074bf6b841e0fc916ffdea212d86750618c8c"},
            WholeGameStrictEntry{0x8C042880u, 0x10Eu, "sha256:b2b6318696bce99ccc615393e22a84c61c4bbcfae0f81c41d466c04daec228d1"},
            WholeGameStrictEntry{0x8C04298Eu, 0x194u, "sha256:a4e6c15d7f6f32f9b74a4b9d07bc4ea86d7bef0e98b785d3858f78bca5330b35"},
            WholeGameStrictEntry{0x8C042B22u, 0x218u, "sha256:b71163eac72d473110ad4cfc7fd1b80e4b79ab8fa6b1af5d4413da58567a29ac"},
            WholeGameStrictEntry{0x8C042DC0u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C0437C2u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C044280u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C0445A0u, 0x50u, "sha256:54505efab473d3e04487e9e26aa2701608b6215b7e28da0a4c1fccd77d09faa0"},
            WholeGameStrictEntry{0x8C0452C0u, 0xAEu, "sha256:f7a898ddf7e502cccb367bb8254f209da4bc5444fd95921b118479147098f2e8"},
            WholeGameStrictEntry{0x8C0456CAu, 0x2Au, "sha256:a09ff60ac17cdf687b8fd082ba3f08e1786fedc348eb7487adce94e0f749a8d0"},
            WholeGameStrictEntry{0x8C0459E0u, 0x90u, "sha256:ad4c0e8f3e0c5a0f7f631a12d1bf0c512b6a93845cd107171e08dfd82b750910"},
            WholeGameStrictEntry{0x8C045B20u, 0x96u, "sha256:a3d56540f50b55c124414917b0c6f77f513f5c061d077ee59f065603c83d3cfc"},
            WholeGameStrictEntry{0x8C04611Cu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C046480u, 0x6Eu, "sha256:ee7c4793ce491b69b07b9206ef1c7cb453e1ab5cbae0d90278aa3e1fa1a56f1d"},
            WholeGameStrictEntry{0x8C0464EEu, 0xF2u, "sha256:9ab8bcee683a748a40a1e7bb533d1d823d6074f223c9a6c27aaeba01d70d2491"},
            WholeGameStrictEntry{0x8C046960u, 0xA8u, "sha256:cfb1203839a84719889fd53134d83d2e5e3b049531cdad7677149720f2e90c6e"},
            WholeGameStrictEntry{0x8C046D60u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C050172u, 0x1Eu, "sha256:30327a950996fb040cad336b645571ca6427f71b91825c2ce3f91a7956cf0fee"},
            WholeGameStrictEntry{0x8C051D38u, 0x18u, "sha256:ddf658be211c3b84579ec1d4f2d66379be6c00533cd4c302c0ebd532e84988b8"},
            WholeGameStrictEntry{0x8C051E00u, 0x56u, "sha256:d8c8e160de6e28ea397175cbd88494494c20debe210b30880a0e55a5c47a89af"},
            WholeGameStrictEntry{0x8C059014u, 0x16u, "sha256:69950faf74c1a8c25dd26da5aa70708d015da7351e120173652e4872c2e66e65"},
            WholeGameStrictEntry{0x8C059A82u, 0x6u, "sha256:b672ddd1a620d42765c029d494e660e9d7a70504e0c41691f02a200687aa48b8"},
            WholeGameStrictEntry{0x8C05A18Eu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C05A822u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C05B7FCu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C05C280u, 0x2Au, "sha256:50d72ba0d0245be15e168c1bda78935bc9d90acb39e52eb75a6945f9f2db4d71"},
            WholeGameStrictEntry{0x8C05D6A4u, 0x42u, "sha256:36b9a02e8770e805d3043744f18e94996829daba7d90457001cc056b1b4f6e97"},
            WholeGameStrictEntry{0x8C05D6E6u, 0x30u, "sha256:9caebf923fdec18795b01d7285be29fa28dea48fb4f970074bc10bed2d86e2ed"},
            WholeGameStrictEntry{0x8C05D716u, 0x30u, "sha256:a5a5c5d7456f60b792cf107c5f88e9b14d07ddb2fb05be8beb493cbcfcd26524"},
            WholeGameStrictEntry{0x8C05D746u, 0x4Eu, "sha256:155a4392b5624c5ff2eb7b78bd9c0720865812619bb040f720da071c55a07859"},
            WholeGameStrictEntry{0x8C05D794u, 0x30u, "sha256:dcc233e528f4bb7f5ab2b818ca0e33973219d90074b527fafe358f24ae03a1a5"},
            WholeGameStrictEntry{0x8C05D7C4u, 0x7Cu, "sha256:0919fd4b00be88f83b1cf8313fa78825b7dae22261ec16e4aa94629dc6efc242"},
            WholeGameStrictEntry{0x8C05D840u, 0x30u, "sha256:542aae965fd1464040c512017f68174ad04d6bac7142351fdb87ca479a1c1241"},
            WholeGameStrictEntry{0x8C05D870u, 0x34u, "sha256:798195bce997917685f2cecf341d1dad61c3ca1531778f66b6397ba740811d65"},
            WholeGameStrictEntry{0x8C05D8A4u, 0x5Cu, "sha256:1eb6e24148674f1d7b2502e3fba1639e7974ee927374d5c9fa5edb79c1a3b09a"},
            WholeGameStrictEntry{0x8C05DDE0u, 0x78u, "sha256:0929dec5abfd162efe2aa9fe4885be1d3f693e7648a3aaa73f2df329517c5e73"},
            WholeGameStrictEntry{0x8C05FF96u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C06019Au, 0x12u, "sha256:7515d572c630a7fdbd8c6f2712a82634609000a109e7f6e6125e500f3e358346"},
            WholeGameStrictEntry{0x8C060A6Au, 0x8u, "sha256:6ce43d1f1a018d5d15b627fee14442bc0ffa8cb1858a6d62d3ac3610d83127cb"},
            WholeGameStrictEntry{0x8C0614A6u, 0x3Au, "sha256:fb331feeda656f882a73ec27f3dfe1c698fa42e21bda9e738075f09ad3757de9"},
            WholeGameStrictEntry{0x8C061B46u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C0627CCu, 0x10u, "sha256:f8ddf192ee3cbd0ff4b6a684cc4c62fc257222fc5e89a09948388203aec3c9d6"},
            WholeGameStrictEntry{0x8C064AF4u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C0651AAu, 0x11Cu, "sha256:d5853d726e7dcbeb6dc5a05b2a30c9cf61eeddddafa8f2d716f6c73a5410c5a3"},
            WholeGameStrictEntry{0x8C065D1Au, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C06620Cu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C0687BEu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C068D4Eu, 0x8u, "sha256:a2b577b6b3631bddfc900eb045fae43ed96d9185997da378448dc3cc1ece15d2"},
            WholeGameStrictEntry{0x8C07BDA8u, 0x6u, "sha256:5efc11eacb727b57b4f32ff2a7d970d9ecd58f2836260245c673f008cc055f1d"},
            WholeGameStrictEntry{0x8C07E1FCu, 0x4u, "sha256:9f14b32f5fd632d6e1f7ad56d217c01add6f431f18639c5b0d37ba9d6f03bc6d"},
            WholeGameStrictEntry{0x8C0876ACu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C087DC0u, 0x6u, "sha256:7823f09348009462909ffdff38344c15192a548cdf82ed69c64545aa519a79a7"},
            WholeGameStrictEntry{0x8C090A48u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C090ED4u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C090ED8u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C099B74u, 0x4u, "sha256:5ded00bc5e7c4ae92bf47eb7afb9cffd4c262d4e59685b1eb274fc85703341ed"},
            WholeGameStrictEntry{0x8C09DEA4u, 0x154u, "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a"},
            WholeGameStrictEntry{0x8C09E280u, 0xA8u, "sha256:14358c8f969d23e446f57551abda9a678279938eae27de52f2fd5df7e6c95a7c"},
            // The object-list family contains consecutive callable entries.
            // Keep each SH-4 function boundary exact: the former broad
            // E460/E600 ranges swallowed three post-bootstrap callback roots
            // and caused the AOT validator to reject otherwise valid roots.
            WholeGameStrictEntry{0x8C09E460u, 0x7Au, "sha256:f73528150125bd82fa95711e88d3550864d16ad5713964d7bae28c8406b327f9"},
            WholeGameStrictEntry{0x8C09E4DAu, 0x78u, "sha256:65935b89bb14af21062f159a230b53c9eee8be2f5f1fd5747cba1b5d764fe2aa"},
            WholeGameStrictEntry{0x8C09E552u, 0xAEu, "sha256:92857f2b0db89b9bbab8cbce71827ebcedc2e05ce84074abf3034052b47cc625"},
            WholeGameStrictEntry{0x8C09E600u, 0x26u, "sha256:0dba193cd1e286c0c339d1ac0b95adcf28d09050561c7545d552efc43f7fd9cb"},
            WholeGameStrictEntry{0x8C09E626u, 0x7Au, "sha256:cd65ff39d5ce1a1e6176b60bd4fd00ba675a970b1d8922c84bb455cbbe10668f"},
            WholeGameStrictEntry{0x8C0A6AB2u, 0x6u, "sha256:7338fc402171cffdf5ebb27727966ecfebb4f26837cbfef7a7f2df1adafd5a28"},
            WholeGameStrictEntry{0x8C0A74E0u, 0x12u, "sha256:971bca9a67cc7ad3b50035df67f177aeef0be9ef9906dab34f0117b4e1448979"},
            WholeGameStrictEntry{0x8C0B3A3Cu, 0x1Eu, "sha256:17cc8a00122585403f7a5b04949d26a921ca018efe4944644640d56470404ad6"},
            WholeGameStrictEntry{0x8C0B4DA2u, 0x34u, "sha256:2da05464bc975ad2200dfa623c0d259d9e5816a8796d9c8941d33881326bd757"},
            WholeGameStrictEntry{0x8C0B5364u, 0x3Cu, "sha256:5644e79a745a49e18af8e909eeca4213d4556f41f9c9565cdeadff73ecee4a1c"},
            WholeGameStrictEntry{0x8C0B5D0Au, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C0B917Eu, 0x6u, "sha256:54d83cc764e214aa1f52f881ca7542685f12a38754533963ff6c73bbbd798df5"},
            WholeGameStrictEntry{0x8C0CED2Eu, 0x4u, "sha256:f961b4d73868233bd65b9d9c220be855c2a9f03d6b9879bdbd1c52ffc5c8baed"},
            WholeGameStrictEntry{0x8C0D6700u, 0xB0u, "sha256:e0e279055f80257a25fcfbea1d871b76db8e6a6e00c3187bf6dcfe52df9f0a69"},
            WholeGameStrictEntry{0x8C0E6752u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C0EB4EAu, 0x6u, "sha256:46352460141e8743a24c54a0a3bc52c5ee09eb846db855612e8c651d92e8cf27"},
            WholeGameStrictEntry{0x8C0EC6B6u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C0F2164u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C107BFEu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C10CD72u, 0x4u, "sha256:f0bf9419d36fad3a538217fe668b430d6b291e0290cd488e99236627e533573a"},
            WholeGameStrictEntry{0x8C5FBC1Cu, 0x4u, "sha256:ae7af792669f771e36f48ac775523492c3d06b57d569be1e70164ea5891dbcae"},
            WholeGameStrictEntry{0x8C5FBC24u, 0x4u, "sha256:680f2216ef2e59a1086c840f193415c76f2d4307990c780df8daef045bb5e230"},
            WholeGameStrictEntry{0x8C5FBC28u, 0x4u, "sha256:f3601608e3b8931a4d7bd4629b508148c9318a8ffd98043e13311bd79773a56c"},
            WholeGameStrictEntry{0x8C605598u, 0x4u, "sha256:f476bc5b53c04bfb8d3f6b8b8d598eb140d09a5c027d04ce7fa0d712e17b95bd"},
            WholeGameStrictEntry{0x8C60559Cu, 0x4u, "sha256:2937cd748e6d1fdf61b398cb6b65ac6c22face244b491513126606b769dd2967"},
            WholeGameStrictEntry{0x8C6055A0u, 0x4u, "sha256:e61b927d0b1c65230d7bacb678a4fd74784b5b0185085928a46d065691a32bae"},
            WholeGameStrictEntry{0x8C6055A4u, 0x4u, "sha256:ddb75468a06fa236b22f98a166deae2ad676eda32b82bdc368339105a64692d8"},
            WholeGameStrictEntry{0x8C6055A8u, 0x4u, "sha256:38172780ed7a8435d4c7cc996c27fc91a544521d3ec7c587a7297c4a4d50aa97"},
            WholeGameStrictEntry{0x8C6055ACu, 0x4u, "sha256:15bd45c02fef0f9bafaa7abd36fbb89fc516ef223c2acdddc2d2a2bcd7d3941c"},
            WholeGameStrictEntry{0x8C6055B0u, 0x4u, "sha256:c9f11a64a2a6923625c30f13bf64eab6ed7308bb66c43b9b31d6f36c46fe9318"},
            WholeGameStrictEntry{0x8C6055B4u, 0x4u, "sha256:38921330b32b3e04c78f2df1705609eee77b2238051209104f51eadee20f601f"},
            WholeGameStrictEntry{0x8C6055B8u, 0x4u, "sha256:8a31cd41baae2f546e99054cf7a2c98f9199991a59237d22ca8ddbe81cc8d4f2"},
            WholeGameStrictEntry{0x8C6055BCu, 0x4u, "sha256:3d97ebff741501f7701d9f87c5651cd4a13ef68c69b2894cfef90e07b112ac90"},
            WholeGameStrictEntry{0x8C6055C0u, 0x4u, "sha256:3d2bad5b187d5c686d30e8d944aecb8ace0ca7295165b9a84a87b4f92ad14c2a"},
            WholeGameStrictEntry{0x8C6446D4u, 0x6u, "sha256:5e3b7945828b8c8593e72264749a1d10bd42572c686f291800c90bc9c1249d5e"},
            WholeGameStrictEntry{0x8C64784Eu, 0x4u, "sha256:1d6ef0ccc78b7d34970636d42403ba1d8436c4b2ff536aba07882efce72314f0"},
            WholeGameStrictEntry{0x8C65B40Eu, 0x6u, "sha256:f47c1eb991a29b1d5a548281e4fd4b8d72a18a35cf97692a23738a452345fb0b"},
            WholeGameStrictEntry{0x8C65BC82u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C6607E0u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C6683FCu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            WholeGameStrictEntry{0x8C669410u, 0x6u, "sha256:41d4ed16fec6de860b1863e0dbde26f1440473f57cbd176b7c66c2208433861c"}};
        // Candidate-only compile roots for open resident object-callback
        // families.  Immutable PAL initializer cells publish byte-closed
        // targets that RuntimeOnly consumers later select through mutable
        // object state.  Multiple cells may name the same target, but every
        // cell and every shared target identity is authenticated below.
        // These records materialize only immutable Candidate targets. Even a
        // complete constructor-family inventory neither enumerates a mutable
        // field domain nor adds a static edge at its consumer, so the
        // corresponding runtime frontier remains open.
        struct ObjectCallbackCandidateAnalysisRoot final {
            std::uint32_t producer_cell;
            std::string_view producer_cell_identity;
            std::uint32_t source_owner;
            std::uint32_t source_owner_size;
            std::string_view source_owner_identity;
            std::uint32_t source_block;
            std::uint32_t source_block_size;
            std::string_view source_block_identity;
            std::uint32_t callsite;
            std::uint32_t continuation;
            std::string_view callsite_identity;
            std::uint32_t target;
            std::uint32_t target_size;
            std::string_view target_identity;
        };
        constexpr auto emerald_coast_object_callback_candidate =
            [](const std::uint32_t producer_cell,
               const std::string_view producer_cell_identity,
               const std::uint32_t target,
               const std::uint32_t target_size,
               const std::string_view target_identity) constexpr {
                return ObjectCallbackCandidateAnalysisRoot{
                    producer_cell,
                    producer_cell_identity,
                    0x8C016076u,
                    0x1B4u,
                    "sha256:f627cd08b0026d4a3e0f4b52bf21cc6d507bd53f837c279d848402714edebf8d",
                    0x8C01612Cu,
                    0xAu,
                    "sha256:2c923c8964a17c148f7406c27811f891176815817612ee081508bbd236da3401",
                    0x8C016132u,
                    0x8C016136u,
                    "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                    target,
                    target_size,
                    target_identity};
            };
        constexpr auto resident_object_callback_candidate =
            [](const std::uint32_t producer_cell,
               const std::string_view producer_cell_identity,
               const std::uint32_t target,
               const std::uint32_t target_size,
               const std::string_view target_identity) constexpr {
                return ObjectCallbackCandidateAnalysisRoot{
                    producer_cell,
                    producer_cell_identity,
                    0x8C09859Cu,
                    0x12Au,
                    "sha256:cb3b524231e8db24fb270a969c2692ef642bbd021d5a7a474d6099ad89bd0d1d",
                    0x8C0985AEu,
                    0x6u,
                    "sha256:aee69938517b85f7f61871b5493d59d44a5b2b9f43de4a7e4ab4f7b5f4b17d30",
                    0x8C0985B0u,
                    0x8C0985B4u,
                    "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                    target,
                    target_size,
                    target_identity};
            };
        constexpr std::array object_callback_candidate_analysis_roots{
            resident_object_callback_candidate(0x8C015B40u, "sha256:674c92bf82e9c87b66ff0733c3f6c6965dff367c24028417ccad90fa76625ae6", 0x8C015AD8u, 0x44u, "sha256:45469ae3c04e959a287a102952c72f99097f5616e7537bca37619a0bd074269d"),
            resident_object_callback_candidate(0x8C046D14u, "sha256:1f78d588e0fb551b7da0091a619f79c0d6634522a104046a295458237299c8cf", 0x8C046D60u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"),
            resident_object_callback_candidate(0x8C05025Cu, "sha256:01c88df6735b138b40b5dbe083c47ca59f3286436b9b9b5074c6bb914fa27f09", 0x8C050172u, 0x1Eu, "sha256:30327a950996fb040cad336b645571ca6427f71b91825c2ce3f91a7956cf0fee"),
            resident_object_callback_candidate(0x8C051B0Cu, "sha256:42bd35d1dca26ebbc2e385e9dafe362165e6039042c5b8685549c7ed48eb3444", 0x8C051D38u, 0x18u, "sha256:ddf658be211c3b84579ec1d4f2d66379be6c00533cd4c302c0ebd532e84988b8"),
            resident_object_callback_candidate(0x8C0590BCu, "sha256:d71abe990e7257e660a25101a81d2cd90b818a81ed9f3c1a9fe749bd86cb232c", 0x8C059014u, 0x16u, "sha256:69950faf74c1a8c25dd26da5aa70708d015da7351e120173652e4872c2e66e65"),
            resident_object_callback_candidate(0x8C059234u, "sha256:d71abe990e7257e660a25101a81d2cd90b818a81ed9f3c1a9fe749bd86cb232c", 0x8C059014u, 0x16u, "sha256:69950faf74c1a8c25dd26da5aa70708d015da7351e120173652e4872c2e66e65"),
            resident_object_callback_candidate(0x8C059D24u, "sha256:d7fad29fc2962338b462c907a429fdc6f4541649aadbdd38620f5394149e7dc9", 0x8C059A82u, 0x6u, "sha256:b672ddd1a620d42765c029d494e660e9d7a70504e0c41691f02a200687aa48b8"),
            resident_object_callback_candidate(0x8C060058u, "sha256:63160458d2e763456f9202c8c17d2ccaf635e8f2b23997e21fa5f3486ac7f29f", 0x8C06019Au, 0x12u, "sha256:7515d572c630a7fdbd8c6f2712a82634609000a109e7f6e6125e500f3e358346"),
            resident_object_callback_candidate(0x8C060568u, "sha256:5f98849f050628aa67f5ce339019e03d89dd99828e914d2fc8c5bd2cca04b251", 0x8C060A6Au, 0x8u, "sha256:6ce43d1f1a018d5d15b627fee14442bc0ffa8cb1858a6d62d3ac3610d83127cb"),
            resident_object_callback_candidate(0x8C060E48u, "sha256:23fdc1970f5548b39800ff1d6c68c9589d4c21ac93a95c0a4bbad4c85d7f84e9", 0x8C0614A6u, 0x3Au, "sha256:fb331feeda656f882a73ec27f3dfe1c698fa42e21bda9e738075f09ad3757de9"),
            resident_object_callback_candidate(0x8C061A7Cu, "sha256:39ed5fad8610adfbbfe7450cdcd5818aff80f03952193bcce1d26346f6e58b44", 0x8C061B46u, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"),
            resident_object_callback_candidate(0x8C0626A8u, "sha256:426c2c1b96d438d107ae8f8480b5be517f2f3e46518f2bc225d3c6bb2c297d7d", 0x8C0627CCu, 0x10u, "sha256:f8ddf192ee3cbd0ff4b6a684cc4c62fc257222fc5e89a09948388203aec3c9d6"),
            resident_object_callback_candidate(0x8C065F00u, "sha256:e417798894ca49a146d8f658c56b29bddfe1d4cfea41b2f09fe526fee55a146b", 0x8C06620Cu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"),
            resident_object_callback_candidate(0x8C0687E4u, "sha256:48bd09c2655adad8cfe1e1388e0e5ffdbeebc098d7fdd87d404d86994352d14b", 0x8C0687BEu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"),
            resident_object_callback_candidate(0x8C068A60u, "sha256:48bd09c2655adad8cfe1e1388e0e5ffdbeebc098d7fdd87d404d86994352d14b", 0x8C0687BEu, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"),
            resident_object_callback_candidate(0x8C0687F0u, "sha256:3810c7ee0faf3d58d2282d297d9d91a52326bf0a187727c9be80a46b7211ef0e", 0x8C068D4Eu, 0x8u, "sha256:a2b577b6b3631bddfc900eb045fae43ed96d9185997da378448dc3cc1ece15d2"),
            resident_object_callback_candidate(0x8C0A765Cu, "sha256:e94c5fa8031a596aa458373633b79d933fdda7e56e46181983100d147559a34a", 0x8C0A74E0u, 0x12u, "sha256:971bca9a67cc7ad3b50035df67f177aeef0be9ef9906dab34f0117b4e1448979"),
            resident_object_callback_candidate(0x8C0B3A08u, "sha256:c4089b195bb040dfa5c6384a63a3cdd5cb4d8b14a850dbb626a3e321a4ecb989", 0x8C0B3A3Cu, 0x1Eu, "sha256:17cc8a00122585403f7a5b04949d26a921ca018efe4944644640d56470404ad6"),
            resident_object_callback_candidate(0x8C0B48E0u, "sha256:38c0e9f986eaa2954b4359ced054edab3da99a8b33f08d7b6544c2e2d2a22f46", 0x8C0B4DA2u, 0x34u, "sha256:2da05464bc975ad2200dfa623c0d259d9e5816a8796d9c8941d33881326bd757"),
            resident_object_callback_candidate(0x8C0B5640u, "sha256:84f5d54f5c66a9f62193a6ba6d6fe8ef03fbeb68fb9acad2f82ce60c092780a8", 0x8C0B5364u, 0x3Cu, "sha256:5644e79a745a49e18af8e909eeca4213d4556f41f9c9565cdeadff73ecee4a1c"),
            resident_object_callback_candidate(0x8C0B5E94u, "sha256:697f25b4440049ca3b0d6561a265e32d2b8dc47b578f16e3701d4b029f003b04", 0x8C0B5D0Au, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"),
            resident_object_callback_candidate(0x8C0B6044u, "sha256:697f25b4440049ca3b0d6561a265e32d2b8dc47b578f16e3701d4b029f003b04", 0x8C0B5D0Au, 0x4u, "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"),
            ObjectCallbackCandidateAnalysisRoot{
                0x8C035990u,
                "sha256:a1c78876ee1c62989cb4cce62b60ff293c2e3530bba979bf5715c76d88ab235a",
                0x8C09859Cu,
                0x12Au,
                "sha256:cb3b524231e8db24fb270a969c2692ef642bbd021d5a7a474d6099ad89bd0d1d",
                0x8C0985AEu,
                0x6u,
                "sha256:aee69938517b85f7f61871b5493d59d44a5b2b9f43de4a7e4ab4f7b5f4b17d30",
                0x8C0985B0u,
                0x8C0985B4u,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C035EC0u,
                0x6Au,
                "sha256:6cca8df2cd9ead78b61e4154b0a009f0cadc68ab3924ed3083702ee11bac282d"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C05FE84u,
                "sha256:eb944e0083f5a1ffadb5d50c10e951d71e793593025251846cda0d854b4c3deb",
                0x8C09859Cu,
                0x12Au,
                "sha256:cb3b524231e8db24fb270a969c2692ef642bbd021d5a7a474d6099ad89bd0d1d",
                0x8C0985AEu,
                0x6u,
                "sha256:aee69938517b85f7f61871b5493d59d44a5b2b9f43de4a7e4ab4f7b5f4b17d30",
                0x8C0985B0u,
                0x8C0985B4u,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C05FF96u,
                0x4u,
                "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C086A5Cu,
                "sha256:e14b1098b9dda260e4419fe83a31eb9922b53913010a46f6391da75dc236d2f6",
                0x8C09859Cu,
                0x12Au,
                "sha256:cb3b524231e8db24fb270a969c2692ef642bbd021d5a7a474d6099ad89bd0d1d",
                0x8C0985AEu,
                0x6u,
                "sha256:aee69938517b85f7f61871b5493d59d44a5b2b9f43de4a7e4ab4f7b5f4b17d30",
                0x8C0985B0u,
                0x8C0985B4u,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C0876ACu,
                0x4u,
                "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C08715Cu,
                "sha256:e14b1098b9dda260e4419fe83a31eb9922b53913010a46f6391da75dc236d2f6",
                0x8C09859Cu,
                0x12Au,
                "sha256:cb3b524231e8db24fb270a969c2692ef642bbd021d5a7a474d6099ad89bd0d1d",
                0x8C0985AEu,
                0x6u,
                "sha256:aee69938517b85f7f61871b5493d59d44a5b2b9f43de4a7e4ab4f7b5f4b17d30",
                0x8C0985B0u,
                0x8C0985B4u,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C0876ACu,
                0x4u,
                "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C1B9B80u,
                "sha256:4d3b93e55574979c0876f008e68f95a5dfb591e51806fdfc3489a659f7566ed6",
                0x8C0573A0u,
                0x126u,
                "sha256:5090209111e5a76e3bd6e0dfad3e0fadb640cd7012b4c0a510f16b0f6d042f2d",
                0x8C05743Cu,
                0xCu,
                "sha256:2291df5a8d60dbe0d3844ca2e9485313300671e30f0cabb2ae877c72b823ec8a",
                0x8C057444u,
                0x8C057448u,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C05DDE0u,
                0x78u,
                "sha256:0929dec5abfd162efe2aa9fe4885be1d3f693e7648a3aaa73f2df329517c5e73"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C1B9B90u,
                "sha256:4d3b93e55574979c0876f008e68f95a5dfb591e51806fdfc3489a659f7566ed6",
                0x8C0573A0u,
                0x126u,
                "sha256:5090209111e5a76e3bd6e0dfad3e0fadb640cd7012b4c0a510f16b0f6d042f2d",
                0x8C05743Cu,
                0xCu,
                "sha256:2291df5a8d60dbe0d3844ca2e9485313300671e30f0cabb2ae877c72b823ec8a",
                0x8C057444u,
                0x8C057448u,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C05DDE0u,
                0x78u,
                "sha256:0929dec5abfd162efe2aa9fe4885be1d3f693e7648a3aaa73f2df329517c5e73"},
            // Complete resident node/object-list callback family. The exact
            // 0x8C09DD3C constructor writes its immutable r5 argument to
            // node+4. Twenty-one PAL callsites share these fourteen literal
            // cells and twelve FunctionMap targets; the mutable list walker
            // at 0x8C09DF36 remains RuntimeOnly/Candidate and therefore open.
            ObjectCallbackCandidateAnalysisRoot{
                0x8C04466Cu,
                "sha256:9e3399b219b731dab265f4db402b64a199a60138e1db5b1d16437d26a253bd1f",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C0445A0u,
                0x50u,
                "sha256:54505efab473d3e04487e9e26aa2701608b6215b7e28da0a4c1fccd77d09faa0"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C0454ACu,
                "sha256:1321235f79fc9efb6f0a4ff04da4956c53790795dbdd77b803afd816c6ea5412",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C0452C0u,
                0xAEu,
                "sha256:f7a898ddf7e502cccb367bb8254f209da4bc5444fd95921b118479147098f2e8"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C045684u,
                "sha256:647b8db4f051b40c6e5f27b681f896822885300cc796666cbc525a3777fdab17",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C0456CAu,
                0x2Au,
                "sha256:a09ff60ac17cdf687b8fd082ba3f08e1786fedc348eb7487adce94e0f749a8d0"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C045B10u,
                "sha256:ea1f6b29dc2470cbbf18bcc28ec986a657429615a83b863614f851942396ac23",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C0459E0u,
                0x90u,
                "sha256:ad4c0e8f3e0c5a0f7f631a12d1bf0c512b6a93845cd107171e08dfd82b750910"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C045D18u,
                "sha256:cd00647db611f46a14e6344a864b6dd21390c277f6d571afb97be3b65f4e9e88",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C045B20u,
                0x96u,
                "sha256:a3d56540f50b55c124414917b0c6f77f513f5c061d077ee59f065603c83d3cfc"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C0467A4u,
                "sha256:177e885064877f59e31166604d8907be7d595d1d9c705c4d72e7c187ff2910fb",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C0464EEu,
                0xF2u,
                "sha256:9ab8bcee683a748a40a1e7bb533d1d823d6074f223c9a6c27aaeba01d70d2491"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C046B38u,
                "sha256:dc9d9fe543a4f57cfaf8e4b1b6e2887a0a3ebd8d07c16505e7f053c5a716e015",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C046960u,
                0xA8u,
                "sha256:cfb1203839a84719889fd53134d83d2e5e3b049531cdad7677149720f2e90c6e"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C048598u,
                "sha256:38e488bf7f6ba84bcfdb557a79837d350e0a0fca7af8e4d785a9ee1693fb3f7f",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C046480u,
                0x6Eu,
                "sha256:ee7c4793ce491b69b07b9206ef1c7cb453e1ab5cbae0d90278aa3e1fa1a56f1d"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C06466Cu,
                "sha256:177e885064877f59e31166604d8907be7d595d1d9c705c4d72e7c187ff2910fb",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C0464EEu,
                0xF2u,
                "sha256:9ab8bcee683a748a40a1e7bb533d1d823d6074f223c9a6c27aaeba01d70d2491"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C07B684u,
                "sha256:38e488bf7f6ba84bcfdb557a79837d350e0a0fca7af8e4d785a9ee1693fb3f7f",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C046480u,
                0x6Eu,
                "sha256:ee7c4793ce491b69b07b9206ef1c7cb453e1ab5cbae0d90278aa3e1fa1a56f1d"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C09E448u,
                "sha256:a4c3641a9c446f900627abc71e1d748790a5784e710b4e584f711221094da54f",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C09E280u,
                0xA8u,
                "sha256:14358c8f969d23e446f57551abda9a678279938eae27de52f2fd5df7e6c95a7c"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C09E5E8u,
                "sha256:12fc7f329c9483312e23f8f2e8d3e4ab9a95cecfff24c7256b722024758967e6",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C09E460u,
                0x7Au,
                "sha256:f73528150125bd82fa95711e88d3550864d16ad5713964d7bae28c8406b327f9"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C09E688u,
                "sha256:78510a847043b230c69bdb003d0d93c63c4b3814c33faa86ff750758471de568",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C09E600u,
                0x26u,
                "sha256:0dba193cd1e286c0c339d1ac0b95adcf28d09050561c7545d552efc43f7fd9cb"},
            ObjectCallbackCandidateAnalysisRoot{
                0x8C0D6868u,
                "sha256:7464756d432742ede6ca399c5509137144b22108e7a0063b2b6ce1c7ba272c52",
                0x8C09DEA4u,
                0x154u,
                "sha256:09827f24d76512fa78c6a6fec9e1c95141f9f996b9fed07cbda399f27134383a",
                0x8C09DF32u,
                0x8u,
                "sha256:be6a53cc1b0a26b8778a5817311279d61184fd999e01a16dce0241599085138c",
                0x8C09DF36u,
                0x8C09DF3Au,
                "sha256:d2bf60bba1f6644535ffa1938184020cb313bbec2455dd0c938238787e37fac9",
                0x8C0D6700u,
                0xB0u,
                "sha256:e0e279055f80257a25fcfbea1d871b76db8e6a6e00c3187bf6dcfe52df9f0a69"},
            // Complete object+0x60 publisher family used by the Level
            // Select -> Emerald Coast path. The 16-way initializer and its
            // adjacent update owner reference these eight immutable cells;
            // five unique callbacks result. The mutable field consumer stays
            // Candidate/RuntimeOnly for values outside this proven family.
            emerald_coast_object_callback_candidate(
                0x8C015C64u,
                "sha256:0b4695bfc16e82f8e7e737e6311b8801a4b98d5636f517156d017f7bf35f5797",
                0x8C016536u,
                0x168u,
                "sha256:bfe18142e14c6348c9eb8539b376bf2e0324d53259c7a891164916cc58580c35"),
            emerald_coast_object_callback_candidate(
                0x8C015C70u,
                "sha256:d29248abc2f7ffc5a0a0c01c09b714bab485a48d82ed072f0f36c6906b3d0bb5",
                0x8C016A70u,
                0x1F0u,
                "sha256:275073c2de9f1e9de3e11da40c533b0c6554ba287848b627d5c8bde10944a1dd"),
            emerald_coast_object_callback_candidate(
                0x8C015C84u,
                "sha256:d286ae57c3ddd2dd20ebdc9343ca48ba842f6ef5b5308eadf02ac0a1eddd5f71",
                0x8C0168A8u,
                0x1C8u,
                "sha256:864149df162624e2d47421df182b73e2185b2f65d4b252005fd10e4d7b720736"),
            emerald_coast_object_callback_candidate(
                0x8C015C8Cu,
                "sha256:9538e6a89a2353deb660fd23cdb847cba477caf99261bafe5e15e41b656e0ea9",
                0x8C016800u,
                0xA8u,
                "sha256:61d61a0df1740ef5eb13e77e5269f0277255bab65ee2e11bb96cb93790499e0e"),
            emerald_coast_object_callback_candidate(
                0x8C015E04u,
                "sha256:9538e6a89a2353deb660fd23cdb847cba477caf99261bafe5e15e41b656e0ea9",
                0x8C016800u,
                0xA8u,
                "sha256:61d61a0df1740ef5eb13e77e5269f0277255bab65ee2e11bb96cb93790499e0e"),
            emerald_coast_object_callback_candidate(
                0x8C015E0Cu,
                "sha256:0b4695bfc16e82f8e7e737e6311b8801a4b98d5636f517156d017f7bf35f5797",
                0x8C016536u,
                0x168u,
                "sha256:bfe18142e14c6348c9eb8539b376bf2e0324d53259c7a891164916cc58580c35"),
            emerald_coast_object_callback_candidate(
                0x8C015E14u,
                "sha256:10e09166c4bbf1f3bdeebc52dc34bd80656f93b6086ca2c53a94b58b414253e0",
                0x8C016754u,
                0x56u,
                "sha256:437edbe31c19bab18cbb0bae01142dfa48e0fa49c0266d27acf8da9cce5adcdf"),
            emerald_coast_object_callback_candidate(
                0x8C015E1Cu,
                "sha256:d29248abc2f7ffc5a0a0c01c09b714bab485a48d82ed072f0f36c6906b3d0bb5",
                0x8C016A70u,
                0x1F0u,
                "sha256:275073c2de9f1e9de3e11da40c533b0c6554ba287848b627d5c8bde10944a1dd")};
        std::vector<katana::runtime::GameProjectFunctionBoundary> functions;
        functions.reserve(
            declared_functions.size() +
            sonic_adventure::private_data::legacy_exact_reexport_boundaries.size() +
            sonic_adventure::private_data::native_graphics_exact_boundaries.size() +
            sonic_adventure::private_data::native_platform_exact_boundaries.size() +
            whole_game_strict_entries.size());
        functions.insert(
            functions.end(), declared_functions.begin(), declared_functions.end());
        functions.insert(
            functions.end(),
            sonic_adventure::private_data::legacy_exact_reexport_boundaries.begin(),
            sonic_adventure::private_data::legacy_exact_reexport_boundaries.end());
        // Historical transfer inventories identified two glyph owners by
        // their exact bytes but gave them obsolete TA-era symbols. Keep one
        // exact boundary for each and promote the semantic native-font names.
        functions.erase(
            std::remove_if(
                functions.begin(), functions.end(),
                [](const auto& boundary) {
                    return boundary.start == 0x8C640862u ||
                           boundary.start == 0x8C640D22u;
                }),
            functions.end());
        functions.insert(
            functions.end(),
            sonic_adventure::private_data::native_graphics_exact_boundaries.begin(),
            sonic_adventure::private_data::native_graphics_exact_boundaries.end());
        functions.insert(
            functions.end(),
            sonic_adventure::private_data::native_platform_exact_boundaries.begin(),
            sonic_adventure::private_data::native_platform_exact_boundaries.end());
        std::vector<std::string> whole_game_strict_symbol_names;
        whole_game_strict_symbol_names.reserve(whole_game_strict_entries.size());
        std::vector<std::uint32_t> whole_game_static_entries;
        whole_game_static_entries.reserve(whole_game_strict_entries.size());
        for (const auto& entry : whole_game_strict_entries) {
            std::ostringstream symbol;
            symbol << "sa_whole_game_static_entry_" << std::hex
                   << std::setw(8) << std::setfill('0') << entry.start;
            whole_game_strict_symbol_names.push_back(symbol.str());
            functions.push_back(
                {entry.start,
                 entry.size,
                 whole_game_strict_symbol_names.back()});
            whole_game_static_entries.push_back(entry.start);
        }
        std::sort(
            functions.begin(), functions.end(),
            [](const auto& left, const auto& right) {
                return left.start < right.start;
            });

        // Validate every resident boundary against the exact PAL boot before
        // serializing the GameProject. A transfer at end-2 owns the following
        // halfword as its architectural delay slot, so an exclusive end at
        // that halfword is never a valid function boundary.
        constexpr std::uint32_t boot_base = 0x8C010000u;
        constexpr std::size_t boot_size = 6'735'296u;
        constexpr std::string_view boot_identity =
            "b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af";
        std::ifstream boot_input(
            std::filesystem::path(argv[4]),
            std::ios::binary | std::ios::ate);
        if (!boot_input ||
            boot_input.tellg() != static_cast<std::streamoff>(boot_size))
            throw std::invalid_argument(
                "verified boot image has the wrong size");
        boot_input.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> boot_bytes(boot_size);
        boot_input.read(
            reinterpret_cast<char*>(boot_bytes.data()),
            static_cast<std::streamsize>(boot_bytes.size()));
        if (!boot_input ||
            katana::io::sha256_bytes(std::string_view(
                reinterpret_cast<const char*>(boot_bytes.data()),
                boot_bytes.size())) != boot_identity)
            throw std::invalid_argument(
                "verified boot image has the wrong identity");
        const auto boot_end =
            static_cast<std::uint64_t>(boot_base) + boot_bytes.size();
        for (const auto& boundary : functions) {
            const auto begin = static_cast<std::uint64_t>(boundary.start);
            const auto end = begin + boundary.size;
            if (begin < boot_base || end > boot_end)
                continue;
            if (boundary.size < 2u || (boundary.size & 1u) != 0u)
                throw std::invalid_argument(
                    "resident function boundary has an invalid size");
            const auto last_address = end - 2u;
            const auto offset =
                static_cast<std::size_t>(last_address - boot_base);
            const auto opcode = static_cast<std::uint16_t>(
                boot_bytes[offset] |
                (static_cast<std::uint16_t>(boot_bytes[offset + 1u]) << 8u));
            if (!katana::sh4::decode(opcode).has_delay_slot)
                continue;
            std::ostringstream message;
            message << "resident function boundary cuts delay slot: entry=0x"
                    << std::hex << std::uppercase << std::setw(8)
                    << std::setfill('0') << boundary.start
                    << " transfer=0x" << std::setw(8) << last_address
                    << " end=0x" << std::setw(8) << end;
            throw std::invalid_argument(message.str());
        }
        // The native post-bootstrap checkpoint binds the exact main-mode
        // dispatch instruction together with its signed-relative table.
        // Pairing this identity with the jump-table declaration lets Katana
        // use the complete table statically while the generated product
        // guards the same bounded range against later writes.
        constexpr std::array static_code_identities{
            katana::runtime::GameProjectCodeIdentity{
                sonic_native_private::frame_completion::table,
                sonic_native_private::frame_completion::table_end -
                    sonic_native_private::frame_completion::table,
                sonic_native_private::frame_completion::table_identity},
            // Exact singleton literals and bounded selector tables. Runtime-
            // only register callbacks in the same owners are not declared.
            // Signed-relative selector identities cover each independently
            // recognized bound/index/table-load producer through the BRAF
            // delay slot. Table payloads remain separately authenticated
            // unless an existing bounded owner identity already contains them.
            // Emerald Coast object+0x60 publishers, shared consumer and the
            // complete five-target callback family. Producer cells remain
            // separate identities because they live in two initializer
            // literal pools rather than executable callback owners.
            // Whole-game object callback producer cells. Each entry binds
            // one immutable resident pointer cell independently from the
            // callback boundary it publishes; equal payloads intentionally
            // retain distinct source identities.
            katana::runtime::GameProjectCodeIdentity{
                0x8C015B40u,
                0x4u,
                "sha256:674c92bf82e9c87b66ff0733c3f6c6965dff367c24028417ccad90fa76625ae6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C046D14u,
                0x4u,
                "sha256:1f78d588e0fb551b7da0091a619f79c0d6634522a104046a295458237299c8cf"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C05025Cu,
                0x4u,
                "sha256:01c88df6735b138b40b5dbe083c47ca59f3286436b9b9b5074c6bb914fa27f09"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C051B0Cu,
                0x4u,
                "sha256:42bd35d1dca26ebbc2e385e9dafe362165e6039042c5b8685549c7ed48eb3444"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0590BCu,
                0x4u,
                "sha256:d71abe990e7257e660a25101a81d2cd90b818a81ed9f3c1a9fe749bd86cb232c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C059234u,
                0x4u,
                "sha256:d71abe990e7257e660a25101a81d2cd90b818a81ed9f3c1a9fe749bd86cb232c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C059D24u,
                0x4u,
                "sha256:d7fad29fc2962338b462c907a429fdc6f4541649aadbdd38620f5394149e7dc9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C060058u,
                0x4u,
                "sha256:63160458d2e763456f9202c8c17d2ccaf635e8f2b23997e21fa5f3486ac7f29f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C060568u,
                0x4u,
                "sha256:5f98849f050628aa67f5ce339019e03d89dd99828e914d2fc8c5bd2cca04b251"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C060E48u,
                0x4u,
                "sha256:23fdc1970f5548b39800ff1d6c68c9589d4c21ac93a95c0a4bbad4c85d7f84e9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C061A7Cu,
                0x4u,
                "sha256:39ed5fad8610adfbbfe7450cdcd5818aff80f03952193bcce1d26346f6e58b44"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0626A8u,
                0x4u,
                "sha256:426c2c1b96d438d107ae8f8480b5be517f2f3e46518f2bc225d3c6bb2c297d7d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C065F00u,
                0x4u,
                "sha256:e417798894ca49a146d8f658c56b29bddfe1d4cfea41b2f09fe526fee55a146b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0687E4u,
                0x4u,
                "sha256:48bd09c2655adad8cfe1e1388e0e5ffdbeebc098d7fdd87d404d86994352d14b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C068A60u,
                0x4u,
                "sha256:48bd09c2655adad8cfe1e1388e0e5ffdbeebc098d7fdd87d404d86994352d14b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0687F0u,
                0x4u,
                "sha256:3810c7ee0faf3d58d2282d297d9d91a52326bf0a187727c9be80a46b7211ef0e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0A765Cu,
                0x4u,
                "sha256:e94c5fa8031a596aa458373633b79d933fdda7e56e46181983100d147559a34a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0B3A08u,
                0x4u,
                "sha256:c4089b195bb040dfa5c6384a63a3cdd5cb4d8b14a850dbb626a3e321a4ecb989"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0B48E0u,
                0x4u,
                "sha256:38c0e9f986eaa2954b4359ced054edab3da99a8b33f08d7b6544c2e2d2a22f46"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0B5640u,
                0x4u,
                "sha256:84f5d54f5c66a9f62193a6ba6d6fe8ef03fbeb68fb9acad2f82ce60c092780a8"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0B5E94u,
                0x4u,
                "sha256:697f25b4440049ca3b0d6561a265e32d2b8dc47b578f16e3701d4b029f003b04"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0B6044u,
                0x4u,
                "sha256:697f25b4440049ca3b0d6561a265e32d2b8dc47b578f16e3701d4b029f003b04"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C015C64u,
                0x4u,
                "sha256:0b4695bfc16e82f8e7e737e6311b8801a4b98d5636f517156d017f7bf35f5797"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C015C70u,
                0x4u,
                "sha256:d29248abc2f7ffc5a0a0c01c09b714bab485a48d82ed072f0f36c6906b3d0bb5"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C015C84u,
                0x4u,
                "sha256:d286ae57c3ddd2dd20ebdc9343ca48ba842f6ef5b5308eadf02ac0a1eddd5f71"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C015C8Cu,
                0x4u,
                "sha256:9538e6a89a2353deb660fd23cdb847cba477caf99261bafe5e15e41b656e0ea9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C015E04u,
                0x4u,
                "sha256:9538e6a89a2353deb660fd23cdb847cba477caf99261bafe5e15e41b656e0ea9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C015E0Cu,
                0x4u,
                "sha256:0b4695bfc16e82f8e7e737e6311b8801a4b98d5636f517156d017f7bf35f5797"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C015E14u,
                0x4u,
                "sha256:10e09166c4bbf1f3bdeebc52dc34bd80656f93b6086ca2c53a94b58b414253e0"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C015E1Cu,
                0x4u,
                "sha256:d29248abc2f7ffc5a0a0c01c09b714bab485a48d82ed072f0f36c6906b3d0bb5"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C016076u,
                0x1B4u,
                "sha256:f627cd08b0026d4a3e0f4b52bf21cc6d507bd53f837c279d848402714edebf8d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C01622Au,
                0x30Cu,
                "sha256:9f83ff5f57d5966913d1cb4079484d624d7f36eea8cd8b6379dc2273dc569d49"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C016536u,
                0x168u,
                "sha256:bfe18142e14c6348c9eb8539b376bf2e0324d53259c7a891164916cc58580c35"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C016754u,
                0x56u,
                "sha256:437edbe31c19bab18cbb0bae01142dfa48e0fa49c0266d27acf8da9cce5adcdf"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C016800u,
                0xA8u,
                "sha256:61d61a0df1740ef5eb13e77e5269f0277255bab65ee2e11bb96cb93790499e0e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0168A8u,
                0x1C8u,
                "sha256:864149df162624e2d47421df182b73e2185b2f65d4b252005fd10e4d7b720736"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C016A70u,
                0x1F0u,
                "sha256:275073c2de9f1e9de3e11da40c533b0c6554ba287848b627d5c8bde10944a1dd"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C015BB0u,
                0x16u,
                "sha256:7bfc124c27aa5d028b3bef094cfebb9ca7e959031377a912e439802ebeb053f7"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C015BCCu,
                0x20u,
                "sha256:c3fa598f1729942780c3050357bce0f5c89ef3b70371b60018fdb2b0df608fbe"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C01B5EAu,
                0x12u,
                "sha256:447055c1bd90cb2ad788bcfdc3580b79f5a6b33fb4c9c4ce4dd8e1836a9b4ff5"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C01B600u,
                0x2Cu,
                "sha256:53513d6b5825a8430845dc44bc97d811e2332b5e506de1f0ed4154f834b22862"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C02396Au,
                0x12u,
                "sha256:8ff7f2198a352480128aa8bd146e9229003604e15fe638bdf3ee3af63e840926"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C023980u,
                0x34u,
                "sha256:230a2aff1001853cedb3c394ff7f258d576cb0e1864bfabeb2aa1a9dd42d0d25"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04CA40u,
                0x7Cu,
                "sha256:550a560e5eab4deef40e9afb31f5303f8df55318ebab0702bf9d50ac77ca121e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04CC46u,
                0x6u,
                "sha256:0634819224b9dc43abb25a9ba5f1d76226fdbc47d7a4b4b80b257ba71b2222c3"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04CC74u,
                0x6u,
                "sha256:75bff0fad6a922d37a84e95dc150b91e733333652561febdebf331e4139ee286"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04CD24u,
                0x4u,
                "sha256:afea605ef19026ccdca35fc1768b3c8adb61b50c69677e5e5bf6807e427ce248"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04CD40u,
                0x4u,
                "sha256:36c39e58c78cd110840ea700c5a5495f694c2a509ae4737f102a351c714e0a1f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04D760u,
                0x76u,
                "sha256:2735c290def0369f4e83d608d60ee1762e7cb93243acf3f73f67204abf9468ea"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04DA42u,
                0x16u,
                "sha256:7f69ce65e6afaf24ad5a81ed74b54c531eb7ec9cac52857ae3cc78befc888d24"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04DA6Cu,
                0x2Eu,
                "sha256:c4cce007c30c9d92fb2c97b13b36ecac7e86ca3684e5ff13eaac5622a62591cc"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04E300u,
                0x70u,
                "sha256:55a46e4e1c5a25d8d0b48b8ae855146f27bab774f2d05985cc92af4cd7f72f44"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C053B82u,
                0x5Au,
                "sha256:a98d326fb5194562e2bfda1e3720fcfb559830a84158b97ba815a98360706bde"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0573A0u,
                0x126u,
                "sha256:5090209111e5a76e3bd6e0dfad3e0fadb640cd7012b4c0a510f16b0f6d042f2d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C05AC66u,
                0x80u,
                "sha256:3f1d212032d5effa3e0e29d6051f4c69273ac295f03a370d65ece0d9189d075b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C079438u,
                0x16u,
                "sha256:406b12cb2af920b5f95865745bba4b2a1ea8c42d5946b1f1d23479623f9fe424"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C079450u,
                0x1Eu,
                "sha256:22f7526d28fedb4e419f53e9dce9124a88b0f3dc46c12aae7a863f670ca617e8"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C07EEAEu,
                0x1Au,
                "sha256:3b2c5879528fcb64867be79f5b9ba212a4197c2bf9932ed0f95dc92a5d53cdbe"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0827DAu,
                0x16u,
                "sha256:4b7089aaa593e08ed36c9c9db9fea9619c344378f59123f622390b7f55d4e8a5"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0827F4u,
                0x44u,
                "sha256:ee2ad00650c2b7e2b0c48eaf7e5554d4fc6dfaa7c120d164fb73275e2e6f3a67"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C085990u,
                0x6Au,
                "sha256:b6f5a4affeb1ece3f2cb01975a73dcb01d1207901c70afc40c5aef6c25af113a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0859FAu,
                0x04u,
                "sha256:09fb6118d57a198bfbc1f134c74bfdefdd5f1b08a7ba250e6c3feb89cb97aa6c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0859FEu,
                0x06u,
                "sha256:38344bd5e39a86dffa044f2603f1dee83ff7ccf2efd761ad112ba5ce305713c0"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C085A04u,
                0x06u,
                "sha256:89111bf409a5af2ff6f61962fb6d3ccf6a8d24929eef9cc9eb3d92c99202e839"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C085A0Au,
                0x06u,
                "sha256:09924ac9e9bd11f53fc8a101ebadf1a669178afeb67920a7079aa2c1476b7069"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C085A10u,
                0x08u,
                "sha256:24e01775e1d7ecc447651530d2bc3b74500b3298403e24dcaf5b380f309ac1a4"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C085A18u,
                0x06u,
                "sha256:de294e42e5c82d39db9812c16a3bcf16bcd0e7839a5f451c85d1da1c91d1bd79"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C085A1Eu,
                0x08u,
                "sha256:02cc6752dfa6ad6abde55508564e1acbadfd86f476913aaef38d7c2c9af78105"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C085A26u,
                0x04u,
                "sha256:5e363c73803475194947549ea77f436251430bb7ba39e47133fea00a9e51e8a8"},
            // Eight signed-relative dispatch entries authenticated directly
            // from the verified PAL boot image.  Keeping the table in its own
            // immutable identity range lets the generic CFG projection prove
            // all targets without widening any callable owner boundary.
            katana::runtime::GameProjectCodeIdentity{
                0x8C085A38u,
                0x10u,
                "sha256:19046dce79ff46c4d100e0ff10645e33b6c7216b1f6002e88b927c8d629808cb"},
            // The 6-byte Emerald resident tail thunk at 0x8C087DC0 loads this
            // exact immutable PAL-image word and jumps to the already mapped
            // 0x8C098DA0 owner. Keeping the literal in a separate identity
            // range proves the PC-relative target without widening either
            // callable boundary.
            katana::runtime::GameProjectCodeIdentity{
                0x8C087F00u,
                0x4u,
                "sha256:98c18dc48de0973384cfff485f83dc936b4fc89383152599c29115de0865dc56"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C08C096u,
                0x16u,
                "sha256:c8d722c46980c7c121f449fe73d54e3e866df3410622607cc96a609d72061c29"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C08C0BCu,
                0x2Au,
                "sha256:c472be2d257bc4189d754c67d0c0a16f50d32111ede4e8b87e38d5cba61b90df"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C08C9D4u,
                0x16u,
                "sha256:ee70b58499afbb9b5c4895cb0f353712bdc5588e4a9caee65d2943a5e2844e92"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C08C9F4u,
                0x2Au,
                "sha256:cc2b1b34a6714dda89e86294e34864a19641b76fb7b119e865405c2a78e84649"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C08D166u,
                0x16u,
                "sha256:9c521daf7bb2ab76976fd8d1393c580096f03321da0eec78f50d6288a113be4a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C08D190u,
                0x2Au,
                "sha256:3641140badf11e6dad1c181a8ddf6dd8761124ca621b5f75afd61787f6542ec2"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C08E90Eu,
                0x16u,
                "sha256:f042b20bd74da5bf1edacd3835984c4994bd6a0916f0e1ab3bf54bc0a86fbb28"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C08E928u,
                0x2Au,
                "sha256:2acc615fd076cde7d6e1285155e4ce426496073234b052c0cdaa8ee85800e174"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0933A6u,
                0x1Cu,
                "sha256:0b3abfdc508cafc7d25fc7d9f09299e4e26cf6ba5a995ff8188c06e628b8ac46"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0933CCu,
                0x56u,
                "sha256:96b74fbb6484de6f5e92c972ddbb45fe3c32c22ddab4c69c8e5f4ec7ae8cedfb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09846Eu,
                0xBEu,
                "sha256:27a85e87250e192be2dae32e2b24d10d93e38e70d14ff807c29d806ee86eca51"},
            // Immutable initializer literals for Candidate callback roots.
            // The complete consumer owners are bound separately; source
            // block/callsite subranges are checked below because
            // CodeIdentity ranges are intentionally non-overlapping.
            katana::runtime::GameProjectCodeIdentity{
                0x8C035990u,
                0x4u,
                "sha256:a1c78876ee1c62989cb4cce62b60ff293c2e3530bba979bf5715c76d88ab235a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C05FE84u,
                0x4u,
                "sha256:eb944e0083f5a1ffadb5d50c10e951d71e793593025251846cda0d854b4c3deb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C086A5Cu,
                0x4u,
                "sha256:e14b1098b9dda260e4419fe83a31eb9922b53913010a46f6391da75dc236d2f6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C08715Cu,
                0x4u,
                "sha256:e14b1098b9dda260e4419fe83a31eb9922b53913010a46f6391da75dc236d2f6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09859Cu,
                0x12Au,
                "sha256:cb3b524231e8db24fb270a969c2692ef642bbd021d5a7a474d6099ad89bd0d1d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C1B9B80u,
                0x4u,
                "sha256:4d3b93e55574979c0876f008e68f95a5dfb591e51806fdfc3489a659f7566ed6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C1B9B90u,
                0x4u,
                "sha256:4d3b93e55574979c0876f008e68f95a5dfb591e51806fdfc3489a659f7566ed6"},
            // Complete resident node/object-list callback constructor
            // inventory: each word is an immutable r5 argument consumed by
            // the exact 0x8C09DD3C node+4 publisher.
            katana::runtime::GameProjectCodeIdentity{
                0x8C04466Cu,
                0x4u,
                "sha256:9e3399b219b731dab265f4db402b64a199a60138e1db5b1d16437d26a253bd1f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0454ACu,
                0x4u,
                "sha256:1321235f79fc9efb6f0a4ff04da4956c53790795dbdd77b803afd816c6ea5412"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C045684u,
                0x4u,
                "sha256:647b8db4f051b40c6e5f27b681f896822885300cc796666cbc525a3777fdab17"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C045B10u,
                0x4u,
                "sha256:ea1f6b29dc2470cbbf18bcc28ec986a657429615a83b863614f851942396ac23"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C045D18u,
                0x4u,
                "sha256:cd00647db611f46a14e6344a864b6dd21390c277f6d571afb97be3b65f4e9e88"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0467A4u,
                0x4u,
                "sha256:177e885064877f59e31166604d8907be7d595d1d9c705c4d72e7c187ff2910fb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C046B38u,
                0x4u,
                "sha256:dc9d9fe543a4f57cfaf8e4b1b6e2887a0a3ebd8d07c16505e7f053c5a716e015"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C048598u,
                0x4u,
                "sha256:38e488bf7f6ba84bcfdb557a79837d350e0a0fca7af8e4d785a9ee1693fb3f7f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C06466Cu,
                0x4u,
                "sha256:177e885064877f59e31166604d8907be7d595d1d9c705c4d72e7c187ff2910fb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C07B684u,
                0x4u,
                "sha256:38e488bf7f6ba84bcfdb557a79837d350e0a0fca7af8e4d785a9ee1693fb3f7f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09E448u,
                0x4u,
                "sha256:a4c3641a9c446f900627abc71e1d748790a5784e710b4e584f711221094da54f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D6868u,
                0x4u,
                "sha256:7464756d432742ede6ca399c5509137144b22108e7a0063b2b6ce1c7ba272c52"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0986CCu,
                0x42u,
                "sha256:4242b231a0e1aa64ff8546e3bd85ff70b31f46616338ab68fb5c1bc981b8ecde"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C098720u,
                0x4u,
                "sha256:ea6950e5525700240abb3b20ec3c0a79c6c3e396cd7106cd95078ba3a2701783"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09872Cu,
                0x4u,
                "sha256:8a672a755de7f7d430f14fb8baaaed9a7e3c708c0b6aa8c6b1dca8139c077e7a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0B22ACu,
                0xAu,
                "sha256:54081c542de8b158464442f57697bcad8559435fc4b969d3b0a7b34c4636df57"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0B27D0u,
                0x5Eu,
                "sha256:34f65e47552aa84611b3421b6981a32ddfcdad9244684479a9afa839f5fe3af8"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0BE0A0u,
                0x6Eu,
                "sha256:877d3e7301c6969387ff10138c0ecff6044d4172718205b58680f86212bf7dd6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0BF334u,
                0xEu,
                "sha256:ba793462a66beba8d5ef756ff5ef5da3285b2144701bfa29216b676c1bcdebc2"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0C719Eu,
                0x10u,
                "sha256:fce9f07a7b4568b287c407f53a6f49035a14d00d13c85c56d991dbcc6b24ffa3"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0C74ECu,
                0x4u,
                "sha256:551572a9f87a0af199dced20f1d78fc9ddb31537c3a7ce5b747bfedb295ab1e9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0DD0A6u,
                0x8u,
                "sha256:7cffab79f6f4918e3da259fe904defcd6339acc0fd5b7f89eba90c1577adb252"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0DD3C8u,
                0x8u,
                "sha256:9b5ce78e59aab47fbf72bdfd0138e10d023eddbf436f414630709f48d6362dab"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0E5934u,
                0xAu,
                "sha256:16673f002cef27cf8d6fa97ab4df59837fd67972ddd831226cb3355636de0442"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C1077A4u,
                0xC6u,
                "sha256:10d90d2fc85eecf3fbecad799c24fca41358095f773bb0d7117b30fcc1d2c9d8"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10786Au,
                0x136u,
                "sha256:0af06a04308756288766797b5599d360d0fd288082096ed78290d8ad5a6ba5d5"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C1079A0u,
                0x4Au,
                "sha256:daece50b44511c9dd931cd2cf0ab8cd025b4ce17ae05084fa832bd7b66653f77"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C107AA0u,
                0x24u,
                "sha256:a8f65a7931420c3b98613bb047da1a7a9464601c193740899ad8320059eb2f41"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C107CC0u,
                0x2Au,
                "sha256:d9f0aab05de07e1664aeae45adbc3f4d33be4c0ced868baa0a36cc8fa7823b3d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C107D40u,
                0x1Cu,
                "sha256:97296b5cf189837d93c6ba7005979c37d864c2a0f5a58ff4ce49aeede320f61b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09A39Eu,
                0xCu,
                "sha256:72f61de9817dbf99d9c2fd5157d90d222459a49ffde3bf795ebe65bfb76fdac8"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09A3E8u,
                0x8u,
                "sha256:72f92be95a57c05466675afc556ec09a407847c41506b3a3fe469a22c6f8d2a2"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09A406u,
                0x6u,
                "sha256:ab0c802abad759991815f0265b8a16cacd54855f861293537967dfd38492fd9f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09A422u,
                0x54u,
                "sha256:5f6b169971177d4d56d33c2d4e56df95bab0412296de725da4969fec1a5d497b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09CE36u,
                0x16u,
                "sha256:511b1f65d91871ebf9907575c863298918be2623505b896126131dd3e21ed22d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09CE68u,
                0x56u,
                "sha256:cc52fbe2757eb90694337df81debda87e164efc3794eebaba6eecb7a6e1b87ae"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0A787Au,
                0x18u,
                "sha256:ec0e9a36523ab77712f9a00b1b3c4041ae2f1150f252d5dfc11d6027801ae8e9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0A7898u,
                0xA8u,
                "sha256:96ed539b594ca15cf452c58bea5c69890e4f50ad21b043a015fec0cdd1d0c90e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0A96BEu,
                0xC4u,
                "sha256:b21d6c6cee479eae0d188ab26d4dab78a5270095ea39cd8421350c35bfbf39ad"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0AFEE6u,
                0x16u,
                "sha256:5f4da4a657096bd7eae67d688d4c83e5f59f4c21749f8053ab3484c6ca7205a6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0AFF00u,
                0x64u,
                "sha256:cc676c3ad390fe892fe2e0e1a81fb670f18cbf4d7c2d08a2db5dac5d486cc1ad"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0B0724u,
                0x6B0u,
                "sha256:82bc1eddfc204f3ea4fcd2142a1a604f59650678793c1817bb80b3079b0d249d"},
            // These bounded owner ranges are byte-identical in the verified
            // boot image and committed post-bootstrap checkpoint. They are
            // byte proofs, never complete-disassembly claims.
            katana::runtime::GameProjectCodeIdentity{
                0x8C0B6088u,
                0xFA0u,
                "sha256:7b1e5c3b5addf74d3de2043476b652bd127c8cd6f74877c1de7d3c05045247cc"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0B717Eu,
                0x18u,
                "sha256:b1cf1c49cc3fc412067e5320ed979c08eafdf4e99287991df33a27e0798c02f5"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0B719Cu,
                0x7Cu,
                "sha256:9f17177dd37e476dde96bc42270549944137a2f91e1ccf950e56652d8a9f03a8"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0BA670u,
                0x8u,
                "sha256:bbe05ef245032eb0ff8c9bccd8af32a1e7606d43a6945913952fb5bc08b56ced"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0BBDAAu,
                0x16u,
                "sha256:a96d4a17f794df3d61128e83cabe0cc275481044af68bd3d7ed0bfe7b410292c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0BBDC4u,
                0x64u,
                "sha256:efc63c1b69301a4b7184ae2669e53c2596a4b266c07327164f8eb5b8002755de"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0BC5C6u,
                0x16u,
                "sha256:7cf98f99162ee641cc83431007d371b4e07c34f2f07d5eb00332b24a8bda6d0e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0BC5E0u,
                0x64u,
                "sha256:f0ae1ba1c91dc4885734f75327c7d8608223e068e107cc454f55746c6dd310b3"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0C0580u,
                0x12FAu,
                "sha256:1d92b06ba7178ff41787660ddf7f460ffefb092020a86f103c41d3401cff99ad"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0C19AAu,
                0x18u,
                "sha256:461e9e58d87dbbcdcac45b2903f9125e673e4456517fc5b8595c9da577367050"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0C19C8u,
                0x6Au,
                "sha256:c3f7487a98b3acb4fcaf43ea47b8f7a6e82dabdcf488ebb36d3383afa10b9d4e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0C443Cu,
                0xEu,
                "sha256:2240ff5823a068b297493349b4143efd051b2b9b8690fce8e064b05af9c4db18"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0C5B84u,
                0x16u,
                "sha256:458756ac275a04ae9818820a8a2959cc60e6e3987e6f406c5538a9f80e42b7c4"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0C5B9Cu,
                0x64u,
                "sha256:31ec530fddf5d79550fc85dc7971ebbc1573ad789e05feb909de96b01c0552ec"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0C61AAu,
                0x16u,
                "sha256:d49d403a7d56438ff5ba0a5461d9f3ec14d6bc035a41e3ba4431add8f8c77006"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0C61C0u,
                0x64u,
                "sha256:50dbbe05eda6c3f1fd3f6862995a570f77187b6558dd55e0eedb88a9c658db88"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0CBD40u,
                0x129Cu,
                "sha256:6aeb221104acfc7f4bbbe12d28698472bec199478e733fb1b29a5ad18fe6566c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0CD198u,
                0x18u,
                "sha256:834923e8dd4eba018bbf8068dcf0482884e8b3be60df3c358b9f528df8073a00"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0CD1B4u,
                0x6Eu,
                "sha256:a6548aa4d24a674eb8aeeae9e9362a64dc6c07995c36fc5c0bbb3189a1cbf009"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0CF9A6u,
                0x8u,
                "sha256:e36c68972c82bc823e49c11182d886aeec6a667a322fb7b0702bdf0be9c17a5e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0CF9AEu,
                0x60u,
                "sha256:faec3d80bb33aa1a9aa8a5baeb3050b9a01a8f66e50fc9c2987f901509a79198"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D0764u,
                0x16u,
                "sha256:16a5e8668c5ab1cddaf5ffc302391d44b57cc73515f01c3350d1ed6fc8f17371"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D077Cu,
                0x64u,
                "sha256:4f05b3379e34e19dd06b848ad1ebfe6b016b28ac776d76922ce0653e10800b06"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D0AC4u,
                0x16u,
                "sha256:8732e8fe87f008e70b0ff1fa9145509064941f39bad61a96dec48995f29cd670"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D0ADCu,
                0x64u,
                "sha256:98a651930ed5d662290ba9f16863c406260c661e4cf36561f8eeff7183610468"},
            // Two large state owners use bounded signed-relative selectors
            // followed by independent absolute literal calls.
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D6B34u,
                0x7Cu,
                "sha256:b128e1d8199c3559eb361ddab09a6eb96292114e6452c516b13c37c0c93dab68"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D6BB8u,
                0x54u,
                "sha256:d983c0ca1225ee367670fb1052d43947983ba0076dc2645b5f0f579493b5953b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D6DC0u,
                0x14u,
                "sha256:38a3c7bd40c00e312fb25131b7149ee78997fd48d87d3c7563cb76166c260cad"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0DB6E2u,
                0x16u,
                "sha256:caf6dd848897031b2fc38c87d966a36960c36e665a1e94bdc043d041d2e2ef5f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0DB6F8u,
                0x64u,
                "sha256:48e386df0407eb4627465d6cba3a4f297e8ef13166fd4d986cf99f2d3fd160ba"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0DBBC4u,
                0x16u,
                "sha256:3a99f2685c027da055ba9f35a659494382300996fec393b0bf6da83bb502a13c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0DBBDCu,
                0x64u,
                "sha256:3fbf1b618fb8388c4330bc14dec9586693215d1e6d165f15ea923b766ffe1a7e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0DFF58u,
                0x20u,
                "sha256:f1f2336331f3eea8a154b50850c3592a0b4f16712fbeb90ad775f60e5ce55511"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0E0040u,
                0x8u,
                "sha256:bde4edae47975099ea407a705943a048e2954ee94ca659db2cd755b259ea67a9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0E0082u,
                0x84u,
                "sha256:0f46878085b4afc78526c6423614c5d9df8eca187f7611056d265f6d2e13d09a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0E010Eu,
                0xAu,
                "sha256:e685a34b0b27f139f398c114a6a5e278aebde2103a7d6fcb61cc38539218f52d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0E0280u,
                0x4u,
                "sha256:32df88441c1320d12680b00b03b0292a270d9f4be36450027e151fecbd659a9f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0E4372u,
                0x45Au,
                "sha256:91732291997a91b9b052f6bf5419730bb82ac2eebc9805806ba8220cbe6729af"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0E4828u,
                0x16u,
                "sha256:cf3beec42fc9fe6307c7aa7dd2a33cc3da354fefeccd1d58a4de06fa6f1df5af"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0E4840u,
                0x64u,
                "sha256:064d448aaffee530f209b85b21ff547dcbb2a86bcd8831cc052dbc54b6779d02"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0ED818u,
                0x48u,
                "sha256:7a3ca2f6a4a63d2b55a857e4d1e8c83aa7df0d864d76ae88eaf1313e4e0598d2"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0ED862u,
                0x8u,
                "sha256:b099895840f9d051b871ab169f50610b6449834504729d10b4e4ca22e2919adc"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0ED86Au,
                0x3Eu,
                "sha256:9e76a5bcb4cefc658c22ca48c621084440bfc125e77e37e7ddf2967e0af7c8eb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0ED9E0u,
                0x14u,
                "sha256:c5ef02b091f0bee19df88ccec6e281ec0143d221be31df557bedf2d45f44de52"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0EDD1Eu,
                0x16u,
                "sha256:ae9241d5cadd6f02d42af805c7772ab1eb59c864d1c94ff29e733a406c564c15"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0EDD34u,
                0x22u,
                "sha256:309177baa13f732bcaaca0309d1fb59b537ee3c6cde6d2b6a319f752a6af429c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FDC94u,
                0x16u,
                "sha256:c743a0605027697d934f659903b77523e7069a4244feb8d9abb3f3828e2f2d03"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FDCB4u,
                0x1Au,
                "sha256:03f2168153801702811c0a85f28a5419238c9d65f2074534596fa4ea29c56241"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10963Au,
                0x5Cu,
                "sha256:eff8dd6ddb33878a26d4972a666d98a6063b82cba0ef0de616f222040397c91e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10CC78u,
                0x18u,
                "sha256:79ef66e08a23cb59ad53c3017110e1bd0c3fd1814ad7d10c816e3d7e782e90ed"},
            // Shared immutable PC literal used by both 17-way dispatch
            // producers at 0x8C10CC80 and 0x8C10CC8C. Keep it separate from
            // the executable owner so the proof does not widen ownership
            // across the intervening padding/alignment interval.
            katana::runtime::GameProjectCodeIdentity{
                0x8C10CC90u,
                0x4u,
                "sha256:ed155232fce2fde2739f7a250ff90e8116b87efba4d9e53f61fa913f39b0bd1b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10CC96u,
                0x42u,
                "sha256:dbbd04c0f4517e4e32fa83a51031eba9e1089ba6652c456540a8e0c7eb063749"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10CCD8u,
                0x44u,
                "sha256:1160cbfbcf5a985795027ba614fcb2a5ebccd460ac62b006fd2372637579cd84"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10CD24u,
                0x4u,
                "sha256:64a3e2a608757d96c8d44199e92c4611678104262265ba491455414ecbae5ecd"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10CD28u,
                0x4u,
                "sha256:079ec93fd4dca7dc6b2f4d463d8d4fd4cc37eac119c490f391853cda19fc42c6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10CD2Cu,
                0x4u,
                "sha256:ea7f62e33734c992bd9b4b4abaaa489617e01d408b55a2df995f6946e37be20a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10CD78u,
                0x44u,
                "sha256:542124931db202ceda7a1d5fb17f8d7edffcd90e3f7626fe68aec8119e3018f6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C60155Au,
                0xEu,
                "sha256:72c82e4973e783c38f413b1d0927c5b52664aca58f8d0dd394432ef2036990fe"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C601568u,
                0x4u,
                "sha256:97b2723ee1ab05666fb86f399cab5bb6e1430e5bb9c040fa2a785911cf296b35"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C60175Cu,
                0x4u,
                "sha256:98a7f8dd7b3d6ff56df0379652ce12357d71f0c4fe2526850acb34c361ce7d5f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C60556Eu,
                0x12u,
                "sha256:c0b9636a901da4936cdf271a5286a405b98c536a9566c8262e20aab7e2440392"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C605580u,
                0x18u,
                "sha256:0b81c80ba65668a17f5f0db43acabe2eadcf2920892329938358140047690b5d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C60808Au,
                0x8u,
                "sha256:b2e3b144c5f3351f06f7fec2aad663514abab95fd16135419e8b4ce3b8bf1945"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C608092u,
                0x26u,
                "sha256:ef2dad4114c5b22491dbb4ad692edda4d6a62128a5bf41f6b1a19d2e275a93f7"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C608108u,
                0x20u,
                "sha256:88e76e1139baa6f7fb239d200f5254da060b00e638c30bdd1c6b61ccdb4274ec"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C63A944u,
                0x70u,
                "sha256:19f9e824fec6bff799477efbecfc9e196680a405ba93c6b0013d6b11cd6e6581"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C63AEE4u,
                0x10u,
                "sha256:6ce761dcf06e4735b8d307ee004fdfb68eff47a10fb92616e76dbcf9441fa40b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C63AEF4u,
                0x22u,
                "sha256:e9cf94c6c372694146ef4c2e5199e63c0a9e7e30df8b0dddc10af37033e887d8"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C64C33Eu,
                0x6u,
                "sha256:121ef449175d429a4667eedabf784ea3d55854a1bf79abe95d2afca592715460"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C64C344u,
                0x60u,
                "sha256:03ecd7bc60c6d3c2b0cffd7950e57c05821fe2c590d8370cd2338a8206666ad0"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C64C3DCu,
                16u,
                "sha256:4ec6461d0433878b8f447715c596cbe219bbacef8e4518b5db59552e1cb358ed"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C654EF2u,
                0x12u,
                "sha256:64efcf0137a71110db75ebd26ccc2aa90cd731027c2cc88873be51fa6b035cac"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C654F0Cu,
                0x18u,
                "sha256:c20db7d2aa53132dd9850d6127707275d5ab17d4e1410bd273071b6c148c6663"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C654FD4u,
                0x12u,
                "sha256:446f6f0228f25ac7b42fd562875833106fbe6e1c44f0ea0ccebaed2f8d37970f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C654FE8u,
                0x18u,
                "sha256:5dfa80aef959cbe6f8851fc8305999544d2b01cf63ab8567b61ec6f390682076"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6658CAu,
                0x6u,
                "sha256:fcd2026e747d9debb0e55c2ab05fa9296c96d39f77593399218574747234de20"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6658D0u,
                0x68u,
                "sha256:a9e797beadc37df6ff4a8470f86e409537422dbbd7c92eb91ff312bbf0d70ccf"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C66C400u,
                0x4u,
                "sha256:360de871e167e67775953a179925bee0b189e947f605a06c295b68ebf9bb6eec"}};
        // Run69 proved these direct calls from complete PC-relative producer
        // slices. Bind both each producer-through-delay-slot range and each
        // distinct literal cell; shared cells remain one identity range.
        constexpr std::array run69_direct_literal_code_identities{
            katana::runtime::GameProjectCodeIdentity{
                0x8C0101FCu,
                0x1Au,
                "sha256:b5ac74f1282d6257124251b66e22574c541fd3e586b502d26d44ceed3f002a30"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0102E0u,
                0x4u,
                "sha256:334641334965ba8fd51e741794fcde062c4e026fcf7a80d29ed9bc8e42512d46"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0114A0u,
                0x8u,
                "sha256:2cc0e51987c4f8b4ad6f9a1e1b1c533a2f31eb82abe644de43417fcde2b9a2f0"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C011538u,
                0x8u,
                "sha256:bdbcf2903a9d5ce95b44b7da322b69dd72bd4a2b60223b56e3d60e2a85b27424"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C011564u,
                0x4u,
                "sha256:abe613d765dd413d264592f0a9faf6324625592e8e7e385a79c3c91cfc34ad35"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C011AA8u,
                0x6u,
                "sha256:52fa1f49ddbb05604ee4b3acb3bc67c5511f889b142ce7ddae7ec5d6924cfc47"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C011BC0u,
                0x4u,
                "sha256:abe613d765dd413d264592f0a9faf6324625592e8e7e385a79c3c91cfc34ad35"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04079Cu,
                0x8u,
                "sha256:0b8e5db913bce6232ea307a1d120727895158eb2f5f4799976408f3257057cb7"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0407CEu,
                0xCu,
                "sha256:986e1a44dd00e13e0811b620d1c24c338e2affc535bd4758640a995c445ad581"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C040860u,
                0x4u,
                "sha256:44deba360f14f5e99d4a7405cf424c0d4212c4f57aba0c3940808dfb74a6cd3f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04086Cu,
                0x4u,
                "sha256:8b054672942a9c50c8cbb9728584d54868f8847b7954f11d4eaed1f88abf8044"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C040898u,
                0x8u,
                "sha256:e3746720a0a944f3054a60e9ea46b48bd5cc4c97ff4a5c84708d0b3143d18de8"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0408CCu,
                0xEu,
                "sha256:6a92ee3fa28fa7de740ac2a6e44863cbb08269639a9e9c28bdfa712f173c9c2f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C040988u,
                0x4u,
                "sha256:44deba360f14f5e99d4a7405cf424c0d4212c4f57aba0c3940808dfb74a6cd3f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C040998u,
                0x4u,
                "sha256:8b054672942a9c50c8cbb9728584d54868f8847b7954f11d4eaed1f88abf8044"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C053AD0u,
                0x8u,
                "sha256:4799758dca88131edeafa4937e4f5571d8d6620aa84d30ce595b0e69f97ebda0"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C053B28u,
                0x4u,
                "sha256:abe613d765dd413d264592f0a9faf6324625592e8e7e385a79c3c91cfc34ad35"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C067E80u,
                0x10u,
                "sha256:c6dde49c7b46c6282fd6418d555fe6192c1ea542aedc0fba875af762fe3f2567"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C067FA4u,
                0x4u,
                "sha256:3722c5fd352cf069404b29c4b001a0a07915012287ca00ba75f2dbfc4d0a974a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C06AB2Au,
                0x3Au,
                "sha256:1a0b60d998e766a3ef080afaa75c288291e19359afda796a96307b574e369012"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C06ADACu,
                0x4u,
                "sha256:76bf2b1ec8b6c9d1597f463b1d08a3f212893eac9153977f6f71504f788dce89"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C08F10Au,
                0x1Au,
                "sha256:56f75f92fe0063ede48a5398b6059226a444dd49e9362f781a7dd8c3dc395ba7"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C08F250u,
                0x4u,
                "sha256:746216ef1cfc19ee57c973518e88f2c4ede904322201cabe8e82eab636bb77cb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C099852u,
                0x1Cu,
                "sha256:29c1ab8b9eb32c86ca49b0247742bed767b050929bd1a37f6b3661720aad8d1c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C099908u,
                0x4u,
                "sha256:b9e45b59cde25e5228a769b46df22fac2bebefecc4a9c035ac64a2690178936b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0CA6E6u,
                0xCu,
                "sha256:7c75d39ba0f8140cb4c262a6566fba663d383db8aa2771fe7c8f4a4abca9dcfc"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0CA940u,
                0x4u,
                "sha256:1e22c48a3345b30e05bc2f1a19516491400ab45c8452f05ecb6d90e5a53cd59f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0CADCAu,
                0xEu,
                "sha256:84694aeec51aaccb2d3a28eff30a0cb141528848c2c94d0e562abc0bbf5a2a3b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0CB0C0u,
                0x4u,
                "sha256:1e22c48a3345b30e05bc2f1a19516491400ab45c8452f05ecb6d90e5a53cd59f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10EAF4u,
                0x8u,
                "sha256:e64075bd1b85ce4738b985f2b8b58b48d8e9c0ec36abaa6fc817aed7ed93730f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10EB24u,
                0x4u,
                "sha256:bfcad19a8671387fa3726ca2d356703f103f65ef38987382cef4e58bf3e7760b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C620B96u,
                0x8u,
                "sha256:a493b968e39ca16f2dbb2ec408459e097e5665ceab694bf27a9c0d30a65eee65"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C620BE0u,
                0x6u,
                "sha256:74a42170b9519feb39eb66cf8c855d31adfd30c6a578cabe0ef3fb8d2368401a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C620C20u,
                0x4u,
                "sha256:1ea314714c5e28119cf026a99ebec2c4c4e1db8308c56db6a99a7242d7c07ed0"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C620C38u,
                0x4u,
                "sha256:3a90fd60f10d485b69a3d0f2576de3038d5695bc24d90ab01a41dd43bd9d6680"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C63874Cu,
                0x6u,
                "sha256:06a83f4b9151ba4d0f2a917f0ec58c82c8e9ca59e83e0bcb4a24818f0deba577"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6387AAu,
                0x6u,
                "sha256:92471e32c8cf79f2f016899ef253d77d0f1391da3b02d911d3ef10d1d0fd44f1"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C638808u,
                0x4u,
                "sha256:61afdd07ad556662b8b9fa2a7e842acd53882ff3e0c8f03f1976da39c0c2d268"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C638820u,
                0x4u,
                "sha256:3a90fd60f10d485b69a3d0f2576de3038d5695bc24d90ab01a41dd43bd9d6680"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C657706u,
                0x16u,
                "sha256:37a2a323c448ea74739d93824938c429778901cf5296efc4c9bdbaedf4b51f09"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C65774Cu,
                0x4u,
                "sha256:2657e1b1cd8e33ce5baa5a46482f4f010765575d1671585c8e2e4b405bc024b2"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C66B552u,
                0x16u,
                "sha256:395e14a31e398540743f0f9d7ebafcb23f07aa8d632ed2a3adb10b4f8373364c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C66B5A0u,
                0x4u,
                "sha256:1326c3729dd77d16e28cae0b32c38cd8c560a99bd84d6897600bb3e16348a99d"}};
        constexpr std::array run69_followup_direct_literal_code_identities{
            katana::runtime::GameProjectCodeIdentity{
                0x8C0109F6u,
                0x6u,
                "sha256:b78c1516f8ed62de66bc93af0cc5699d7ca5fd0e0d6266fa1c2bf9cb23b73e2b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C010A2Au,
                0x6u,
                "sha256:1f321dfe98cc74fa24ea5bf5376978da9915918d183846939d0a78a3a56b4dbb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C010ACCu,
                0x4u,
                "sha256:c2d0066168110068d8c489ed7587d3a6a3dca00b4d3c211cee3c067045c00f54"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C010B3Eu,
                0x8u,
                "sha256:5c87fc19f4d720e898439a6d82861d8cb8ad89e167152416151b7290b0863b73"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C010C24u,
                0x4u,
                "sha256:abe613d765dd413d264592f0a9faf6324625592e8e7e385a79c3c91cfc34ad35"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C011E68u,
                0x6u,
                "sha256:0bba964722a44897cb47152421b16872b07c6d26a292d49ddcaa98094d15660b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C011F48u,
                0x4u,
                "sha256:abe613d765dd413d264592f0a9faf6324625592e8e7e385a79c3c91cfc34ad35"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04D88Au,
                0x6u,
                "sha256:b5cbe9d145924845d5cbcde1c08cc1418f31096aa0cc5aa178cb0c7e5ff8ed89"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04D918u,
                0x4u,
                "sha256:d7093b0f9861a20fd5b4b5db70a91ca319d09cf5f5e25b3cb6129d2dabf91c3c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C062ABAu,
                0x10u,
                "sha256:301d3650d0c6e3f932aea63c68f3a9861f69de600259277896e17ff9935501ca"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C062C20u,
                0x4u,
                "sha256:89d704ed7d43e348c96d93168944968bd66318fbee9106a657496cca2a10257b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C065FE8u,
                0x16u,
                "sha256:59f1b3f00ef574cc5d2a3d57af5393f5ef5b152fe4f91d7af9e82445d46102e6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C066100u,
                0x4u,
                "sha256:3722c5fd352cf069404b29c4b001a0a07915012287ca00ba75f2dbfc4d0a974a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C068D02u,
                0x22u,
                "sha256:3febd0bc90a3a5b0aad79f4257fb7086e8e27ea776e80cdd7b0981c8c37af883"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C068D86u,
                0x12u,
                "sha256:82ead8cace47aed396d9490c880cc1c3c84bfec8741e0963bf06555a079c8ab1"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C068E00u,
                0x4u,
                "sha256:efef064682e1e38fae0861d2b93c28c0112d3ff6f0da4651267ed67e54259010"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C068E14u,
                0x4u,
                "sha256:e21493859d6a3e635290a15cb1e59ff5e4d3ddb93f7b6293530dc1313492fa1a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C099D0Au,
                0x1Au,
                "sha256:604eaf3d648d77b7c618a9444f6725fed4e20a6b964a46080e0e41a8cd81db90"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C099E90u,
                0x4u,
                "sha256:746216ef1cfc19ee57c973518e88f2c4ede904322201cabe8e82eab636bb77cb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C099EF2u,
                0x1Au,
                "sha256:32616ff76238825ff4c801b57a312cf20a1078600229050a59c59dcd3b8c0cdc"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C099F6Au,
                0x1Au,
                "sha256:4370bc087204633ab20b1da5517a0e0e9aa28a41047e0f209d6eeea805d6f1a3"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09A094u,
                0x4u,
                "sha256:746216ef1cfc19ee57c973518e88f2c4ede904322201cabe8e82eab636bb77cb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09AAFEu,
                0x18u,
                "sha256:bf29f4c71dd5e572dba4279a648d7a424cef33f36136fce6e39e1e05b202ba50"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09AC38u,
                0x4u,
                "sha256:746216ef1cfc19ee57c973518e88f2c4ede904322201cabe8e82eab636bb77cb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09B61Au,
                0x2Eu,
                "sha256:ae3d1c07224d7e41e7e36b5976fc330e92352c624f92ee029f825caf7b39d77b"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09B724u,
                0x4u,
                "sha256:746216ef1cfc19ee57c973518e88f2c4ede904322201cabe8e82eab636bb77cb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09EFECu,
                0x12u,
                "sha256:43b999bb55bba51b25239c12a21935b6fcc1c469afd59615619a7e115d4fcf38"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09F1B0u,
                0x4u,
                "sha256:e710522daf1e63f8b5065524d98cc87584e3075fdf63cd75542106b1d8234180"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09F1FAu,
                0x18u,
                "sha256:c27dbb6ae617d1fe6410a97ce4a6e09e6812d6741aa0df8847af1ecc23197453"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C09F2E8u,
                0x4u,
                "sha256:d7d2a9903dda0aecf058ff07825135f0aa33ceebe3a795808117c869cfd94236"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D2E26u,
                0x44u,
                "sha256:e42b54574534fe69f1d41595a69345f049ea7093e374fc94ebff61405c2118ae"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D30E0u,
                0x4u,
                "sha256:e710522daf1e63f8b5065524d98cc87584e3075fdf63cd75542106b1d8234180"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FACB8u,
                0x38u,
                "sha256:e8951148377358f21bd3cd40ea124b0ec4623aa3d967d1df33e70939e179ef5a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FAF10u,
                0x4u,
                "sha256:44deba360f14f5e99d4a7405cf424c0d4212c4f57aba0c3940808dfb74a6cd3f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C100E6Au,
                0x1Cu,
                "sha256:e4a13bbd59ee8a659462dddf1ed7ff8cd055bbfaa5b7052ed47d1f78f97d9fa0"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C100F4Cu,
                0x4u,
                "sha256:210f6ef8035e36b2e52b7cf3ffd991b9eaeb835f48fdb3c5c64f8e74e0fa393d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62FE70u,
                0x1Cu,
                "sha256:2068926812e6ed8c830d5deb2bbce2d26f6c76d2a5f0cc90ed449b492e429115"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62FF20u,
                0x4u,
                "sha256:4377a35378c24c164c04a2c4984307d560caabad775110d8a79504558ac769e4"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C634348u,
                0x20u,
                "sha256:4880f47bc5e811c651c149b13ca83be150b814a6a4c29f93fbb07980473726ab"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C634394u,
                0x4u,
                "sha256:01266f3f522e6dc77ba4cc9e3fc9eb94514cc2a6d6d80bdfde3cfa4a3eedc6f2"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C634E4Au,
                0x20u,
                "sha256:298e94dce8c01fa8011a3c7d16bfd09bc9c17ff96589983495f6112241614fc6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C634E8Cu,
                0x4u,
                "sha256:eb6770cbe827d3d655056e7f8b51d597d350910caa15bd2b96d2aaee19eb2213"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C64EE90u,
                0x6u,
                "sha256:ac45e3356d770b6c6cc1bb10309a7b727ff2581c2ac5af102f64da0b7e36ef6c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C64F05Cu,
                0x4u,
                "sha256:91e32139e8a013bdeeb314379f945f81940d8693f3ce9b48731991f61cfcad2f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C65CC1Eu,
                0x10u,
                "sha256:01fac36bbaef98f343a0253c6f4ca5f1e4e0a2ca34a41b0b2bd09613f545862f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C65CCA8u,
                0x4u,
                "sha256:15d447397269c5402e2da4aa3b2ff9a41f1c6a4cd12e8b63a0d100190de4fa4d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C665B8Eu,
                0xCu,
                "sha256:557e8ba751dcba603378d41342f5e9e58d880b7316036da01c2370d650ea6052"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C665BE8u,
                0x4u,
                "sha256:a2341f4a7d37f2b595989836d381aac5c7c7852344536d18b7d761306943d47d"}};
        // Run70's remaining guarded tail transfers each select between
        // exactly two PC-relative code literals. Bind the complete local
        // selector/epilogue slice, any remote literal cells and the first
        // decoded instruction at both target owners. The generic analyzer
        // still has to prove the closed two-writer CFG and applies a runtime
        // target-set guard; these identities do not declare a target edge.
        constexpr std::array run70_guarded_pointer_set_code_identities{
            katana::runtime::GameProjectCodeIdentity{
                0x8C6261EAu,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62646Eu,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C627242u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6276B8u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C627AA2u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C628002u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62B4A2u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62B8C6u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62BC66u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62BFCCu,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62DD28u,
                0x2Eu,
                "sha256:95fc668fc2fa20e418863069c82b0cca375533b702d484e0e5d1fd1b390fb412"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62DDD4u,
                0xCu,
                "sha256:fc031691951396848aff4a448ae0cdd2ae10513e27697e57d47d24b111a00096"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62F4FEu,
                0x56u,
                "sha256:77f958df3422ce7738b2da453da323a2133f914cc6dafb30e9ec2c7d7a71b397"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C63573Au,
                0x66u,
                "sha256:8ce48164e157128e2fb18481a17e8bbc4205730d01731fa06da9f7570e928fee"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6375D2u,
                0x62u,
                "sha256:36dce33e9225816fff9530935ae84edaf04d1496030ea7fc73029bc7fb5e4b33"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C63838Eu,
                0x52u,
                "sha256:9130d64850d4dcb107483801f3ebfc54899e99db0bb7093d4ec87f2d7c5430e3"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C638454u,
                0x4u,
                "sha256:d99ab2b2fdc7d376ff64e59672c8f3366446fdc8c7ed558913fb467f31c9cb09"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C623AF2u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C623CF8u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6240A6u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62432Eu,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62980Eu,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C629D6Au,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C639C34u,
                0x2u,
                "sha256:d1448ffe88748be226aeccf0fdb2a17443d111c445691e6a089fa4124ed372a9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C639F38u,
                0x2u,
                "sha256:d1448ffe88748be226aeccf0fdb2a17443d111c445691e6a089fa4124ed372a9"},
            // Run81: 0x8C637BA2 is a guarded JMP selected by bit 23 of
            // u32[0x8C890254]. Authenticate the mask producer, the complete
            // selector/dispatch tail and both target entry opcodes; the CFA
            // must still retain the two-target runtime guard and JMP delay.
            katana::runtime::GameProjectCodeIdentity{
                0x8C637634u,
                0x98u,
                "sha256:fc9608b0e0c24d9e6c9e95255cb63cdea372284225f32d0ea9feeab4b9df6727"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C637B7Cu,
                0x4Cu,
                "sha256:5ca2f48be96fef92c2a2d580bb24fa4a33e6b546fc4396773f8ae2472126f36d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C626E4Au,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C626F80u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            // Run81: 0x8C6313F6 is the corresponding guarded JSR family.
            // Both bit-23 outcomes remain independently owned call targets;
            // no singleton table or unconditional target is declared here.
            katana::runtime::GameProjectCodeIdentity{
                0x8C6313DCu,
                0x60u,
                "sha256:35509bfae6a70d4198a73e245fc137276f8ff33e1cf8c1319ac8520532e452e8"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C624F6Eu,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C625180u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            // Run81: the bounded CFG at 0x8C6365B4 selects exactly one of
            // two PC-relative literals. These ranges authenticate the full
            // selector/dispatch slice, both literal cells and both target
            // entry opcodes; the generic CFA still has to prove the closed
            // two-writer set and retain the runtime target-set guard.
            katana::runtime::GameProjectCodeIdentity{
                0x8C6365A6u,
                0x12u,
                "sha256:cc82dda37e17536f4a1972586bc67b10164a68aec291ebf4a8b46f7b996c324e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6365F8u,
                0x8u,
                "sha256:e73470848664c0524413a034232967c665023d6ddf3fb2accc8465d7335a823c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62AA14u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62AFA0u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            // Run81: 0x8C63229E is a guarded JSR whose two immutable
            // PC-relative writers select 0x8C628FAE or 0x8C629424 from
            // u32[0x8C890264] bit 0. Keep this as a guarded set rather than
            // manufacturing a singleton jump-table edge.
            katana::runtime::GameProjectCodeIdentity{
                0x8C632288u,
                0x1Au,
                "sha256:01fd3e0b9fc91725c6cca1134243ce550db25b85bf9850da25edc02eae1ab6f3"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6322E8u,
                0xCu,
                "sha256:954e6135d5507f53905d52dbb1679897061bd48c906f8859add78d2dde9af9ad"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C628FAEu,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C629424u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            // Run81: 0x8C636C92 is the corresponding guarded JMP for
            // u32[0x8C890254] bit 23, with the exact target set
            // {0x8C626934, 0x8C626ADC}. The generic CFA remains responsible
            // for proving both paths and retaining the runtime set guard.
            katana::runtime::GameProjectCodeIdentity{
                0x8C636C68u,
                0x2Eu,
                "sha256:ce7fe182a0187c52b7a3b06932032d8261fcc3a7681a152b6a22e4a1a7a2c514"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C636CB4u,
                0x10u,
                "sha256:d81cdd19b7442baf8c20d05c21f8c3de3cfa83f245a7a745208f31f05029b035"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C626934u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C626ADCu,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            // Run81: 0x8C630AD8 is a guarded JMP selected by bit 0 of
            // u32[0x8C890264]. Authenticate the complete local selector and
            // both literal/target entries; the generic CFA retains the
            // two-target runtime guard and the JMP delay-slot semantics.
            katana::runtime::GameProjectCodeIdentity{
                0x8C630AACu,
                0x32u,
                "sha256:6faa199acd1800ab4c496794dea92c14e05bc81180873efe99337e10207d7a86"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C630B08u,
                0xCu,
                "sha256:a101879e381ad9a7d10b3186b5d7d0a6e559c572c2aa4f10b762b28b983ae4be"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C628522u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C628AACu,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            // Run81: 0x8C632C1E is a guarded JSR selected by bit 23 of
            // u32[0x8C890254]. This is deliberately not represented as a
            // singleton table; both targets and the JSR delay slot remain
            // part of the exact guarded control-flow contract.
            katana::runtime::GameProjectCodeIdentity{
                0x8C632C04u,
                0x1Eu,
                "sha256:44049568b4b7f13ab232fb763a362c82a49bdeaea7a15fff081358167517fbf1"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C632C50u,
                0x14u,
                "sha256:30732cfbe9dc00e6cd2268b5bacd9be05970188a5408d44cd797e43bf40959a8"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62552Eu,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6257B2u,
                0x2u,
                "sha256:688a195bf604493145a2a6a8a4908938d3c242cd3ccd07958d015a7ca056e532"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C041A9Eu,
                0x18u,
                "sha256:42f4716f4b9d68f672e72ab1a1e7e35823149625643c3e80ee9042593906af47"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C041BCCu,
                0x8u,
                "sha256:d4206acfa94d58fc83b5b610041088d1b63917802251a1463e6c39520fde370e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62D2B2u,
                0x20u,
                "sha256:def61d9bf402f62b30d8cc54c30a57d300c9f4057306bc9d9428648a50dfcbf3"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62D300u,
                0x8u,
                "sha256:13a4f1bdf94e7e9bfb0cc2b6252468067af394c43a90d3f4412d85086fcf1ed2"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62E6C8u,
                0x2Eu,
                "sha256:c3b4dab0aad6a2b347613d89dd9869543df36c10a85dd88f4cf62750678d6cc9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C62E730u,
                0x8u,
                "sha256:d2a9e27220167915502fa1c3fd37057f728944e6c08abcbdb6cd1fd9c04bcdcb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C633C10u,
                0x1Cu,
                "sha256:2f44312639b2ec5ef3e2ac38d574082547f5ed5278015cd18566010ca7a8c052"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C633C74u,
                0x8u,
                "sha256:37a2e560770acab3fcb14431d92223948db4634e63045404a4fbd9e38ba523c6"}};
        // Run70's follow-up pool contains seventeen additional singleton calls
        // whose complete PC-relative producer, dispatch/delay slot and
        // literal cells are byte-identical in the current PAL image. The
        // analyzer still resolves every value and verifies the unique
        // FunctionMap target; these ranges only authenticate the source
        // bytes. The 0x8C062F7E owner identity deliberately replaces smaller
        // overlapping producer/dispatch identities for that one function.
        constexpr std::array run70_followup_direct_literal_code_identities{
            katana::runtime::GameProjectCodeIdentity{
                0x8C0115A6u,
                0x6u,
                "sha256:994c40634a5e8f01693f4ea5303a29bc4c62c39519a80b5f4ca807b88bc1a49a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0115ACu,
                0x2u,
                "sha256:23a5c635290c5a3b0a6e845a5bf93c58a30610baa9bd4e72979414492319707f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0115E4u,
                0x6u,
                "sha256:f5d0fe4a125b7440ef11116c09374daf86e7a34196e312f67e89e9f4e1b78842"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0115EAu,
                0x2u,
                "sha256:23a5c635290c5a3b0a6e845a5bf93c58a30610baa9bd4e72979414492319707f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C011678u,
                0x6u,
                "sha256:c7d19a950aa05e14cc2bd11430aa5eaada0ee91c9bd25e6339d8df70a437fcac"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0116CCu,
                0x4u,
                "sha256:abe613d765dd413d264592f0a9faf6324625592e8e7e385a79c3c91cfc34ad35"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C011A66u,
                0x6u,
                "sha256:514cefc5e21afb93681b1b9a158775fcd63bae744cca684c19b6eec49b69bda7"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C01212Cu,
                0x6u,
                "sha256:6719043f5bdfa4b6a54f3704ddb29668eaa0c85a248d34aeba4540941ea0116a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C012224u,
                0x4u,
                "sha256:abe613d765dd413d264592f0a9faf6324625592e8e7e385a79c3c91cfc34ad35"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C02F0C2u,
                0x2u,
                "sha256:8dcca75386022bb9f9982b61bb3bb40f8acdb776f4d0692a18715b5b06850eb6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C02F0C4u,
                0x1Au,
                "sha256:be06e78bd9f10c3552c201b07814ffc347df18d145f463f3f12da2fe71bba5ff"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C02F0DEu,
                0x4u,
                "sha256:bbe96936aaafb39cf5e9cddd1b8788212a5c546436bb658922fa15004b00a254"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C02F2D0u,
                0x4u,
                "sha256:44deba360f14f5e99d4a7405cf424c0d4212c4f57aba0c3940808dfb74a6cd3f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C03199Cu,
                0x2u,
                "sha256:9eaad7406560c191cd47354766da4d7ee59ba463146d79bae25ef688eff6be01"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C03199Eu,
                0x14u,
                "sha256:1bbdb6525a7ba75db28f495e855f84a05ca079061b494b9aa6f51ff8d8e1814d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0319B2u,
                0x4u,
                "sha256:bd486a952c5de133a040f7989d07f368992bb9af9684f4642b1ecab27db31894"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C031B0Cu,
                0x4u,
                "sha256:20b19889894c51a27bbba806e179769f4cbfaeba63675db6b88ec9578fb7aaab"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C638E0Cu,
                0x2u,
                "sha256:d1448ffe88748be226aeccf0fdb2a17443d111c445691e6a089fa4124ed372a9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04D98Au,
                0x6u,
                "sha256:9c50cc9664a406288903c89bcf13f45dafdf685264cb143fc2fcbc70d9cf1dc6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C04DA04u,
                0x4u,
                "sha256:d7093b0f9861a20fd5b4b5db70a91ca319d09cf5f5e25b3cb6129d2dabf91c3c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C062F7Eu,
                0x6Eu,
                "sha256:9d225e16aad9b29b38ace99953c6229bfa3810338969c127c9518e9d02ccf201"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C063020u,
                0x4u,
                "sha256:0f064f0f1e91bb8d87d15c4b18f1c8b213dd71ec0f39cb317767fc76b989ce30"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C07B346u,
                0x2u,
                "sha256:b1aeb6e3f3eada4f921f9d1abd7ccf16ae176335c739d6788665d23adebb7d51"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C07B348u,
                0x14u,
                "sha256:beab118d5c004c6872cc3a53edcda25c380189439ad826c7d86a3b929182b137"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C07B35Cu,
                0x4u,
                "sha256:3a01762ca9db6444474ee82263b83e320778171f0efcb592e54d964e22bb5958"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C07B418u,
                0x4u,
                "sha256:76bf2b1ec8b6c9d1597f463b1d08a3f212893eac9153977f6f71504f788dce89"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10D274u,
                0x2u,
                "sha256:d33089bc64c8458bbd86222c01048227eaa441a6649ab4ca583c97e84c364d2f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6533BCu,
                0x16u,
                "sha256:ff52b710fa899f8ecbf921ed2a3a3bac01b0f055b9f20f31a88dac6853862b75"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C011508u,
                0x8u,
                "sha256:9f81129520df8779b679a72b56d3244ff19b4aa58e0fdd5ab5d67740df88b995"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C010FCCu,
                0x76u,
                "sha256:bf1f5eb3b7a50d1428c03cdfe6836794192ff056f5d3145179485b1cd19784a5"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C011060u,
                0x1D8u,
                "sha256:3357c20370fcf1be590c392765b66f71355021aedb31dae4e281c0a20b12ea25"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C602D32u,
                0x6u,
                "sha256:735d197e05f63c6a40c2917de9a0244b3198ffa9cd99d342d12273a4ab7792c4"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6032C4u,
                0x2Eu,
                "sha256:88718f703ba7ce2ad5ceed638f7f98437f49aaf9d1df03e1df643bb6dfa0575f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C602E9Cu,
                0x2Eu,
                "sha256:a4943cd981ddac47275c890cf03625b69ac2de551a175c53c81f03d0b930fecf"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C602ECAu,
                0x2Eu,
                "sha256:cc4b2c203901d2a8ffa90367e5597a531cb13d3ea2b679a43a6d28fe3475d2eb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C603324u,
                0x1Au,
                "sha256:a8d2002b280f44a1cf80255abee16bc39e0bd515c928f49e87b7445b540ae2ed"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6033B4u,
                0x2Eu,
                "sha256:844b9a45db6ddae41f63b8639254685a5516471f9e22d60c7b463e36eb526e45"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C603448u,
                0x58u,
                "sha256:f9b3258d46581219f9d79309cdf94c643ae2b7715adebb54f4f3ea073b40c355"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6034A0u,
                0x5Eu,
                "sha256:9ed7f47f8559ace8833618b22adb65fe1ed68d90cce48419da1b3f831dd8b3de"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6034FEu,
                0x5Eu,
                "sha256:e5568b9f7cc9463ddde2c5fd23b7d7cd5f5cf50f09296af3d9f4772bbe510eb1"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C60355Cu,
                0x56u,
                "sha256:044cb0a4d221443728402744af6718148758828615c86cf2c781f10188f33f1f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C069F92u,
                0x22u,
                "sha256:4459d5779fa02ede449dc3acaaf186476f4fea8e683720ff77beb845d7010187"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C06A108u,
                0x4u,
                "sha256:2e8aa6c7e4d25869fb6f95d37608cb2220b7ac5406ebab7af4566217441ea4a5"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C043A20u,
                0x2u,
                "sha256:7ad7f102a487fce43785b2bd793cd15a79f3129f232f55e07235535dc2778dd0"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0DF232u,
                0x1Au,
                "sha256:3effb257030263f4ea7907529e5c92ebd06105cbe202ddba336269c3c861cdfc"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0DF460u,
                0x4u,
                "sha256:b76393bcd52cd45a5ef6306edfa4745145157e20b706314cccdf5aa54b4b57d6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C639E9Cu,
                0x2u,
                "sha256:d1448ffe88748be226aeccf0fdb2a17443d111c445691e6a089fa4124ed372a9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FFF42u,
                0x1Cu,
                "sha256:0771b727379bc66860e7659f80dc741142d419d122bf3e487d024f2148ae5e70"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C100180u,
                0x4u,
                "sha256:44deba360f14f5e99d4a7405cf424c0d4212c4f57aba0c3940808dfb74a6cd3f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C639BB0u,
                0x2u,
                "sha256:921f958afaa4aec7b957171d966d3fc26642b80e79678bca8166a5b3ba0fd079"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10EE28u,
                0x8u,
                "sha256:e64075bd1b85ce4738b985f2b8b58b48d8e9c0ec36abaa6fc817aed7ed93730f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10EE58u,
                0x4u,
                "sha256:4bbb88d5ef40d9b13031655e8d7182b3db0823f5c787f72248afc5aa5d314dd6"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C10FA88u,
                0x2u,
                "sha256:8df24a2fac3c95e7619c69219a1138d6a6a389c1c02449e96cc8bdb760ca145d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FC7ACu,
                0x2u,
                "sha256:1927b50fd269d5588c6739e2b680aa62ff4a1ca4ef2ad599ec987139ba7504ad"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FC7AEu,
                0xEu,
                "sha256:4e89b677c4d7e77dcabf60e368d7b0ec78cd309f96c0370046e17bdcb26a065a"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FC7BCu,
                0x4u,
                "sha256:de58852b93f3736cba44a05630eb6ed82293cc6f760d79317a6afb851633637c"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FCA24u,
                0x4u,
                "sha256:44deba360f14f5e99d4a7405cf424c0d4212c4f57aba0c3940808dfb74a6cd3f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C64BD5Au,
                0x6u,
                "sha256:323c99abfc5d7d0937fd92dd2d4eb894a861ea3194cf59ef1445e36983eee07e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C64BE0Cu,
                0x4u,
                "sha256:be0e640d346fea483d0bae11dc8c9bf22c9f819da75362699198121d32108c15"}};
        // Exact private source authentication for the still-relevant
        // historical Egg-Fleet A backlog. Every producer-through-delay-slot
        // range and every table/literal range was hashed from the verified
        // PAL boot image above. These identities do not make adjacent
        // RuntimeOnly transfers authoritative and do not authorize an address
        // range: each is consumed only by the exact dispatch declaration
        // below.
        constexpr std::array historical_a_closure_code_identities{
            katana::runtime::GameProjectCodeIdentity{
                0x8C1001D4u,
                0x1Eu,
                "sha256:b60fc7820e7ee94301db371bd227b75cee8cb1e3216657ad0f0f00b3d55f45bb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0181DAu,
                0x16u,
                "sha256:2a25ac61b655c05d77fd241cebc97bf25a4630f8886f8a1208e0342060782404"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0181F4u,
                0x22u,
                "sha256:b096d7498946b487c1fde90cde738acb7b867e5500c328bb1ac95a1062d05492"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FAB12u,
                0x1Cu,
                "sha256:f1360e820eb562d079b7a9470fd6fd17d9060e919f81ae5dbb208a780edd60b1"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FAB3Cu,
                0x16u,
                "sha256:df0abe3ed7526d75310ba45d4c32c49f7233d42ce9baaa51ec7db372f8ae8e65"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0E0ED2u,
                0x1Eu,
                "sha256:82dbff0205211ac1b376ad1143a217f57fd390724c91a827b0d92792f6f999c4"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0E0EF0u,
                0x6Cu,
                "sha256:9f009f14994bdf0f999a820c6dba1c20f06dc88a3d323824b55c7a83a61de9c2"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D7992u,
                0x1Eu,
                "sha256:b8482f6a87a39b9034bb44ffbf6a85251e312b0bcfccfaf8c6e7a102a70757e7"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0D79B0u,
                0x66u,
                "sha256:8ea4164f425f6668bced1518d062296a2a7fb1d29d9ab9588ed60265b344f4f1"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C101E7Eu,
                0x1Eu,
                "sha256:14d6d70742758dd1d385fb863faaa5f6209aa3f69fe664276ad97db16524d0c4"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C102220u,
                0x4u,
                "sha256:76bf2b1ec8b6c9d1597f463b1d08a3f212893eac9153977f6f71504f788dce89"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0F08A4u,
                0x2Eu,
                "sha256:a98a31ccc0d33c45145d9d14103cf9ca274fd5ef601892ad81d4608f3e651b31"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0F0994u,
                0x4u,
                "sha256:1e22c48a3345b30e05bc2f1a19516491400ab45c8452f05ecb6d90e5a53cd59f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C63E114u,
                0x66u,
                "sha256:7174727e659fbb10f975cbc0b87601141398e6b66e76311bd6a524090f148ebc"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C63E188u,
                0x4u,
                "sha256:c2532af3e999ad936de0a2ba66244cca83001be861948d8a78133a5682976137"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6735C8u,
                0x8u,
                "sha256:e2c87d13b2db72622cee4dada31e3415c1fb29e798bde90c16bfe196aa58eda2"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C6735D0u,
                0x18u,
                "sha256:058216065403cc038223c9ab69633389339b1ff4c71486ec2054138f4e5fd612"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C100AD2u,
                0x1Cu,
                "sha256:71f4128ee353f0a4697f948e505aeb2a86b929b34b85eef0f2d13154fe931703"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C100BBCu,
                0x4u,
                "sha256:210f6ef8035e36b2e52b7cf3ffd991b9eaeb835f48fdb3c5c64f8e74e0fa393d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C061358u,
                0xCu,
                "sha256:7e5c5c823bff9d0305d0aa15167a900bcc718c8b9a2222098d9d56675890c0ed"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0614F8u,
                0x4u,
                "sha256:0acf4c73bdefa5b31d6f9e6e87db408c8fb93b39d1f887199b75904117a6fa6e"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C154CBCu,
                0x14u,
                "sha256:1a14da1657c92346fb20a53982cc1c2302feff94bb30e8fbc4d0b8416dd653e9"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C06273Eu,
                0xCu,
                "sha256:f69d053201f8f644e732a6d5a1a2672e8faa435bae83c961239944a2f37838db"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0627F4u,
                0x4u,
                "sha256:6773ffd1a79aee7643ba163608024fafcd1ca9e3d18711016b82be488a483acf"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C154E48u,
                0x10u,
                "sha256:70f23a654a056d10b1eddbc3328620eb33798bd1a3f14774543b3121c66f71cb"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C100712u,
                0x1Cu,
                "sha256:9bb4d61b5cbc368dc37dbf3959af19e4da4b6ca9c9ba2d28646d336d94087b98"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C100818u,
                0x4u,
                "sha256:210f6ef8035e36b2e52b7cf3ffd991b9eaeb835f48fdb3c5c64f8e74e0fa393d"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FFBF4u,
                0x1Cu,
                "sha256:3f090a05cf1bf394acc7ecca9d052e1a40784a5f6a9722345244b477bade6065"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C0FFE3Cu,
                0x4u,
                "sha256:44deba360f14f5e99d4a7405cf424c0d4212c4f57aba0c3940808dfb74a6cd3f"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C065880u,
                0x16u,
                "sha256:f4a0a2494166320fc9af77e0529d2fe8a698811fc3d676d8b59a2c0ce61b1923"},
            katana::runtime::GameProjectCodeIdentity{
                0x8C065954u,
                0x4u,
                "sha256:3722c5fd352cf069404b29c4b001a0a07915012287ca00ba75f2dbfc4d0a974a"}};
        std::vector<katana::runtime::GameProjectCodeIdentity> code_identities;
        code_identities.reserve(
            static_code_identities.size() +
            run69_direct_literal_code_identities.size() +
            run69_followup_direct_literal_code_identities.size() +
            run70_guarded_pointer_set_code_identities.size() +
            run70_followup_direct_literal_code_identities.size() +
            historical_a_closure_code_identities.size() +
            whole_game_strict_entries.size());
        code_identities.insert(
            code_identities.end(),
            static_code_identities.begin(),
            static_code_identities.end());
        code_identities.insert(
            code_identities.end(),
            run69_direct_literal_code_identities.begin(),
            run69_direct_literal_code_identities.end());
        code_identities.insert(
            code_identities.end(),
            run69_followup_direct_literal_code_identities.begin(),
            run69_followup_direct_literal_code_identities.end());
        code_identities.insert(
            code_identities.end(),
            run70_guarded_pointer_set_code_identities.begin(),
            run70_guarded_pointer_set_code_identities.end());
        code_identities.insert(
            code_identities.end(),
            run70_followup_direct_literal_code_identities.begin(),
            run70_followup_direct_literal_code_identities.end());
        code_identities.insert(
            code_identities.end(),
            historical_a_closure_code_identities.begin(),
            historical_a_closure_code_identities.end());
        for (const auto& entry : whole_game_strict_entries)
            code_identities.push_back(
                {entry.start, entry.size, entry.code_identity});
        std::sort(
            code_identities.begin(), code_identities.end(),
            [](const auto& left, const auto& right) {
                return left.address < right.address;
            });
        // Mirror the artifact's structural identity gate here so a bad
        // authoring range reports its exact private address before writing.
        for (std::size_t index = 0u; index < code_identities.size(); ++index) {
            const auto& current = code_identities[index];
            const auto valid_hex_identity = [](const std::string_view value) {
                constexpr std::string_view prefix = "sha256:";
                if (value.size() != prefix.size() + 64u ||
                    !value.starts_with(prefix))
                    return false;
                return std::all_of(
                    value.begin() + static_cast<std::ptrdiff_t>(prefix.size()),
                    value.end(), [](const char digit) {
                        return (digit >= '0' && digit <= '9') ||
                               (digit >= 'a' && digit <= 'f') ||
                               (digit >= 'A' && digit <= 'F');
                    });
            };
            const bool valid_range =
                current.address != 0u && (current.address & 1u) == 0u &&
                current.size != 0u &&
                current.size <=
                    (std::uint64_t{1u} << 32u) - current.address;
            const bool overlaps_previous =
                index != 0u &&
                static_cast<std::uint64_t>(
                    code_identities[index - 1u].address) +
                        code_identities[index - 1u].size >
                    current.address;
            if (!valid_range ||
                !valid_hex_identity(current.byte_identity) ||
                overlaps_previous) {
                std::ostringstream error;
                error << "game-project code identity invalid at 0x"
                      << std::hex << current.address << " size=0x"
                      << current.size;
                throw std::runtime_error(error.str());
            }
        }
        const auto require_candidate_byte_identity =
            [&](const std::uint32_t address,
                const std::uint32_t size,
                const std::string_view identity,
                const std::string_view label) {
                constexpr std::string_view prefix = "sha256:";
                const auto begin = static_cast<std::uint64_t>(address);
                const auto end = begin + size;
                if (identity.size() != prefix.size() + 64u ||
                    !identity.starts_with(prefix) ||
                    begin < boot_base || end > boot_end) {
                    throw std::runtime_error(
                        std::string("object-callback Candidate has invalid ") +
                        std::string(label) + " range/identity");
                }
                const auto offset =
                    static_cast<std::size_t>(begin - boot_base);
                const auto actual = katana::io::sha256_bytes(std::string_view(
                    reinterpret_cast<const char*>(boot_bytes.data() + offset),
                    size));
                if (actual != identity.substr(prefix.size())) {
                    throw std::runtime_error(
                        std::string("object-callback Candidate ") +
                        std::string(label) + " byte identity mismatch");
                }
            };
        for (std::size_t index = 0u;
             index < object_callback_candidate_analysis_roots.size(); ++index) {
            const auto& candidate =
                object_callback_candidate_analysis_roots[index];
            const auto source_owner_end =
                static_cast<std::uint64_t>(candidate.source_owner) +
                candidate.source_owner_size;
            const auto source_block_end =
                static_cast<std::uint64_t>(candidate.source_block) +
                candidate.source_block_size;
            if (candidate.source_block < candidate.source_owner ||
                source_block_end > source_owner_end ||
                candidate.callsite < candidate.source_block ||
                candidate.callsite + 4u != candidate.continuation ||
                source_block_end != candidate.continuation) {
                throw std::runtime_error(
                    "object-callback Candidate source/callsite contract changed");
            }
            for (std::size_t previous = 0u; previous < index; ++previous) {
                const auto& other =
                    object_callback_candidate_analysis_roots[previous];
                if (candidate.producer_cell == other.producer_cell) {
                    throw std::runtime_error(
                        "object-callback Candidate inventory repeats a producer");
                }
                if (candidate.target == other.target &&
                    (candidate.target_size != other.target_size ||
                     candidate.target_identity != other.target_identity)) {
                    throw std::runtime_error(
                        "object-callback Candidate shared target identity changed");
                }
            }
            const auto exact_boundary_count =
                [&](const std::uint32_t address, const std::uint32_t size) {
                    return std::count_if(
                        functions.begin(), functions.end(),
                        [&](const auto& boundary) {
                            return boundary.start == address &&
                                   boundary.size == size;
                        });
                };
            const auto exact_identity_count =
                [&](const std::uint32_t address,
                    const std::uint32_t size,
                    const std::string_view identity) {
                    return std::count_if(
                        code_identities.begin(), code_identities.end(),
                        [&](const auto& code_identity) {
                            return code_identity.address == address &&
                                   code_identity.size == size &&
                                   code_identity.byte_identity == identity;
                        });
                };
            const auto containing_identity_count =
                [&](const std::uint32_t address, const std::uint32_t size) {
                    const auto begin = static_cast<std::uint64_t>(address);
                    const auto end = begin + size;
                    return std::count_if(
                        code_identities.begin(), code_identities.end(),
                        [&](const auto& code_identity) {
                            const auto identity_begin =
                                static_cast<std::uint64_t>(
                                    code_identity.address);
                            const auto identity_end = identity_begin +
                                code_identity.size;
                            return begin >= identity_begin &&
                                   end <= identity_end;
                        });
                };
            if (exact_boundary_count(
                    candidate.source_owner,
                    candidate.source_owner_size) != 1u ||
                exact_boundary_count(
                    candidate.target,
                    candidate.target_size) != 1u ||
                containing_identity_count(
                    candidate.producer_cell,
                    sizeof(std::uint32_t)) != 1u ||
                exact_identity_count(
                    candidate.source_owner,
                    candidate.source_owner_size,
                    candidate.source_owner_identity) != 1u ||
                exact_identity_count(
                    candidate.target,
                    candidate.target_size,
                    candidate.target_identity) != 1u) {
                throw std::runtime_error(
                    "object-callback Candidate boundary/identity is not unique");
            }
            require_candidate_byte_identity(
                candidate.producer_cell,
                sizeof(std::uint32_t),
                candidate.producer_cell_identity,
                "producer-cell");
            require_candidate_byte_identity(
                candidate.source_owner,
                candidate.source_owner_size,
                candidate.source_owner_identity,
                "source-owner");
            require_candidate_byte_identity(
                candidate.source_block,
                candidate.source_block_size,
                candidate.source_block_identity,
                "source-block");
            require_candidate_byte_identity(
                candidate.callsite,
                4u,
                candidate.callsite_identity,
                "callsite-delay");
            require_candidate_byte_identity(
                candidate.target,
                candidate.target_size,
                candidate.target_identity,
                "target");
            const auto producer_offset = static_cast<std::size_t>(
                candidate.producer_cell - boot_base);
            const auto producer_target = static_cast<std::uint32_t>(
                boot_bytes[producer_offset] |
                (static_cast<std::uint32_t>(
                     boot_bytes[producer_offset + 1u]) << 8u) |
                (static_cast<std::uint32_t>(
                     boot_bytes[producer_offset + 2u]) << 16u) |
                (static_cast<std::uint32_t>(
                     boot_bytes[producer_offset + 3u]) << 24u));
            if (producer_target != candidate.target)
                throw std::runtime_error(
                    "object-callback Candidate producer no longer names target");
        }
        // The immutable literal at 0x8C665934 is the sole target of the JSR
        // at 0x8C6658D0 (0x8C65247E in this exact boot executable).  Model it
        // as a one-entry call table so RuntimeOnly codegen can bind the
        // title's outer four-byte-copy loop without guessing from live RAM.
        constexpr std::array callback_dispatches{
            // Still-relevant historical Egg-Fleet A closures. The first two
            // owners use an unsigned selector guard followed by a complete
            // signed-relative table; their out-of-range paths load the
            // adjacent immutable epilogue literal. The remaining declarations
            // bind only the exact singleton or bounded absolute table proven
            // by the producer slices above. Every table target has one owner
            // in the current private FunctionMap/CFG.
            katana::runtime::GameProjectJumpTable{
                0x8C0E0EE0u,
                0x8C0E0EF0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0E0EECu,
                0x8C0E0EF4u,
                52u,
                sizeof(std::uint16_t),
                0x8C0E0EF0u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0D79A0u,
                0x8C0D79B0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0D79ACu,
                0x8C0D79B4u,
                49u,
                sizeof(std::uint16_t),
                0x8C0D79B0u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0FAB2Au,
                0x8C0FAB3Cu,
                11u,
                sizeof(std::uint16_t),
                0x8C0FAB2Eu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0181ECu,
                0x8C0181F4u,
                17u,
                sizeof(std::uint16_t),
                0x8C0181F0u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C101E98u,
                0x8C102220u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0F08CEu,
                0x8C0F0994u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C63E162u,
                0x8C6735D0u,
                6u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C100AEAu,
                0x8C100BBCu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C061360u,
                0x8C154CBCu,
                5u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C062746u,
                0x8C154E48u,
                4u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C10072Au,
                0x8C100818u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0FFC0Cu,
                0x8C0FFE3Cu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C065892u,
                0x8C065954u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            // Run70 follow-up singleton calls. Each independently hashed
            // literal cell contains one current FunctionMap entry; duplicate
            // target values and shared cells remain separate dispatch proofs.
            katana::runtime::GameProjectJumpTable{
                0x8C0115AAu,
                0x8C0116CCu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0115E8u,
                0x8C0116CCu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C01167Au,
                0x8C0116CCu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C011A68u,
                0x8C011BC0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C01212Eu,
                0x8C012224u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C02F0DEu,
                0x8C02F2D0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0319B2u,
                0x8C031B0Cu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C04D98Cu,
                0x8C04DA04u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C062FD0u,
                0x8C063020u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C07B35Cu,
                0x8C07B418u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C069FB0u,
                0x8C06A108u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0DF248u,
                0x8C0DF460u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0FFF5Au,
                0x8C100180u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C01150Cu,
                0x8C011564u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C10EE2Cu,
                0x8C10EE58u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0FC7BCu,
                0x8C0FCA24u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C64BD5Cu,
                0x8C64BE0Cu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            // Run69 direct-literal cohort. Every JSR consumes exactly the
            // listed immutable PAL word and every word names one existing
            // FunctionMap entry. Shared targets and cells remain independent
            // dispatch proofs.
            katana::runtime::GameProjectJumpTable{
                0x8C010212u,
                0x8C0102E0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0109F8u,
                0x8C010ACCu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C010A2Cu,
                0x8C010ACCu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C010B42u,
                0x8C010C24u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0114A4u,
                0x8C011564u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C01153Cu,
                0x8C011564u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C011AAAu,
                0x8C011BC0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C011E6Au,
                0x8C011F48u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C015BC2u,
                0x8C015BCCu,
                16u,
                sizeof(std::uint16_t),
                0x8C015BC6u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C01B5F8u,
                0x8C01B600u,
                22u,
                sizeof(std::uint16_t),
                0x8C01B5FCu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C023978u,
                0x8C023980u,
                26u,
                sizeof(std::uint16_t),
                0x8C02397Cu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            // This PAL owner uses four independent immutable function
            // literals. Both target functions are shared deliberately across
            // two paths; every dispatch remains a separately authenticated
            // singleton so unrelated runtime-only calls stay unresolved.
            katana::runtime::GameProjectJumpTable{
                0x8C0407A0u,
                0x8C040860u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0407D6u,
                0x8C04086Cu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C04089Cu,
                0x8C040988u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0408D6u,
                0x8C040998u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C04CA74u,
                0x8C04CA8Cu,
                24u,
                sizeof(std::uint16_t),
                0x8C04CA78u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C04CC48u,
                0x8C04CD24u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C04CC76u,
                0x8C04CD40u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C04D780u,
                0x8C04D788u,
                19u,
                sizeof(std::uint16_t),
                0x8C04D784u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C04D88Cu,
                0x8C04D918u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C04DA54u,
                0x8C04DA6Cu,
                23u,
                sizeof(std::uint16_t),
                0x8C04DA58u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C04E320u,
                0x8C04E328u,
                19u,
                sizeof(std::uint16_t),
                0x8C04E324u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            // The exact PAL v1.003 post-bootstrap image bounds the main-mode
            // selector to 26 entries immediately before this BRAF. Each
            // identity-bound signed halfword is relative to the architectural
            // post-delay address 0x8C053B98 and all decoded targets stay
            // inside the declared sa_main_mode_dispatcher owner.
            katana::runtime::GameProjectJumpTable{
                0x8C053AD4u,
                0x8C053B28u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C053B94u,
                0x8C053BA8u,
                26u,
                sizeof(std::uint16_t),
                0x8C053B98u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            // Current PAL bytes and the same-GDI generated port agree on the
            // complete guarded selectors. Runtime-only transfers elsewhere
            // in either owner remain independent residual frontiers.
            katana::runtime::GameProjectJumpTable{
                0x8C062AC6u,
                0x8C062C20u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C065FFAu,
                0x8C066100u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C067E8Cu,
                0x8C067FA4u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C068D20u,
                0x8C068E00u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C068D94u,
                0x8C068E14u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C06AB60u,
                0x8C06ADACu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C07944Au,
                0x8C079450u,
                15u,
                sizeof(std::uint16_t),
                0x8C07944Eu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0827ECu,
                0x8C0827F4u,
                34u,
                sizeof(std::uint16_t),
                0x8C0827F0u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0859F2u,
                0x8C085A38u,
                8u,
                sizeof(std::uint16_t),
                0x8C0859F6u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C08C0A8u,
                0x8C08C0BCu,
                21u,
                sizeof(std::uint16_t),
                0x8C08C0ACu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C08C9E6u,
                0x8C08C9F4u,
                21u,
                sizeof(std::uint16_t),
                0x8C08C9EAu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C08D178u,
                0x8C08D190u,
                21u,
                sizeof(std::uint16_t),
                0x8C08D17Cu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C08E920u,
                0x8C08E928u,
                21u,
                sizeof(std::uint16_t),
                0x8C08E924u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C08F120u,
                0x8C08F250u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0933BEu,
                0x8C0933CCu,
                43u,
                sizeof(std::uint16_t),
                0x8C0933C2u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C09986Au,
                0x8C099908u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C099D20u,
                0x8C099E90u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C099F08u,
                0x8C09A094u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C099F80u,
                0x8C09A094u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C09A3A0u,
                0x8C09A3E8u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C09A3A6u,
                0x8C09A3ECu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C09A408u,
                0x8C09A438u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C09A434u,
                0x8C09A454u,
                17u,
                sizeof(std::uint16_t),
                0x8C09A438u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C09AB12u,
                0x8C09AC38u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C09B644u,
                0x8C09B724u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C09CE48u,
                0x8C09CE68u,
                43u,
                sizeof(std::uint16_t),
                0x8C09CE4Cu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C09EFFAu,
                0x8C09F1B0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C09F20Eu,
                0x8C09F2E8u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            // MOV.B sign-extends the selector and CMP/HS rejects both
            // negative values and unsigned indices >=84. The immutable PAL
            // table therefore covers exactly 0..83; its 82 unique targets
            // each map to one current FunctionMap owner. Two unrelated
            // runtime-only transfers in the source owner remain deferred.
            katana::runtime::GameProjectJumpTable{
                0x8C0A788Eu,
                0x8C0A7898u,
                84u,
                sizeof(std::uint16_t),
                0x8C0A7892u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            // MOV.B sign-extends the selector, ADD #-1 maps the accepted
            // values to 0..82, and CMP/HS rejects every value >=83 before
            // this BRAF. The authenticated PAL table therefore provides the
            // complete signed-relative target set without a runtime guess.
            katana::runtime::GameProjectJumpTable{
                0x8C0A96D2u,
                0x8C0A96DCu,
                83u,
                sizeof(std::uint16_t),
                0x8C0A96D6u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0AFEF8u,
                0x8C0AFF00u,
                50u,
                sizeof(std::uint16_t),
                0x8C0AFEFCu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0B07B6u,
                0x8C0B07C0u,
                50u,
                sizeof(std::uint16_t),
                0x8C0B07BAu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            // The concrete task sites and adjacent bounded tables currently
            // evidenced for the three owners. Every singleton names the
            // architectural PC literal actually loaded by its dispatch; every
            // relative table uses the explicit unsigned guard bound and BRAF
            // base. Missing disassembly is never treated as absence here.
            katana::runtime::GameProjectJumpTable{
                0x8C0B60F8u,
                0x8C0B616Cu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0B61C2u,
                0x8C0B61C8u,
                64u,
                sizeof(std::uint16_t),
                0x8C0B61C6u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0B6256u,
                0x8C0B6420u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0B6280u,
                0x8C0B642Cu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0B7192u,
                0x8C0B719Cu,
                62u,
                sizeof(std::uint16_t),
                0x8C0B7196u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0BBDBCu,
                0x8C0BBDC4u,
                50u,
                sizeof(std::uint16_t),
                0x8C0BBDC0u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            // The decremented selector is checked unsigned against 50 before
            // this BRAF. The authenticated PAL table contains all 50 signed
            // offsets relative to PC+4; all 28 unique landings stay inside
            // the single current FunctionMap owner at 0x8C0BC55C.
            katana::runtime::GameProjectJumpTable{
                0x8C0BC5D8u,
                0x8C0BC5E0u,
                50u,
                sizeof(std::uint16_t),
                0x8C0BC5DCu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0C05E4u,
                0x8C0C0600u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0C062Eu,
                0x8C0C0650u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0C063Eu,
                0x8C0C0654u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0C064Au,
                0x8C0C0658u,
                54u,
                sizeof(std::uint16_t),
                0x8C0C064Eu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0C19BEu,
                0x8C0C19C8u,
                53u,
                sizeof(std::uint16_t),
                0x8C0C19C2u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0C5B96u,
                0x8C0C5B9Cu,
                50u,
                sizeof(std::uint16_t),
                0x8C0C5B9Au,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0C61BCu,
                0x8C0C61C0u,
                50u,
                sizeof(std::uint16_t),
                0x8C0C61C0u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0CA6EEu,
                0x8C0CA940u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0CADD4u,
                0x8C0CB0C0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0CBD6Cu,
                0x8C0CBEE8u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0CBD78u,
                0x8C0CBEF0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0CBDEEu,
                0x8C0CBEF8u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0CBE12u,
                0x8C0CBF00u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0CBF44u,
                0x8C0CBF50u,
                56u,
                sizeof(std::uint16_t),
                0x8C0CBF48u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0CD1ACu,
                0x8C0CD1B4u,
                55u,
                sizeof(std::uint16_t),
                0x8C0CD1B0u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0D0776u,
                0x8C0D077Cu,
                50u,
                sizeof(std::uint16_t),
                0x8C0D077Au,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0D0AD6u,
                0x8C0D0ADCu,
                50u,
                sizeof(std::uint16_t),
                0x8C0D0ADAu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0D2E66u,
                0x8C0D30E0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0D6B46u,
                0x8C0D6B4Cu,
                50u,
                sizeof(std::uint16_t),
                0x8C0D6B4Au,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0D6BBEu,
                0x8C0D6DC0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0D6BE8u,
                0x8C0D6DCCu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0D6C08u,
                0x8C0D6DD0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0DB6F4u,
                0x8C0DB6F8u,
                50u,
                sizeof(std::uint16_t),
                0x8C0DB6F8u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0DBBD6u,
                0x8C0DBBDCu,
                50u,
                sizeof(std::uint16_t),
                0x8C0DBBDAu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0DFF5Eu,
                0x8C0E0040u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0DFF74u,
                0x8C0E0044u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0E0094u,
                0x8C0E0098u,
                55u,
                sizeof(std::uint16_t),
                0x8C0E0098u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0E0114u,
                0x8C0E0280u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0E4432u,
                0x8C0E4438u,
                50u,
                sizeof(std::uint16_t),
                0x8C0E4436u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0E483Au,
                0x8C0E4840u,
                50u,
                sizeof(std::uint16_t),
                0x8C0E483Eu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0ED82Au,
                0x8C0ED83Cu,
                18u,
                sizeof(std::uint16_t),
                0x8C0ED82Eu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0ED86Au,
                0x8C0ED9E0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0ED88Au,
                0x8C0ED9ECu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0ED8A4u,
                0x8C0ED9F0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0EDD30u,
                0x8C0EDD34u,
                17u,
                sizeof(std::uint16_t),
                0x8C0EDD34u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C0FACECu,
                0x8C0FAF10u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C0FDCA6u,
                0x8C0FDCB4u,
                13u,
                sizeof(std::uint16_t),
                0x8C0FDCAAu,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C100E82u,
                0x8C100F4Cu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C10964Cu,
                0x8C109664u,
                25u,
                sizeof(std::uint16_t),
                0x8C109650u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C10CC80u,
                0x8C10CCD8u,
                17u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C10CC8Cu,
                0x8C10CCD8u,
                17u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C10CD28u,
                0x8C10CD78u,
                17u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C10EAF8u,
                0x8C10EB24u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C601568u,
                0x8C66C400u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C60557Cu,
                0x8C605580u,
                12u,
                sizeof(std::uint16_t),
                0x8C605580u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            // Task 3 has two independent literal cells and two sites sharing
            // the same proven literal target cell; all remain singleton
            // declarations so each dispatch is identity-bound separately.
            katana::runtime::GameProjectJumpTable{
                0x8C608092u,
                0x8C60811Cu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C608098u,
                0x8C608120u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C6080A2u,
                0x8C608108u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C6080B4u,
                0x8C608108u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            // The PAL transform dispatcher loads two independent immutable
            // function literals immediately before these JSR sites. Keep
            // them as separate singleton call tables: 0x8C620C20 names the
            // QACR1 publisher at 0x8C6063F0, while 0x8C620C38 names the QACR0
            // publisher at 0x8C605DA8. This proves only the two call edges;
            // it does not promote either displaced hardware owner.
            katana::runtime::GameProjectJumpTable{
                0x8C620B9Au,
                0x8C620C20u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C620BE2u,
                0x8C620C38u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            // Three additional Run69 singleton transfers load their target
            // directly from authenticated PAL literals. The complete
            // producer-through-delay-slot ranges above ensure that no live
            // register or mutable table state is promoted by these entries.
            katana::runtime::GameProjectJumpTable{
                0x8C62FE88u,
                0x8C62FF20u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C634364u,
                0x8C634394u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C634E66u,
                0x8C634E8Cu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Jump},
            // Two independent transform-family callbacks loaded directly
            // from immutable PAL literal cells. Their target owners already
            // exist in the current FunctionMap; this binds only these calls.
            katana::runtime::GameProjectJumpTable{
                0x8C63874Eu,
                0x8C638808u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C6387ACu,
                0x8C638820u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C63A950u,
                0x8C63A954u,
                17u,
                sizeof(std::uint16_t),
                0x8C63A954u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C63AEF0u,
                0x8C63AEF4u,
                17u,
                sizeof(std::uint16_t),
                0x8C63AEF4u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            // Two immutable NINJA function-pointer tables selected by proven
            // attribute bits. Model the complete indirect-call closure rather
            // than promoting whichever target a single product run happens to
            // observe first.
            katana::runtime::GameProjectJumpTable{
                0x8C63EB3Au,
                0x8C6735F8u,
                2u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C63EB54u,
                0x8C673600u,
                4u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            // Each of these is an independent one-entry absolute call
            // literal. Keeping them separate prevents false target
            // correlation across the four dispatch sites.
            katana::runtime::GameProjectJumpTable{
                0x8C64C344u,
                0x8C64C3DCu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C64C37Cu,
                0x8C64C3E0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C64C394u,
                0x8C64C3E4u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C64C3A0u,
                0x8C64C3E8u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C64EE92u,
                0x8C64F05Cu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C654F00u,
                0x8C654F0Cu,
                12u,
                sizeof(std::uint16_t),
                0x8C654F04u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C654FE2u,
                0x8C654FE8u,
                12u,
                sizeof(std::uint16_t),
                0x8C654FE6u,
                katana::runtime::GameProjectTableEncoding::SignedRelative16,
                katana::runtime::GameProjectControlTransferKind::Jump},
            katana::runtime::GameProjectJumpTable{
                0x8C657718u,
                0x8C65774Cu,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C65CC2Au,
                0x8C65CCA8u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C6658D0u,
                0x8C665934u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C665B96u,
                0x8C665BE8u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectJumpTable{
                0x8C66B564u,
                0x8C66B5A0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectTableEncoding::Absolute32,
                katana::runtime::GameProjectControlTransferKind::Call}};
        auto sorted_callback_dispatches = callback_dispatches;
        std::sort(
            sorted_callback_dispatches.begin(), sorted_callback_dispatches.end(),
            [](const auto& left, const auto& right) {
                return left.dispatch_address < right.dispatch_address;
            });
        for (std::size_t index = 1u;
             index < sorted_callback_dispatches.size(); ++index) {
            if (sorted_callback_dispatches[index - 1u].dispatch_address ==
                sorted_callback_dispatches[index].dispatch_address)
                throw std::runtime_error(
                    "game-project jump-table dispatch identities overlap");
        }
        constexpr std::array base_subsystem_callback_tables{
            // Two immutable PAL object initializers load the same exact
            // 0x8C014936 tail thunk from their literal pools and publish it
            // to the object callback slot at +0x18.  Root both producer
            // cells so the native product carries the callback before a
            // mutable object instance selects it at runtime.
            katana::runtime::GameProjectCallbackTable{
                0x8C014B14u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectCallbackTable{
                0x8C014CE0u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectControlTransferKind::Call},
            // The PAL object initializer publishes two callbacks through
            // adjacent immutable literals. The +24 member at 0x8C051B0C is
            // already in the resident object-callback inventory; retain the
            // complete producer pair by rooting its +20 member as well.
            katana::runtime::GameProjectCallbackTable{
                0x8C051B08u,
                1u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectControlTransferKind::Call},
            // The resident PAL object-list descriptor at 0x8C1957E8 binds
            // 76 immutable 16-byte records at [0x8C195250, 0x8C195710).
            // The complete producer at 0x8C01AAE0 scales its signed r4 index
            // by 16, indexes the descriptor's +4 table pointer, loads each
            // record's +0 word and stores it directly to object+12. The
            // separate 0x8C01AD2C registrar accepts an already selected r4
            // callback and is not evidence for this table's siblings. The
            // common consumer calls object+12 at 0x8C01A01C. Four records
            // name generation-local 0x0C90xxxx overlay entries;
            // split around those cells so only all 72 resident Boot slots
            // become product roots. The overlay members remain governed by
            // their own image/generation-bound Loaded-AOT inventories.
            katana::runtime::GameProjectCallbackTable{
                0x8C195250u,
                46u,
                4u * sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectCallbackTable{
                0x8C195550u,
                7u,
                4u * sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectCallbackTable{
                0x8C1955D0u,
                1u,
                4u * sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectControlTransferKind::Call},
            katana::runtime::GameProjectCallbackTable{
                0x8C1955F0u,
                18u,
                4u * sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectControlTransferKind::Call},
            // The title's immutable 27-record spatial-callback family is
            // described by the adjacent header at 0x8C1957E8
            // (count=0x1B, records=0x8C195710).  Each eight-byte record keeps
            // its externally invoked callback at +4.  Runtime object records
            // copy these pointers into field +0x10 before the common
            // dispatcher calls them, so bind the complete source family
            // instead of learning one callback from each runtime crash.
            katana::runtime::GameProjectCallbackTable{
                0x8C195710u,
                27u,
                2u * sizeof(std::uint32_t),
                sizeof(std::uint32_t),
                katana::runtime::GameProjectControlTransferKind::Call},
            // The item-box dispatcher indexes this complete immutable PAL
            // family with index*8, reads the target at +0 and stops at the
            // null target immediately following the ninth record. The +4
            // words are data arguments, never executable entries.
            katana::runtime::GameProjectCallbackTable{
                0x8C1B9AE4u,
                9u,
                2u * sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectControlTransferKind::Call},
            // The producer-completion owner selects one of 44 x 8 static
            // title callbacks. The complete matrix contains only five unique
            // identity-bound targets; declaring the table makes those real
            // asynchronous entries roots without promoting any Kamui/device
            // continuation or arbitrary stored code pointer.
            katana::runtime::GameProjectCallbackTable{
                sonic_native_private::frame_completion::table,
                sonic_native_private::frame_completion::entry_count,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectControlTransferKind::Call},
            // The title's static subsystem descriptor is a 24-byte record
            // with two adjacent callback slots at +16/+20. Bind the complete
            // pair as one compact callback table: both targets are externally
            // invoked by the descriptor walker and therefore remain roots.
            katana::runtime::GameProjectCallbackTable{
                0x8C1C4930u,
                2u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectControlTransferKind::Call},
            // This immutable subsystem descriptor stores two adjacent PAL
            // function entries after its data pointer.  The second entry is
            // the complete 0x8C64D0E6 event owner observed by Egg Carrier;
            // root the full function pair instead of learning only the
            // runtime-selected member.
            katana::runtime::GameProjectCallbackTable{
                0x8C673C28u,
                2u,
                sizeof(std::uint32_t),
                0u,
                katana::runtime::GameProjectControlTransferKind::Call}};
        std::vector<katana::runtime::GameProjectCallbackTable>
            subsystem_callback_tables(
                base_subsystem_callback_tables.begin(),
                base_subsystem_callback_tables.end());
        subsystem_callback_tables.reserve(
            base_subsystem_callback_tables.size() +
            object_callback_candidate_analysis_roots.size());
        for (const auto& candidate : object_callback_candidate_analysis_roots) {
            subsystem_callback_tables.push_back(
                katana::runtime::GameProjectCallbackTable{
                    candidate.producer_cell,
                    1u,
                    sizeof(std::uint32_t),
                    0u,
                    katana::runtime::GameProjectControlTransferKind::Call});
        }
        std::sort(
            subsystem_callback_tables.begin(),
            subsystem_callback_tables.end(),
            [](const auto& left, const auto& right) {
                return left.table_address < right.table_address;
            });
        for (std::size_t index = 1u;
             index < subsystem_callback_tables.size(); ++index) {
            if (subsystem_callback_tables[index - 1u].table_address ==
                subsystem_callback_tables[index].table_address)
                throw std::runtime_error(
                    "game-project callback-table roots overlap");
        }
        constexpr std::size_t runtime_window_size = 0x10000u;
        constexpr std::size_t runtime_image_size = 0x4000u;
        std::ifstream runtime_input(
            std::filesystem::path(argv[3]),
            std::ios::binary | std::ios::ate);
        if (!runtime_input ||
            runtime_input.tellg() !=
                static_cast<std::streamoff>(runtime_window_size))
            throw std::invalid_argument(
                "runtime-handle-window must be the exact 64 KiB product capture");
        runtime_input.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> runtime_image_bytes(runtime_image_size);
        runtime_input.read(
            reinterpret_cast<char*>(runtime_image_bytes.data()),
            static_cast<std::streamsize>(runtime_image_bytes.size()));
        if (!runtime_input)
            throw std::runtime_error(
                "runtime-handle-window could not be read");
        std::ofstream runtime_image_output(
            std::filesystem::path(argv[2]),
            std::ios::binary | std::ios::trunc);
        runtime_image_output.write(
            reinterpret_cast<const char*>(runtime_image_bytes.data()),
            static_cast<std::streamsize>(runtime_image_bytes.size()));
        if (!runtime_image_output)
            throw std::runtime_error(
                "private runtime image could not be written");
        runtime_image_output.close();
        // Complete set of code-pointer roots stored inside the exact,
        // hash-bound 16 KiB runtime image. Direct callees remain discovered
        // normally by Katana; data/string pointer tables are intentionally
        // excluded.
        constexpr std::array<std::uint32_t, 15u> runtime_entries{
            0x20u,
            0x96u,
            0xA6u,
            0x1C0u,
            0x63Eu,
            0x732u,
            0x74Cu,
            0x92Eu,
            0x936u,
            0xDC0u,
            0x12F6u,
            0x13C0u,
            0x14A0u,
            0x15C0u,
            0x15D8u};
        const std::array runtime_images{
            katana::runtime::GameProjectRuntimeImage{
                "sa-pal-v1003-runtime-handle-v1",
                "sha256:81aba837f9de5e327ae593647b773ec8feb1f0f5e7277c885e43cee0dddddebd",
                0x89000000u,
                0x8C900000u,
                static_cast<std::uint32_t>(runtime_image_bytes.size()),
                runtime_entries}};

        std::vector<std::string> mapped_function_names;
        mapped_function_names.reserve(mapped_function_entries.size());
        std::vector<katana::runtime::GameProjectSymbol> mapped_functions;
        mapped_functions.reserve(mapped_function_entries.size());
        std::size_t stable_mapped_function_index = 0u;
        for (std::size_t index = 0u;
             index < mapped_function_entries.size();
             ++index) {
            std::string symbol_name;
            std::uint32_t symbol_size = 0u;
            switch (mapped_function_entries[index]) {
            case 0x8C015B80u:
                symbol_name = "sa_agent_exact_function_8c015b80";
                break;
            case 0x8C093658u:
                symbol_name = "sa_agent_exact_function_8c093658";
                break;
            case 0x8C0937B2u:
                symbol_name = "sa_agent_exact_function_8c0937b2";
                break;
            case 0x8C09D866u:
                symbol_name = "sa_agent_exact_function_8c09d866";
                break;
            case 0x8C087DC0u:
                // Added after the stable bulk map was published. Give the
                // exact tail thunk its identity-bound boundary name and size
                // without shifting the historical sa_aot_function_N symbols
                // that follow it. Matching the boundary size also makes the
                // duplicate symbol declaration losslessly idempotent.
                symbol_name = "sa_whole_game_static_entry_8c087dc0";
                symbol_size = 0x6u;
                break;
            default:
                symbol_name =
                    "sa_aot_function_" +
                    std::to_string(stable_mapped_function_index++);
                break;
            }
            mapped_function_names.push_back(std::move(symbol_name));
            mapped_functions.push_back({
                mapped_function_entries[index],
                mapped_function_names.back(),
                symbol_size,
                katana::runtime::GameProjectSymbolKind::Function});
        }
        if (stable_mapped_function_index != 4810u)
            throw std::runtime_error(
                "base function-entry symbol identity changed");

        katana::runtime::GameProjectDefinition definition;
        definition.project_id = "sonic-adventure-pal-v1003";
        definition.project_version = "74";
        definition.identity = {
            "73dd6546704fb1ac491fc44672442610d40ddf9bf34876a596c8c4f0739410e2",
            "1ST_READ.BIN",
            "sha256:b3563abfa536deacfbb508f44bc45936010e761865fe3d9ca4344511372768af"};
        definition.required_product_milestone =
            katana::runtime::RequiredProductMilestone::FirstVisibleGameFrame;
        definition.function_boundaries = functions;
        definition.jump_tables = sorted_callback_dispatches;
        definition.callback_tables = subsystem_callback_tables;
        definition.symbols = mapped_functions;
        definition.code_identities = code_identities;
        definition.runtime_images = runtime_images;
        definition.static_entries = whole_game_static_entries;

        const auto artifact =
            katana::runtime::GameProjectArtifact::write(
                std::filesystem::path(argv[1]), definition);
        const auto reloaded = katana::runtime::GameProjectArtifact::load(
            std::filesystem::path(argv[1]));
        const auto& reloaded_definition = reloaded->definition();
        if (reloaded->artifact_identity() != artifact->artifact_identity() ||
            reloaded_definition.project_version != definition.project_version ||
            reloaded_definition.function_boundaries.size() != functions.size() ||
            reloaded_definition.static_entries.size() !=
                whole_game_static_entries.size() ||
            reloaded_definition.callback_tables.size() !=
                subsystem_callback_tables.size())
            throw std::runtime_error(
                "written game-project artifact did not retain its boundary identity");
        for (std::size_t index = 0u; index < functions.size(); ++index) {
            const auto& expected = functions[index];
            const auto& actual = reloaded_definition.function_boundaries[index];
            if (actual.start != expected.start || actual.size != expected.size ||
                actual.symbol != expected.symbol)
                throw std::runtime_error(
                    "written game-project artifact changed a function boundary");
        }
        if (!std::equal(
                reloaded_definition.static_entries.begin(),
                reloaded_definition.static_entries.end(),
                whole_game_static_entries.begin(),
                whole_game_static_entries.end()))
            throw std::runtime_error(
                "written game-project artifact changed a static entry root");
        for (std::size_t index = 0u;
             index < subsystem_callback_tables.size(); ++index) {
            const auto& expected = subsystem_callback_tables[index];
            const auto& actual = reloaded_definition.callback_tables[index];
            if (actual.table_address != expected.table_address ||
                actual.entry_count != expected.entry_count ||
                actual.entry_stride != expected.entry_stride ||
                actual.pointer_offset != expected.pointer_offset ||
                actual.transfer != expected.transfer ||
                actual.image_id != expected.image_id)
                throw std::runtime_error(
                    "written game-project artifact changed a callback root");
        }
        std::cout << "KATANA_GAME_PROJECT_OK identity="
                  << artifact->artifact_identity()
                  << " boundaries=" << functions.size()
                  << " payload=" << std::filesystem::path(argv[2]).string()
                  << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "KATANA_GAME_PROJECT_ERROR " << error.what() << '\n';
        return 1;
    }
}
