# Include after SonicPreparedReads/SonicFpuCalls and before katana_generated
# is linked. The inverse unit is deliberately absent from this experiment.
set(SARECOMP_COMPACT_AOT_EXPERIMENT "OFF" CACHE STRING "Compact AOT: OFF, CONTROL or COMPACT")
set_property(CACHE SARECOMP_COMPACT_AOT_EXPERIMENT PROPERTY STRINGS OFF CONTROL COMPACT)
if(NOT SARECOMP_COMPACT_AOT_EXPERIMENT MATCHES "^(OFF|CONTROL|COMPACT)$")
    message(FATAL_ERROR "Unknown SARECOMP_COMPACT_AOT_EXPERIMENT")
endif()
set(sonic_compact_aot_audit_argument)
if(NOT SARECOMP_COMPACT_AOT_EXPERIMENT STREQUAL "OFF")
    if(DEFINED SARECOMP_RAM_READ_EXPERIMENT AND NOT SARECOMP_RAM_READ_EXPERIMENT STREQUAL "OFF")
        message(FATAL_ERROR "Compact AOT overlaps the prepared RAM experiment's selected units")
    endif()
    if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Compact AOT requires the pinned Windows clang-cl toolchain")
    endif()
    string(TOLOWER "${SARECOMP_COMPACT_AOT_EXPERIMENT}" compact_aot_mode)
    set(compact_aot_dir "${CMAKE_BINARY_DIR}/generated/compact-aot-${compact_aot_mode}")
    set(compact_aot_target "sonic_compact_aot_${compact_aot_mode}")
    set(compact_aot_profile "${SONIC_ROOT}/runs/execution-inverse-isolated-01/execution-ip-resolved.json")
    set(compact_aot_units
        unit-v8C029400-8C029B00-9bed0201322da5d9.cpp
        unit-v8C10EC94-8C10FCEC-2baf008af52674a0.cpp
        unit-v8C033122-8C0342E0-7edcb8468a2b4534.cpp
        unit-v8C036BC0-8C037C3C-aa2f5ddfed3d4270.cpp
        unit-v8C056ED4-8C0585E0-d3674ae50a86c851.cpp
        unit-v8C09036C-8C09168E-46c0db15f975e3ef.cpp
        unit-v8C0273A2-8C028EC2-3c3ba866ec5c4265.cpp
        unit-v8C051E56-8C053338-ce4429c39e6969de.cpp)
    set(compact_aot_sources)
    set(compact_aot_inputs)
    foreach(unit IN LISTS compact_aot_units)
        list(APPEND compact_aot_sources "${compact_aot_dir}/${unit}")
        list(APPEND compact_aot_inputs "${SONIC_WORKING}/generated/code/${unit}")
    endforeach()
    add_custom_command(OUTPUT ${compact_aot_sources} "${compact_aot_dir}/preparation.json"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-compact-aot.py" prepare
            --source-root "${SONIC_WORKING}/generated" --destination "${compact_aot_dir}"
            --profile "${compact_aot_profile}" --mode "${compact_aot_mode}"
        DEPENDS "${SONIC_ROOT}/tools/prepare-compact-aot.py" "${compact_aot_profile}"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts" ${compact_aot_inputs}
        VERBATIM)
    # Separate target names retain both object caches across mode switches.
    # Only the selected mode is part of the active build graph.
    add_library(${compact_aot_target} STATIC ${compact_aot_sources})
    set_target_properties(${compact_aot_target} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/compact-aot-${compact_aot_mode}"
        INTERPROCEDURAL_OPTIMIZATION FALSE)
    target_include_directories(${compact_aot_target} PRIVATE "${SONIC_WORKING}/generated/include")
    if(SARECOMP_COMPACT_AOT_EXPERIMENT STREQUAL "COMPACT")
        target_compile_options(${compact_aot_target} PRIVATE /O1 /Ob1)
    else()
        # Explicit Release-style control; both modes use identical source bytes.
        target_compile_options(${compact_aot_target} PRIVATE /O2 /Ob2)
    endif()
    target_compile_options(${compact_aot_target} PRIVATE /EHsc /utf-8 /fp:strict /bigobj /clang:-fno-lto)
    target_link_libraries(${compact_aot_target} PRIVATE KatanaRecomp::native_port_runtime)
    target_link_libraries(game PRIVATE ${compact_aot_target})
    # Root's closure-audit integration admits this exact mode-specific archive,
    # together with the independently selected sonic_fpu_calls archive.
    set(sonic_compact_aot_audit_argument "--compact-aot-${compact_aot_mode}")
    add_custom_command(TARGET game POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-compact-aot.py" audit
            --map "${CMAKE_BINARY_DIR}/game-native-port.map"
            --report "${compact_aot_dir}/preparation.json" VERBATIM)
endif()
