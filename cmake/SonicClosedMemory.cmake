add_executable(sonic-native-closed-memory-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_native_closed_memory.cpp"
    "${SONIC_ROOT}/src/sonic_palette_lighting.cpp" "${SONIC_ROOT}/src/sonic_matrix_stack.cpp"
    "${SONIC_ROOT}/src/sonic_matrix_vectors.cpp" "${SONIC_ROOT}/src/sonic_matrix_inverse.cpp"
    "${SONIC_ROOT}/src/sonic_motion_sampling.cpp")
target_include_directories(sonic-native-closed-memory-tests PRIVATE "${SONIC_ROOT}/src"
    "${CMAKE_BINARY_DIR}/generated/motion-sampling" "${CMAKE_BINARY_DIR}/generated/fpu-body")
add_dependencies(sonic-native-closed-memory-tests ${animation_title})
if(TARGET sonic_linux_title)
    target_compile_options(sonic-native-closed-memory-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
    target_link_options(sonic-native-closed-memory-tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(sonic-native-closed-memory-tests PRIVATE sonic_linux_services)
else()
    target_compile_options(sonic-native-closed-memory-tests PRIVATE /EHsc /utf-8 /fp:strict)
    if(SARECOMP_NATIVE_MEMORY_CAPABILITY)
        foreach(owner IN ITEMS sonic-native-closed-memory-tests sonic-animation-hierarchy-tests)
            get_target_property(existing_libraries ${owner} LINK_LIBRARIES)
            if(NOT existing_libraries)
                set(existing_libraries)
            endif()
            set_property(TARGET ${owner} PROPERTY LINK_LIBRARIES sonic_internal_diagnostics ${existing_libraries})
        endforeach()
    endif()
    target_link_libraries(sonic-native-closed-memory-tests PRIVATE
        "${SONIC_ROOT}/.local/toolchain/katana_core.lib" "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
        KatanaRecomp::native_port_runtime)
endif()
