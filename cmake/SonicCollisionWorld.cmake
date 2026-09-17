set(collision-world_dir "${CMAKE_BINARY_DIR}/generated/collision-world-resolver")
set(collision-world_outputs "${collision-world_dir}/world-identities.inc")
foreach(world_part world eligibility hierarchy polygons vertices object_allocate object_release polygon_dot buckets_clear polygon_allocate buckets_join)
    list(APPEND collision-world_outputs "${collision-world_dir}/world-${world_part}.inc")
endforeach()
add_custom_command(OUTPUT ${collision-world_outputs}
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-collision-world.py"
        --ram "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
        --output "${collision-world_dir}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-collision-world.py" "${SONIC_ROOT}/tools/prepare_collision_candidates.py"
        "${SONIC_ROOT}/tools/prepare-movement-resolver.py" "${SONIC_ROOT}/tools/prepare_near_collision.py" "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
    VERBATIM)
target_sources(${animation_title} PRIVATE "${SONIC_ROOT}/src/sonic_collision_world.cpp" ${collision-world_outputs})
target_include_directories(${animation_title} PRIVATE "${collision-world_dir}")
add_executable(sonic-collision-world-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_collision_world.cpp"
    "${SONIC_ROOT}/src/sonic_collision_world.cpp" ${collision-world_outputs} "${animation_reference}")
target_include_directories(sonic-collision-world-tests PRIVATE "${SONIC_ROOT}/src" "${collision-world_dir}")
target_compile_definitions(sonic-collision-world-tests PRIVATE SARECOMP_COLLISION_WORLD_TEST_COVERAGE=1)
target_link_libraries(sonic-collision-world-tests PRIVATE sonic_palette_batch)
if(TARGET sonic_linux_title)
    # Use the game's already qualified bounded/glibc comparison bridge. The
    # oracle compares all 16 MiB at every callback; Zig's bytewise bcmp otherwise
    # dominates TCG test time without adding coverage or a stricter comparison.
    target_sources(sonic-collision-world-tests PRIVATE $<TARGET_OBJECTS:sonic_linux_memory>)
    target_sources(sonic-collision-world-tests PRIVATE "${SONIC_LINUX_SDK}/src/decoder/decoder.cpp" "${SONIC_LINUX_SDK}/src/decoder/instruction_metadata.cpp")
    target_compile_options(sonic-collision-world-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
    target_link_options(sonic-collision-world-tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(sonic-collision-world-tests PRIVATE sonic_linux_services)
else()
    if(SARECOMP_NATIVE_MEMORY_CAPABILITY)
        get_target_property(collision-world_libraries sonic-collision-world-tests LINK_LIBRARIES)
        set_property(TARGET sonic-collision-world-tests PROPERTY LINK_LIBRARIES sonic_internal_diagnostics ${collision-world_libraries})
    endif()
    target_include_directories(sonic-collision-world-tests PRIVATE "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
    target_compile_options(sonic-collision-world-tests PRIVATE /EHsc /utf-8 /fp:strict)
    target_link_libraries(sonic-collision-world-tests PRIVATE "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
        "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib" KatanaRecomp::native_port_runtime)
endif()
