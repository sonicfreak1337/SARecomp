#include "katana_port.hpp"
#include "katana/runtime/crash_capsule.hpp"
#include "katana/runtime/native_port_content.hpp"
#include "katana/runtime/native_port_graphics.hpp"
#include "sonic_presentation.hpp"
#include "sonic_user_paths.hpp"
#include "sonic_startup.hpp"
#include "sonic_input.hpp"
#include "sonic_menu_runtime.hpp"
#include "sonic_profiles.hpp"
#include "sonic_restart.hpp"
#include "sonic_diagnostics.hpp"
#include "sonic_internal_diagnostics.hpp"
#include "native_provider_identity.hpp"
#include "sonic_configuration.hpp"
#include "sonic_errors.hpp"

#include "katana/runtime/native_port_telemetry.hpp"
#include "katana/runtime/native_port_texture_asset.hpp"
#include "katana/runtime/native_port_platform.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <string_view>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "sonic_configuration.hpp"
#include "sonic_errors.hpp"
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#ifndef KATANA_PORT_BUILD_PROFILE_NAME
#define KATANA_PORT_BUILD_PROFILE_NAME "unspecified"
#endif
#ifndef KATANA_PORT_PGO_MODE_NAME
#define KATANA_PORT_PGO_MODE_NAME "off"
#endif
#ifndef KATANA_PORT_PGO_PROFILE_SHA256_NAME
#define KATANA_PORT_PGO_PROFILE_SHA256_NAME "none"
#endif

