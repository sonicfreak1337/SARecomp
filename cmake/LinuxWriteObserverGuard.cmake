option(SARECOMP_LINUX_WRITE_OBSERVER_GUARD "Cache observer permission in generation-checked AOT guards" OFF)
if(SARECOMP_LINUX_WRITE_OBSERVER_GUARD)
    if(SARECOMP_LINUX_PRELOADED_READS OR SARECOMP_LINUX_READ_GROUPS OR
       SARECOMP_LINUX_AOT_STATISTICS OR SARECOMP_LINUX_FPU_REGIONS OR
       SARECOMP_LINUX_HARDWARE_FPU OR SARECOMP_LINUX_CODE_LAYOUT OR
       NOT SARECOMP_LINUX_PGO STREQUAL "OFF")
        message(FATAL_ERROR "Observer guard needs an isolated AOT comparison")
    endif()
    include("${SONIC_ROOT}/cmake/LinuxPreloadedReadUnits.cmake")
    set(observer_guard_dir "${CMAKE_BINARY_DIR}/generated/write-observer-guard")
    set(observer_guard_sources)
    set(observer_guard_inputs)
    set(observer_guard_arguments)
    get_target_property(guest_sources sonic_linux_guest SOURCES)
    foreach(unit IN LISTS linux_preloaded_units)
        list(APPEND observer_guard_sources "${observer_guard_dir}/${unit}")
        list(APPEND observer_guard_inputs "${SONIC_WORKING}/generated/code/${unit}")
        list(APPEND observer_guard_arguments --unit "${unit}")
        list(FIND guest_sources "${SONIC_WORKING}/generated/code/${unit}" index)
        if(index LESS 0)
            message(FATAL_ERROR "Observer guard original member missing: ${unit}")
        endif()
        list(REMOVE_AT guest_sources ${index})
        list(INSERT guest_sources ${index} "${observer_guard_dir}/${unit}")
        set_source_files_properties("${observer_guard_dir}/${unit}" PROPERTIES
            INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
    endforeach()
    set_property(TARGET sonic_linux_guest PROPERTY SOURCES ${guest_sources})
    add_custom_command(OUTPUT ${observer_guard_sources} "${observer_guard_dir}/preparation.json"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-write-observer-guard.py"
            --source-root "${SONIC_WORKING}/generated" --destination "${observer_guard_dir}"
            --memory-header "${SONIC_LINUX_SDK}/include/katana/runtime/memory.hpp"
            --memory-source "${SONIC_LINUX_SDK}/src/runtime/memory.cpp"
            --helper "${SONIC_ROOT}/src/sonic_write_observer_guard.hpp"
            ${observer_guard_arguments}
        DEPENDS "${SONIC_ROOT}/tools/prepare-write-observer-guard.py"
            "${SONIC_ROOT}/src/sonic_write_observer_guard.hpp"
            "${SONIC_LINUX_SDK}/include/katana/runtime/memory.hpp"
            "${SONIC_LINUX_SDK}/src/runtime/memory.cpp"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts" ${observer_guard_inputs}
        VERBATIM)
endif()
add_executable(sonic-linux-write-observer-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_write_observer_guard.cpp")
target_include_directories(sonic-linux-write-observer-tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic-linux-write-observer-tests PRIVATE -O2 -g0)
target_link_libraries(sonic-linux-write-observer-tests PRIVATE sonic_linux_aot_runtime)
