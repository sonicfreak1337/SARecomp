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
add_library(sonic_startup STATIC src/sonic_startup.cpp)
target_compile_options(sonic_startup PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_startup PRIVATE bcrypt psapi user32 gdi32)
add_executable(sonic_startup_tests EXCLUDE_FROM_ALL tools/test_startup.cpp)
target_include_directories(sonic_startup_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_startup_tests PRIVATE /EHsc /utf-8)
target_link_libraries(sonic_startup_tests PRIVATE sonic_startup)

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
target_sources(katana_native_title_adapter PRIVATE src/sonic_presentation.cpp src/sonic_render_culling.cpp src/sonic_language.cpp src/sonic_camera.cpp)
target_sources(katana_native_title_adapter PRIVATE src/sonic_quit_prompt.cpp src/sonic_quit_prompt_image.cpp)
target_sources(katana_native_title_adapter PRIVATE src/sonic_legacy_video.cpp src/sonic_options_display.cpp)
target_sources(katana_native_title_adapter PRIVATE src/sonic_input.cpp src/sonic_menu.cpp
    src/sonic_menu_image.cpp src/sonic_menu_text.cpp src/sonic_menu_runtime.cpp src/sonic_profiles.cpp src/sonic_restart.cpp)
target_sources(katana_native_title_adapter PRIVATE src/sonic_tutorial_prompt.cpp src/sonic_tutorial_art.cpp)
target_sources(katana_native_title_adapter PRIVATE src/sonic_palette_lighting.cpp src/sonic_vertex_normals.cpp src/sonic_matrix_stack.cpp src/sonic_collision_math.cpp src/sonic_matrix_inverse.cpp)
target_sources(katana_native_title_adapter PRIVATE src/sonic_triangle_contacts.cpp)
target_sources(katana_native_title_adapter PRIVATE src/sonic_atan_math.cpp)
target_sources(katana_native_title_adapter PRIVATE src/sonic_amy_hammer_effect.cpp)
add_executable(sonic_amy_hammer_effect_tests EXCLUDE_FROM_ALL
    tools/test_amy_hammer_effect.cpp src/sonic_amy_hammer_effect.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_amy_hammer_effect_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_amy_hammer_effect_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_amy_hammer_effect_tests PRIVATE bcrypt
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)
add_executable(sonic_transition_task_graph_tests EXCLUDE_FROM_ALL tools/test_transition_task_graph.cpp)
target_include_directories(sonic_transition_task_graph_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_transition_task_graph_tests PRIVATE /EHsc /utf-8)
add_executable(sonic_atan_math_tests EXCLUDE_FROM_ALL
    tools/test_atan_math.cpp src/sonic_atan_math.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_atan_math_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_atan_math_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_atan_math_tests PRIVATE bcrypt
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)
add_executable(sonic_triangle_contacts_tests EXCLUDE_FROM_ALL
    tools/test_triangle_contacts.cpp src/sonic_triangle_contacts.cpp src/sonic_collision_math.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_triangle_contacts_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_triangle_contacts_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_triangle_contacts_tests PRIVATE bcrypt
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)
add_executable(sonic_matrix_inverse_tests EXCLUDE_FROM_ALL
    tools/test_matrix_inverse.cpp src/sonic_matrix_inverse.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_matrix_inverse_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_matrix_inverse_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_matrix_inverse_tests PRIVATE bcrypt
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)
add_executable(sonic_matrix_stack_tests EXCLUDE_FROM_ALL
    tools/test_matrix_stack.cpp src/sonic_matrix_stack.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_matrix_stack_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_matrix_stack_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_matrix_stack_tests PRIVATE bcrypt
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)
add_executable(sonic_collision_math_tests EXCLUDE_FROM_ALL
    tools/test_collision_math.cpp src/sonic_collision_math.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_collision_math_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_collision_math_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_collision_math_tests PRIVATE bcrypt
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)
add_executable(sonic_descriptor_index_tests EXCLUDE_FROM_ALL tools/test_descriptor_index.cpp)
target_include_directories(sonic_descriptor_index_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_features(sonic_descriptor_index_tests PRIVATE cxx_std_20)
target_compile_options(sonic_descriptor_index_tests PRIVATE /EHsc /utf-8)
add_executable(sonic_palette_lighting_tests EXCLUDE_FROM_ALL
    tools/test_palette_lighting.cpp src/sonic_palette_lighting.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_palette_lighting_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_palette_lighting_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_palette_lighting_tests PRIVATE bcrypt
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)
add_executable(sonic_vertex_normals_tests EXCLUDE_FROM_ALL
    tools/test_vertex_normals.cpp src/sonic_vertex_normals.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_vertex_normals_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_vertex_normals_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_vertex_normals_tests PRIVATE bcrypt
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)
add_executable(sonic_tutorial_prompt_tests EXCLUDE_FROM_ALL tools/test_tutorial_prompt.cpp
    src/sonic_tutorial_prompt.cpp src/sonic_tutorial_art.cpp src/sonic_input.cpp src/sonic_presentation.cpp)
target_include_directories(sonic_tutorial_prompt_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_tutorial_prompt_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_tutorial_prompt_tests PRIVATE KatanaRecomp::native_port_runtime user32 gdi32 gdiplus)
target_link_libraries(katana_native_title_adapter PUBLIC user32 gdi32 gdiplus)
target_include_directories(katana_native_title_adapter PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_compile_options(katana_native_title_adapter PRIVATE /W4 /permissive- /EHsc /utf-8 /fp:strict)
target_link_libraries(katana_native_title_adapter PUBLIC KatanaRecomp::native_port_runtime sonic_startup)

add_library(katana_generated STATIC IMPORTED)
set_target_properties(katana_generated PROPERTIES
    IMPORTED_LOCATION "${SONIC_BASE}/aot/katana_generated.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${SONIC_BASE}/product/generated/include")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# Keep the current hook bindings; the frozen r354 dispatch predates new Options.
# Task-owner interpolation instrumentation is disabled in the preparation script.
set(motion_dispatch "${CMAKE_BINARY_DIR}/generated/motion-dispatch/native-port-dispatch.cpp")
add_custom_command(OUTPUT "${motion_dispatch}"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-motion-dispatch.py"
        "${SONIC_WORKING}/generated/code/native-port-dispatch.cpp" "${motion_dispatch}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-motion-dispatch.py"
        "${SONIC_WORKING}/generated/code/native-port-dispatch.cpp"
        "${SONIC_WORKING}/generated/.katana-generated-artifacts" VERBATIM)
add_library(sonic_dispatch STATIC "${motion_dispatch}")
# Preserve the generated owner in the original link audit. This small archive
# resolves the dispatch before the frozen archive; its old member is unused.
set_target_properties(sonic_dispatch PROPERTIES OUTPUT_NAME katana_generated
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dispatch")
target_include_directories(sonic_dispatch PRIVATE "${SONIC_WORKING}/generated/include" "${SONIC_ROOT}/src")
target_compile_options(sonic_dispatch PRIVATE /EHsc /utf-8)
target_link_libraries(sonic_dispatch PRIVATE KatanaRecomp::native_port_runtime)
add_executable(sonic_dispatch_memo_tests EXCLUDE_FROM_ALL tools/test_dispatch_memo.cpp)
target_include_directories(sonic_dispatch_memo_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_dispatch_memo_tests PRIVATE /EHsc /utf-8)
add_executable(game launcher/main.cpp)

# Private, bounded CPU sampling utility; no product dependency or overhead.
add_executable(sonic_execution_sampler EXCLUDE_FROM_ALL tools/sample_execution_thread.cpp)
target_compile_options(sonic_execution_sampler PRIVATE /EHsc /utf-8)
target_link_libraries(sonic_execution_sampler PRIVATE psapi dbghelp)
target_include_directories(game PRIVATE "${CMAKE_BINARY_DIR}/generated")
add_executable(sonic_menu_preview EXCLUDE_FROM_ALL tools/menu_preview.cpp
    src/sonic_menu.cpp src/sonic_menu_text.cpp src/sonic_menu_image.cpp
    src/sonic_input.cpp src/sonic_presentation.cpp)
target_include_directories(sonic_menu_preview PRIVATE "${SONIC_ROOT}/src"
    "${SONIC_BASE}/sdk/include" "${SONIC_BASE}/sdk/generated/include")
target_compile_options(sonic_menu_preview PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_menu_preview PRIVATE KatanaRecomp::native_port_runtime sonic_startup user32 gdi32 gdiplus)
add_executable(sonic_menu_assets_preview EXCLUDE_FROM_ALL tools/menu_assets_preview.cpp)
add_executable(sonic_options_display_tests EXCLUDE_FROM_ALL tools/test_options_display.cpp src/sonic_options_display.cpp)
target_compile_options(sonic_options_display_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_options_display_tests PRIVATE KatanaRecomp::native_port_runtime)
add_executable(sonic_enhancement_tests EXCLUDE_FROM_ALL tools/test_enhancements.cpp
    src/sonic_menu.cpp src/sonic_menu_text.cpp src/sonic_input.cpp src/sonic_profiles.cpp src/sonic_presentation.cpp)
target_include_directories(sonic_enhancement_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_enhancement_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_enhancement_tests PRIVATE sonic_startup KatanaRecomp::native_port_runtime user32)
add_executable(sonic_input_mapping_tests EXCLUDE_FROM_ALL tools/test_input_mapping.cpp
    src/sonic_input.cpp src/sonic_presentation.cpp)
target_include_directories(sonic_input_mapping_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_input_mapping_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_input_mapping_tests PRIVATE KatanaRecomp::native_port_runtime user32)
add_executable(sonic_rumble_tests EXCLUDE_FROM_ALL tools/test_rumble.cpp)
target_include_directories(sonic_rumble_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_rumble_tests PRIVATE /EHsc /utf-8 /fp:strict)
add_executable(sonic_movie_audio_tests EXCLUDE_FROM_ALL tools/test_movie_audio.cpp src/sonic_presentation.cpp)
target_include_directories(sonic_movie_audio_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_movie_audio_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_movie_audio_tests PRIVATE KatanaRecomp::native_port_runtime user32)
add_executable(sonic_restart_tests EXCLUDE_FROM_ALL tools/test_display_restart.cpp
    src/sonic_restart.cpp src/sonic_menu.cpp src/sonic_menu_image.cpp src/sonic_menu_text.cpp
    src/sonic_input.cpp src/sonic_presentation.cpp)
target_include_directories(sonic_restart_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_restart_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_restart_tests PRIVATE sonic_startup KatanaRecomp::native_port_runtime user32 gdi32 gdiplus)
target_compile_options(sonic_menu_assets_preview PRIVATE /EHsc /utf-8)
target_link_libraries(sonic_menu_assets_preview PRIVATE KatanaRecomp::native_port_runtime gdiplus)
add_executable(sonic_config WIN32 launcher/config.cpp src/sonic_presentation.cpp)
set_target_properties(sonic_config PROPERTIES OUTPUT_NAME sonic-config
    RUNTIME_OUTPUT_DIRECTORY "${SONIC_ROOT}/out/experimental")
target_include_directories(sonic_config PRIVATE "${SONIC_ROOT}/src"
    "${SONIC_BASE}/sdk/include" "${SONIC_BASE}/sdk/generated/include")
target_compile_options(sonic_config PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_config PRIVATE user32 gdi32 shell32)
add_dependencies(game sonic_config)
include(cmake/SonicVulkan.cmake)
target_sources(game PRIVATE $<TARGET_OBJECTS:sonic_graphics>)
# A source-verified local platform object fixes only Recompiled's DualSense
# right stick. It precedes the pinned archive; no SDK tree or AOT is rebuilt.
set(camera_platform "${CMAKE_BINARY_DIR}/generated/camera-platform")
add_custom_command(OUTPUT "${camera_platform}/native_port_platform.cpp"
        "${camera_platform}/native_port_input_policy.hpp"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-camera-platform.py"
        "${SONIC_BASE}/katana-source-178448be.zip" "${camera_platform}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-camera-platform.py"
        "${SONIC_BASE}/katana-source-178448be.zip" VERBATIM)
add_library(sonic_camera_platform OBJECT "${camera_platform}/native_port_platform.cpp")
target_include_directories(sonic_camera_platform PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_camera_platform PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_camera_platform PRIVATE KatanaRecomp::native_port_runtime)
target_sources(game PRIVATE $<TARGET_OBJECTS:sonic_camera_platform>)
add_library(sonic_sony_input STATIC src/sonic_sony_input.cpp)
file(SHA256 "${SONIC_ROOT}/third_party/sdl3/lib/x64/SDL3.dll" sonic_sdl_runtime_sha256)
if(NOT sonic_sdl_runtime_sha256 STREQUAL "1f98969319302a100931f4385e5918a0bd53ab07773040682d22e7edb54858c0")
    message(FATAL_ERROR "Pinned SDL 3.4.16 x64 runtime identity changed")
endif()
target_include_directories(sonic_sony_input PRIVATE "${SONIC_ROOT}/third_party/sdl3/include")
target_compile_options(sonic_sony_input PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_sony_input PRIVATE KatanaRecomp::native_port_runtime)
target_link_libraries(game PRIVATE sonic_sony_input)
add_executable(sonic_sony_input_tests EXCLUDE_FROM_ALL tools/test_sony_input.cpp)
target_include_directories(sonic_sony_input_tests PRIVATE "${SONIC_ROOT}/src" "${SONIC_ROOT}/third_party/sdl3/include")
target_compile_options(sonic_sony_input_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_sony_input_tests PRIVATE sonic_sony_input KatanaRecomp::native_port_runtime)

# Port-local endpoint replacement; preserve the frozen stream/domain ABI.
set(audio_output "${CMAKE_BINARY_DIR}/generated/audio-output")
add_custom_command(OUTPUT "${audio_output}/native_port_audio.cpp"
        "${audio_output}/native_port_audio_execution_domain.hpp"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-audio-output.py"
        "${SONIC_BASE}/katana-source-178448be.zip" "${audio_output}" "${SONIC_ROOT}/src/sonic_audio_recovery.inc"
    DEPENDS "${SONIC_ROOT}/tools/prepare-audio-output.py" "${SONIC_ROOT}/src/sonic_audio_recovery.inc"
        "${SONIC_BASE}/katana-source-178448be.zip" VERBATIM)
add_library(sonic_audio_output OBJECT "${audio_output}/native_port_audio.cpp")
target_include_directories(sonic_audio_output PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_audio_output PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_audio_output PRIVATE KatanaRecomp::native_port_runtime ole32 winmm)
target_sources(game PRIVATE $<TARGET_OBJECTS:sonic_audio_output>)
target_link_libraries(game PRIVATE ole32)
add_executable(sonic_audio_recovery_tests EXCLUDE_FROM_ALL tools/test_audio_recovery.cpp $<TARGET_OBJECTS:sonic_audio_output>)
target_include_directories(sonic_audio_recovery_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_audio_recovery_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_audio_recovery_tests PRIVATE KatanaRecomp::native_port_runtime ole32 winmm)

# Route mixed MLT containers at the actual note/program boundary, including
# sequence notes and restored voices. The frozen SDK and AOT remain intact.
set(audio_buses "${CMAKE_BINARY_DIR}/generated/audio-buses")
add_custom_command(OUTPUT "${audio_buses}/native_port_sound_bank.cpp"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-audio-buses.py"
        "${SONIC_BASE}/katana-source-178448be.zip" "${audio_buses}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-audio-buses.py"
        "${SONIC_BASE}/katana-source-178448be.zip" VERBATIM)
add_library(sonic_audio_buses OBJECT "${audio_buses}/native_port_sound_bank.cpp")
target_include_directories(sonic_audio_buses PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_audio_buses PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_audio_buses PRIVATE KatanaRecomp::native_port_runtime)
target_sources(game PRIVATE $<TARGET_OBJECTS:sonic_audio_buses>)
add_executable(sonic_audio_bus_tests EXCLUDE_FROM_ALL tools/test_audio_buses.cpp
    src/sonic_presentation.cpp $<TARGET_OBJECTS:sonic_audio_buses> $<TARGET_OBJECTS:sonic_audio_output>)
target_include_directories(sonic_audio_bus_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_audio_bus_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_audio_bus_tests PRIVATE KatanaRecomp::native_port_runtime ole32 winmm)

add_executable(sonic_input_poll_tests EXCLUDE_FROM_ALL tools/test_input_poll.cpp
    src/sonic_input.cpp src/sonic_presentation.cpp $<TARGET_OBJECTS:sonic_camera_platform>)
target_include_directories(sonic_input_poll_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_input_poll_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_input_poll_tests PRIVATE KatanaRecomp::native_port_runtime user32)
target_link_libraries(sonic_input_poll_tests PRIVATE sonic_sony_input)
target_link_libraries(game PRIVATE sonic_vulkan)
target_include_directories(game BEFORE PRIVATE "${SONIC_WORKING}/generated/include")
target_include_directories(game PRIVATE "${SONIC_ROOT}/src")
target_compile_options(game PRIVATE /W4 /permissive- /EHsc /utf-8)
target_compile_definitions(game PRIVATE KATANA_NATIVE_PORT_PRODUCT=1
    KATANA_PORT_BUILD_PROFILE_NAME="performance" KATANA_PORT_PGO_MODE_NAME="off")
include(cmake/SonicPreparedReads.cmake)
include(cmake/SonicFpuCalls.cmake)
include(cmake/SonicCompactAot.cmake)
include(cmake/SonicFpuRuntime.cmake)
target_link_libraries(game PRIVATE sonic_dispatch katana_generated katana_native_title_adapter)
set_target_properties(game PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SONIC_ROOT}/out/experimental")
target_link_options(game PRIVATE /STACK:16777216 "/MAP:${CMAKE_BINARY_DIR}/game-native-port.map"
    /threads:3 "/lldltocache:${CMAKE_BINARY_DIR}/thinlto-cache")
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/generated/sonic-link-audit.cpp"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-renderer-link-audit.py"
        "${SONIC_WORKING}/generated/tools/native-port-link-audit.cpp"
        "${CMAKE_BINARY_DIR}/generated/sonic-link-audit.cpp"
        ${sonic_ram_read_audit_argument}
        ${sonic_fpu_call_audit_argument}
        ${sonic_compact_aot_audit_argument}
        ${sonic_fpu_runtime_audit_argument}
    DEPENDS "${SONIC_ROOT}/tools/prepare-renderer-link-audit.py"
        "${SONIC_WORKING}/generated/tools/native-port-link-audit.cpp" VERBATIM)
add_executable(katana_native_port_link_audit EXCLUDE_FROM_ALL
    "${CMAKE_BINARY_DIR}/generated/sonic-link-audit.cpp")
add_dependencies(game katana_native_port_link_audit)
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SONIC_ROOT}/third_party/sdl3/lib/x64/SDL3.dll" "$<TARGET_FILE_DIR:game>/SDL3.dll"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:game>/licenses/sdl3"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SONIC_ROOT}/third_party/sdl3/LICENSE.txt" "$<TARGET_FILE_DIR:game>/licenses/sdl3/LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:game>/assets/ui"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SONIC_ROOT}/assets/ui/options-background.png"
        "$<TARGET_FILE_DIR:game>/assets/ui/options-background.png"
    COMMAND "$<TARGET_FILE:katana_native_port_link_audit>"
        "$<TARGET_FILE:game>" "${CMAKE_BINARY_DIR}/game-native-port.map"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:game> "$<TARGET_FILE_DIR:game>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SONIC_BASE}/product/FFmpeg-LGPL.txt"
        "${SONIC_BASE}/product/FFmpeg-NOTICE.txt"
        "${SONIC_BASE}/product/FFmpeg-BUILD-CONFIGURATION.txt"
        "$<TARGET_FILE_DIR:game>"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:game>/licenses/volk"
        "$<TARGET_FILE_DIR:game>/licenses/vulkan-headers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SONIC_ROOT}/third_party/README.md" "$<TARGET_FILE_DIR:game>/THIRD-PARTY-NOTICES.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SONIC_ROOT}/third_party/volk/LICENSE.md" "$<TARGET_FILE_DIR:game>/licenses/volk/LICENSE.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SONIC_ROOT}/third_party/vulkan-headers/LICENSE.md"
        "${SONIC_ROOT}/third_party/vulkan-headers/LICENSES/Apache-2.0.txt"
        "${SONIC_ROOT}/third_party/vulkan-headers/LICENSES/MIT.txt"
        "$<TARGET_FILE_DIR:game>/licenses/vulkan-headers"
    COMMAND_EXPAND_LISTS VERBATIM)
