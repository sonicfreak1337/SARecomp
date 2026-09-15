option(SARECOMP_LINUX_AOT_STATISTICS "Experimental handling of selected AOT statistics" OFF)
set(SARECOMP_LINUX_AOT_STATISTICS_MODE "RUNTIME" CACHE STRING "AOT statistics: RUNTIME or OMIT")
set_property(CACHE SARECOMP_LINUX_AOT_STATISTICS_MODE PROPERTY STRINGS RUNTIME OMIT)
if(NOT SARECOMP_LINUX_AOT_STATISTICS_MODE MATCHES "^(RUNTIME|OMIT)$")
    message(FATAL_ERROR "Unknown AOT statistics mode")
endif()
if(SARECOMP_LINUX_AOT_STATISTICS AND (SARECOMP_LINUX_READ_GROUPS OR SARECOMP_LINUX_PRELOADED_READS))
    message(FATAL_ERROR "AOT statistics needs a separate comparison")
endif()
set(aot_statistics_dir "${CMAKE_BINARY_DIR}/generated/aot-statistics")
set(aot_statistics_definitions)
if(SARECOMP_LINUX_AOT_STATISTICS_MODE STREQUAL "OMIT")
    string(APPEND aot_statistics_dir "-omit")
    list(APPEND aot_statistics_definitions SARECOMP_AOT_STATISTICS_OMIT=1)
endif()
string(TOLOWER "${SARECOMP_LINUX_AOT_STATISTICS_MODE}" aot_statistics_mode)
set(aot_statistics_sources)
set(aot_statistics_inputs)
set(aot_statistics_arguments)
if(SARECOMP_LINUX_AOT_STATISTICS)
    include("${SONIC_ROOT}/cmake/LinuxPreloadedReadUnits.cmake")
    get_target_property(guest_sources sonic_linux_guest SOURCES)
    foreach(unit IN LISTS linux_preloaded_units)
        list(APPEND aot_statistics_sources "${aot_statistics_dir}/${unit}")
        list(APPEND aot_statistics_inputs "${SONIC_WORKING}/generated/code/${unit}")
        list(APPEND aot_statistics_arguments --unit "${unit}")
        list(FIND guest_sources "${SONIC_WORKING}/generated/code/${unit}" unit_index)
        if(unit_index LESS 0)
            message(FATAL_ERROR "Statistics member missing: ${unit}")
        endif()
        list(REMOVE_AT guest_sources ${unit_index})
        list(INSERT guest_sources ${unit_index} "${aot_statistics_dir}/${unit}")
        set_source_files_properties("${aot_statistics_dir}/${unit}" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
        set_source_files_properties("${aot_statistics_dir}/${unit}" PROPERTIES COMPILE_DEFINITIONS "${aot_statistics_definitions}")
    endforeach()
    set_property(TARGET sonic_linux_guest PROPERTY SOURCES ${guest_sources})
endif()
add_custom_command(OUTPUT ${aot_statistics_sources} "${aot_statistics_dir}/sonic_aot_statistics.hpp"
        "${aot_statistics_dir}/preparation.json"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-aot-statistics.py"
        --source-root "${SONIC_WORKING}/generated" --destination "${aot_statistics_dir}"
        --runtime-header "${SONIC_BASE}/sdk/include/katana/runtime/runtime.hpp"
        --mode "${aot_statistics_mode}"
        ${aot_statistics_arguments}
    DEPENDS "${SONIC_ROOT}/tools/prepare-aot-statistics.py"
        "${SONIC_BASE}/sdk/include/katana/runtime/runtime.hpp"
        "${SONIC_WORKING}/generated/.katana-generated-artifacts" ${aot_statistics_inputs} VERBATIM)
add_executable(sonic-linux-aot-statistics-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_aot_statistics.cpp" "${aot_statistics_dir}/sonic_aot_statistics.hpp")
target_include_directories(sonic-linux-aot-statistics-tests PRIVATE "${SONIC_ROOT}/src" "${aot_statistics_dir}")
target_compile_options(sonic-linux-aot-statistics-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
target_compile_definitions(sonic-linux-aot-statistics-tests PRIVATE ${aot_statistics_definitions})
target_link_libraries(sonic-linux-aot-statistics-tests PRIVATE sonic_linux_aot_runtime)
