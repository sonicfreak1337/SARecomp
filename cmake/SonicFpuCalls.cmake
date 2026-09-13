# Private one-unit comparison. OFF leaves the frozen AOT archive in control.
set(SARECOMP_FPU_CALL_EXPERIMENT "OFF" CACHE STRING "FPU comparison: OFF, CONTROL, DIRECT, INVERSE")
set_property(CACHE SARECOMP_FPU_CALL_EXPERIMENT PROPERTY STRINGS OFF CONTROL DIRECT INVERSE)
if(NOT SARECOMP_FPU_CALL_EXPERIMENT MATCHES "^(OFF|CONTROL|DIRECT|INVERSE)$")
    message(FATAL_ERROR "Unknown SARECOMP_FPU_CALL_EXPERIMENT")
endif()

# The component target is excluded from normal builds. Only the explicit
# INVERSE experiment links its source-bound arithmetic into the selected unit.
set(sonic_inverse_arithmetic_dir "${CMAKE_BINARY_DIR}/generated/inverse-arithmetic")
add_custom_command(OUTPUT
        "${sonic_inverse_arithmetic_dir}/sonic_inverse_arithmetic.hpp"
        "${sonic_inverse_arithmetic_dir}/provenance.json"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-inverse-arithmetic.py"
        --sdk "${SONIC_BASE}/katana-source-178448be.zip"
        --output-dir "${sonic_inverse_arithmetic_dir}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-inverse-arithmetic.py"
        "${SONIC_BASE}/katana-source-178448be.zip"
        "${SONIC_BASE}/product/generated/code/unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp"
    VERBATIM)
add_executable(sonic_inverse_arithmetic_tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_inverse_arithmetic.cpp"
    "${sonic_inverse_arithmetic_dir}/sonic_inverse_arithmetic.hpp")
target_include_directories(sonic_inverse_arithmetic_tests PRIVATE "${sonic_inverse_arithmetic_dir}")
target_compile_options(sonic_inverse_arithmetic_tests PRIVATE /EHsc /utf-8 /fp:strict /bigobj)
target_link_libraries(sonic_inverse_arithmetic_tests PRIVATE KatanaRecomp::native_port_runtime)
set(sonic_fpu_call_audit_argument)
if(NOT SARECOMP_FPU_CALL_EXPERIMENT STREQUAL "OFF")
    if(NOT SARECOMP_RAM_READ_EXPERIMENT STREQUAL "OFF")
        message(FATAL_ERROR "Keep CPU experiments separate for attribution")
    endif()
    string(TOLOWER "${SARECOMP_FPU_CALL_EXPERIMENT}" fpu_call_mode)
    set(fpu_call_dir "${CMAKE_BINARY_DIR}/generated/fpu-calls-${fpu_call_mode}")
    set(fpu_call_unit unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp)
    set(fpu_call_extra_arguments)
    set(fpu_call_extra_dependencies)
    if(SARECOMP_FPU_CALL_EXPERIMENT STREQUAL "INVERSE")
        set(fpu_call_extra_arguments --inverse-dir "${sonic_inverse_arithmetic_dir}")
        set(fpu_call_extra_dependencies "${sonic_inverse_arithmetic_dir}/sonic_inverse_arithmetic.hpp"
            "${sonic_inverse_arithmetic_dir}/provenance.json")
    endif()
    add_custom_command(OUTPUT "${fpu_call_dir}/${fpu_call_unit}" "${fpu_call_dir}/preparation.json"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-fpu-calls.py" prepare
            --source-root "${SONIC_WORKING}/generated" --destination "${fpu_call_dir}"
            --sdk "${SONIC_BASE}/katana-source-178448be.zip" --mode "${fpu_call_mode}" ${fpu_call_extra_arguments}
        DEPENDS "${SONIC_ROOT}/tools/prepare-fpu-calls.py"
            "${SONIC_WORKING}/generated/code/${fpu_call_unit}"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts"
            "${SONIC_BASE}/katana-source-178448be.zip" ${fpu_call_extra_dependencies} VERBATIM)
    add_library(sonic_fpu_calls STATIC "${fpu_call_dir}/${fpu_call_unit}")
    set_target_properties(sonic_fpu_calls PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fpu-calls-${fpu_call_mode}")
    target_include_directories(sonic_fpu_calls PRIVATE "${SONIC_WORKING}/generated/include" "${sonic_inverse_arithmetic_dir}")
    target_compile_options(sonic_fpu_calls PRIVATE /EHsc /utf-8 /fp:strict /bigobj /clang:-fno-lto)
    target_link_libraries(sonic_fpu_calls PRIVATE KatanaRecomp::native_port_runtime)
    target_link_libraries(game PRIVATE sonic_fpu_calls)
    set(sonic_fpu_call_audit_argument --fpu-call-experiment)
    add_custom_command(TARGET game POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-fpu-calls.py" audit
            --map "${CMAKE_BINARY_DIR}/game-native-port.map" --report "${fpu_call_dir}/preparation.json" VERBATIM)
endif()
