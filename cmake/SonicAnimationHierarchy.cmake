set(animation_dir "${CMAKE_BINARY_DIR}/generated/animation-hierarchy")
set(animation_identity "${animation_dir}/animation-identities.inc")
set(pose_identity "${animation_dir}/pose-blend-identities.inc")
set(animation_outputs "${animation_identity}" "${pose_identity}")
set(animation_reference_arguments)
set(animation_reference_dependencies)
if(TARGET sonic_linux_title)
    set(animation_reference "${animation_dir}/animation-reference-interpreter.cpp")
    list(APPEND animation_outputs "${animation_reference}")
    list(APPEND animation_reference_arguments --interpreter "${SONIC_LINUX_SDK}/src/runtime/dynamic_interpreter.cpp")
    list(APPEND animation_reference_dependencies "${SONIC_LINUX_SDK}/src/runtime/dynamic_interpreter.cpp")
else()
    set(animation_reference "${SONIC_MOTION_REFERENCE}")
endif()
add_custom_command(OUTPUT ${animation_outputs}
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-animation-hierarchy.py"
        --ram "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
        --output "${animation_dir}" ${animation_reference_arguments}
    DEPENDS "${SONIC_ROOT}/tools/prepare-animation-hierarchy.py" "${SONIC_ROOT}/tools/prepare_collision_candidates.py"
        "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin" ${animation_reference_dependencies}
    VERBATIM)
if(TARGET sonic_linux_title)
    set(animation_title sonic_linux_title)
    add_executable(sonic-animation-hierarchy-tests EXCLUDE_FROM_ALL
        "${SONIC_ROOT}/tools/test_animation_hierarchy.cpp" "${SONIC_ROOT}/src/sonic_animation_hierarchy.cpp"
        "${animation_identity}" "${animation_reference}"
        "${SONIC_LINUX_SDK}/src/decoder/decoder.cpp" "${SONIC_LINUX_SDK}/src/decoder/instruction_metadata.cpp")
    target_compile_options(sonic-animation-hierarchy-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
    target_link_options(sonic-animation-hierarchy-tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(sonic-animation-hierarchy-tests PRIVATE sonic_linux_services)
else()
    set(animation_title katana_native_title_adapter)
    add_executable(sonic-animation-hierarchy-tests EXCLUDE_FROM_ALL
        "${SONIC_ROOT}/tools/test_animation_hierarchy.cpp" "${SONIC_ROOT}/src/sonic_animation_hierarchy.cpp"
        "${animation_identity}" "${animation_reference}")
    target_include_directories(sonic-animation-hierarchy-tests PRIVATE "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
    target_compile_options(sonic-animation-hierarchy-tests PRIVATE /EHsc /utf-8 /fp:strict)
    target_link_libraries(sonic-animation-hierarchy-tests PRIVATE
        "${SONIC_ROOT}/.local/toolchain/katana_core.lib" "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
        KatanaRecomp::native_port_runtime)
endif()
target_sources(${animation_title} PRIVATE "${SONIC_ROOT}/src/sonic_animation_hierarchy.cpp" "${animation_identity}")
target_sources(${animation_title} PRIVATE "${SONIC_ROOT}/src/sonic_model_pipeline.cpp"
    "${SONIC_ROOT}/src/sonic_object_activation.cpp")
target_sources(${animation_title} PRIVATE "${SONIC_ROOT}/src/sonic_pose_blend.cpp" "${pose_identity}")
target_sources(sonic-animation-hierarchy-tests PRIVATE "${SONIC_ROOT}/src/sonic_pose_blend.cpp" "${pose_identity}")
target_include_directories(${animation_title} PRIVATE "${animation_dir}")
target_include_directories(sonic-animation-hierarchy-tests PRIVATE "${SONIC_ROOT}/src" "${animation_dir}")
include("${SONIC_ROOT}/cmake/SonicClosedMemory.cmake")
include("${SONIC_ROOT}/cmake/SonicPaletteBatch.cmake")
include("${SONIC_ROOT}/cmake/SonicProjectionBatch.cmake")
include("${SONIC_ROOT}/cmake/SonicRenderContext.cmake")
include("${SONIC_ROOT}/cmake/SonicCollisionMemory.cmake")
include("${SONIC_ROOT}/cmake/SonicModelPipeline.cmake")
include("${SONIC_ROOT}/cmake/SonicObjectActivationTests.cmake")
include("${SONIC_ROOT}/cmake/SonicMovement.cmake")
include("${SONIC_ROOT}/cmake/SonicCollisionWorld.cmake")
include("${SONIC_ROOT}/cmake/SonicRenderHierarchy.cmake")
