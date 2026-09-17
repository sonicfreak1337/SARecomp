set(movement_dir "${CMAKE_BINARY_DIR}/generated/movement-resolver")
set(movement_outputs "${movement_dir}/movement-body.inc" "${movement_dir}/movement-identities.inc")
add_custom_command(OUTPUT ${movement_outputs}
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-movement-resolver.py"
        --ram "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
        --output "${movement_dir}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-movement-resolver.py" "${SONIC_ROOT}/tools/prepare_collision_candidates.py"
        "${SONIC_ROOT}/tools/prepare_near_collision.py" "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
    VERBATIM)
target_sources(${animation_title} PRIVATE "${SONIC_ROOT}/src/sonic_movement_resolver.cpp" ${movement_outputs})
target_include_directories(${animation_title} PRIVATE "${movement_dir}")
add_executable(sonic-movement-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_movement_resolver.cpp"
    "${SONIC_ROOT}/src/sonic_movement_resolver.cpp" ${movement_outputs} "${animation_reference}")
target_include_directories(sonic-movement-tests PRIVATE "${SONIC_ROOT}/src" "${movement_dir}")
target_compile_definitions(sonic-movement-tests PRIVATE SARECOMP_MOVEMENT_TEST_COVERAGE=1)
target_link_libraries(sonic-movement-tests PRIVATE sonic_palette_batch)
if(TARGET sonic_linux_title)
    target_sources(sonic-movement-tests PRIVATE "${SONIC_LINUX_SDK}/src/decoder/decoder.cpp" "${SONIC_LINUX_SDK}/src/decoder/instruction_metadata.cpp")
    target_compile_options(sonic-movement-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
    target_link_options(sonic-movement-tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(sonic-movement-tests PRIVATE sonic_linux_services)
else()
    if(SARECOMP_NATIVE_MEMORY_CAPABILITY)
        get_target_property(movement_libraries sonic-movement-tests LINK_LIBRARIES)
        set_property(TARGET sonic-movement-tests PROPERTY LINK_LIBRARIES sonic_internal_diagnostics ${movement_libraries})
    endif()
    target_include_directories(sonic-movement-tests PRIVATE "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
    target_compile_options(sonic-movement-tests PRIVATE /EHsc /utf-8 /fp:strict)
    target_link_libraries(sonic-movement-tests PRIVATE "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
        "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib" KatanaRecomp::native_port_runtime)
endif()
