set(sonic_sdl_audio "${CMAKE_BINARY_DIR}/generated/linux-audio/native_port_audio.cpp")
add_custom_command(OUTPUT "${sonic_sdl_audio}"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-linux-audio.py"
        "${SONIC_LINUX_SDK}/src/runtime/native_port_audio.cpp" "${sonic_sdl_audio}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-linux-audio.py"
        "${SONIC_LINUX_SDK}/src/runtime/native_port_audio.cpp" VERBATIM)
add_library(sonic_linux_audio STATIC "${sonic_sdl_audio}"
    "${SONIC_LINUX_SDK}/src/runtime/native_port_audio_execution_domain.cpp"
    "${SONIC_LINUX_SDK}/src/runtime/native_port_audio_command_queue.cpp"
    "${SONIC_LINUX_SDK}/src/runtime/native_port_telemetry.cpp")
target_include_directories(sonic_linux_audio PRIVATE "${SONIC_ROOT}/src" "${SONIC_ROOT}/src/linux"
    "${SONIC_LINUX_SDK}/src/runtime")
target_compile_options(sonic_linux_audio PRIVATE -O2 -g0 -ffunction-sections -fdata-sections -ffp-contract=off)
target_link_libraries(sonic_linux_audio PUBLIC sonic_linux_sdk_headers SDL3::SDL3 Threads::Threads)
