# Bounded Windows counterpart of the qualified Linux source transformation.
# OFF preserves the retained product; BASE/EXTENDED are private comparisons.
set(SARECOMP_WINDOWS_RAM_REGIONS "OFF" CACHE STRING "Native RAM prefix comparison: OFF, BASE, EXTENDED")
set_property(CACHE SARECOMP_WINDOWS_RAM_REGIONS PROPERTY STRINGS OFF BASE EXTENDED)
if(NOT SARECOMP_WINDOWS_RAM_REGIONS MATCHES "^(OFF|BASE|EXTENDED)$")
    message(FATAL_ERROR "Unknown Windows RAM region mode")
endif()
set(sonic_ram_region_audit_argument)
if(NOT SARECOMP_WINDOWS_RAM_REGIONS STREQUAL "OFF")
    # This switch is normally declared by the later animation bridge include.
    option(SARECOMP_NATIVE_ANIMATION_BRIDGE "Compile the internal native animation comparison route" ON)
    if(NOT SARECOMP_NATIVE_MEMORY_CAPABILITY OR
       NOT SARECOMP_RAM_READ_EXPERIMENT STREQUAL "OFF" OR
       NOT SARECOMP_COMPACT_AOT_EXPERIMENT STREQUAL "OFF")
        message(FATAL_ERROR "Windows regions require the native memory capability and retained guest members")
    endif()
    include("${SONIC_ROOT}/cmake/LinuxPreloadedReadUnits.cmake")
    set(sonic_windows_ram_units ${linux_preloaded_units})
    if(DEFINED fpu_call_unit AND fpu_call_unit IN_LIST sonic_windows_ram_units)
        message(FATAL_ERROR "RAM regions overlap the selected arithmetic owner")
    endif()
    set(sonic_windows_ram_dir "${CMAKE_BINARY_DIR}/generated/region-writes")
    set(sonic_windows_ram_args)
    if(SARECOMP_WINDOWS_RAM_REGIONS STREQUAL "EXTENDED")
        set(sonic_windows_ram_dir "${CMAKE_BINARY_DIR}/generated/region-extended-writes")
        list(APPEND sonic_windows_ram_args --region-extended)
    endif()
    set(sonic_windows_ram_outputs "${sonic_windows_ram_dir}/memory.cpp"
        "${sonic_windows_ram_dir}/native_port_runtime.cpp" "${sonic_windows_ram_dir}/preparation.json")
    set(sonic_windows_ram_inputs)
    set(sonic_windows_ram_sources)
    set(sonic_windows_ram_bridges
        unit-v8C056ED4-8C0585E0-d3674ae50a86c851.cpp
        unit-v8C604642-8C606E54-2ff8d7f677b95797.cpp
        unit-v8C0412C8-8C0425A0-1c2be1678b040d69.cpp)
    foreach(unit IN LISTS sonic_windows_ram_units)
        list(APPEND sonic_windows_ram_outputs "${sonic_windows_ram_dir}/${unit}")
        list(APPEND sonic_windows_ram_inputs "${SONIC_WORKING}/generated/code/${unit}")
        # The shared animation bridge consumes these three prepared members.
        if(NOT SARECOMP_NATIVE_ANIMATION_BRIDGE OR NOT unit IN_LIST sonic_windows_ram_bridges)
            list(APPEND sonic_windows_ram_sources "${sonic_windows_ram_dir}/${unit}")
        endif()
    endforeach()
    string(JOIN "\n" sonic_windows_ram_list ${sonic_windows_ram_units})
    file(GENERATE OUTPUT "${sonic_windows_ram_dir}/units.txt" CONTENT "${sonic_windows_ram_list}\n")
    add_custom_command(OUTPUT ${sonic_windows_ram_outputs}
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-scalar-writes.py"
            --sdk-zip "${SONIC_BASE}/katana-source-178448be.zip"
            --runtime-source "${sonic_internal_diagnostics_dir}/native_port_runtime.cpp"
            --source-root "${SONIC_WORKING}/generated"
            --units-file "${sonic_windows_ram_dir}/units.txt"
            --destination "${sonic_windows_ram_dir}" --mode region ${sonic_windows_ram_args}
        DEPENDS "${SONIC_ROOT}/tools/prepare-scalar-writes.py"
            "${SONIC_ROOT}/tools/prepare-ram-regions.py" "${SONIC_ROOT}/tools/prepare-ram-read-aot.py"
            "${SONIC_BASE}/katana-source-178448be.zip"
            "${sonic_internal_diagnostics_dir}/native_port_runtime.cpp"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts"
            "${sonic_windows_ram_dir}/units.txt" ${sonic_windows_ram_inputs}
        VERBATIM)
    add_library(sonic_ram_regions STATIC ${sonic_windows_ram_sources})
    target_include_directories(sonic_ram_regions PRIVATE "${SONIC_ROOT}/src" "${SONIC_WORKING}/generated/include")
    target_compile_options(sonic_ram_regions PRIVATE /EHsc /utf-8 /fp:strict /bigobj /clang:-fno-lto)
    target_link_libraries(sonic_ram_regions PRIVATE KatanaRecomp::native_port_runtime)
    target_link_libraries(game PRIVATE sonic_ram_regions)
    target_compile_definitions(game PRIVATE SARECOMP_WINDOWS_RAM_REGIONS_DEFAULT=1)
    set(sonic_ram_region_audit_argument --ram-regions)
    add_custom_command(TARGET game POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/audit-ram-region-link.py"
            --source-root "${SONIC_WORKING}/generated"
            --preparation "${sonic_windows_ram_dir}/preparation.json"
            --map "${CMAKE_BINARY_DIR}/game-native-port.map"
            --report "${sonic_windows_ram_dir}/link-audit.json"
        VERBATIM)
    set(windows_region_fixture "${sonic_windows_ram_dir}/ram_region_aot_fixture.inc")
    set(windows_extended_fixture "${sonic_windows_ram_dir}/ram_region_extended_fixture.inc")
    set(windows_region_source "${SONIC_WORKING}/generated/code/unit-v8C0CBD40-8C0CCFDC-8bb83195ded15666.cpp")
    set(windows_mixed_source "${SONIC_WORKING}/generated/code/unit-v8C036BC0-8C037C3C-aa2f5ddfed3d4270.cpp")
    set(windows_vector_source "${SONIC_WORKING}/generated/code/unit-v8C033122-8C0342E0-7edcb8468a2b4534.cpp")
    set(windows_extended_source "${SONIC_WORKING}/generated/code/unit-v8C01995E-8C01AAE0-6cf3d8aa9df0e7a3.cpp")
    add_custom_command(OUTPUT "${windows_region_fixture}" "${windows_extended_fixture}"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-ram-region-test.py"
            --source "${windows_region_source}" --mixed-source "${windows_mixed_source}"
            --vector-source "${windows_vector_source}" --output "${windows_region_fixture}"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-ram-extended-test.py"
            --source "${windows_extended_source}" --output "${windows_extended_fixture}"
        DEPENDS "${SONIC_ROOT}/tools/prepare-ram-region-test.py" "${SONIC_ROOT}/tools/prepare-ram-extended-test.py"
            "${SONIC_ROOT}/tools/prepare-ram-regions.py" "${windows_region_source}"
            "${windows_mixed_source}" "${windows_vector_source}" "${windows_extended_source}"
        VERBATIM)
    add_executable(sonic-windows-ram-region-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_ram_regions.cpp"
        "${windows_region_fixture}" "${windows_extended_fixture}")
    target_include_directories(sonic-windows-ram-region-tests PRIVATE "${SONIC_ROOT}/src"
        "${SONIC_ROOT}/tools" "${sonic_windows_ram_dir}")
    target_compile_options(sonic-windows-ram-region-tests PRIVATE /O2 /EHsc /utf-8 /fp:strict /bigobj)
    target_link_libraries(sonic-windows-ram-region-tests PRIVATE sonic_internal_diagnostics
        "${SONIC_ROOT}/.local/toolchain/katana_core.lib" "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib"
        KatanaRecomp::native_port_runtime)
endif()
