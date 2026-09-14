// Included inside katana::runtime by the source-bound Linux adapter.
namespace {
using Failure = NativePortPlatformFailure;
using SaveStatus = NativePortSaveLoadStatus;
namespace fs = std::filesystem;

[[noreturn]] void fail_platform(Failure failure, std::uint32_t error, const char* operation) {
    throw NativePortPlatformError(failure, error, operation);
}
void checked(bool okay, Failure failure, const char* operation) {
    if (!okay) fail_platform(failure, errno, operation);
}
class Fd {
    int value_ = -1;
public:
    explicit Fd(int value = -1) noexcept : value_(value) {}
    ~Fd() { if (value_ >= 0) ::close(value_); }
    Fd(Fd&& other) noexcept : value_(std::exchange(other.value_, -1)) {}
    Fd& operator=(Fd&& other) noexcept {
        if (this != &other) { if (value_ >= 0) ::close(value_); value_ = std::exchange(other.value_, -1); }
        return *this;
    }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    int get() const noexcept { return value_; }
};
void owner(std::thread::id thread) {
    if (thread != std::this_thread::get_id()) fail_platform(Failure::ThreadViolation, 0, "owner-thread");
}
bool within(const fs::path& path, const fs::path& root) {
    auto p = path.begin();
    for (const auto& component : root) { if (p == path.end() || *p++ != component) return false; }
    return true;
}
Fd directory_chain(const fs::path& path, bool create, Failure failure) {
    if (!path.is_absolute()) fail_platform(failure, EINVAL, "directory-absolute");
    Fd directory(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    checked(directory.get() >= 0, failure, "directory-root");
    for (const auto& component : path.relative_path()) {
        if (component.empty() || component == "." || component == "..") fail_platform(failure, EINVAL, "directory-component");
        int next = ::openat(directory.get(), component.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0 && errno == ENOENT && create) {
            checked(::mkdirat(directory.get(), component.c_str(), 0700) == 0 || errno == EEXIST, failure, "directory-create");
            next = ::openat(directory.get(), component.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        }
        checked(next >= 0, failure, "directory-open");
        directory = Fd(next);
    }
    return directory;
}
Fd relative_file(int root, const fs::path& path, Failure failure) {
    if (!valid_content_relative_path(path)) fail_platform(failure, EINVAL, "relative-path");
    Fd directory(::fcntl(root, F_DUPFD_CLOEXEC, 0));
    checked(directory.get() >= 0, failure, "directory-duplicate");
    for (const auto& component : path.parent_path()) {
        Fd next(::openat(directory.get(), component.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
        checked(next.get() >= 0, failure, "relative-directory"); directory = std::move(next);
    }
    Fd result(::openat(directory.get(), path.filename().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
    checked(result.get() >= 0, failure, "relative-open");
    struct stat info{};
    checked(::fstat(result.get(), &info) == 0, failure, "relative-stat");
    if (!S_ISREG(info.st_mode)) fail_platform(failure, EINVAL, "relative-regular-file");
    return result;
}
void read_exact(int fd, std::uint64_t offset, std::span<std::byte> bytes, Failure failure) {
    while (!bytes.empty()) {
        const auto count = ::pread(fd, bytes.data(), std::min<std::size_t>(bytes.size(), 1024 * 1024), off_t(offset));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) fail_platform(failure, count < 0 ? errno : EIO, "file-read");
        bytes = bytes.subspan(std::size_t(count)); offset += std::uint64_t(count);
    }
}
void write_exact(int fd, std::uint64_t offset, std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
        const auto count = ::pwrite(fd, bytes.data(), std::min<std::size_t>(bytes.size(), 1024 * 1024), off_t(offset));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) fail_platform(Failure::SaveWrite, count < 0 ? errno : EIO, "file-write");
        bytes = bytes.subspan(std::size_t(count)); offset += std::uint64_t(count);
    }
}
class Sha256 {
    katana::io::Sha256Accumulator hash_;
public:
    void update(const void* bytes, std::size_t count) { hash_.update({static_cast<const char*>(bytes), count}); }
    template<class T> void update_scalar(T value) {
        std::array<char, sizeof(T)> bytes{};
        for (unsigned i = 0; i < sizeof(T); ++i) bytes[i] = char(value >> (8 * i));
        hash_.update({bytes.data(), bytes.size()});
    }
    std::array<std::byte, 32> finish() {
        const auto hex = hash_.finish(); std::array<std::byte, 32> bytes{};
        const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
        for (unsigned i = 0; i < 32; ++i) bytes[i] = std::byte(nibble(hex[2*i]) * 16 + nibble(hex[2*i+1]));
        return bytes;
    }
};
std::string digest_identity(const std::array<std::byte, 32>& bytes) {
    constexpr char hex[] = "0123456789abcdef"; std::string result = "sha256:";
    for (const auto byte : bytes) { const auto v = std::to_integer<unsigned>(byte); result += hex[v >> 4]; result += hex[v & 15]; }
    return result;
}
// Exactly the Windows v2 codec, copied from the identity-bound SDK source.
#include "native_save_codec.inc"

SaveProbe probe_save(int root, const std::string& name, std::string_view project,
                     const NativePortSaveKey& key, std::uint32_t maximum) {
    Fd file(::openat(root, name.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK));
    if (file.get() < 0 && errno == ENOENT) return {};
    checked(file.get() >= 0, Failure::SaveBoundary, "save-open");
    struct stat info{}; checked(::fstat(file.get(), &info) == 0, Failure::SaveRead, "save-size");
    if (!S_ISREG(info.st_mode) || info.st_nlink != 1) fail_platform(Failure::SaveBoundary, EINVAL, "save-regular-file");
    SaveProbe result;
    if (info.st_size < off_t(save_header_bytes) || std::uint64_t(info.st_size) > save_header_bytes + std::uint64_t(maximum)) {
        result.status = SaveStatus::Corrupt; return result;
    }
    std::vector<std::byte> bytes(std::size_t(info.st_size)); read_exact(file.get(), 0, bytes, Failure::SaveRead);
    result.bytes_read = bytes.size(); const auto source = std::span<const std::byte>(bytes);
    result.status = SaveStatus::Corrupt;
    if (!std::equal(save_magic.begin(), save_magic.end(), source.begin()) ||
        read_little<std::uint32_t>(source, 16) != save_format_version ||
        read_little<std::uint32_t>(source, 20) != save_header_bytes ||
        read_little<std::uint32_t>(source, 28) != 0) return result;
    result.stored_schema_version = read_little<std::uint32_t>(source, 24);
    result.generation = read_little<std::uint64_t>(source, 32);
    const auto length = read_little<std::uint64_t>(source, 40);
    if (!result.stored_schema_version || !result.generation || length != source.size() - save_header_bytes) return result;
    const auto payload = source.subspan(save_header_bytes);
    const auto hash = save_digest(project, key.slot_id, result.stored_schema_version, result.generation, payload);
    if (!std::equal(hash.begin(), hash.end(), source.begin() + 48)) return result;
    // An intact newer schema is authoritative even if an old backup matches.
    result.status = result.stored_schema_version == key.schema_version ? SaveStatus::Loaded : SaveStatus::IncompatibleSchema;
    if (result.status == SaveStatus::Loaded) result.payload.assign(payload.begin(), payload.end());
    return result;
}
void atomic_save(int root, const std::string& name, std::span<const std::byte> bytes) {
    static std::atomic<std::uint64_t> sequence{0};
    const auto temporary = ".save-" + std::to_string(::getpid()) + "-" + std::to_string(++sequence) + ".tmp";
    Fd file(::openat(root, temporary.c_str(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    checked(file.get() >= 0, Failure::SaveWrite, "save-temporary");
    try {
        write_exact(file.get(), 0, bytes);
        checked(::fsync(file.get()) == 0, Failure::SaveWrite, "save-flush");
        checked(::renameat(root, temporary.c_str(), root, name.c_str()) == 0, Failure::SaveWrite, "save-publish");
        checked(::fsync(root) == 0, Failure::SaveWrite, "save-directory-flush");
    } catch (...) { ::unlinkat(root, temporary.c_str(), 0); throw; }
}
struct PlatformTelemetry { NativePortPlatformSnapshot snapshot; };
bool background_input() { const auto* v = std::getenv("KATANA_PORT_BACKGROUND_TEST"); return v && std::string_view(v) == "1"; }
std::int16_t invert_axis(std::int16_t value) { return std::int16_t(std::clamp(-int(value), -32768, 32767)); }
float normalize_axis(std::int16_t value) { return value < 0 ? value / 32768.f : value / 32767.f; }
std::uint8_t trigger_axis(std::int16_t value) { return std::uint8_t((std::max(0, int(value)) * 255 + 16383) / 32767); }
}

class NativePortReadOnlyFile::Impl final {
    Fd file_;
    std::string id_;
    NativePortContentFileSnapshot snapshot_;
    std::shared_ptr<PlatformTelemetry> telemetry_;
    std::thread::id thread_ = std::this_thread::get_id();
public:
    Impl(Fd file, std::string id, std::uint64_t size, std::shared_ptr<PlatformTelemetry> telemetry)
        : file_(std::move(file)), id_(std::move(id)), snapshot_{size}, telemetry_(std::move(telemetry)) {}
    const std::string& logical_id() const noexcept { return id_; }
    std::uint64_t byte_size() const noexcept { return snapshot_.byte_size; }
    void read_at(std::uint64_t offset, std::span<std::byte> bytes) {
        owner(thread_);
        if (offset > byte_size() || bytes.size() > byte_size() - offset) fail_platform(Failure::ContentRead, EINVAL, "content-range");
        read_exact(file_.get(), offset, bytes, Failure::ContentRead);
        saturating_increment(snapshot_.read_operations); saturating_add(snapshot_.bytes_read, bytes.size());
        saturating_increment(telemetry_->snapshot.content_read_operations); saturating_add(telemetry_->snapshot.content_bytes_read, bytes.size());
    }
    NativePortContentFileSnapshot snapshot() const { owner(thread_); return snapshot_; }
};

class NativePortPlatformServices::Impl final {
    fs::path content_root_, user_root_;
    Fd content_, saves_, save_lock_;
    std::string project_;
    std::uint64_t content_limit_;
    std::uint32_t save_limit_;
    std::thread::id thread_ = std::this_thread::get_id();
    std::shared_ptr<PlatformTelemetry> telemetry_ = std::make_shared<PlatformTelemetry>();
    std::array<SDL_Gamepad*, native_port_gamepad_count> pads_{};
    NativePortInputSnapshot input_{};
    std::chrono::steady_clock::time_point next_discovery_{};
    bool input_initialized_ = false;
    static void validate_key(const NativePortSaveKey& key) {
        if (!valid_path_identifier(key.slot_id, maximum_save_slot_bytes) || !key.schema_version)
            fail_platform(Failure::InvalidConfig, EINVAL, "save-key");
    }
    SaveProbe load_internal(const NativePortSaveKey& key) const {
        validate_key(key); const auto name = std::string(key.slot_id) + ".ksave";
        auto primary = probe_save(saves_.get(), name, project_, key, save_limit_);
        if (primary.status == SaveStatus::Loaded || primary.status == SaveStatus::IncompatibleSchema) return primary;
        auto backup = probe_save(saves_.get(), name + ".bak", project_, key, save_limit_);
        saturating_add(backup.bytes_read, primary.bytes_read);
        if (backup.status == SaveStatus::Loaded) { backup.status = SaveStatus::RecoveredFromBackup; return backup; }
        if (backup.status == SaveStatus::IncompatibleSchema) return backup;
        if (primary.status == SaveStatus::Corrupt) backup.status = SaveStatus::Corrupt;
        return backup;
    }
    void discover() {
        const auto now = std::chrono::steady_clock::now();
        if (now < next_discovery_) return;
        next_discovery_ = now + std::chrono::milliseconds(500);
        int count = 0; auto* ids = SDL_GetGamepads(&count);
        if (!ids) return;
        for (int i = 0; i < count; ++i) {
            if (background_input() && !SDL_IsJoystickVirtual(ids[i])) continue;
            if (std::any_of(pads_.begin(), pads_.end(), [&](auto* pad) { return pad && SDL_GetGamepadID(pad) == ids[i]; })) continue;
            auto slot = std::find(pads_.begin(), pads_.end(), nullptr);
            if (slot == pads_.end()) break;
            if (auto* pad = SDL_OpenGamepad(ids[i])) { *slot = pad; saturating_increment(input_.connection_generation); }
        }
        SDL_free(ids);
    }
public:
    explicit Impl(const NativePortPlatformConfig& config)
        : project_(copy_validated_path_identifier(config.project_id, maximum_platform_identifier_bytes, "project-id")),
          content_limit_(config.maximum_content_file_bytes), save_limit_(config.maximum_save_payload_bytes) {
        if (config.contract_version != native_port_platform_contract_version || config.content_root.empty() || config.user_data_root.empty() ||
            !content_limit_ || content_limit_ > maximum_content_budget_bytes || !save_limit_ || save_limit_ > maximum_save_budget_bytes)
            fail_platform(Failure::InvalidConfig, EINVAL, "platform-config");
        // Replays were explicitly retired from the product. Reject an explicit
        // trace request instead of quietly running it with physical input.
        if (!config.input_record_path.empty() || !config.input_replay_path.empty() || !config.input_initial_state_path.empty())
            fail_platform(Failure::InputBackend, ENOTSUP, "linux-input-trace-unavailable");
        content_root_ = fs::absolute(config.content_root).lexically_normal();
        user_root_ = fs::absolute(config.user_data_root).lexically_normal();
        const auto canonical_content = fs::weakly_canonical(content_root_), canonical_user = fs::weakly_canonical(user_root_);
        if (within(canonical_content, canonical_user) || within(canonical_user, canonical_content))
            fail_platform(Failure::InvalidConfig, EINVAL, "content-save-root-overlap");
        content_ = directory_chain(content_root_, false, Failure::ContentBoundary);
        const auto user = directory_chain(user_root_, true, Failure::SaveBoundary);
        saves_ = directory_chain(user_root_ / project_ / "saves", true, Failure::SaveBoundary);
        save_lock_ = Fd(::openat(saves_.get(), ".katana-save.lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK, 0600));
        checked(save_lock_.get() >= 0, Failure::SaveBoundary, "save-lock-open");
        struct stat info{}; checked(::fstat(save_lock_.get(), &info) == 0, Failure::SaveBoundary, "save-lock-stat");
        if (!S_ISREG(info.st_mode) || info.st_nlink != 1) fail_platform(Failure::SaveBoundary, EINVAL, "save-lock-file");
        checked(::flock(save_lock_.get(), LOCK_EX | LOCK_NB) == 0, Failure::SaveConflict, "save-lock-held");
        input_initialized_ = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        if (!input_initialized_ && config.require_gamepad_backend) fail_platform(Failure::InputBackend, ENODEV, "sdl-gamepad-init");
        auto& s = telemetry_->snapshot; s.native_file_backend = s.native_save_backend = true;
        s.native_gamepad_backend = input_initialized_; s.maximum_save_payload_bytes = save_limit_;
    }
    ~Impl() {
        if (thread_ != std::this_thread::get_id()) std::terminate();
        for (auto* pad : pads_) if (pad) { SDL_RumbleGamepad(pad, 0, 0, 0); SDL_CloseGamepad(pad); }
        if (input_initialized_) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }
    const fs::path& content_root() const { owner(thread_); return content_root_; }
    const fs::path& user_data_root() const { owner(thread_); return user_root_; }
    std::unique_ptr<NativePortReadOnlyFile> open_content_file(const NativePortContentFileBinding& binding) {
        owner(thread_);
        const auto id = copy_validated_identifier(binding.logical_id, maximum_platform_identifier_bytes, "content-logical-id");
        if (!valid_content_relative_path(binding.content_relative_path) || !valid_native_port_sha256_identity(binding.byte_identity) ||
            !binding.byte_size || binding.byte_size > content_limit_ || binding.source_offset > std::uint64_t(INT64_MAX) ||
            binding.byte_size > std::uint64_t(INT64_MAX) - binding.source_offset)
            fail_platform(Failure::InvalidConfig, EINVAL, "content-binding");
        auto source = relative_file(content_.get(), binding.content_relative_path, Failure::ContentBoundary);
        struct stat info{}; checked(::fstat(source.get(), &info) == 0, Failure::ContentRead, "content-size");
        if (info.st_size < 0 || binding.source_offset > std::uint64_t(info.st_size) || binding.byte_size > std::uint64_t(info.st_size) - binding.source_offset)
            fail_platform(Failure::ContentBoundary, EINVAL, "content-source-range");
        // POSIX advisory locks cannot deny external writes. Seal the exact
        // verified range, so later reads cannot observe changed original data.
        Fd immutable(::memfd_create("sarecomp-content", MFD_CLOEXEC | MFD_ALLOW_SEALING));
        checked(immutable.get() >= 0, Failure::ContentRead, "content-snapshot-create");
        std::vector<std::byte> buffer(std::size_t(std::min<std::uint64_t>(binding.byte_size, 1024 * 1024)));
        Sha256 hash;
        for (std::uint64_t offset = 0; offset < binding.byte_size;) {
            const auto count = std::size_t(std::min<std::uint64_t>(buffer.size(), binding.byte_size - offset));
            const auto chunk = std::span(buffer).first(count);
            read_exact(source.get(), binding.source_offset + offset, chunk, Failure::ContentRead);
            hash.update(chunk.data(), chunk.size()); write_exact(immutable.get(), offset, chunk); offset += count;
        }
        if (digest_identity(hash.finish()) != binding.byte_identity) fail_platform(Failure::ContentIdentity, EINVAL, "content-identity");
        checked(::fcntl(immutable.get(), F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL) == 0, Failure::ContentRead, "content-seal");
        saturating_increment(telemetry_->snapshot.content_open_operations); saturating_add(telemetry_->snapshot.content_bytes_verified, binding.byte_size);
        return std::unique_ptr<NativePortReadOnlyFile>(new NativePortReadOnlyFile(std::make_unique<NativePortReadOnlyFile::Impl>(std::move(immutable), id, binding.byte_size, telemetry_)));
    }
    NativePortInputSnapshot poll_gamepads() {
        owner(thread_);
        if (input_initialized_) {
            for (auto*& pad : pads_) if (pad && !SDL_GamepadConnected(pad)) {
                SDL_CloseGamepad(pad); pad = nullptr; saturating_increment(input_.connection_generation); next_discovery_ = {};
            }
            discover();
        }
        unsigned sony = 0;
        constexpr std::array<SDL_GamepadButton, 14> buttons{
            SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
            SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH};
        for (unsigned i = 0; i < pads_.size(); ++i) {
            const auto previous = input_.gamepads[i]; auto& state = input_.gamepads[i]; state = {};
            auto* pad = pads_[i]; if (!pad) continue;
            state.connected = true;
            for (unsigned b = 0; b < buttons.size(); ++b) if (SDL_GetGamepadButton(pad, buttons[b])) state.buttons |= 1u << b;
            state.left_stick_x_raw = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
            state.left_stick_y_raw = invert_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY));
            state.right_stick_x_raw = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
            state.right_stick_y_raw = invert_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY));
            state.left_trigger_raw = trigger_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
            state.right_trigger_raw = trigger_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
            state.left_stick_x = normalize_axis(state.left_stick_x_raw); state.left_stick_y = normalize_axis(state.left_stick_y_raw);
            state.right_stick_x = normalize_axis(state.right_stick_x_raw); state.right_stick_y = normalize_axis(state.right_stick_y_raw);
            state.left_trigger = state.left_trigger_raw / 255.f; state.right_trigger = state.right_trigger_raw / 255.f;
            const bool same = previous.connected && previous.buttons == state.buttons &&
                previous.left_stick_x_raw == state.left_stick_x_raw && previous.left_stick_y_raw == state.left_stick_y_raw &&
                previous.right_stick_x_raw == state.right_stick_x_raw && previous.right_stick_y_raw == state.right_stick_y_raw &&
                previous.left_trigger_raw == state.left_trigger_raw && previous.right_trigger_raw == state.right_trigger_raw;
            state.packet_number = same || previous.packet_number == UINT32_MAX ? previous.packet_number : previous.packet_number + 1;
            const auto kind = SDL_GetGamepadType(pad);
            if (kind == SDL_GAMEPAD_TYPE_PS3 || kind == SDL_GAMEPAD_TYPE_PS4 || kind == SDL_GAMEPAD_TYPE_PS5) sony |= 1u << i;
        }
        ::sonic::linux_host::sony_controller_slots.store(sony);
        saturating_increment(input_.poll_sequence); saturating_increment(telemetry_->snapshot.input_polls);
        telemetry_->snapshot.input_connection_generation = input_.connection_generation;
        return input_;
    }
    bool set_gamepad_vibration(std::uint32_t index, const NativePortGamepadVibration& vibration) {
        owner(thread_);
        if (index >= pads_.size()) fail_platform(Failure::InvalidController, EINVAL, "vibration-controller");
        if (!std::isfinite(vibration.low_frequency) || !std::isfinite(vibration.high_frequency) || vibration.low_frequency < 0 ||
            vibration.low_frequency > 1 || vibration.high_frequency < 0 || vibration.high_frequency > 1)
            fail_platform(Failure::InvalidVibration, EINVAL, "vibration-range");
        if (background_input() || !pads_[index]) return false;
        return SDL_RumbleGamepad(pads_[index], std::uint16_t(std::lround(vibration.low_frequency * 65535)),
                                std::uint16_t(std::lround(vibration.high_frequency * 65535)), 250);
    }
    NativePortSaveLoadResult load_save(const NativePortSaveKey& key) {
        owner(thread_); auto result = load_internal(key);
        saturating_increment(telemetry_->snapshot.save_load_operations); saturating_add(telemetry_->snapshot.save_bytes_read, result.bytes_read);
        return {result.status, result.stored_schema_version, result.generation, std::move(result.payload)};
    }
    std::uint64_t store_save(const NativePortSaveKey& key, std::span<const std::byte> payload) {
        owner(thread_); validate_key(key);
        if (payload.size() > save_limit_) fail_platform(Failure::ResourceLimit, EFBIG, "save-payload-budget");
        const auto previous = load_internal(key);
        if (previous.status == SaveStatus::IncompatibleSchema) fail_platform(Failure::SaveIncompatible, EINVAL, "save-store-schema");
        if (previous.status == SaveStatus::Corrupt) fail_platform(Failure::SaveCorrupt, EINVAL, "save-store-corrupt");
        if (previous.generation == UINT64_MAX) fail_platform(Failure::SaveConflict, EOVERFLOW, "save-generation");
        const auto name = std::string(key.slot_id) + ".ksave";
        const auto generation = previous.generation + 1;
        const auto bytes = encode_save(project_, key, generation, payload);
        if (previous.status == SaveStatus::Loaded) {
            const auto backup = encode_save(project_, key, previous.generation, previous.payload);
            atomic_save(saves_.get(), name + ".bak", backup);
        }
        atomic_save(saves_.get(), name, bytes);
        const auto verified = probe_save(saves_.get(), name, project_, key, save_limit_);
        if (verified.status != SaveStatus::Loaded || verified.generation != generation || verified.payload.size() != payload.size() ||
            !std::equal(payload.begin(), payload.end(), verified.payload.begin())) fail_platform(Failure::SaveWrite, EIO, "save-post-commit-verify");
        saturating_increment(telemetry_->snapshot.save_store_operations); saturating_add(telemetry_->snapshot.save_bytes_written, bytes.size());
        saturating_add(telemetry_->snapshot.save_bytes_read, previous.bytes_read + verified.bytes_read); return generation;
    }
    NativePortPlatformSnapshot snapshot() const { owner(thread_); return telemetry_->snapshot; }
    void validate_input_initial_state(std::span<const std::byte>) { owner(thread_); }
    void start_input_after_initial_state_load() { owner(thread_); }
    void begin_input_initial_state_restore() { owner(thread_); }
    bool input_initial_state_pending() const { owner(thread_); return false; }
    void finalize_clean_shutdown() { owner(thread_); }
};
