# Private one-unit comparison. OFF leaves the frozen AOT archive in control.
set(SARECOMP_FPU_CALL_EXPERIMENT "OFF" CACHE STRING "FPU forwarding experiment: OFF, CONTROL, DIRECT")
set_property(CACHE SARECOMP_FPU_CALL_EXPERIMENT PROPERTY STRINGS OFF CONTROL DIRECT)
if(NOT SARECOMP_FPU_CALL_EXPERIMENT MATCHES "^(OFF|CONTROL|DIRECT)$")
    message(FATAL_ERROR "Unknown SARECOMP_FPU_CALL_EXPERIMENT")
endif()
set(sonic_fpu_call_audit_argument)
if(NOT SARECOMP_FPU_CALL_EXPERIMENT STREQUAL "OFF")
    if(NOT SARECOMP_RAM_READ_EXPERIMENT STREQUAL "OFF")
        message(FATAL_ERROR "Keep CPU experiments separate for attribution")
    endif()
    string(TOLOWER "${SARECOMP_FPU_CALL_EXPERIMENT}" fpu_call_mode)
    set(fpu_call_dir "${CMAKE_BINARY_DIR}/generated/fpu-calls-${fpu_call_mode}")
    set(fpu_call_unit unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp)
    add_custom_command(OUTPUT "${fpu_call_dir}/${fpu_call_unit}" "${fpu_call_dir}/preparation.json"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-fpu-calls.py" prepare
            --source-root "${SONIC_WORKING}/generated" --destination "${fpu_call_dir}"
            --sdk "${SONIC_BASE}/katana-source-178448be.zip" --mode "${fpu_call_mode}"
        DEPENDS "${SONIC_ROOT}/tools/prepare-fpu-calls.py"
            "${SONIC_WORKING}/generated/code/${fpu_call_unit}"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts"
            "${SONIC_BASE}/katana-source-178448be.zip" VERBATIM)
    add_library(sonic_fpu_calls STATIC "${fpu_call_dir}/${fpu_call_unit}")
    set_target_properties(sonic_fpu_calls PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fpu-calls-${fpu_call_mode}")
    target_include_directories(sonic_fpu_calls PRIVATE "${SONIC_WORKING}/generated/include")
    target_compile_options(sonic_fpu_calls PRIVATE /EHsc /utf-8 /fp:strict /bigobj /clang:-fno-lto)
    target_link_libraries(sonic_fpu_calls PRIVATE KatanaRecomp::native_port_runtime)
    target_link_libraries(game PRIVATE sonic_fpu_calls)
    set(sonic_fpu_call_audit_argument --fpu-call-experiment)
    add_custom_command(TARGET game POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-fpu-calls.py" audit
            --map "${CMAKE_BINARY_DIR}/game-native-port.map" --report "${fpu_call_dir}/preparation.json" VERBATIM)
endif()