file(GENERATE OUTPUT "${SONIC_ROOT}/out/experimental/katana-content-root.txt"
    CONTENT "../../.local/baseline/r354/native-content\n")
if(NOT EXISTS "${SONIC_ROOT}/out/experimental/sonic-display.ini")
    configure_file(sonic-display.ini "${SONIC_ROOT}/out/experimental/sonic-display.ini" COPYONLY)
endif()
add_executable(sonic_presentation_tests EXCLUDE_FROM_ALL tools/test_presentation.cpp src/sonic_presentation.cpp)
target_include_directories(sonic_presentation_tests PRIVATE "${SONIC_ROOT}/src" "${SONIC_BASE}/sdk/include" "${SONIC_BASE}/sdk/generated/include")
target_compile_options(sonic_presentation_tests PRIVATE /EHsc /fp:strict)

add_executable(sonic_quit_prompt_tests EXCLUDE_FROM_ALL tools/test_quit_prompt.cpp
    src/sonic_quit_prompt.cpp src/sonic_quit_prompt_image.cpp src/sonic_presentation.cpp src/sonic_input.cpp)
target_include_directories(sonic_quit_prompt_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_quit_prompt_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_quit_prompt_tests PRIVATE
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime gdi32 user32)

add_executable(sonic_camera_tests EXCLUDE_FROM_ALL tools/test_camera.cpp
    src/sonic_camera.cpp src/sonic_presentation.cpp src/sonic_input.cpp)
