cmake_minimum_required(VERSION 3.25)
project(SonicAdventureRecompiled VERSION 0.49.9 LANGUAGES CXX)

# This project consumes the accepted SDK/AOT. It has no Katana source target.
file(REAL_PATH "${CMAKE_SOURCE_DIR}" SONIC_ROOT)
file(REAL_PATH "${CMAKE_BINARY_DIR}" SONIC_BUILD)
cmake_path(IS_PREFIX SONIC_ROOT "${SONIC_BUILD}" NORMALIZE inside_project)
if(NOT inside_project OR NOT SONIC_BUILD MATCHES "/build[^/]*/?$")
    message(FATAL_ERROR "Build only in a build-* directory inside this Sonic project")
endif()
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(FATAL_ERROR "The pinned r354 SDK requires Windows x64 clang-cl")
endif()
set(SONIC_BASE "${SONIC_ROOT}/.local/baseline/r354")
if(NOT EXISTS "${SONIC_BASE}/aot/katana_generated.lib")
    message(FATAL_ERROR "Restore the private r354 development bundle first")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
include(cmake/PinnedSdk.cmake)
include(cmake/AdapterIdentity.cmake)
set(SONIC_WORKING "${SONIC_ROOT}/.local/working-product")

# Authoring-only SDK imports never enter the game target's dependency closure.
add_executable(sonic_native_port_manifest EXCLUDE_FROM_ALL src/native_port_manifest.cpp)
target_include_directories(sonic_native_port_manifest PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(sonic_native_port_manifest PRIVATE
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime gdi32)
target_compile_options(sonic_native_port_manifest PRIVATE /EHsc /utf-8)

# Rebuild the small authoring helper against the pinned r354 SDK. The archived
# helper predates this SDK's analysis-key schema and must not refresh r354.
add_executable(sonic_native_provider_refresh EXCLUDE_FROM_ALL tools/refresh_native_provider.cpp)
target_compile_definitions(sonic_native_provider_refresh PRIVATE KATANA_ANALYZER_ABI_VERSION=80)
target_compile_options(sonic_native_provider_refresh PRIVATE /EHsc /utf-8)
target_link_libraries(sonic_native_provider_refresh PRIVATE
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime gdi32)

add_library(katana_native_title_adapter STATIC
    src/native_title_adapter.cpp
    src/native_latent_texture_dispatch_provider.cpp
    src/native_spg_status_provider.cpp
    src/sonic_native_sound_catalog.cpp
    src/sonic_native_texture_catalog.cpp
    src/sonic_native_transform_stream.cpp
    src/sonic_private_scenario_launcher.cpp)
target_sources(katana_native_title_adapter PRIVATE src/sonic_presentation.cpp src/sonic_render_culling.cpp)
target_include_directories(katana_native_title_adapter PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_compile_options(katana_native_title_adapter PRIVATE /W4 /permissive- /EHsc /utf-8 /fp:strict)
target_link_libraries(katana_native_title_adapter PUBLIC KatanaRecomp::native_port_runtime)

add_library(katana_generated STATIC IMPORTED)
set_target_properties(katana_generated PROPERTIES
    IMPORTED_LOCATION "${SONIC_BASE}/aot/katana_generated.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${SONIC_BASE}/product/generated/include")
add_library(sonic_dispatch STATIC "${SONIC_WORKING}/generated/code/native-port-dispatch.cpp")
# Preserve the generated owner in the original link audit. This small archive
# resolves the dispatch before the frozen archive; its old member is unused.
set_target_properties(sonic_dispatch PROPERTIES OUTPUT_NAME katana_generated
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dispatch")
target_include_directories(sonic_dispatch PRIVATE "${SONIC_WORKING}/generated/include")
target_compile_options(sonic_dispatch PRIVATE /EHsc /utf-8)
target_link_libraries(sonic_dispatch PRIVATE KatanaRecomp::native_port_runtime)
add_executable(game launcher/main.cpp)
target_include_directories(game BEFORE PRIVATE "${SONIC_WORKING}/generated/include")
target_include_directories(game PRIVATE "${SONIC_ROOT}/src")
target_compile_options(game PRIVATE /W4 /permissive- /EHsc /utf-8)
target_compile_definitions(game PRIVATE KATANA_NATIVE_PORT_PRODUCT=1
    KATANA_PORT_BUILD_PROFILE_NAME="performance" KATANA_PORT_PGO_MODE_NAME="off")
target_link_libraries(game PRIVATE sonic_dispatch katana_generated katana_native_title_adapter)
set_target_properties(game PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SONIC_ROOT}/out/experimental")
target_link_options(game PRIVATE /STACK:16777216 "/MAP:${CMAKE_BINARY_DIR}/game-native-port.map"
    /threads:3 "/lldltocache:${CMAKE_BINARY_DIR}/thinlto-cache")
add_executable(katana_native_port_link_audit EXCLUDE_FROM_ALL
    "${SONIC_WORKING}/generated/tools/native-port-link-audit.cpp")
add_dependencies(game katana_native_port_link_audit)
add_custom_command(TARGET game POST_BUILD
    COMMAND "$<TARGET_FILE:katana_native_port_link_audit>"
        "$<TARGET_FILE:game>" "${CMAKE_BINARY_DIR}/game-native-port.map"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:game> "$<TARGET_FILE_DIR:game>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SONIC_BASE}/product/FFmpeg-LGPL.txt"
        "${SONIC_BASE}/product/FFmpeg-NOTICE.txt"
        "${SONIC_BASE}/product/FFmpeg-BUILD-CONFIGURATION.txt"
        "$<TARGET_FILE_DIR:game>"
    COMMAND_EXPAND_LISTS VERBATIM)
file(GENERATE OUTPUT "${SONIC_ROOT}/out/experimental/katana-content-root.txt"
    CONTENT "../../.local/baseline/r354/native-content\n")
if(NOT EXISTS "${SONIC_ROOT}/out/experimental/sonic-display.ini")
    configure_file(sonic-display.ini "${SONIC_ROOT}/out/experimental/sonic-display.ini" COPYONLY)
endif()
add_executable(sonic_presentation_tests EXCLUDE_FROM_ALL tools/test_presentation.cpp src/sonic_presentation.cpp)
target_include_directories(sonic_presentation_tests PRIVATE "${SONIC_ROOT}/src" "${SONIC_BASE}/sdk/include" "${SONIC_BASE}/sdk/generated/include")
target_compile_options(sonic_presentation_tests PRIVATE /EHsc /fp:strict)

# Oracle only: the interpreter and authoring SDK never enter the game link.
file(ARCHIVE_EXTRACT INPUT "${SONIC_BASE}/katana-source-178448be.zip"
    DESTINATION "${CMAKE_BINARY_DIR}/cull-test-sdk"
    PATTERNS "include/katana/runtime/dynamic_interpreter.hpp"
        "include/katana/sh4/*.hpp" "src/runtime/dynamic_interpreter.cpp")
add_executable(sonic_render_culling_tests EXCLUDE_FROM_ALL
    tools/test_render_culling.cpp src/sonic_render_culling.cpp src/sonic_presentation.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_render_culling_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_render_culling_tests PRIVATE /EHsc /fp:strict)
target_link_libraries(sonic_render_culling_tests PRIVATE
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)

add_executable(sonic_sdk_color_tests EXCLUDE_FROM_ALL tools/test_sdk_color.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_sdk_color_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_sdk_color_tests PRIVATE /EHsc /fp:strict)
target_link_libraries(sonic_sdk_color_tests PRIVATE
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)
