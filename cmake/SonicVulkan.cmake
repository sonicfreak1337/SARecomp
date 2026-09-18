# Port-local graphics adaptation; all other pinned SDK and AOT members stay frozen.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(SONIC_SPIRV "${CMAKE_BINARY_DIR}/generated/sonic_vulkan_shaders.hpp")
add_custom_command(OUTPUT "${SONIC_SPIRV}"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/compile-vulkan-shaders.py" --output "${SONIC_SPIRV}"
    DEPENDS "${SONIC_ROOT}/tools/compile-vulkan-shaders.py"
        "${SONIC_ROOT}/src/renderer/pinned/native_port_graphics.cpp"
    VERBATIM)
set_source_files_properties(third_party/volk/volk.c PROPERTIES LANGUAGE CXX)
add_library(sonic_vulkan STATIC src/renderer/sonic_vulkan.cpp third_party/volk/volk.c "${SONIC_SPIRV}")
target_include_directories(sonic_vulkan PRIVATE "${CMAKE_BINARY_DIR}/generated"
    "${SONIC_ROOT}/third_party/volk" "${SONIC_ROOT}/third_party/vulkan-headers/include")
target_compile_definitions(sonic_vulkan PRIVATE VK_USE_PLATFORM_WIN32_KHR VK_NO_PROTOTYPES)
target_compile_options(sonic_vulkan PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_vulkan PRIVATE KatanaRecomp::native_port_runtime sonic_startup)
add_executable(sonic_vulkan_present_tests EXCLUDE_FROM_ALL tools/test_vulkan_present.cpp)
target_include_directories(sonic_vulkan_present_tests PRIVATE "${SONIC_ROOT}/src" "${SONIC_ROOT}/third_party/vulkan-headers/include")
target_compile_options(sonic_vulkan_present_tests PRIVATE /EHsc /utf-8)
add_library(sonic_graphics OBJECT src/renderer/pinned/native_port_graphics.cpp)
add_executable(sonic_model_vertex_stream_tests EXCLUDE_FROM_ALL tools/test_model_vertex_stream.cpp)
target_include_directories(sonic_model_vertex_stream_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_model_vertex_stream_tests PRIVATE /EHsc /fp:strict)
target_link_libraries(sonic_model_vertex_stream_tests PRIVATE KatanaRecomp::native_port_runtime)
add_executable(sonic_model_packet_tests EXCLUDE_FROM_ALL tools/test_model_packets.cpp
    src/sonic_input.cpp src/sonic_presentation.cpp $<TARGET_OBJECTS:sonic_graphics>)
target_include_directories(sonic_model_packet_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_model_packet_tests PRIVATE /EHsc /fp:strict)
target_link_libraries(sonic_model_packet_tests PRIVATE sonic_vulkan KatanaRecomp::native_port_runtime)
target_compile_options(sonic_graphics PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_graphics PRIVATE KatanaRecomp::native_port_runtime sonic_startup)
add_executable(sonic_renderer_tests EXCLUDE_FROM_ALL tools/test_renderers.cpp src/sonic_input.cpp src/sonic_presentation.cpp $<TARGET_OBJECTS:sonic_graphics>)
target_include_directories(sonic_renderer_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_renderer_tests PRIVATE /EHsc /fp:strict)
target_link_libraries(sonic_renderer_tests PRIVATE sonic_vulkan KatanaRecomp::native_port_runtime)
add_executable(sonic_vulkan_submission_benchmark EXCLUDE_FROM_ALL tools/benchmark_vulkan_submission.cpp
    src/sonic_input.cpp src/sonic_presentation.cpp $<TARGET_OBJECTS:sonic_graphics>)
target_include_directories(sonic_vulkan_submission_benchmark PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_vulkan_submission_benchmark PRIVATE /EHsc /fp:strict)
target_link_libraries(sonic_vulkan_submission_benchmark PRIVATE sonic_vulkan KatanaRecomp::native_port_runtime)

add_executable(sonic_host_ui_tests EXCLUDE_FROM_ALL tools/test_host_ui.cpp
    src/sonic_input.cpp src/sonic_presentation.cpp $<TARGET_OBJECTS:sonic_graphics>)
target_include_directories(sonic_host_ui_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_host_ui_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_host_ui_tests PRIVATE sonic_vulkan KatanaRecomp::native_port_runtime)

add_executable(sonic_pacing_tests EXCLUDE_FROM_ALL tools/test_pacing.cpp
    src/sonic_input.cpp src/sonic_presentation.cpp $<TARGET_OBJECTS:sonic_graphics>)
target_include_directories(sonic_pacing_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_pacing_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_pacing_tests PRIVATE sonic_vulkan KatanaRecomp::native_port_runtime)

add_executable(sonic_render_completion_tests EXCLUDE_FROM_ALL tools/test_render_completion.cpp
    src/sonic_input.cpp src/sonic_presentation.cpp $<TARGET_OBJECTS:sonic_graphics>)
target_include_directories(sonic_render_completion_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_render_completion_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_render_completion_tests PRIVATE sonic_vulkan KatanaRecomp::native_port_runtime)

add_executable(sonic_fullscreen_effect_tests EXCLUDE_FROM_ALL tools/test_fullscreen_effects.cpp
    src/sonic_input.cpp src/sonic_presentation.cpp $<TARGET_OBJECTS:sonic_graphics>)
target_include_directories(sonic_fullscreen_effect_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_fullscreen_effect_tests PRIVATE /EHsc /fp:strict)
target_link_libraries(sonic_fullscreen_effect_tests PRIVATE sonic_vulkan KatanaRecomp::native_port_runtime)

#[[ Archived with the withdrawn render-interpolation prototype.
add_executable(sonic_motion_tests EXCLUDE_FROM_ALL tools/test_motion.cpp
    src/sonic_input.cpp src/sonic_presentation.cpp $<TARGET_OBJECTS:sonic_graphics>)
target_include_directories(sonic_motion_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_motion_tests PRIVATE /EHsc /fp:strict)
target_link_libraries(sonic_motion_tests PRIVATE sonic_vulkan KatanaRecomp::native_port_runtime)
]]