target_include_directories(sonic_camera_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_camera_tests PRIVATE /EHsc /fp:strict)
target_link_libraries(sonic_camera_tests PRIVATE
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)

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

add_executable(sonic_texture_sentinel_tests EXCLUDE_FROM_ALL tools/test_texture_sentinel.cpp)
add_executable(sonic_contour_tests EXCLUDE_FROM_ALL tools/test_contour.cpp)
target_include_directories(sonic_contour_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_contour_tests PRIVATE /EHsc /utf-8)
target_include_directories(sonic_texture_sentinel_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_texture_sentinel_tests PRIVATE /EHsc /utf-8)

add_executable(sonic_language_tests EXCLUDE_FROM_ALL tools/test_language.cpp
    src/sonic_language.cpp src/sonic_presentation.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_language_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_language_tests PRIVATE /EHsc /fp:strict)
target_link_libraries(sonic_language_tests PRIVATE
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)

add_executable(sonic_legacy_video_tests EXCLUDE_FROM_ALL tools/test_legacy_video.cpp
    src/sonic_legacy_video.cpp
    "${CMAKE_BINARY_DIR}/cull-test-sdk/src/runtime/dynamic_interpreter.cpp")
target_include_directories(sonic_legacy_video_tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
target_compile_options(sonic_legacy_video_tests PRIVATE /EHsc /fp:strict)
target_link_libraries(sonic_legacy_video_tests PRIVATE
    bcrypt
    "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
    "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
    KatanaRecomp::native_port_runtime)
