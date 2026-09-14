set(sonic_posix_platform "${CMAKE_BINARY_DIR}/generated/linux-platform")
add_custom_command(OUTPUT "${sonic_posix_platform}/native_port_platform.cpp" "${sonic_posix_platform}/native_save_codec.inc"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-linux-platform.py"
        "${SONIC_LINUX_SDK}/src/runtime/native_port_platform.cpp" "${sonic_posix_platform}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-linux-platform.py"
        "${SONIC_LINUX_SDK}/src/runtime/native_port_platform.cpp" VERBATIM)
add_library(sonic_linux_platform STATIC "${sonic_posix_platform}/native_port_platform.cpp"
    "${SONIC_LINUX_SDK}/src/io/input_provenance.cpp"
    "${SONIC_LINUX_SDK}/src/io/json_report.cpp" "${SONIC_LINUX_SDK}/src/progress.cpp")
target_include_directories(sonic_linux_platform PRIVATE "${SONIC_ROOT}/src/linux" "${sonic_posix_platform}")
target_compile_options(sonic_linux_platform PRIVATE -O2 -g0 -ffunction-sections -fdata-sections)
target_link_libraries(sonic_linux_platform PUBLIC sonic_linux_sdk_headers SDL3::SDL3 Threads::Threads)