namespace {
std::filesystem::path native_product_user_data_root() {
    return sonic::paths::data_root(sonic::paths::executable());
}
std::atomic<std::uint32_t> native_product_crash_latch{0u};
std::atomic<katana::runtime::CpuState*> native_product_cpu{nullptr};
katana::runtime::CrashCapsule native_product_crash_capsule{};
#if defined(_WIN32)
std::atomic<HANDLE> native_product_crash_file{INVALID_HANDLE_VALUE};
void native_product_write_fault_handle(
        const HANDLE handle, std::string_view bytes) noexcept {
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return;
    while (!bytes.empty()) {
        DWORD written = 0u;
        const auto request = static_cast<DWORD>(
            std::min<std::size_t>(
                bytes.size(), std::numeric_limits<DWORD>::max()));
        if (WriteFile(handle, bytes.data(), request, &written, nullptr) ==
                FALSE || written == 0u) return;
        bytes.remove_prefix(written);
    }
}
#else
std::atomic<int> native_product_crash_file{-1};
void native_product_write_fault_descriptor(
        const int descriptor, std::string_view bytes) noexcept {
    if (descriptor < 0) return;
    while (!bytes.empty()) {
        const auto written = ::write(
            descriptor, bytes.data(), bytes.size());
        if (written < 0) {
            if (errno == EINTR) continue;
            return;
        }
        if (written == 0) return;
        bytes.remove_prefix(static_cast<std::size_t>(written));
    }
}
#endif
void native_product_write_fault_bytes(
        std::string_view bytes) noexcept {
    sonic::diagnostics::pending_crash.append(bytes);
#if !defined(SARECOMP_AUTOMATIC_CRASH_CAPSULES)
    return;
#endif
#if defined(_WIN32)
    native_product_write_fault_handle(
        GetStdHandle(STD_ERROR_HANDLE), bytes);
    native_product_write_fault_handle(
        native_product_crash_file.load(std::memory_order_acquire),
        bytes);
#else
    native_product_write_fault_descriptor(STDERR_FILENO, bytes);
    native_product_write_fault_descriptor(
        native_product_crash_file.load(std::memory_order_acquire),
        bytes);
#endif
}
void native_product_flush_fault_file() noexcept {
#if defined(_WIN32)
    const auto handle = native_product_crash_file.load(
        std::memory_order_acquire);
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
        static_cast<void>(FlushFileBuffers(handle));
#else
    const auto descriptor = native_product_crash_file.load(
        std::memory_order_acquire);
    if (descriptor >= 0) static_cast<void>(::fsync(descriptor));
#endif
}
void native_product_write_fault_u64(
        std::uint64_t value) noexcept;
class NativeProductCrashSession final {
  public:
    void arm(const std::filesystem::path& executable_path) noexcept {
#if defined(SARECOMP_AUTOMATIC_CRASH_CAPSULES)
      try {
        const auto root = sonic::paths::data_root(executable_path) / "logs";
        std::error_code directory_error;
        std::filesystem::create_directories(root, directory_error);
        if (directory_error)
            throw std::system_error(
                directory_error, "bringup-crash-session-directory");
        const auto epoch_milliseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
#if defined(_WIN32)
        const auto process_id = static_cast<std::uint64_t>(
            GetCurrentProcessId());
#else
        const auto process_id = static_cast<std::uint64_t>(::getpid());
#endif
        path_ = root / ("katana-crash-session-" +
            std::to_string(epoch_milliseconds) + "-" +
            std::to_string(process_id) + ".log");
#if defined(_WIN32)
        const auto handle = CreateFileW(
            path_.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::system_error(
                static_cast<int>(GetLastError()), std::system_category(),
                "bringup-crash-session-open");
        native_product_crash_file.store(handle, std::memory_order_release);
#else
        const auto descriptor = ::open(
            path_.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (descriptor < 0)
            throw std::system_error(
                errno, std::generic_category(),
                "bringup-crash-session-open");
        native_product_crash_file.store(
            descriptor, std::memory_order_release);
#endif
        native_product_write_fault_bytes(
            "KATANA_CRASH_CAPSULE_ARMED version=1 epoch_ms=");
        native_product_write_fault_u64(epoch_milliseconds);
        native_product_write_fault_bytes(" pid=");
        native_product_write_fault_u64(process_id);
        native_product_write_fault_bytes("\n");
        native_product_flush_fault_file();
        std::cerr << "KATANA_CRASH_CAPSULE_PATH path="
                  << path_.string() << '\n';
      } catch(...) {
        std::fputs("KATANA_CRASH_CAPSULE_FILE_UNAVAILABLE stderr_only=1\n",stderr);
      }
#else
      (void)executable_path;
#endif
    }
    ~NativeProductCrashSession() noexcept {
        native_product_flush_fault_file();
#if defined(_WIN32)
        const auto handle = native_product_crash_file.exchange(
            INVALID_HANDLE_VALUE, std::memory_order_acq_rel);
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
            static_cast<void>(CloseHandle(handle));
#else
        const auto descriptor = native_product_crash_file.exchange(
            -1, std::memory_order_acq_rel);
        if (descriptor >= 0) static_cast<void>(::close(descriptor));
#endif
    }
  private:
    std::filesystem::path path_;
};
void native_product_write_fault_u64(
        const std::uint64_t value) noexcept {
    std::array<char, 32u> bytes{};
    const auto converted = std::to_chars(
        bytes.data(), bytes.data() + bytes.size(), value);
    if (converted.ec == std::errc{})
        native_product_write_fault_bytes(std::string_view(
            bytes.data(), static_cast<std::size_t>(
                converted.ptr - bytes.data())));
}
void native_product_write_crash_capsule_v1(
        const katana::runtime::CrashCapsule& capsule) noexcept {
    native_product_write_fault_bytes(
        "KATANA_CRASH_CAPSULE version=");
    native_product_write_fault_u64(
        katana::runtime::crash_capsule_contract_version);
    native_product_write_fault_bytes(" last_pc=");
    native_product_write_fault_u64(capsule.last_pc);
    native_product_write_fault_bytes(" last_block=");
    native_product_write_fault_u64(capsule.last_block);
    native_product_write_fault_bytes(" last_mmio_address=");
    native_product_write_fault_u64(capsule.last_mmio_address);
    native_product_write_fault_bytes(" last_mmio_value=");
    native_product_write_fault_u64(capsule.last_mmio_value);
    native_product_write_fault_bytes(" last_scheduler_cycle=");
    native_product_write_fault_u64(capsule.last_scheduler_cycle);
    native_product_write_fault_bytes(
        " last_scheduler_event_id=");
    native_product_write_fault_u64(
        capsule.last_scheduler_event_id);
    native_product_write_fault_bytes(
        " last_scheduler_event_kind=");
    native_product_write_fault_u64(
        capsule.last_scheduler_event_kind);
    native_product_write_fault_bytes(" first_error=");
    native_product_write_fault_u64(capsule.first_error_code);
    native_product_write_fault_bytes(" first_error_pc=");
    native_product_write_fault_u64(capsule.first_error_pc);
    native_product_write_fault_bytes(" first_error_target=");
    native_product_write_fault_u64(capsule.first_error_target);
    native_product_write_fault_bytes(" ring_events=");
    native_product_write_fault_u64(capsule.event_count);
    native_product_write_fault_bytes("\n");
}
bool native_product_claim_crash() noexcept {
    std::uint32_t expected = 0u;
    const bool claimed=native_product_crash_latch.compare_exchange_strong(
        expected, 1u, std::memory_order_acq_rel,
        std::memory_order_relaxed);
    if(claimed)sonic::diagnostics::pending_crash.begin();
    return claimed;
}
void native_product_capture_cpu() noexcept {
    const auto* const cpu = native_product_cpu.load(
        std::memory_order_acquire);
    if (cpu == nullptr) return;
    native_product_crash_capsule.note_block(
        cpu->pc, cpu->active_block_virtual_start,
        0u);
    native_product_crash_capsule.note_v2_cpu(cpu->pc, cpu->pr);
    native_product_crash_capsule.note_v2_dispatch(
        cpu->active_instruction_pc,
        cpu->active_block_virtual_start);
    native_product_crash_capsule.note_v2_wait(
        cpu->sleeping ? 1u : 0u, 0u,
        0u, 0u);
    native_product_crash_capsule.note_v3_cpu_state(
        cpu->r, cpu->read_sr(), cpu->t, cpu->sleeping,
        cpu->trap_pending, cpu->gbr, cpu->vbr, cpu->mach,
        cpu->macl, cpu->fpul, cpu->read_fpscr(),
        cpu->active_instruction_physical_pc,
        cpu->active_block_physical_start, cpu->active_block_size,
        cpu->attempted_guest_instructions,
        cpu->retired_guest_instructions, cpu->total_guest_cycles,
        cpu->pending_guest_cycles, cpu->exception_generation,
        cpu->last_exception_instruction_pc,
        cpu->last_exception_instruction_physical_pc,
        cpu->last_exception_owner_pc);
    native_product_crash_capsule.reset_v3_memory_windows();
    const auto capture_memory_window = [&](
            const std::uint32_t guest_focus,
            const std::uint32_t source_mask) noexcept {
        const auto aligned_focus = guest_focus & ~std::uint32_t{3u};
        constexpr std::uint32_t prefix_bytes = 16u;
        constexpr std::uint32_t captured_bytes =
            static_cast<std::uint32_t>(
                katana::runtime::crash_capsule_memory_window_word_capacity *
                sizeof(std::uint32_t));
        if (aligned_focus < prefix_bytes) return;
        const auto guest_base = aligned_focus - prefix_bytes;
        if (guest_base >
            std::numeric_limits<std::uint32_t>::max() -
                (captured_bytes - sizeof(std::uint32_t))) return;
        const auto physical_base =
            katana::runtime::canonical_physical_address(guest_base);
        std::array<std::uint32_t,
            katana::runtime::crash_capsule_memory_window_word_capacity>
            words{};
        std::uint32_t valid_word_mask = 0u;
        for (std::size_t index = 0u; index < words.size(); ++index) {
            const auto offset = static_cast<std::uint32_t>(
                index * sizeof(std::uint32_t));
            if (cpu->memory.try_read_direct_linear_u32(
                    physical_base + offset, words[index]))
                valid_word_mask |= 1u << index;
        }
        native_product_crash_capsule.note_v3_memory_window(
            guest_focus, guest_base, physical_base, source_mask,
            valid_word_mask, words);
    };
    for (std::size_t index = 0u; index < cpu->r.size(); ++index)
        capture_memory_window(cpu->r[index], 1u << index);
    capture_memory_window(cpu->pc, 1u << 16u);
    capture_memory_window(cpu->pr, 1u << 17u);
    capture_memory_window(cpu->gbr, 1u << 18u);
    capture_memory_window(cpu->vbr, 1u << 19u);
}
void native_product_emit_claimed_crash(
        const std::uint32_t host_code,
        const std::string_view host_type,
        const std::uint32_t contract_code,
        const std::string_view contract_type,
        const std::string_view contract_detail = {},
        const std::uint32_t callsite = 0u,
        const std::uint32_t target = 0u) noexcept {
    const auto effective_callsite = callsite != 0u
        ? callsite : native_product_crash_capsule.last_pc;
    const auto effective_target = target != 0u
        ? target : native_product_crash_capsule.last_pc;
    native_product_crash_capsule.note_v2_host_exception(
        host_code, host_type);
    native_product_crash_capsule.note_v2_contract(
        contract_code, contract_type);
    if (!contract_detail.empty())
        native_product_crash_capsule.note_v3_contract_detail(
            contract_detail);
    if (effective_callsite != 0u || effective_target != 0u)
        native_product_crash_capsule.note_v2_dispatch(
            effective_callsite, effective_target);
    if (native_product_crash_capsule.first_error_latched == 0u) {
        native_product_crash_capsule.note_first_error(
            contract_code != 0u ? contract_code : 0xFFFFFFFFu,
            effective_callsite, effective_target);
    }
    native_product_write_crash_capsule_v1(
        native_product_crash_capsule);
    const auto line = katana::runtime::serialize_crash_capsule_v4(
        native_product_crash_capsule);
    native_product_write_fault_bytes(
        "KATANA_CRASH_CAPSULE version=4 " );
    native_product_write_fault_bytes(line.view());
    native_product_write_fault_bytes("\n");
    const auto line_v5 = katana::runtime::
        serialize_crash_capsule_v5(native_product_crash_capsule);
    native_product_write_fault_bytes(
        "KATANA_CRASH_CAPSULE version=5 " );
    native_product_write_fault_bytes(line_v5.view());
    native_product_write_fault_bytes("\n");
    sonic::diagnostics::pending_crash.finish();
    native_product_flush_fault_file();
}
void native_product_emit_crash(
        const std::uint32_t host_code,
        const std::string_view host_type,
        const std::uint32_t contract_code,
        const std::string_view contract_type,
        const std::string_view contract_detail = {},
        const std::uint32_t callsite = 0u,
        const std::uint32_t target = 0u) noexcept {
    native_product_capture_cpu();
    if (!native_product_claim_crash()) return;
    native_product_emit_claimed_crash(
        host_code, host_type, contract_code, contract_type,
        contract_detail, callsite, target);
}
class NativeProductCpuBinding final {
  public:
    void bind(katana::runtime::CpuState& cpu) noexcept {
        native_product_cpu.store(&cpu, std::memory_order_release);
    }
    ~NativeProductCpuBinding() noexcept {
        native_product_cpu.store(nullptr, std::memory_order_release);
    }
};
class NativePortTitleStateCleanupGuard final {
  public:
    explicit NativePortTitleStateCleanupGuard(
            katana::runtime::NativePortContext& context) noexcept
        : context_(&context) {}
    NativePortTitleStateCleanupGuard(
        const NativePortTitleStateCleanupGuard&) = delete;
    NativePortTitleStateCleanupGuard& operator=(
        const NativePortTitleStateCleanupGuard&) = delete;
    ~NativePortTitleStateCleanupGuard() noexcept { release_now(); }
    void release_now() noexcept {
        if (context_ == nullptr) return;
        auto& context = *context_;
        context_ = nullptr;
        const auto cleanup = context.title_state_cleanup;
        context.title_state_cleanup = nullptr;
        if (cleanup != nullptr) cleanup(context);
    }
  private:
    katana::runtime::NativePortContext* context_ = nullptr;
};
#if defined(_WIN32)
std::atomic<LPTOP_LEVEL_EXCEPTION_FILTER>
    native_product_previous_filter{nullptr};
thread_local bool native_product_seh_active = false;
LONG WINAPI native_product_unhandled_exception_filter(
        EXCEPTION_POINTERS* exception) noexcept {
    if (!native_product_seh_active) {
        native_product_seh_active = true;
        const auto code = exception != nullptr &&
                exception->ExceptionRecord != nullptr
            ? static_cast<std::uint32_t>(
                  exception->ExceptionRecord->ExceptionCode)
            : 0xFFFFFFFFu;
        if (native_product_claim_crash()) {
        native_product_capture_cpu();
        native_product_write_fault_bytes(
            "KATANA_WINDOWS_SEH {\"schema\":1,\"code\":");
        native_product_write_fault_u64(code);
        native_product_write_fault_bytes("}\n");
        native_product_emit_claimed_crash(
            code, "windows-seh", code, "host-seh-boundary",
            {}, 0u, 0u);
        sonic::diagnostics::record(sonic::diagnostics::Failure::Runtime,code);
        try {
            if(sonic::profiles::library_root().empty())
                sonic::profiles::initialize(native_product_user_data_root());
            sonic::errors::show();
        } catch(...) {}
        }
    }
    const auto previous = native_product_previous_filter.load(
        std::memory_order_acquire);
    if (previous != nullptr &&
        previous != &native_product_unhandled_exception_filter)
        return previous(exception);
    return EXCEPTION_CONTINUE_SEARCH;
}
class NativeProductExceptionFilterScope final {
  public:
    NativeProductExceptionFilterScope() noexcept
        : previous_(SetUnhandledExceptionFilter(
              &native_product_unhandled_exception_filter)) {
        native_product_previous_filter.store(
            previous_, std::memory_order_release);
    }
    ~NativeProductExceptionFilterScope() noexcept {
        static_cast<void>(SetUnhandledExceptionFilter(previous_));
    }
  private:
    LPTOP_LEVEL_EXCEPTION_FILTER previous_ = nullptr;
};
#endif
} // namespace

int run_game(int argc, char** argv) {
    std::optional<katana::runtime::NativePortMemory> diagnostic_memory;
    NativeProductCrashSession native_product_crash_session;
#if defined(_WIN32)
    NativeProductExceptionFilterScope native_product_seh_filter;
#endif
    NativeProductCpuBinding diagnostic_cpu_binding;
    katana::runtime::NativePortTelemetry native_performance_telemetry;
    std::optional<katana::runtime::NativePortTelemetryWriter>
        native_performance_writer;
    std::optional<katana::runtime::NativePortGraphicsSnapshot>
        native_performance_graphics_snapshot;
    std::string native_graphics_failure_detail;
    std::optional<katana::runtime::NativePortGraphicsFailure>
        native_graphics_failure_kind;
    const auto* const native_performance_telemetry_opt_in =
        std::getenv("KATANA_NATIVE_PERFORMANCE_TELEMETRY");
    const bool native_performance_telemetry_enabled =
        native_performance_telemetry_opt_in != nullptr &&
        std::string_view(native_performance_telemetry_opt_in) == "1";
    bool native_performance_snapshot_emitted = false;
    const auto emit_native_performance_snapshot = [&]() noexcept {
        try {
            if (!native_performance_telemetry_enabled ||
                native_performance_snapshot_emitted)
                return;
            native_performance_snapshot_emitted = true;
            if (native_performance_writer)
                native_performance_telemetry.publish(
                    *native_performance_writer);
            const auto snapshot = native_performance_telemetry.snapshot();
            auto snapshot_json = snapshot.serialize_json();
            if (native_performance_graphics_snapshot) {
                if (snapshot_json.empty() || snapshot_json.back() != '}')
                    throw std::runtime_error(
                        "native-performance-snapshot-json");
                snapshot_json.pop_back();
                const auto& graphics =
                    *native_performance_graphics_snapshot;
                snapshot_json +=
                    ",\"graphics\":{\"available\":true";
                snapshot_json += ",\"render_producer_wait_ns\":" +
                    std::to_string(graphics.render_producer_wait_ns);
                snapshot_json +=
                    ",\"render_resource_fence_wait_ns\":" +
                    std::to_string(
                        graphics.render_resource_fence_wait_ns);
                snapshot_json += ",\"resource_fence_count\":" +
                    std::to_string(graphics.resource_fence_count);
                snapshot_json += ",\"frame_prefix_publications\":" +
                    std::to_string(graphics.frame_prefix_publications);
                snapshot_json += "}}";
            }
            std::cout << "KATANA_NATIVE_PERFORMANCE_SNAPSHOT "
                      << snapshot_json << '\n';
        } catch (...) {}
    };
    const bool input_record_launch = (argc == 3 || argc == 5) &&
        std::string_view(argv[1]) == "--record-input" &&
        std::string_view(argv[2]).size() != 0u;
    const bool input_replay_launch = (argc == 3 || argc == 5) &&
        std::string_view(argv[1]) == "--replay-input" &&
        std::string_view(argv[2]).size() != 0u;
#ifdef _WIN32
    bool automatic_input_record_launch = argc == 1 && sonic::diagnostics::runtime_checks_enabled();
#else
    bool automatic_input_record_launch = false;
#endif
    const bool direct_launch = argc == 1 || input_record_launch ||
        input_replay_launch;
    try {
        constexpr bool hardware_closure_complete = false;
        constexpr std::size_t hardware_gap_count = 96u;
        constexpr bool has_bringup_probes = true;
        constexpr bool product_contract_complete =
            hardware_closure_complete && !has_bringup_probes;
        const bool explicit_bringup = !product_contract_complete &&
            (direct_launch ||
             ((argc == 4 || argc == 6) &&
              std::string_view(argv[1]) ==
                  "--bringup-incomplete-hardware-closure" &&
              std::string_view(argv[2]) == "--content-root"));
        const bool closed_product = product_contract_complete &&
            (direct_launch ||
             ((argc == 3 || argc == 5) &&
              std::string_view(argv[1]) == "--content-root"));
        const auto optional_argument =
            (input_record_launch || input_replay_launch) ? 3 :
            (explicit_bringup ? 4 : 3);
        const bool has_presentation_fps =
            (explicit_bringup || closed_product) &&
            argc == optional_argument + 2 &&
            std::string_view(argv[optional_argument]) ==
                "--presentation-fps";
        if (!explicit_bringup && !closed_product) {
            std::cerr << "usage: game "
                      << (product_contract_complete
                              ? ""
                              : "--bringup-incomplete-hardware-closure ")
                      << "--content-root <verified-native-content> "
                         "[--presentation-fps <title-supported>]\n"
                         "       game\n"
                         "       game (--record-input <trace.kat1>"
                         " | --replay-input <trace.kat1>) "
                         "[--presentation-fps <title-supported>]\n";
            return 2;
        }
        if (argc == optional_argument + 2 &&
            !has_presentation_fps) {
            std::cerr << "usage: invalid optional argument\n";
            return 2;
        }
        std::uint32_t presentation_fps = 0u;
        if (has_presentation_fps) {
            const std::string_view value(
                argv[optional_argument + 1]);
            const auto parsed = std::from_chars(
                value.data(), value.data() + value.size(),
                presentation_fps);
            if (parsed.ec != std::errc{} ||
                parsed.ptr != value.data() + value.size() ||
                presentation_fps == 0u) {
                std::cerr << "presentation fps must be a positive integer\n";
                return 2;
            }
        }
        if (explicit_bringup)
            std::cerr << "KATANA_NATIVE_PORT_BRINGUP "
                      << "unresolved_hardware_sites="
                      << hardware_gap_count
                      << " probes=" << has_bringup_probes << '\n';
        std::cerr << "KATANA_NATIVE_BUILD_PROFILE name="
                  << KATANA_PORT_BUILD_PROFILE_NAME
                  << " pgo_mode="
                  << KATANA_PORT_PGO_MODE_NAME
                  << " pgo_profile_sha256="
                  << KATANA_PORT_PGO_PROFILE_SHA256_NAME
                  << " aot_all_o2="
                  << (std::string_view(KATANA_PORT_BUILD_PROFILE_NAME) ==
                      "performance") << '\n';
        std::error_code executable_error;
        auto executable_path = std::filesystem::weakly_canonical(
            std::filesystem::path(argv[0]), executable_error);
        if (executable_error || executable_path.empty())
            executable_path = std::filesystem::absolute(argv[0]);
        sonic::restart::recover(executable_path);
        if (!sonic::configuration::first_start(executable_path)) return 0;
        sonic::startup::Session startup;
        if (explicit_bringup)
            native_product_crash_session.arm(executable_path);
        sonic::presentation::initialize(executable_path);
        sonic::menu::initialize(executable_path);
        sonic::profiles::initialize(native_product_user_data_root());
        sonic::input::set_replay(input_replay_launch);
        std::filesystem::path content_root;
        if (direct_launch) {
            const auto configuration_path =
#ifdef _WIN32
                executable_path.parent_path() /
#else
                sonic::paths::content_store_root(executable_path) /
#endif
                "katana-content-root.txt";
            std::error_code configuration_error;
            const bool has_configuration =
                std::filesystem::exists(configuration_path,
                                        configuration_error);
            if (configuration_error)
                throw std::runtime_error(
                    "direct-launch-content-configuration-unreadable");
            if (has_configuration) {
                std::ifstream configuration(configuration_path);
                std::string configured_root;
                if (!configuration ||
                    !std::getline(configuration, configured_root))
                    throw std::runtime_error(
                        "direct-launch-content-configuration-empty");
                const auto first = configured_root.find_first_not_of(
                    " \t\r\n");
                const auto last = configured_root.find_last_not_of(
                    " \t\r\n");
                if (first == std::string::npos)
                    throw std::runtime_error(
                        "direct-launch-content-configuration-empty");
                configured_root = configured_root.substr(
                    first, last - first + 1u);
                std::string trailing_line;
                while (std::getline(configuration, trailing_line)) {
                    if (trailing_line.find_first_not_of(
                            " \t\r\n") != std::string::npos)
                        throw std::runtime_error(
                            "direct-launch-content-configuration-multiline");
                }
                content_root =
                    std::filesystem::path(configured_root);
                if (content_root.is_relative())
                    content_root = configuration_path.parent_path() /
                                   content_root;
            } else {
                content_root = executable_path.parent_path() /
                               "content";
            }
            std::cerr << "KATANA_NATIVE_DIRECT_LAUNCH content_root="
                      << content_root.string() << '\n';
        } else {
            const auto content_root_argument =
                explicit_bringup ? 3 : 2;
            content_root = argv[content_root_argument];
        }
        std::filesystem::path input_trace_path;
        if (input_record_launch || input_replay_launch) {
            input_trace_path = std::filesystem::path(argv[2]);
            if (input_trace_path.is_relative())
                input_trace_path = executable_path.parent_path() /
                                   input_trace_path;
        } else if (automatic_input_record_launch) {
          try {
            const auto epoch_milliseconds = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count());
#if defined(_WIN32)
            const auto process_id = static_cast<std::uint64_t>(
                GetCurrentProcessId());
#else
            const auto process_id = static_cast<std::uint64_t>(
                ::getpid());
#endif
            const auto directory=sonic::paths::data_root(executable_path)/"recordings";
            std::filesystem::create_directories(directory);
            input_trace_path = directory / ("katana-input-" +
                std::to_string(epoch_milliseconds) + "-" +
                std::to_string(process_id) + ".kat1");
          } catch(...) {
            automatic_input_record_launch=false;input_trace_path.clear();
            std::fputs("KATANA_AUTOMATIC_INPUT_RECORD_UNAVAILABLE recording_disabled=1\n",stderr);
          }
        }
        std::error_code content_root_error;
        if (!std::filesystem::is_directory(content_root,
                                           content_root_error) ||
            content_root_error)
            throw std::runtime_error(
                "native content directory missing; set "
                "katana-content-root.txt beside game.exe");
        const auto& definition = katana_port_generated::native_port_definition();
        katana::runtime::validate_native_port_definition(definition);
        native_product_crash_capsule.note_v2_runtime_module(
            definition.project_id, 1u);
        native_product_crash_capsule.note_v2_source_module(
            definition.executable.content_identity, 1u, 0u);
        if (presentation_fps == 0u)
            presentation_fps = sonic::presentation::settings().presentation_fps;
        if (presentation_fps < definition.frame_timing
                                   .simulation_rate_hz ||
            presentation_fps > definition.frame_timing
                                   .maximum_presentation_rate_hz) {
            std::cerr << "presentation fps outside title contract: "
                      << definition.frame_timing.simulation_rate_hz
                      << ".."
                      << definition.frame_timing
                             .maximum_presentation_rate_hz
                      << '\n';
            return 2;
        }
        const auto& link = katana::runtime::native_port_link_contract();
        if (link.version != katana::runtime::native_port_profile_contract_version ||
            link.allows_guest_cpu_interpreter ||
            link.allows_legacy_device_runtime || link.allows_software_pvr ||
            link.allows_ta_packet_renderer ||
            link.allows_aica_command_translation)
            throw std::runtime_error("native-port-link-contract");
        sonic::startup::prefetch_program();
        sonic::startup::phase("Loading game data...");
        diagnostic_memory.emplace(
            definition.bootstrap.cache_control_value);
        auto& memory = *diagnostic_memory;
        memory.load_verified_images(
            content_root,
            definition.images);
        auto& cpu = memory.cpu();
        diagnostic_cpu_binding.bind(cpu);
        if (sonic::diagnostics::runtime_checks_enabled())
            cpu.memory.attach_crash_capsule(native_product_crash_capsule);
        katana::runtime::reset_cpu(
            cpu, {definition.bootstrap.entry_point,
                  definition.bootstrap.stack_pointer,
                  definition.bootstrap.vector_base,
                  definition.bootstrap.status_register,
                  definition.bootstrap.fpscr});
        katana::runtime::NativePortPlatformConfig platform_config;
        if (!input_replay_launch)
            platform_config.keyboard_controls = std::make_shared<
                katana::runtime::NativePortKeyboardControls>();
        platform_config.content_root = content_root;
        platform_config.user_data_root =
            sonic::profiles::data_root(sonic::presentation::settings().active_profile);
        platform_config.project_id = definition.project_id;
        if (const auto* initial_state = std::getenv(
                "KATANA_NATIVE_INPUT_START_STATE");
            initial_state != nullptr && *initial_state != 0)
            platform_config.input_initial_state_path =
                std::filesystem::u8path(initial_state);
        platform_config.input_identity = "katana-native-input-compatibility-v1-sha256-123a38b24cdaceb8207236dd8b8f27d6cf174884c219deb83548728f15817084";
        if (input_record_launch || automatic_input_record_launch)
            platform_config.input_record_path = input_trace_path;
        platform_config.stop_input_recording_at_capacity =
            automatic_input_record_launch;
        if (input_replay_launch)
            platform_config.input_replay_path = input_trace_path;
        const auto input_trace_mode = input_replay_launch ? 2u :
            (input_record_launch ? 1u :
             (automatic_input_record_launch ? 3u : 0u));
        if (input_trace_mode != 0u) {
            native_product_crash_capsule.note_v3_input_trace(
                input_trace_path.filename().string(),
                platform_config.input_identity, input_trace_mode);
            std::cerr << "KATANA_NATIVE_INPUT_TRACE mode="
                      << input_trace_mode << " file="
                      << input_trace_path.string() << '\n';
        }
        sonic::startup::phase("Connecting controllers...");
        katana::runtime::NativePortPlatformServices platform(
            platform_config);
        katana::runtime::NativePortGraphicsConfig graphics_config;
        graphics_config.keyboard_controls =
            platform_config.keyboard_controls;
        graphics_config.title = definition.project_id;
        sonic::presentation::configure(graphics_config);
        const auto native_product_development_state_directory =
            (platform_config.user_data_root / definition.project_id /
             "states").string();
        graphics_config.development_state_directory =
            native_product_development_state_directory;
        katana::runtime::NativePortFramePacingConfig frame_pacing;
        frame_pacing.simulation_rate_hz =
            definition.frame_timing.simulation_rate_hz;
        frame_pacing.presentation_rate_hz = presentation_fps;
        frame_pacing.maximum_presentation_rate_hz =
            definition.frame_timing.maximum_presentation_rate_hz;
        graphics_config.telemetry =
            native_performance_telemetry_enabled
                ? &native_performance_telemetry : nullptr;
        sonic::startup::phase("Preparing graphics...");
        katana::runtime::NativePortDesktopHost host(
            graphics_config, frame_pacing);
        if(!sonic::restart::confirm_display(host,platform))return 0;
        sonic::startup::phase("Starting game...");
        bool frame_pacing_snapshot_emitted = false;
        const auto emit_terminal_runtime_telemetry = [&]() noexcept {
            try {
                if (!frame_pacing_snapshot_emitted) {
                    frame_pacing_snapshot_emitted = true;
                    const auto snapshot = host.frame_pacing_snapshot();
                    std::cout << "KATANA_NATIVE_FRAME_PACING_SNAPSHOT {\"schema\":1"
                              << ",\"simulation_frames\":"
                              << snapshot.simulation_frames
                              << ",\"presentation_frames\":"
                              << snapshot.presentation_frames
                              << ",\"repeated_presentations\":"
                              << snapshot.repeated_presentations
                              << ",\"late_simulation_frames\":"
                              << snapshot.late_simulation_frames
                              << ",\"missed_presentation_deadlines\":"
                              << snapshot.missed_presentation_deadlines
                              << ",\"simulation_rate_hz\":"
                              << snapshot.simulation_rate_hz
                              << ",\"presentation_rate_hz\":"
                              << snapshot.presentation_rate_hz
                              << ",\"enabled\":"
                              << (snapshot.enabled ? "true" : "false")
                              << "}\n" << std::flush;
                }
            } catch (...) {}
            emit_native_performance_snapshot();
        };
        std::optional<katana::runtime::NativePortTelemetryHostProxy>
            telemetry_host;
        if (native_performance_telemetry_enabled) {
            native_performance_writer.emplace(
                &native_performance_telemetry);
            telemetry_host.emplace(
                host, *native_performance_writer);
        }
        std::cerr << "KATANA_NATIVE_FRAME_PACING simulation_fps="
                  << frame_pacing.simulation_rate_hz
                  << " presentation_fps="
                  << frame_pacing.presentation_rate_hz
                  << " catch_up=disabled\n";
        katana::runtime::NativePortTextureRegistry textures(
            host.graphics());
        katana::runtime::NativePortContext context;
        NativePortTitleStateCleanupGuard title_state_cleanup(
            context);
        context.cpu = &cpu;
        context.host = &host;
        if (telemetry_host) context.host = &*telemetry_host;
        context.telemetry = native_performance_telemetry_enabled
            ? &native_performance_telemetry : nullptr;
        context.telemetry_writer = native_performance_writer
            ? &*native_performance_writer : nullptr;
        context.graphics = &host.graphics();
        context.platform = &platform;
        context.textures = &textures;
        context.cpu_control = &memory.cpu_control();
        context.crash_capsule = sonic::diagnostics::runtime_checks_enabled()
            ? &native_product_crash_capsule : nullptr;
        if (const auto* timeout = std::getenv(
                "KATANA_NATIVE_DIAGNOSTIC_TIMEOUT_MS");
            timeout != nullptr) {
            std::uint64_t timeout_ms = 0u;
            const std::string_view value(timeout);
            const auto parsed = std::from_chars(
                value.data(), value.data() + value.size(), timeout_ms);
            constexpr std::uint64_t maximum_timeout_ms =
                20u * 60u * 1000u;
            if (parsed.ec != std::errc{} ||
                parsed.ptr != value.data() + value.size() ||
                timeout_ms == 0u || timeout_ms > maximum_timeout_ms)
                throw std::runtime_error(
                    "native-diagnostic-timeout-invalid");
            const auto now = host.monotonic_time_nanoseconds();
            if (timeout_ms >
                (std::numeric_limits<std::uint64_t>::max() - now) /
                    1000000u)
                throw std::runtime_error(
                    "native-diagnostic-timeout-overflow");
            context.host_deadline_nanoseconds =
                now + timeout_ms * 1000000u;
        }
        const auto capture_product_state = [&]() noexcept {
            native_product_crash_capsule.note_v3_product_state(
                static_cast<std::uint32_t>(context.stop_reason),
                static_cast<std::uint32_t>(context.bootstrap_phase),
                context.frame_index, host.presented_frames());
            try {
                const auto snapshot = platform.snapshot();
                native_product_crash_capsule.note_v3_platform(
                    snapshot.input_polls,
                    snapshot.input_connection_generation);
            } catch (...) {}
            try {
                const auto snapshot = host.graphics().snapshot();
                const auto& witness = snapshot.last_contract_failure;
                if (witness.valid) {
                    std::string detail{"graphics-contract"};
                    const auto append = [&](
                            const std::string_view name,
                            const std::uint64_t value) {
                        detail.push_back(';');
                        detail.append(name);
                        detail.push_back('=');
                        detail.append(std::to_string(value));
                    };
                    append("failure", static_cast<std::uint32_t>(
                        witness.failure));
                    append("frame", witness.frame);
                    append("draw", witness.draw_sequence);
                    append("batch", witness.batch_identity);
                    append("order", witness.submission_order);
                    append("batch_semantic",
                        static_cast<std::uint32_t>(
                            witness.batch_semantic));
                    append("draw_class",
                        static_cast<std::uint32_t>(
                            witness.draw_class));
                    append("logical_use",
                        static_cast<std::uint32_t>(
                            witness.diagnostics.logical_use));
                    append("origin",
                        static_cast<std::uint32_t>(
                            witness.diagnostics.origin));
                    append("intent",
                        static_cast<std::uint32_t>(
                            witness.diagnostics.intent));
                    append("material",
                        witness.diagnostics.material_identity);
                    append("origin_identity",
                        witness.diagnostics.origin_identity);
                    append("model",
                        witness.diagnostics.model_identity);
                    append("texture_list",
                        witness.diagnostics.texture_list_index);
                    append("mesh",
                        witness.diagnostics.mesh_index);
                    append("primitive",
                        witness.diagnostics.primitive_index);
                    append("texture_list_identity",
                        witness.diagnostics.texture_binding
                            .texture_list_identity);
                    append("texture_list_epoch",
                        witness.diagnostics.texture_binding
                            .texture_list_epoch);
                    append("last_writer",
                        witness.diagnostics.texture_binding
                            .last_writer_identity);
                    append("last_writer_sequence",
                        witness.diagnostics.texture_binding
                            .last_writer_sequence);
                    append("expected_asset",
                        witness.diagnostics.texture_binding
                            .expected_asset_identity);
                    append("resolver",
                        static_cast<std::uint32_t>(
                            witness.diagnostics.texture_binding
                                .resolver));
                    // Keep the draw witness until the outer graphics catch;
                    // emitting error.what() must not overwrite this context.
                    native_graphics_failure_detail = std::move(detail);
                    native_graphics_failure_kind = witness.failure;
                }
            } catch (...) {}
        };
        try {
            katana_native_bootstrap_dispatch(context);
            host.graphics().finish();
            native_performance_graphics_snapshot.emplace(
                host.graphics().snapshot());
        } catch (...) {
            capture_product_state();
            emit_terminal_runtime_telemetry();
            throw;
        }
        capture_product_state();
        emit_terminal_runtime_telemetry();
        // Controlled AOT/hook failures need the same durable evidence as a
        // thrown exception. Capture it before cleanup changes task memory.
        if (context.stop_reason != katana::runtime::NativePortStopReason::None &&
            context.stop_reason != katana::runtime::NativePortStopReason::HostRequested &&
            context.stop_reason != katana::runtime::NativePortStopReason::HostDeadline) {
            native_product_emit_crash(
                0u, "controlled-runtime-stop",
                static_cast<std::uint32_t>(context.stop_reason),
                "native-port-stop-reason");
        }
        native_product_write_fault_bytes("KATANA_SESSION_STOP reason=");
        native_product_write_fault_u64(static_cast<std::uint32_t>(context.stop_reason));
        native_product_write_fault_bytes(" frame=");
        native_product_write_fault_u64(context.frame_index);
        native_product_write_fault_bytes("\n");
        native_product_flush_fault_file();
        title_state_cleanup.release_now();
        const bool normal_stop =
            context.stop_reason ==
                katana::runtime::NativePortStopReason::None ||
            context.stop_reason ==
                katana::runtime::NativePortStopReason::HostRequested;
        if (normal_stop)
            platform.finalize_clean_shutdown();
        if (normal_stop && context.acceptance_reached() &&
            context.bootstrap_phase ==
                katana::runtime::NativePortBootstrapPhase::Completed) {
            std::cout << "KATANA_NATIVE_PRODUCT_GATE status=accepted milestone="
                      << definition.acceptance.milestone_id << '\n';
            return 0;
        }
        if (context.stop_reason == katana::runtime::NativePortStopReason::HostRequested) {
            // Leaving at the title is a successful user action even before
            // the diagnostic FirstVisibleGameFrame acceptance milestone.
            // Report that distinction; do not claim the game gate passed.
            std::cout << "SARECOMP_USER_EXIT clean=1 game_gate_reached="
                      << context.acceptance_reached() << '\n';
            return 0;
        }
        std::cerr << "KATANA_NATIVE_PRODUCT_GATE status=not-reached milestone="
                  << definition.acceptance.milestone_id
                  << " stop_reason="
                  << static_cast<unsigned>(context.stop_reason)
                  << " bootstrap_phase="
                  << static_cast<unsigned>(context.bootstrap_phase)
                  << '\n';
        std::cerr << "KATANA_RUNTIME_STOP_FRONTIER {\"version\":2"
                  << ",\"stop_reason\":"
                  << static_cast<unsigned>(context.stop_reason)
                  << ",\"pc\":" << cpu.pc
                  << ",\"pr\":" << cpu.pr
                  << ",\"active_instruction\":"
                  << cpu.active_instruction_pc
                  << ",\"active_instruction_physical\":"
                  << cpu.active_instruction_physical_pc
                  << ",\"active_block\":"
                  << cpu.active_block_virtual_start
                  << ",\"active_block_physical\":"
                  << cpu.active_block_physical_start
                  << ",\"active_block_size\":"
                  << cpu.active_block_size
                  << ",\"gpr\":[";
        for (std::size_t index = 0u; index < cpu.r.size(); ++index) {
            if (index != 0u) std::cerr << ',';
            std::cerr << cpu.r[index];
        }
        std::cerr << ']'
                  << ",\"gbr\":" << cpu.gbr
                  << ",\"vbr\":" << cpu.vbr
                  << ",\"sr\":" << cpu.sr
                  << ",\"mach\":" << cpu.mach
                  << ",\"macl\":" << cpu.macl
                  << ",\"fpul\":" << cpu.fpul
                  << ",\"fpscr\":" << cpu.fpscr
                  << ",\"retired_guest_instructions\":"
                  << cpu.retired_guest_instructions
                  << ",\"attempted_guest_instructions\":"
                  << cpu.attempted_guest_instructions
                  << "}\n";
        return 1;
    } catch (const katana::runtime::NativePortGraphicsError& error) {
        sonic::diagnostics::record(sonic::diagnostics::Failure::Graphics,static_cast<std::uint32_t>(error.failure()));
        emit_native_performance_snapshot();
        std::string combined_graphics_detail;
        std::string_view graphics_detail = error.what();
        try {
            if (native_graphics_failure_kind == error.failure() &&
                !native_graphics_failure_detail.empty()) {
                combined_graphics_detail = error.what();
                combined_graphics_detail.push_back(';');
                combined_graphics_detail += native_graphics_failure_detail;
                graphics_detail = combined_graphics_detail;
            }
        } catch (...) {
            // The original typed failure still survives allocation failure.
        }
        native_product_emit_crash(
            2u, "NativePortGraphicsError",
            static_cast<std::uint32_t>(error.failure()),
            "native-port-graphics", graphics_detail);
        try {
        std::cerr << "KATANA_NATIVE_GRAPHICS_CONTRACT failure="
                  << static_cast<unsigned>(error.failure())
                  << " detail=crash-capsule-v4\n";
        } catch (...) {}
        return 1;
    } catch (const katana::runtime::NativePortContractError& error) {
        sonic::diagnostics::record(sonic::diagnostics::Failure::Contract,static_cast<std::uint32_t>(error.failure()));
        emit_native_performance_snapshot();
        native_product_emit_crash(
            1u, "NativePortContractError",
            static_cast<std::uint32_t>(error.failure()),
            "native-port-contract", error.detail());
        try {
        std::cerr << "KATANA_NATIVE_PORT_CONTRACT failure="
                  << static_cast<unsigned>(error.failure())
                  << " detail=crash-capsule-v3\n";
        } catch (...) {}
        return 1;
    } catch (const std::exception& error) {
        emit_native_performance_snapshot();
        sonic::diagnostics::record(sonic::diagnostics::Failure::Runtime);
        native_product_emit_crash(
            2u, "std-exception", 0xFFFFFFFFu,
            "native-product-runtime", error.what());
        try {
        std::cerr << "KATANA_NATIVE_PORT_FAILURE "
                     "token=std-exception what="
                  << error.what()
                  << " detail=crash-capsule-v3\n";
        } catch (...) {}
        return 1;
    } catch (...) {
        emit_native_performance_snapshot();
        sonic::diagnostics::record(sonic::diagnostics::Failure::Unknown);
        native_product_emit_crash(
            3u, "unknown-exception", 0xFFFFFFFFu,
            "native-product-runtime");
        return 1;
    }
}

int main(int argc,char** argv){
#if defined(SARECOMP_LINUX_PERFORMANCE_DEFAULTS) && !defined(_WIN32)
    // Set once before any worker or cached environment lookup. Explicit
    // developer overrides still select the original path for comparisons.
    ::setenv("SARECOMP_RAM_REGIONS", "1", 0);
    ::setenv("SARECOMP_PREPARED_TRANSFERS", "1", 0);
    std::cerr << "SONIC_CPU_PATH_DEFAULTS version=20260916 ram_regions="
              << (std::getenv("SARECOMP_RAM_REGIONS") ? std::getenv("SARECOMP_RAM_REGIONS") : "unset")
              << " prepared_transfers="
              << (std::getenv("SARECOMP_PREPARED_TRANSFERS") ? std::getenv("SARECOMP_PREPARED_TRANSFERS") : "unset") << '\n';
#endif
    try { sonic::diagnostics::initialize_internal_policy(sonic::paths::executable()); } catch (...) {}
    std::cerr << "SONIC_INTERNAL_DIAGNOSTICS version=1 enabled="
              << sonic::diagnostics::runtime_checks_enabled() << '\n';
    sonic::diagnostics::source_identity=sonic_native_title_adapter_source_identity;
    sonic::diagnostics::build_profile=KATANA_PORT_BUILD_PROFILE_NAME;
    const auto result=run_game(argc,argv);
    if(result){
        try{if(sonic::diagnostics::failure.load()!=sonic::diagnostics::Failure::None&&sonic::profiles::library_root().empty())sonic::profiles::initialize(native_product_user_data_root());}catch(...){}
        sonic::errors::show();return result;
    }
    if(const auto next=sonic::menu::take_restart()){
        try {
            // run_game has released its CPU, host, audio and save provider.
            sonic::profiles::apply_pending_restore();
            return sonic::restart::launch(std::filesystem::absolute(argv[0]),*next,sonic::menu::restart_language(),&sonic::menu::restart_baseline());
        }catch(const std::exception& e){std::cerr<<"SONIC_RESTART failure="<<e.what()<<'\n';sonic::diagnostics::record(sonic::diagnostics::Failure::Restart);sonic::errors::show();return 1;}
    }
    return 0;
}
