#pragma once
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// Narrow dynamically loaded interface. The fake test transport never loads or
// initializes SDL, so automated checks cannot touch a physical controller.
#define SONIC_SONY_SDL_FUNCTIONS(X) \
    X(SDL_GetVersion) X(SDL_SetMainReady) X(SDL_SetHintWithPriority) \
    X(SDL_InitSubSystem) X(SDL_QuitSubSystem) X(SDL_WasInit) \
    X(SDL_GetGamepads) X(SDL_free) X(SDL_OpenGamepad) X(SDL_CloseGamepad) \
    X(SDL_GetGamepadPath) X(SDL_GetGamepadVendorForID) X(SDL_GetGamepadProductForID) \
    X(SDL_GetGamepadAxis) X(SDL_GetGamepadButton) X(SDL_GamepadConnected) \
    X(SDL_GetGamepadProperties) X(SDL_GetBooleanProperty) X(SDL_UpdateGamepads) \
    X(SDL_SetGamepadEventsEnabled) X(SDL_SetJoystickEventsEnabled) X(SDL_RumbleGamepad)
namespace sonic::sony::detail {
struct SdlApi {
#define SONIC_DECLARE_SDL(name) decltype(&::name) name = nullptr;
    SONIC_SONY_SDL_FUNCTIONS(SONIC_DECLARE_SDL)
#undef SONIC_DECLARE_SDL
};
}
