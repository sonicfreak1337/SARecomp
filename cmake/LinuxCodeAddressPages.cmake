option(SARECOMP_LINUX_CODE_ADDRESS_PAGES "Direct page proofs for generated code-address translation" OFF)
set(code_pages_dir "${CMAKE_BINARY_DIR}/generated/code-address-pages")
set(code_pages_outputs "${code_pages_dir}/block_abi.cpp" "${code_pages_dir}/preparation.json")
set(code_pages_inputs)
set(code_pages_args)
if(SARECOMP_LINUX_CODE_ADDRESS_PAGES)
    if(SARECOMP_LINUX_PRELOADED_READS OR SARECOMP_LINUX_READ_GROUPS OR
       SARECOMP_LINUX_SCALAR_WRITES OR SARECOMP_LINUX_STACK_FRAMES OR
       SARECOMP_LINUX_RAM_GUARD_PROBE OR SARECOMP_LINUX_AOT_STATISTICS OR
       SARECOMP_LINUX_FPU_REGIONS OR SARECOMP_LINUX_HARDWARE_FPU OR
       SARECOMP_LINUX_WRITE_OBSERVER_GUARD OR SARECOMP_LINUX_CONSTINIT_DISPATCH OR
       SARECOMP_LINUX_FPU_REGISTER_CACHE OR NOT SARECOMP_LINUX_PGO STREQUAL "OFF")
        message(FATAL_ERROR "Code-address preparation supports original or qualified RAM-region inputs")
    endif()
    # Consume the effective sources after other explicitly selected experiments;
    # only the lookup expression changes, not their instruction semantics.
    include("${SONIC_ROOT}/cmake/LinuxPreloadedReadUnits.cmake")
    get_target_property(code_pages_sources sonic_linux_guest SOURCES)
    foreach(unit IN LISTS linux_preloaded_units)
        set(matching_sources)
        foreach(source IN LISTS code_pages_sources)
            get_filename_component(name "${source}" NAME)
            if(name STREQUAL unit)
                list(APPEND matching_sources "${source}")
            endif()
        endforeach()
        list(LENGTH matching_sources matches)
        if(NOT matches EQUAL 1)
            message(FATAL_ERROR "Code-address member is not unique: ${unit}")
        endif()
        list(GET matching_sources 0 previous)
        list(FIND code_pages_sources "${previous}" index)
        list(REMOVE_AT code_pages_sources ${index})
        list(INSERT code_pages_sources ${index} "${code_pages_dir}/${unit}")
        list(APPEND code_pages_inputs "${previous}")
        list(APPEND code_pages_outputs "${code_pages_dir}/${unit}")
        set_source_files_properties("${code_pages_dir}/${unit}" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
    endforeach()
    set_property(TARGET sonic_linux_guest PROPERTY SOURCES ${code_pages_sources})
    string(JOIN "\n" unit_list ${code_pages_inputs})
    file(GENERATE OUTPUT "${code_pages_dir}/units.txt" CONTENT "${unit_list}\n")
    list(APPEND code_pages_args --units-file "${code_pages_dir}/units.txt")
    list(APPEND code_pages_inputs "${code_pages_dir}/units.txt")
    get_target_property(runtime_sources sonic_linux_aot_runtime SOURCES)
    list(FIND runtime_sources "${SONIC_LINUX_SDK}/src/runtime/block_abi.cpp" index)
    if(index LESS 0)
        message(FATAL_ERROR "Code-address runtime source already replaced")
    endif()
    list(REMOVE_AT runtime_sources ${index})
    list(INSERT runtime_sources ${index} "${code_pages_dir}/block_abi.cpp")
    set_property(TARGET sonic_linux_aot_runtime PROPERTY SOURCES ${runtime_sources})
endif()
add_custom_command(OUTPUT ${code_pages_outputs}
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-code-address-pages.py"
        --runtime "${SONIC_LINUX_SDK}/src/runtime/block_abi.cpp"
        --source-root "${SONIC_WORKING}/generated"
        --destination "${code_pages_dir}" ${code_pages_args}
    DEPENDS "${SONIC_ROOT}/tools/prepare-code-address-pages.py"
        "${SONIC_LINUX_SDK}/src/runtime/block_abi.cpp"
        "${SONIC_WORKING}/generated/.katana-generated-artifacts" ${code_pages_inputs}
    VERBATIM)
set_source_files_properties("${code_pages_dir}/block_abi.cpp" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
add_executable(sonic-linux-code-address-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_code_address_pages.cpp" "${code_pages_dir}/block_abi.cpp")
target_include_directories(sonic-linux-code-address-tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic-linux-code-address-tests PRIVATE -O2 -g0 -ffunction-sections -fdata-sections)
target_link_options(sonic-linux-code-address-tests PRIVATE -Wl,--gc-sections)
target_link_libraries(sonic-linux-code-address-tests PRIVATE sonic_linux_sdk_headers Threads::Threads)
