option(SARECOMP_LINUX_CONSTINIT_DISPATCH "Prove constant initialization of selected dispatch TLS declarations" OFF)
if(SARECOMP_LINUX_CONSTINIT_DISPATCH)
    if(SARECOMP_LINUX_PRELOADED_READS OR SARECOMP_LINUX_READ_GROUPS OR
       SARECOMP_LINUX_AOT_STATISTICS OR SARECOMP_LINUX_FPU_REGIONS OR
       SARECOMP_LINUX_HARDWARE_FPU OR SARECOMP_LINUX_CODE_LAYOUT OR
       SARECOMP_LINUX_WRITE_OBSERVER_GUARD OR NOT SARECOMP_LINUX_PGO STREQUAL "OFF")
        message(FATAL_ERROR "Dispatch constinit needs an isolated AOT comparison")
    endif()
    include("${SONIC_ROOT}/cmake/LinuxPreloadedReadUnits.cmake")
    set(constinit_dir "${CMAKE_BINARY_DIR}/generated/constinit-dispatch")
    set(constinit_sources)
    set(constinit_inputs)
    set(constinit_arguments)
    set(constinit_effective_dispatch "${CMAKE_BINARY_DIR}/generated/motion-dispatch/native-port-dispatch.cpp")
    get_target_property(guest_sources sonic_linux_guest SOURCES)
    foreach(unit IN LISTS linux_preloaded_units)
        list(APPEND constinit_sources "${constinit_dir}/${unit}")
        list(APPEND constinit_inputs "${SONIC_WORKING}/generated/code/${unit}")
        list(APPEND constinit_arguments --unit "${unit}")
        list(FIND guest_sources "${SONIC_WORKING}/generated/code/${unit}" index)
        if(index LESS 0)
            message(FATAL_ERROR "Constinit original member missing: ${unit}")
        endif()
        list(REMOVE_AT guest_sources ${index})
        list(INSERT guest_sources ${index} "${constinit_dir}/${unit}")
    endforeach()
    set_property(TARGET sonic_linux_guest PROPERTY SOURCES ${guest_sources})
    get_target_property(game_sources game SOURCES)
    list(FIND game_sources "${constinit_effective_dispatch}" dispatch_index)
    if(dispatch_index LESS 0)
        message(FATAL_ERROR "Constinit effective dispatch member missing")
    endif()
    list(REMOVE_AT game_sources ${dispatch_index})
    list(INSERT game_sources ${dispatch_index} "${constinit_dir}/native-port-dispatch.cpp")
    set_property(TARGET game PROPERTY SOURCES ${game_sources})
    add_custom_command(OUTPUT ${constinit_sources} "${constinit_dir}/preparation.json"
        "${constinit_dir}/definition-proof.cpp" "${constinit_dir}/native-port-dispatch.cpp"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-constinit-dispatch.py"
            --source-root "${SONIC_WORKING}/generated" --destination "${constinit_dir}"
            --runtime-dispatch "${constinit_effective_dispatch}" ${constinit_arguments}
        DEPENDS "${SONIC_ROOT}/tools/prepare-constinit-dispatch.py"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts"
            "${SONIC_WORKING}/generated/code/native-port-dispatch.cpp"
            "${constinit_effective_dispatch}" ${constinit_inputs}
        VERBATIM)
    # Compile the exact initializers against the effective Linux SDK. This
    # object is only a compile-time proof and is never linked into the product.
    add_library(sonic_linux_tls_definition_proof OBJECT EXCLUDE_FROM_ALL "${constinit_dir}/definition-proof.cpp")
    target_link_libraries(sonic_linux_tls_definition_proof PRIVATE sonic_linux_sdk_headers)
    add_dependencies(game sonic_linux_tls_definition_proof)
endif()
