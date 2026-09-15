option(SARECOMP_LINUX_HARDWARE_FPU "Experiment with exact scalar SSE arithmetic and retained fallbacks" OFF)
set(hardware_fpu_dir "${CMAKE_BINARY_DIR}/generated/hardware-fpu")
set(hardware_fpu_outputs)
foreach(name fpu.cpp fpu-reference.cpp sonic_fpu_reference.hpp sonic_fpu_runtime_probe.hpp provenance.json)
    list(APPEND hardware_fpu_outputs "${hardware_fpu_dir}/${name}")
endforeach()
add_custom_command(OUTPUT ${hardware_fpu_outputs}
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-hardware-fpu.py"
        --sdk "${SONIC_BASE}/katana-source-178448be.zip"
        --helper "${SONIC_ROOT}/src/sonic_hardware_fpu.inc" --output-dir "${hardware_fpu_dir}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-hardware-fpu.py" "${SONIC_ROOT}/tools/prepare-fpu-runtime.py"
        "${SONIC_ROOT}/src/sonic_hardware_fpu.inc" "${SONIC_BASE}/katana-source-178448be.zip" VERBATIM)
set_source_files_properties("${hardware_fpu_dir}/fpu.cpp" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
if(SARECOMP_LINUX_HARDWARE_FPU)
    get_target_property(hardware_fpu_runtime_sources sonic_linux_aot_runtime SOURCES)
    list(FIND hardware_fpu_runtime_sources "${CMAKE_BINARY_DIR}/generated/fpu-runtime/fpu.cpp" hardware_fpu_index)
    if(hardware_fpu_index LESS 0)
        message(FATAL_ERROR "No unique prepared FPU runtime to replace")
    endif()
    list(REMOVE_AT hardware_fpu_runtime_sources ${hardware_fpu_index})
    list(INSERT hardware_fpu_runtime_sources ${hardware_fpu_index} "${hardware_fpu_dir}/fpu.cpp")
    set_property(TARGET sonic_linux_aot_runtime PROPERTY SOURCES ${hardware_fpu_runtime_sources})
endif()
add_executable(sonic-linux-hardware-fpu-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_fpu_runtime.cpp" "${hardware_fpu_dir}/fpu.cpp" "${hardware_fpu_dir}/fpu-reference.cpp")
target_include_directories(sonic-linux-hardware-fpu-tests PRIVATE "${hardware_fpu_dir}" "${SONIC_ROOT}/src")
target_compile_definitions(sonic-linux-hardware-fpu-tests PRIVATE SONIC_FPU_RUNTIME_TEST_PROBE=1)
target_compile_options(sonic-linux-hardware-fpu-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
target_link_libraries(sonic-linux-hardware-fpu-tests PRIVATE sonic_linux_aot_runtime)
