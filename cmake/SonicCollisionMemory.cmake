# Qualification uses the original instruction executor in addition to the
# authenticated closed RAM path; no test interpreter is linked into game.
if(TARGET sonic_linux_title)
    add_library(sonic_collision_test_oracle STATIC EXCLUDE_FROM_ALL
        "${SONIC_LINUX_SDK}/src/runtime/dynamic_interpreter.cpp"
        "${SONIC_LINUX_SDK}/src/decoder/decoder.cpp"
        "${SONIC_LINUX_SDK}/src/decoder/instruction_metadata.cpp")
    target_compile_options(sonic_collision_test_oracle PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
    target_link_libraries(sonic_collision_test_oracle PUBLIC sonic_linux_services)
    foreach(family IN ITEMS collision_math triangle_contacts collision_candidates)
        add_executable(sonic_${family}_tests EXCLUDE_FROM_ALL
            "${SONIC_ROOT}/tools/test_${family}.cpp" "${SONIC_ROOT}/src/sonic_${family}.cpp")
        target_include_directories(sonic_${family}_tests PRIVATE "${SONIC_ROOT}/src"
            "${CMAKE_BINARY_DIR}/generated/collision-candidates" "${CMAKE_BINARY_DIR}/generated/fpu-body")
        target_compile_options(sonic_${family}_tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
        target_link_options(sonic_${family}_tests PRIVATE -Wl,--gc-sections)
        target_link_libraries(sonic_${family}_tests PRIVATE sonic_collision_test_oracle)
    endforeach()
    target_sources(sonic_triangle_contacts_tests PRIVATE "${SONIC_ROOT}/src/sonic_collision_math.cpp")
    target_sources(sonic_collision_candidates_tests PRIVATE "${SONIC_ROOT}/src/sonic_collision_math.cpp"
        "${SONIC_ROOT}/src/sonic_matrix_stack.cpp" "${SONIC_ROOT}/src/sonic_matrix_vectors.cpp"
        "${SONIC_ROOT}/src/sonic_matrix_inverse.cpp")
elseif(SARECOMP_NATIVE_MEMORY_CAPABILITY)
    foreach(family IN ITEMS collision_math triangle_contacts collision_candidates)
        get_target_property(libraries sonic_${family}_tests LINK_LIBRARIES)
        set_property(TARGET sonic_${family}_tests PROPERTY LINK_LIBRARIES sonic_internal_diagnostics ${libraries})
    endforeach()
endif()
add_executable(sonic_collision_memory_tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_collision_memory.cpp")
target_include_directories(sonic_collision_memory_tests PRIVATE "${SONIC_ROOT}/src")
if(TARGET sonic_linux_title)
    target_compile_options(sonic_collision_memory_tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
    target_link_libraries(sonic_collision_memory_tests PRIVATE sonic_linux_services)
else()
    target_compile_options(sonic_collision_memory_tests PRIVATE /EHsc /utf-8 /fp:strict)
    target_link_libraries(sonic_collision_memory_tests PRIVATE sonic_internal_diagnostics
        "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib" KatanaRecomp::native_port_runtime)
endif()
