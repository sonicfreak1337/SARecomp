# ABI-identical, source-bound FPU runtime experiment. No retained AOT recompiles.
option(SARECOMP_FPU_RUNTIME_FAST "Use exact nontrapping FPU runtime fast path" OFF)
set(sonic_fpu_runtime_dir "${CMAKE_BINARY_DIR}/generated/fpu-runtime")
add_custom_command(OUTPUT
        "${sonic_fpu_runtime_dir}/fpu.cpp"
        "${sonic_fpu_runtime_dir}/fpu-reference.cpp"
        "${sonic_fpu_runtime_dir}/sonic_fpu_reference.hpp"
        "${sonic_fpu_runtime_dir}/sonic_fpu_runtime_probe.hpp"
        "${sonic_fpu_runtime_dir}/provenance.json"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-fpu-runtime.py"
        --sdk "${SONIC_BASE}/katana-source-178448be.zip"
        --output-dir "${sonic_fpu_runtime_dir}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-fpu-runtime.py"
        "${SONIC_BASE}/katana-source-178448be.zip" VERBATIM)

add_executable(sonic_fpu_runtime_tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_fpu_runtime.cpp"
    "${sonic_fpu_runtime_dir}/fpu.cpp"
    "${sonic_fpu_runtime_dir}/fpu-reference.cpp")
target_include_directories(sonic_fpu_runtime_tests PRIVATE "${sonic_fpu_runtime_dir}")
target_compile_options(sonic_fpu_runtime_tests PRIVATE /EHsc /utf-8 /fp:strict /clang:-fno-lto)
target_compile_definitions(sonic_fpu_runtime_tests PRIVATE SONIC_FPU_RUNTIME_TEST_PROBE=1)
target_link_libraries(sonic_fpu_runtime_tests PRIVATE KatanaRecomp::native_port_runtime)

set(sonic_fpu_body_dir "${CMAKE_BINARY_DIR}/generated/fpu-body")
add_custom_command(OUTPUT
        "${sonic_fpu_body_dir}/sonic_fpu_body.hpp"
        "${sonic_fpu_body_dir}/provenance.json"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-fpu-body.py"
        --sdk "${SONIC_BASE}/katana-source-178448be.zip"
        --output-dir "${sonic_fpu_body_dir}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-fpu-body.py"
        "${SONIC_BASE}/katana-source-178448be.zip" VERBATIM)
add_custom_target(sonic_fpu_body_prepare DEPENDS
    "${sonic_fpu_body_dir}/sonic_fpu_body.hpp" "${sonic_fpu_body_dir}/provenance.json")
add_library(sonic_fpu_body INTERFACE)
add_dependencies(sonic_fpu_body sonic_fpu_body_prepare)
target_include_directories(sonic_fpu_body INTERFACE "${sonic_fpu_body_dir}")
target_compile_features(sonic_fpu_body INTERFACE cxx_std_20)
target_link_libraries(katana_native_title_adapter PRIVATE sonic_fpu_body)
target_link_libraries(sonic_matrix_inverse_tests PRIVATE sonic_fpu_body)
target_link_libraries(sonic_atan_math_tests PRIVATE sonic_fpu_body)
add_executable(sonic_fpu_body_tests EXCLUDE_FROM_ALL
    tools/test_fpu_body.cpp "${sonic_fpu_runtime_dir}/fpu.cpp"
    "${sonic_fpu_runtime_dir}/fpu-reference.cpp")
target_include_directories(sonic_fpu_body_tests PRIVATE "${sonic_fpu_runtime_dir}")
target_compile_options(sonic_fpu_body_tests PRIVATE /EHsc /utf-8 /fp:strict /clang:-fno-lto)
target_link_libraries(sonic_fpu_body_tests PRIVATE sonic_fpu_body KatanaRecomp::native_port_runtime)

set(sonic_fpu_runtime_audit_argument)
if(SARECOMP_FPU_RUNTIME_FAST)
    add_library(sonic_fpu_runtime STATIC "${sonic_fpu_runtime_dir}/fpu.cpp")
    target_compile_options(sonic_fpu_runtime PRIVATE /EHsc /utf-8 /fp:strict /clang:-fno-lto)
    target_link_libraries(sonic_fpu_runtime PRIVATE KatanaRecomp::native_port_runtime)
    target_link_libraries(game PRIVATE sonic_fpu_runtime)
    set(sonic_fpu_runtime_audit_argument --fpu-runtime-fast)
    add_custom_command(TARGET game POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/audit-fpu-runtime.py"
            "${CMAKE_BINARY_DIR}/game-native-port.map" VERBATIM)
endif()
