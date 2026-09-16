option(SARECOMP_LINUX_PROCEDURE_REGISTERS "Share registers across a reviewed private AOT closure" OFF)
if(SARECOMP_LINUX_PROCEDURE_REGISTERS)
    if(SARECOMP_LINUX_CODE_ADDRESS_PAGES OR SARECOMP_LINUX_PRELOADED_READS OR
       SARECOMP_LINUX_READ_GROUPS OR SARECOMP_LINUX_SCALAR_WRITES OR
       SARECOMP_LINUX_STACK_FRAMES OR SARECOMP_LINUX_RAM_GUARD_PROBE OR
       SARECOMP_LINUX_AOT_STATISTICS OR SARECOMP_LINUX_FPU_REGIONS OR
       SARECOMP_LINUX_HARDWARE_FPU OR SARECOMP_LINUX_WRITE_OBSERVER_GUARD OR
       SARECOMP_LINUX_CONSTINIT_DISPATCH OR SARECOMP_LINUX_FPU_REGISTER_CACHE OR
       NOT SARECOMP_LINUX_PGO STREQUAL "OFF")
        message(FATAL_ERROR "Private procedures support original or qualified RAM-region inputs")
    endif()
    set(procedure_dir "${CMAKE_BINARY_DIR}/generated/private-procedures")
    set(procedure_units
        unit-v8C056ED4-8C0585E0-d3674ae50a86c851.cpp
        unit-v8C0400A0-8C04124E-c3a8c709f8ba2806.cpp
        unit-v8C0412C8-8C0425A0-1c2be1678b040d69.cpp
        unit-v8C054D6C-8C055E6E-98a3248797026115.cpp)
    get_target_property(procedure_sources sonic_linux_guest SOURCES)
    set(procedure_inputs)
    set(procedure_outputs "${procedure_dir}/preparation.json")
    foreach(unit IN LISTS procedure_units)
        set(matches)
        foreach(source IN LISTS procedure_sources)
            get_filename_component(name "${source}" NAME)
            if(name STREQUAL unit)
                list(APPEND matches "${source}")
            endif()
        endforeach()
        list(LENGTH matches count)
        if(NOT count EQUAL 1)
            message(FATAL_ERROR "Private procedure member not unique: ${unit}")
        endif()
        list(GET matches 0 previous)
        list(FIND procedure_sources "${previous}" index)
        list(REMOVE_AT procedure_sources ${index})
        list(INSERT procedure_sources ${index} "${procedure_dir}/${unit}")
        list(APPEND procedure_inputs "${previous}")
        list(APPEND procedure_outputs "${procedure_dir}/${unit}")
        set_source_files_properties("${procedure_dir}/${unit}" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
    endforeach()
    set_property(TARGET sonic_linux_guest PROPERTY SOURCES ${procedure_sources})
    string(JOIN "\n" input_list ${procedure_inputs})
    file(GENERATE OUTPUT "${procedure_dir}/units.txt" CONTENT "${input_list}\n")
    add_custom_command(OUTPUT ${procedure_outputs}
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-procedure-registers.py"
            --source-root "${SONIC_WORKING}/generated" --units-file "${procedure_dir}/units.txt"
            --destination "${procedure_dir}"
        DEPENDS "${SONIC_ROOT}/tools/prepare-procedure-registers.py"
            "${SONIC_ROOT}/src/sonic_procedure_registers.hpp"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts"
            "${procedure_dir}/units.txt" ${procedure_inputs} VERBATIM)
endif()
