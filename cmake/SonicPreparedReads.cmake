# Development-only comparison. OFF keeps every original selected AOT member.
# CONTROL recompiles byte-identical copies with the same flags as PREPARED.
set(SARECOMP_RAM_READ_EXPERIMENT "OFF" CACHE STRING "Bounded AOT read experiment: OFF, CONTROL or PREPARED")
set_property(CACHE SARECOMP_RAM_READ_EXPERIMENT PROPERTY STRINGS OFF CONTROL PREPARED)
if(NOT SARECOMP_RAM_READ_EXPERIMENT MATCHES "^(OFF|CONTROL|PREPARED)$")
    message(FATAL_ERROR "Unknown SARECOMP_RAM_READ_EXPERIMENT")
endif()
set(sonic_ram_read_audit_argument)
if(NOT SARECOMP_RAM_READ_EXPERIMENT STREQUAL "OFF")
    string(TOLOWER "${SARECOMP_RAM_READ_EXPERIMENT}" ram_read_mode)
    set(ram_read_dir "${CMAKE_BINARY_DIR}/generated/ram-read-${ram_read_mode}")
    set(ram_read_units
        unit-v8C029400-8C029B00-9bed0201322da5d9.cpp
        unit-v8C036BC0-8C037C3C-aa2f5ddfed3d4270.cpp)
    set(ram_read_sources)
    set(ram_read_inputs)
    foreach(unit IN LISTS ram_read_units)
        list(APPEND ram_read_sources "${ram_read_dir}/${unit}")
        list(APPEND ram_read_inputs "${SONIC_WORKING}/generated/code/${unit}")
    endforeach()
    add_custom_command(OUTPUT ${ram_read_sources} "${ram_read_dir}/preparation.json"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-ram-read-aot.py" prepare
            --source-root "${SONIC_WORKING}/generated" --destination "${ram_read_dir}"
            --helper "${SONIC_ROOT}/src/sonic_prepared_read.hpp" --mode "${ram_read_mode}"
        DEPENDS "${SONIC_ROOT}/tools/prepare-ram-read-aot.py" "${SONIC_ROOT}/src/sonic_prepared_read.hpp"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts" ${ram_read_inputs} VERBATIM)
    add_library(sonic_ram_reads STATIC ${ram_read_sources})
    set_target_properties(sonic_ram_reads PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/ram-read-${ram_read_mode}")
    target_include_directories(sonic_ram_reads PRIVATE "${SONIC_ROOT}/src" "${SONIC_WORKING}/generated/include")
    target_compile_options(sonic_ram_reads PRIVATE /EHsc /utf-8 /fp:strict /bigobj /clang:-fno-lto)
    target_link_libraries(sonic_ram_reads PRIVATE KatanaRecomp::native_port_runtime)
    # Resolve these complete selected units before the retained archive. A
    # separate map audit below proves the new owner and excludes the old member.
    target_link_libraries(game PRIVATE sonic_ram_reads)
    set(sonic_ram_read_audit_argument --ram-read-experiment)
    add_custom_command(TARGET game POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-ram-read-aot.py" audit
            --map "${CMAKE_BINARY_DIR}/game-native-port.map" --report "${ram_read_dir}/preparation.json" VERBATIM)
endif()
add_executable(sonic_prepared_read_tests EXCLUDE_FROM_ALL tools/test_prepared_read.cpp)
target_include_directories(sonic_prepared_read_tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_prepared_read_tests PRIVATE /EHsc /utf-8 /fp:strict)
target_link_libraries(sonic_prepared_read_tests PRIVATE KatanaRecomp::native_port_runtime)
