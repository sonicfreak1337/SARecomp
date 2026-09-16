option(SARECOMP_LINUX_SCALAR_WRITES "Fuse native scalar RAM write proofs and stores" OFF)
set(SARECOMP_LINUX_SCALAR_WRITE_SCOPE "PROFILE" CACHE STRING "Scalar write replacement scope: PROFILE or ALL")
set_property(CACHE SARECOMP_LINUX_SCALAR_WRITE_SCOPE PROPERTY STRINGS PROFILE ALL)
if(SARECOMP_LINUX_SCALAR_WRITES)
    if(SARECOMP_LINUX_PRELOADED_READS OR SARECOMP_LINUX_READ_GROUPS OR
       SARECOMP_LINUX_AOT_STATISTICS OR SARECOMP_LINUX_FPU_REGIONS OR
       SARECOMP_LINUX_HARDWARE_FPU OR SARECOMP_LINUX_WRITE_OBSERVER_GUARD OR
       SARECOMP_LINUX_CONSTINIT_DISPATCH OR NOT SARECOMP_LINUX_PGO STREQUAL "OFF")
        message(FATAL_ERROR "Scalar writes require unchanged guest sources")
    endif()
    set(scalar_dir "${CMAKE_BINARY_DIR}/generated/scalar-writes")
    get_target_property(scalar_guest_sources sonic_linux_guest SOURCES)
    if(SARECOMP_LINUX_SCALAR_WRITE_SCOPE STREQUAL "PROFILE")
        include("${SONIC_ROOT}/cmake/LinuxPreloadedReadUnits.cmake")
        set(scalar_units ${linux_preloaded_units})
    elseif(SARECOMP_LINUX_SCALAR_WRITE_SCOPE STREQUAL "ALL")
        set(scalar_units)
        foreach(source IN LISTS scalar_guest_sources)
            get_filename_component(source_dir "${source}" DIRECTORY)
            get_filename_component(unit "${source}" NAME)
            if(source_dir STREQUAL "${SONIC_WORKING}/generated/code" AND unit MATCHES "^unit-v.*\\.cpp$")
                list(APPEND scalar_units "${unit}")
            endif()
        endforeach()
    else()
        message(FATAL_ERROR "Unknown scalar write scope")
    endif()
    set(scalar_outputs "${scalar_dir}/memory.cpp" "${scalar_dir}/native_port_runtime.cpp")
    set(scalar_inputs)
    foreach(unit IN LISTS scalar_units)
        set(source "${SONIC_WORKING}/generated/code/${unit}")
        list(FIND scalar_guest_sources "${source}" index)
        if(index LESS 0)
            message(FATAL_ERROR "Retained scalar write member missing: ${unit}")
        endif()
        list(REMOVE_AT scalar_guest_sources ${index})
        list(INSERT scalar_guest_sources ${index} "${scalar_dir}/${unit}")
        list(APPEND scalar_outputs "${scalar_dir}/${unit}")
        list(APPEND scalar_inputs "${source}")
        set_source_files_properties("${scalar_dir}/${unit}" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
    endforeach()
    set_property(TARGET sonic_linux_guest PROPERTY SOURCES ${scalar_guest_sources})
    string(JOIN "\n" scalar_unit_list ${scalar_units})
    file(GENERATE OUTPUT "${scalar_dir}/units.txt" CONTENT "${scalar_unit_list}\n")
    add_custom_command(OUTPUT ${scalar_outputs} "${scalar_dir}/preparation.json"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-scalar-writes.py"
            --memory-source "${SONIC_LINUX_SDK}/src/runtime/memory.cpp"
            --runtime-source "${CMAKE_BINARY_DIR}/generated/internal-diagnostics/native_port_runtime.cpp"
            --source-root "${SONIC_WORKING}/generated"
            --units-file "${scalar_dir}/units.txt" --destination "${scalar_dir}"
        DEPENDS "${SONIC_ROOT}/tools/prepare-scalar-writes.py" "${SONIC_ROOT}/src/sonic_scalar_write_view.hpp"
            "${SONIC_LINUX_SDK}/src/runtime/memory.cpp"
            "${CMAKE_BINARY_DIR}/generated/internal-diagnostics/native_port_runtime.cpp"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts"
            "${scalar_dir}/units.txt" ${scalar_inputs}
        VERBATIM)
    foreach(pair IN ITEMS "sonic_linux_aot_runtime|memory|${SONIC_LINUX_SDK}/src/runtime"
                          "sonic_linux_services|native_port_runtime|${CMAKE_BINARY_DIR}/generated/internal-diagnostics")
        string(REPLACE "|" ";" parts "${pair}")
        list(GET parts 0 owner)
        list(GET parts 1 stem)
        list(GET parts 2 previous_dir)
        get_target_property(sources ${owner} SOURCES)
        list(FIND sources "${previous_dir}/${stem}.cpp" index)
        if(index LESS 0)
            message(FATAL_ERROR "Scalar write runtime source missing: ${stem}")
        endif()
        list(REMOVE_AT sources ${index})
        list(INSERT sources ${index} "${scalar_dir}/${stem}.cpp")
        set_property(TARGET ${owner} PROPERTY SOURCES ${sources})
        set_source_files_properties("${scalar_dir}/${stem}.cpp" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
    endforeach()
    add_executable(sonic-linux-scalar-write-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_scalar_writes.cpp")
    target_include_directories(sonic-linux-scalar-write-tests PRIVATE "${SONIC_ROOT}/src" "${SONIC_ROOT}/tools")
    target_compile_options(sonic-linux-scalar-write-tests PRIVATE -O2 -g0)
    target_link_options(sonic-linux-scalar-write-tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(sonic-linux-scalar-write-tests PRIVATE sonic_linux_services)
endif()
