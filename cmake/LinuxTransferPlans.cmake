option(SARECOMP_LINUX_TRANSFER_PLANS "Reuse successful indirect transfer plans across unchanged executable epochs" OFF)
if(SARECOMP_LINUX_TRANSFER_PLANS)
    if(SARECOMP_LINUX_CONSTINIT_DISPATCH OR SARECOMP_LINUX_PRELOADED_READS OR
       SARECOMP_LINUX_READ_GROUPS OR SARECOMP_LINUX_AOT_STATISTICS OR
       SARECOMP_LINUX_FPU_REGIONS OR SARECOMP_LINUX_HARDWARE_FPU OR
       SARECOMP_LINUX_CODE_LAYOUT OR SARECOMP_LINUX_WRITE_OBSERVER_GUARD OR
       NOT SARECOMP_LINUX_PGO STREQUAL "OFF")
        message(FATAL_ERROR "Transfer plans need an isolated comparison")
    endif()
    set(transfer_input "${CMAKE_BINARY_DIR}/generated/motion-dispatch/native-port-dispatch.cpp")
    set(transfer_output "${CMAKE_BINARY_DIR}/generated/transfer-plans/native-port-dispatch.cpp")
    get_target_property(transfer_sources game SOURCES)
    list(FIND transfer_sources "${transfer_input}" transfer_index)
    if(transfer_index LESS 0)
        message(FATAL_ERROR "Effective dispatcher missing from game")
    endif()
    list(REMOVE_AT transfer_sources ${transfer_index})
    list(INSERT transfer_sources ${transfer_index} "${transfer_output}")
    set_property(TARGET game PROPERTY SOURCES ${transfer_sources})
    add_custom_command(OUTPUT "${transfer_output}" "${CMAKE_BINARY_DIR}/generated/transfer-plans/preparation.json"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-transfer-plans.py"
            --source "${transfer_input}" --destination "${transfer_output}"
        DEPENDS "${transfer_input}" "${SONIC_ROOT}/tools/prepare-transfer-plans.py"
            "${SONIC_ROOT}/src/sonic_prepared_transfers.hpp" VERBATIM)
endif()
add_executable(sonic-linux-transfer-plan-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_transfer_plans.cpp")
target_include_directories(sonic-linux-transfer-plan-tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic-linux-transfer-plan-tests PRIVATE -O2 -g0)
