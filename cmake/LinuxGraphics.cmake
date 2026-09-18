set(sonic_linux_graphics_dir "${CMAKE_BINARY_DIR}/generated/linux-graphics")
add_executable(sonic-linux-model-vertex-stream-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_model_vertex_stream.cpp")
target_include_directories(sonic-linux-model-vertex-stream-tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic-linux-model-vertex-stream-tests PRIVATE -O2 -g0 -ffp-contract=off)
target_link_libraries(sonic-linux-model-vertex-stream-tests PRIVATE sonic_linux_sdk_headers)
set(sonic_linux_graphics_shared)
foreach(part IN ITEMS constants types methods members)
    list(APPEND sonic_linux_graphics_shared "${sonic_linux_graphics_dir}/linux_graphics_${part}.inc")
endforeach()
add_custom_command(OUTPUT ${sonic_linux_graphics_shared}
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-linux-graphics.py"
        "${SONIC_ROOT}/src/renderer/pinned/native_port_graphics.cpp" "${sonic_linux_graphics_dir}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-linux-graphics.py" "${SONIC_ROOT}/src/renderer/pinned/native_port_graphics.cpp" VERBATIM)
add_library(sonic_linux_graphics STATIC "${SONIC_ROOT}/src/renderer/pinned/native_port_graphics.cpp"
    "${SONIC_LINUX_SDK}/src/runtime/native_port_telemetry.cpp"
    "${SONIC_LINUX_SDK}/src/runtime/native_port_frame_queue.cpp"
    "${SONIC_LINUX_SDK}/src/runtime/native_port_graphics_command_stream.cpp" ${sonic_linux_graphics_shared})
target_include_directories(sonic_linux_graphics PRIVATE "${SONIC_LINUX_SDK}/src/runtime" "${sonic_linux_graphics_dir}")
target_compile_options(sonic_linux_graphics PRIVATE -O2 -g0 -ffunction-sections -fdata-sections -ffp-contract=off)
target_link_libraries(sonic_linux_graphics PUBLIC sonic_linux_vulkan sonic_linux_platform sonic_linux_sdk_headers SDL3::SDL3 Threads::Threads)
add_executable(sonic-linux-renderer-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_renderers.cpp"
    "${SONIC_ROOT}/src/sonic_input.cpp" "${SONIC_ROOT}/src/sonic_presentation.cpp")
target_include_directories(sonic-linux-renderer-tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic-linux-renderer-tests PRIVATE -O2 -g0 -ffp-contract=off)
target_link_libraries(sonic-linux-renderer-tests PRIVATE sonic_linux_graphics)
set_target_properties(sonic-linux-renderer-tests PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/lib")
add_executable(sonic-linux-menu-input-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_linux_menu_input.cpp"
    "${SONIC_ROOT}/src/sonic_input.cpp" "${SONIC_ROOT}/src/sonic_presentation.cpp"
    "${SONIC_ROOT}/src/sonic_menu.cpp" "${SONIC_ROOT}/src/sonic_menu_text.cpp")
target_include_directories(sonic-linux-menu-input-tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic-linux-menu-input-tests PRIVATE -O2 -g0)
target_link_libraries(sonic-linux-menu-input-tests PRIVATE sonic_linux_graphics)
set_target_properties(sonic-linux-menu-input-tests PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/lib")
add_executable(sonic-linux-submission-benchmark EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/benchmark_vulkan_submission.cpp"
    "${SONIC_ROOT}/src/sonic_input.cpp" "${SONIC_ROOT}/src/sonic_presentation.cpp")
target_include_directories(sonic-linux-submission-benchmark PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic-linux-submission-benchmark PRIVATE -O2 -g0)
target_link_libraries(sonic-linux-submission-benchmark PRIVATE sonic_linux_graphics)
set_target_properties(sonic-linux-submission-benchmark PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/lib")
add_executable(sonic-linux-model-packet-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_model_packets.cpp"
    "${SONIC_ROOT}/src/sonic_input.cpp" "${SONIC_ROOT}/src/sonic_presentation.cpp")
target_include_directories(sonic-linux-model-packet-tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic-linux-model-packet-tests PRIVATE -O2 -g0 -ffp-contract=off)
target_link_libraries(sonic-linux-model-packet-tests PRIVATE sonic_linux_graphics)
set_target_properties(sonic-linux-model-packet-tests PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/lib")
