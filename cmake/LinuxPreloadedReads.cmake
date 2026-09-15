# Isolated whole-instruction read experiment. The accepted AOT archive and
# existing installers remain unchanged; off restores every original member.
option(SARECOMP_LINUX_PRELOADED_READS "Fuse admitted ordinary AOT RAM reads" OFF)
option(SARECOMP_LINUX_PRELOADED_FPU_READS "Also fuse admitted scalar FMOV reads" OFF)
if(SARECOMP_LINUX_PRELOADED_FPU_READS AND NOT SARECOMP_LINUX_PRELOADED_READS)
    message(FATAL_ERROR "Scalar FMOV preloading requires SARECOMP_LINUX_PRELOADED_READS")
endif()
include("${SONIC_ROOT}/cmake/LinuxPreloadedReadUnits.cmake")
if(SARECOMP_LINUX_PRELOADED_READS)
    set(preloaded_dir "${CMAKE_BINARY_DIR}/generated/preloaded-reads")
    set(preloaded_sources)
    set(preloaded_inputs)
    set(preloaded_arguments)
    if(SARECOMP_LINUX_PRELOADED_FPU_READS)
        list(APPEND preloaded_arguments --scalar-fpu)
    endif()
    get_target_property(guest_sources sonic_linux_guest SOURCES)
    foreach(unit IN LISTS linux_preloaded_units)
        list(APPEND preloaded_sources "${preloaded_dir}/${unit}")
        list(APPEND preloaded_inputs "${SONIC_WORKING}/generated/code/${unit}")
        list(APPEND preloaded_arguments --unit "${unit}")
        list(FIND guest_sources "${SONIC_WORKING}/generated/code/${unit}" unit_index)
        if(unit_index LESS 0)
            message(FATAL_ERROR "Selected preloaded AOT member is missing: ${unit}")
        endif()
        list(REMOVE_AT guest_sources ${unit_index})
        list(INSERT guest_sources ${unit_index} "${preloaded_dir}/${unit}")
        set_source_files_properties("${preloaded_dir}/${unit}" PROPERTIES
            INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
    endforeach()
    add_custom_command(OUTPUT ${preloaded_sources} "${preloaded_dir}/preparation.json"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-ram-read-aot.py" prepare
            --source-root "${SONIC_WORKING}/generated" --destination "${preloaded_dir}"
            --helper "${SONIC_ROOT}/src/sonic_prepared_read.hpp" --mode preloaded ${preloaded_arguments}
        DEPENDS "${SONIC_ROOT}/tools/prepare-ram-read-aot.py" "${SONIC_ROOT}/src/sonic_prepared_read.hpp"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts" ${preloaded_inputs} VERBATIM)
    set_property(TARGET sonic_linux_guest PROPERTY SOURCES ${guest_sources})
endif()
add_executable(sonic-linux-preloaded-read-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_prepared_read.cpp")
target_include_directories(sonic-linux-preloaded-read-tests PRIVATE "${SONIC_ROOT}/src")
target_compile_definitions(sonic-linux-preloaded-read-tests PRIVATE SONIC_TEST_PRELOADED_READ=1)
target_compile_options(sonic-linux-preloaded-read-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
target_link_libraries(sonic-linux-preloaded-read-tests PRIVATE sonic_linux_aot_runtime)
