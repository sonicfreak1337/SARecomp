set(render_context_identity "${CMAKE_BINARY_DIR}/generated/render-context/render-context-identities.inc")
add_custom_command(OUTPUT "${render_context_identity}"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-render-context.py"
        --ram "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
        --output "${render_context_identity}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-render-context.py" "${SONIC_ROOT}/tools/prepare_collision_candidates.py"
        "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin" VERBATIM)
target_sources(${animation_title} PRIVATE "${SONIC_ROOT}/src/sonic_render_context.cpp" "${render_context_identity}")
target_include_directories(${animation_title} PRIVATE "${CMAKE_BINARY_DIR}/generated/render-context")
add_executable(sonic-render-context-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_render_context.cpp" "${SONIC_ROOT}/src/sonic_render_context.cpp"
    "${render_context_identity}" "${animation_reference}")
target_include_directories(sonic-render-context-tests PRIVATE "${SONIC_ROOT}/src" "${CMAKE_BINARY_DIR}/generated/render-context")
if(TARGET sonic_linux_title)
    target_sources(sonic-render-context-tests PRIVATE
        "${SONIC_LINUX_SDK}/src/decoder/decoder.cpp" "${SONIC_LINUX_SDK}/src/decoder/instruction_metadata.cpp")
    target_compile_options(sonic-render-context-tests PRIVATE -O2 -g0)
    target_link_options(sonic-render-context-tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(sonic-render-context-tests PRIVATE sonic_linux_services)
else()
    target_include_directories(sonic-render-context-tests PRIVATE "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
    target_compile_options(sonic-render-context-tests PRIVATE /EHsc /utf-8)
    if(SARECOMP_NATIVE_MEMORY_CAPABILITY)
        target_link_libraries(sonic-render-context-tests PRIVATE sonic_internal_diagnostics)
    endif()
    target_link_libraries(sonic-render-context-tests PRIVATE "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
        "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib" KatanaRecomp::native_port_runtime)
endif()
