#include "sonic_sony_input.hpp"
#include "sonic_sony_sdl_api.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace sonic::sony {
namespace {
using Clock = std::chrono::steady_clock;
using Button = katana::runtime::NativePortGamepadButton;
bool background() {
    const auto* value = std::getenv("KATANA_PORT_BACKGROUND_TEST");
    return value && *value && *value != '0';
}
bool admitted(Uint16 vendor, Uint16 product) {
    return vendor == 0x054c && (product == 0x05c4 || product == 0x09cc || product == 0x0ba0 ||
                              product == 0x0ce6 || product == 0x0df2);
}
std::uint64_t path_identity(const char* path) {
    if (!path || !*path) return 0;
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto* byte = reinterpret_cast<const unsigned char*>(path); *byte; ++byte) {
        const unsigned char lower = *byte >= 'A' && *byte <= 'Z' ? *byte + ('a' - 'A') : *byte;
        hash = (hash ^ lower) * 1099511628211ull;
    }
    hash &= 0x00ffffffffffffffull; // SDK stores the backend in the upper byte.
    return hash ? hash : 1;
}
std::int16_t invert(std::int16_t axis) {
    return static_cast<std::int16_t>(std::clamp(-int(axis), -32768, 32767));
}
float normalize(std::int16_t axis) {
    return axis < 0 ? float(axis) / 32768.f : float(axis) / 32767.f;
}
std::uint8_t trigger(std::int16_t axis) {
    return static_cast<std::uint8_t>((std::max(int(axis), 0) * 255 + 16383) / 32767);
}
}

struct Backend::Impl {
    struct Device {
        SDL_JoystickID instance = 0;
        SDL_Gamepad* pad = nullptr;
        std::uint64_t identity = 0;
        unsigned endpoint = 0;
        bool dualsense = false, can_rumble = false, motors_active = false;
    };
    detail::SdlApi api{};
    HMODULE library = nullptr;
    bool fake = false, initialized = false, attempted = false;
    unsigned next_endpoint = 4; // 0..3 belong exclusively to XInput.
    Clock::time_point next_discovery{};
    mutable std::mutex mutex;
    std::vector<Device> devices;
    std::vector<Sample> samples;

