add_executable(sonic-model-pipeline-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_model_pipeline.cpp" "${SONIC_ROOT}/src/sonic_model_pipeline.cpp"
    "${SONIC_ROOT}/src/sonic_palette_lighting.cpp" "${SONIC_ROOT}/src/sonic_render_context.cpp"
    "${render_context_identity}" "${animation_reference}")
target_include_directories(sonic-model-pipeline-tests PRIVATE "${SONIC_ROOT}/src" "${CMAKE_BINARY_DIR}/generated/render-context")
target_link_libraries(sonic-model-pipeline-tests PRIVATE sonic_palette_batch)
if(TARGET sonic_fpu_body)
    target_link_libraries(sonic-model-pipeline-tests PRIVATE sonic_fpu_body)
else()
    target_include_directories(sonic-model-pipeline-tests PRIVATE "${CMAKE_BINARY_DIR}/generated/fpu-body")
endif()
if(TARGET sonic_linux_title)
    target_sources(sonic-model-pipeline-tests PRIVATE
        "${SONIC_LINUX_SDK}/src/decoder/decoder.cpp" "${SONIC_LINUX_SDK}/src/decoder/instruction_metadata.cpp")
    target_compile_options(sonic-model-pipeline-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
    target_link_options(sonic-model-pipeline-tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(sonic-model-pipeline-tests PRIVATE sonic_linux_services)
else()
    if(SARECOMP_NATIVE_MEMORY_CAPABILITY)
        get_target_property(pipeline_libraries sonic-model-pipeline-tests LINK_LIBRARIES)
        set_property(TARGET sonic-model-pipeline-tests PROPERTY LINK_LIBRARIES sonic_internal_diagnostics ${pipeline_libraries})
    endif()
    target_include_directories(sonic-model-pipeline-tests PRIVATE "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
    target_compile_options(sonic-model-pipeline-tests PRIVATE /EHsc /utf-8 /fp:strict)
    target_link_libraries(sonic-model-pipeline-tests PRIVATE
        "${SONIC_ROOT}/.local/toolchain/katana_core.lib" "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
        KatanaRecomp::native_port_runtime)
endif()
