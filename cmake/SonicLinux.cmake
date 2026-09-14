# Separate Linux product. Never consumes Windows object/archive files.
set(SONIC_ROOT "${PROJECT_SOURCE_DIR}")
set(SONIC_BASE "${SONIC_ROOT}/.local/baseline/r354")
set(SONIC_LINUX_SDK "${SONIC_ROOT}/.local/linux-sdk")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
enable_language(C)
find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(NOT EXISTS "${SONIC_LINUX_SDK}/src/runtime/native_port.cpp")
    message(FATAL_ERROR "Restore the pinned port-local Linux SDK source first")
endif()

# Preserve the accepted guest ABI; build provenance and host implementations
# are separate. The Windows snapshot is read-only input to this build.
add_library(sonic_linux_sdk_headers INTERFACE)
target_include_directories(sonic_linux_sdk_headers INTERFACE
    "${SONIC_LINUX_SDK}/include" "${SONIC_BASE}/sdk/generated/include")
set(sonic_aot_sources
    block_abi block_table block_dispatch block_guards cache_control
    code_invalidation crash_report exception fpu fpu_simd guest_program_range
    memory native_bringup_dispatch native_port runtime)
list(TRANSFORM sonic_aot_sources PREPEND "${SONIC_LINUX_SDK}/src/runtime/")
list(TRANSFORM sonic_aot_sources APPEND ".cpp")
add_library(sonic_linux_aot_runtime STATIC ${sonic_aot_sources})
target_compile_options(sonic_linux_aot_runtime PRIVATE -frounding-math -ffp-contract=off)
target_link_libraries(sonic_linux_aot_runtime PUBLIC sonic_linux_sdk_headers Threads::Threads)
set_source_files_properties("${SONIC_LINUX_SDK}/src/runtime/fpu_simd.cpp"
    PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")

# SDL uses runtime-loaded X11/ALSA/udev libraries. XWayland is available inside
# Steam Deck's gamescope session; a direct Wayland backend can be added later.
set(sonic_sysroot "${SONIC_ROOT}/.local/linux-sysroot")
list(APPEND CMAKE_INCLUDE_PATH "${sonic_sysroot}/usr/include")
list(APPEND CMAKE_LIBRARY_PATH "${sonic_sysroot}/usr/lib/x86_64-linux-gnu"
    "${sonic_sysroot}/lib/x86_64-linux-gnu")
foreach(lib IN ITEMS X11 Xext Xcursor Xi Xfixes Xrandr Xrender Xss Xtst asound)
    file(GLOB candidates "${sonic_sysroot}/usr/lib/x86_64-linux-gnu/lib${lib}.so.*"
        "${sonic_sysroot}/lib/x86_64-linux-gnu/lib${lib}.so.*")
    list(SORT candidates COMPARE NATURAL)
    if(candidates)
        list(GET candidates 0 library)
        string(TOUPPER "${lib}" upper)
        set(${upper}_LIB "${library}" CACHE FILEPATH "Pinned cross sysroot library" FORCE)
        set(X11_${lib}_LIB "${library}" CACHE FILEPATH "Pinned X11 sysroot library" FORCE)
        if(lib STREQUAL "asound")
            set(ALSA_LIBRARY "${library}" CACHE FILEPATH "Pinned ALSA library" FORCE)
        endif()
    endif()
endforeach()
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_X11 ON CACHE BOOL "" FORCE)
set(SDL_X11_SHARED ON CACHE BOOL "" FORCE)
set(SDL_ALSA ON CACHE BOOL "" FORCE)
set(SDL_ALSA_SHARED ON CACHE BOOL "" FORCE)
set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN ON CACHE BOOL "" FORCE)
set(SDL_HIDAPI ON CACHE BOOL "" FORCE)
set(SDL_LIBUDEV ON CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
# Zig's compiler driver does not accept CMake's Clang PCH emission command.
# This affects build throughput only, never the SDL runtime configuration.
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
add_subdirectory("${SONIC_ROOT}/.local/linux-deps/sdl/SDL3-3.4.16" sdl EXCLUDE_FROM_ALL)
include("${SONIC_ROOT}/cmake/LinuxPlatform.cmake")
include("${SONIC_ROOT}/cmake/LinuxAudio.cmake")
include("${SONIC_ROOT}/cmake/LinuxMovie.cmake")
include("${SONIC_ROOT}/cmake/LinuxStartup.cmake")

set(sonic_spirv "${CMAKE_BINARY_DIR}/generated/sonic_vulkan_shaders.hpp")
add_custom_command(OUTPUT "${sonic_spirv}"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/compile-vulkan-shaders.py" --output "${sonic_spirv}"
    DEPENDS "${SONIC_ROOT}/tools/compile-vulkan-shaders.py"
        "${SONIC_ROOT}/src/renderer/pinned/native_port_graphics.cpp" VERBATIM)
add_library(sonic_linux_vulkan STATIC src/renderer/sonic_vulkan.cpp
    third_party/volk/volk.c "${sonic_spirv}")
target_include_directories(sonic_linux_vulkan PRIVATE "${CMAKE_BINARY_DIR}/generated"
    "${SONIC_ROOT}/third_party/volk" "${SONIC_ROOT}/third_party/vulkan-headers/include")
target_compile_definitions(sonic_linux_vulkan PRIVATE VK_NO_PROTOTYPES)
target_link_libraries(sonic_linux_vulkan PRIVATE sonic_linux_sdk_headers sonic_linux_startup SDL3::SDL3 ${CMAKE_DL_LIBS})
include("${SONIC_ROOT}/cmake/LinuxGraphics.cmake")

# The current retained guest source is compiled to ELF once. No analysis,
# discovery or export is performed by this build or by the end-user installer.
set(SONIC_WORKING "${SONIC_ROOT}/.local/working-product")
file(GLOB sonic_linux_guest_sources CONFIGURE_DEPENDS
    "${SONIC_WORKING}/generated/code/unit-*.cpp"
    "${SONIC_WORKING}/generated/code/native-port-*-shard-*.cpp")
if(NOT sonic_linux_guest_sources)
    message(FATAL_ERROR "Restore the current retained guest source pack")
endif()
add_library(sonic_linux_guest STATIC ${sonic_linux_guest_sources})
target_include_directories(sonic_linux_guest PRIVATE "${SONIC_WORKING}/generated/include")
target_link_libraries(sonic_linux_guest PRIVATE sonic_linux_sdk_headers)
target_compile_options(sonic_linux_guest PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
include("${SONIC_ROOT}/cmake/LinuxTitle.cmake")