    bool load() {
        if (fake) return true;
        std::wstring executable(32768, L'\0');
        const auto count = GetModuleFileNameW(nullptr, executable.data(), DWORD(executable.size()));
        if (!count || count == executable.size()) return false;
        executable.resize(count);
        const auto path = std::filesystem::path(executable).parent_path() / L"SDL3.dll";
        library = LoadLibraryExW(path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!library) return false;
#define SONIC_LOAD_SDL(name) api.name = reinterpret_cast<decltype(api.name)>(GetProcAddress(library, #name)); if (!api.name) return false;
        SONIC_SONY_SDL_FUNCTIONS(SONIC_LOAD_SDL)
#undef SONIC_LOAD_SDL
        return api.SDL_GetVersion() == SDL_VERSION;
    }
    bool hint(const char* key, const char* value) {
        return api.SDL_SetHintWithPriority(key, value, SDL_HINT_OVERRIDE);
    }
    bool configure() {
        // Hints are global within SDL. Refuse to take over another live owner.
        if (api.SDL_WasInit(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)) return false;
        for (const auto* key : {"SDL_JOYSTICK_HIDAPI", "SDL_JOYSTICK_DIRECTINPUT",
                "SDL_JOYSTICK_RAWINPUT", "SDL_XINPUT_ENABLED", "SDL_JOYSTICK_WGI",
                "SDL_JOYSTICK_GAMEINPUT", "SDL_JOYSTICK_GAMEINPUT_RAW", "SDL_HIDAPI_LIBUSB",
                "SDL_JOYSTICK_HIDAPI_XBOX", "SDL_JOYSTICK_HIDAPI_XBOX_360",
                "SDL_JOYSTICK_HIDAPI_XBOX_360_WIRELESS", "SDL_JOYSTICK_HIDAPI_XBOX_ONE",
                "SDL_JOYSTICK_HIDAPI_GIP", "SDL_JOYSTICK_HIDAPI_PS5_PLAYER_LED"}) {
            if (!hint(key, "0")) return false;
        }
        return hint("SDL_JOYSTICK_HIDAPI_PS4", "1") && hint("SDL_JOYSTICK_HIDAPI_PS5", "1") &&
            hint("SDL_GAMECONTROLLER_IGNORE_DEVICES", "") &&
            hint("SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT", "0x054c/0x05c4,0x054c/0x09cc,0x054c/0x0ba0,0x054c/0x0ce6,0x054c/0x0df2") &&
            hint("SDL_JOYSTICK_ENHANCED_REPORTS", "auto");
    }
    void close(Device& device) {
        if (device.motors_active) api.SDL_RumbleGamepad(device.pad, 0, 0, 0);
        api.SDL_CloseGamepad(device.pad);
    }
    void discover() {
        int count = 0;
        const auto ids = api.SDL_GetGamepads(&count);
        if (!ids) return;
        for (int index = 0; index < count; ++index) {
            const auto id = ids[index];
            if (std::ranges::any_of(devices, [&](const auto& device) { return device.instance == id; })) continue;
            const auto product = api.SDL_GetGamepadProductForID(id);
            if (!admitted(api.SDL_GetGamepadVendorForID(id), product)) continue;
            auto* pad = api.SDL_OpenGamepad(id);
            if (!pad) continue;
            const auto identity = path_identity(api.SDL_GetGamepadPath(pad));
            if (!identity || next_endpoint >= unsigned(std::numeric_limits<int>::max()) ||
                std::ranges::any_of(devices, [&](const auto& device) { return device.identity == identity; })) {
                api.SDL_CloseGamepad(pad);
                continue;
            }
            const auto can_rumble = api.SDL_GetBooleanProperty(api.SDL_GetGamepadProperties(pad),
                SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false);
            devices.push_back({id, pad, identity, next_endpoint++, product == 0x0ce6 || product == 0x0df2, can_rumble});
            std::fprintf(stderr, "SONIC_SONY_INPUT connected=1 backend=sdl3 analog_triggers=1 rumble=%d\n", int(can_rumble));
        }
        api.SDL_free(ids);
    }
    Sample read(const Device& device) {
        Sample sample{device.identity, device.dualsense};
        auto& state = sample.state;
        state.connected = true;
        const std::pair<SDL_GamepadButton, Button> buttons[] = {
            {SDL_GAMEPAD_BUTTON_SOUTH, Button::A}, {SDL_GAMEPAD_BUTTON_EAST, Button::B},
            {SDL_GAMEPAD_BUTTON_WEST, Button::X}, {SDL_GAMEPAD_BUTTON_NORTH, Button::Y},
            {SDL_GAMEPAD_BUTTON_START, Button::Menu}, {SDL_GAMEPAD_BUTTON_BACK, Button::View},
            {SDL_GAMEPAD_BUTTON_LEFT_STICK, Button::LeftStick}, {SDL_GAMEPAD_BUTTON_RIGHT_STICK, Button::RightStick},
            {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, Button::LeftShoulder}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, Button::RightShoulder},
            {SDL_GAMEPAD_BUTTON_DPAD_UP, Button::DpadUp}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN, Button::DpadDown},
            {SDL_GAMEPAD_BUTTON_DPAD_LEFT, Button::DpadLeft}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Button::DpadRight}};
        for (const auto [source, target] : buttons)
            if (api.SDL_GetGamepadButton(device.pad, source)) state.buttons |= std::uint32_t(target);
        state.left_stick_x_raw = api.SDL_GetGamepadAxis(device.pad, SDL_GAMEPAD_AXIS_LEFTX);
        state.left_stick_y_raw = invert(api.SDL_GetGamepadAxis(device.pad, SDL_GAMEPAD_AXIS_LEFTY));
        state.right_stick_x_raw = api.SDL_GetGamepadAxis(device.pad, SDL_GAMEPAD_AXIS_RIGHTX);
        state.right_stick_y_raw = invert(api.SDL_GetGamepadAxis(device.pad, SDL_GAMEPAD_AXIS_RIGHTY));
        state.left_trigger_raw = trigger(api.SDL_GetGamepadAxis(device.pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
        state.right_trigger_raw = trigger(api.SDL_GetGamepadAxis(device.pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
        state.left_stick_x = normalize(state.left_stick_x_raw);
        state.left_stick_y = normalize(state.left_stick_y_raw);
        state.right_stick_x = normalize(state.right_stick_x_raw);
        state.right_stick_y = normalize(state.right_stick_y_raw);
        state.left_trigger = float(state.left_trigger_raw) / 255.f;
        state.right_trigger = float(state.right_trigger_raw) / 255.f;
        return sample;
    }
};

Backend::Backend(const detail::SdlApi* test_api) : impl_(std::make_unique<Impl>()) {
    if (test_api && background()) { impl_->api = *test_api; impl_->fake = true; }
}
Backend::~Backend() { shutdown(); }
bool Backend::initialize() {
    auto& self = *impl_;
    const std::lock_guard lock(self.mutex);
    if (self.attempted) return self.initialized;
    self.attempted = true;
    if ((!self.fake && background()) || !self.load() || !self.configure()) return false;
    self.api.SDL_SetMainReady();
    if (!self.api.SDL_InitSubSystem(SDL_INIT_GAMEPAD)) return false;
    self.initialized = true;
    self.api.SDL_SetGamepadEventsEnabled(false);
    self.api.SDL_SetJoystickEventsEnabled(false);
    return true;
}
bool Backend::active() const noexcept { return impl_->initialized; }
void Backend::update() {
    auto& self = *impl_;
    const std::lock_guard lock(self.mutex);
    if (!self.initialized) return;
    self.api.SDL_UpdateGamepads();
    std::erase_if(self.devices, [&](auto& device) {
        if (self.api.SDL_GamepadConnected(device.pad)) return false;
        self.close(device);
        self.next_discovery = {};
        return true;
    });
    const auto now = Clock::now();
    if (now >= self.next_discovery) {
        self.discover();
        self.next_discovery = now + std::chrono::milliseconds(500);
    }
    self.samples.clear();
    for (const auto& device : self.devices)
        if (self.api.SDL_GamepadConnected(device.pad)) self.samples.push_back(self.read(device));
}
std::span<const Sample> Backend::samples() const noexcept { return impl_->samples; }
int Backend::endpoint(std::uint64_t identity) const noexcept {
    const std::lock_guard lock(impl_->mutex);
    for (const auto& device : impl_->devices)
        if (device.identity == identity && device.can_rumble) return int(device.endpoint);
    return -1;
}
bool Backend::rumble(unsigned endpoint, std::uint16_t low, std::uint16_t high) noexcept {
    const std::lock_guard lock(impl_->mutex);
    for (auto& device : impl_->devices) {
        if (device.endpoint != endpoint || !device.can_rumble) continue;
        // Engine supplies the true deadline. SDL's timeout is a second fail-safe,
        // never the source of frame-dependent expiry or an extra strength gain.
        const auto success = impl_->api.SDL_RumbleGamepad(device.pad, low, high, low || high ? 65000 : 0);
        if (success) device.motors_active = low || high;
        return success;
    }
    return false;
}
void Backend::shutdown() {
    auto& self = *impl_;
    const std::lock_guard lock(self.mutex);
    for (auto& device : self.devices) self.close(device);
    self.devices.clear();
    self.samples.clear();
    if (self.initialized) self.api.SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    self.initialized = false;
    if (self.library) FreeLibrary(self.library);
    self.library = nullptr;
}
}
