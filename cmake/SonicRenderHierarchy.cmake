set(render_hierarchy_dir "${CMAKE_BINARY_DIR}/generated/render-hierarchy")
set(render_hierarchy_outputs "${render_hierarchy_dir}/hierarchy-identities.inc"
    "${render_hierarchy_dir}/hierarchy-switch.inc" "${render_hierarchy_dir}/hierarchy-members.inc")
foreach(part hierarchy static_position static_zyx static_yxz static_scale srt_static srt_single srt_double srt_triple srt_quad
    position scale rotate_zyx rotate_yxz key_index float_key angle_key push pop translate_register scale_register rotate_zyx_register rotate_yxz_register
    blend_hierarchy blend_static blend_single blend_double blend_triple blend_quad blend_static_position blend_static_scale blend_static_angle
    blend_position blend_scale blend_angle blend_key_index blend_float_key blend_angle_key blend_apply)
    list(APPEND render_hierarchy_outputs "${render_hierarchy_dir}/hierarchy-${part}.inc")
endforeach()
add_custom_command(OUTPUT ${render_hierarchy_outputs}
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-render-hierarchy.py"
        --ram "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
        --output "${render_hierarchy_dir}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-render-hierarchy.py" "${SONIC_ROOT}/tools/prepare-collision-world.py"
        "${SONIC_ROOT}/tools/prepare_collision_candidates.py" "${SONIC_ROOT}/tools/prepare_motion_sampling.py"
        "${SONIC_ROOT}/tools/prepare-movement-resolver.py" "${SONIC_ROOT}/tools/prepare_near_collision.py"
        "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
    VERBATIM)
target_sources(${animation_title} PRIVATE "${SONIC_ROOT}/src/sonic_render_hierarchy.cpp" ${render_hierarchy_outputs})
target_include_directories(${animation_title} PRIVATE "${render_hierarchy_dir}")
add_executable(sonic-render-hierarchy-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_render_hierarchy.cpp"
    "${SONIC_ROOT}/src/sonic_render_hierarchy.cpp" ${render_hierarchy_outputs} "${animation_reference}")
target_include_directories(sonic-render-hierarchy-tests PRIVATE "${SONIC_ROOT}/src" "${render_hierarchy_dir}")
target_compile_definitions(sonic-render-hierarchy-tests PRIVATE SARECOMP_RENDER_HIERARCHY_TEST_COVERAGE=1)
target_link_libraries(sonic-render-hierarchy-tests PRIVATE sonic_palette_batch)
if(TARGET sonic_linux_title)
    # Use the game's already qualified bounded/glibc comparison bridge. The
    # oracle compares all 16 MiB at every callback; Zig's bytewise bcmp otherwise
    # dominates TCG test time without adding coverage or a stricter comparison.
    target_sources(sonic-render-hierarchy-tests PRIVATE $<TARGET_OBJECTS:sonic_linux_memory>)
    target_sources(sonic-render-hierarchy-tests PRIVATE "${SONIC_LINUX_SDK}/src/decoder/decoder.cpp" "${SONIC_LINUX_SDK}/src/decoder/instruction_metadata.cpp")
    target_compile_options(sonic-render-hierarchy-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
    target_link_options(sonic-render-hierarchy-tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(sonic-render-hierarchy-tests PRIVATE sonic_linux_services)
else()
    if(SARECOMP_NATIVE_MEMORY_CAPABILITY)
        get_target_property(render_hierarchy_libraries sonic-render-hierarchy-tests LINK_LIBRARIES)
        set_property(TARGET sonic-render-hierarchy-tests PROPERTY LINK_LIBRARIES sonic_internal_diagnostics ${render_hierarchy_libraries})
    endif()
    target_include_directories(sonic-render-hierarchy-tests PRIVATE "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
    target_compile_options(sonic-render-hierarchy-tests PRIVATE /EHsc /utf-8 /fp:strict)
    target_link_libraries(sonic-render-hierarchy-tests PRIVATE "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
        "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib" KatanaRecomp::native_port_runtime)
endif()
