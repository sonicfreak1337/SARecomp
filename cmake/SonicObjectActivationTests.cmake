add_executable(sonic-object-activation-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_object_activation.cpp"
    "${SONIC_ROOT}/src/sonic_object_activation.cpp" "${animation_reference}")
target_include_directories(sonic-object-activation-tests PRIVATE "${SONIC_ROOT}/src")
target_link_libraries(sonic-object-activation-tests PRIVATE sonic_palette_batch)
if(TARGET sonic_linux_title)
    target_sources(sonic-object-activation-tests PRIVATE
        "${SONIC_LINUX_SDK}/src/decoder/decoder.cpp" "${SONIC_LINUX_SDK}/src/decoder/instruction_metadata.cpp")
    target_compile_options(sonic-object-activation-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
    target_link_options(sonic-object-activation-tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(sonic-object-activation-tests PRIVATE sonic_linux_services)
else()
    if(SARECOMP_NATIVE_MEMORY_CAPABILITY)
        get_target_property(activation_libraries sonic-object-activation-tests LINK_LIBRARIES)
        set_property(TARGET sonic-object-activation-tests PROPERTY LINK_LIBRARIES sonic_internal_diagnostics ${activation_libraries})
    endif()
    target_include_directories(sonic-object-activation-tests PRIVATE "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
    target_compile_options(sonic-object-activation-tests PRIVATE /EHsc /utf-8 /fp:strict)
    target_link_libraries(sonic-object-activation-tests PRIVATE
        "${SONIC_ROOT}/.local/toolchain/katana_core.lib" "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
        KatanaRecomp::native_port_runtime)
endif()
