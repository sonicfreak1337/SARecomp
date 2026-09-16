option(SARECOMP_LINUX_FPU_REGISTER_CACHE "Retain integer registers across qualified nontrapping FPU calls" OFF)
add_executable(sonic-linux-fpu-register-cache-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_fpu_register_cache.cpp")
target_include_directories(sonic-linux-fpu-register-cache-tests PRIVATE "${SONIC_ROOT}/src" "${SONIC_ROOT}/tools")
target_compile_options(sonic-linux-fpu-register-cache-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
target_link_libraries(sonic-linux-fpu-register-cache-tests PRIVATE sonic_linux_aot_runtime)
if(SARECOMP_LINUX_FPU_REGISTER_CACHE)
    if(SARECOMP_LINUX_PRELOADED_READS OR SARECOMP_LINUX_READ_GROUPS OR
       SARECOMP_LINUX_AOT_STATISTICS OR SARECOMP_LINUX_FPU_REGIONS OR
       SARECOMP_LINUX_HARDWARE_FPU OR SARECOMP_LINUX_WRITE_OBSERVER_GUARD OR
       SARECOMP_LINUX_CONSTINIT_DISPATCH OR SARECOMP_LINUX_SCALAR_WRITES OR
       SARECOMP_LINUX_STACK_FRAMES OR NOT SARECOMP_LINUX_PGO STREQUAL "OFF")
        message(FATAL_ERROR "FPU register retention needs original AOT envelopes")
    endif()
    include("${SONIC_ROOT}/cmake/LinuxPreloadedReadUnits.cmake")
    set(fpu_cache_dir "${CMAKE_BINARY_DIR}/generated/fpu-register-cache")
    get_target_property(fpu_cache_sources sonic_linux_guest SOURCES)
    set(fpu_cache_outputs)
    set(fpu_cache_inputs)
    foreach(unit IN LISTS linux_preloaded_units)
        set(source "${SONIC_WORKING}/generated/code/${unit}")
        list(FIND fpu_cache_sources "${source}" index)
        if(index LESS 0)
            message(FATAL_ERROR "FPU register-cache original member missing: ${unit}")
        endif()
        list(REMOVE_AT fpu_cache_sources ${index})
        list(INSERT fpu_cache_sources ${index} "${fpu_cache_dir}/${unit}")
        list(APPEND fpu_cache_outputs "${fpu_cache_dir}/${unit}")
        list(APPEND fpu_cache_inputs "${source}")
        set_source_files_properties("${fpu_cache_dir}/${unit}" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
    endforeach()
    set_property(TARGET sonic_linux_guest PROPERTY SOURCES ${fpu_cache_sources})
    string(JOIN "\n" fpu_cache_units ${linux_preloaded_units})
    file(GENERATE OUTPUT "${fpu_cache_dir}/units.txt" CONTENT "${fpu_cache_units}\n")
    add_custom_command(OUTPUT ${fpu_cache_outputs} "${fpu_cache_dir}/preparation.json"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-fpu-register-cache.py"
            --source-root "${SONIC_WORKING}/generated" --destination "${fpu_cache_dir}"
            --runtime "${CMAKE_BINARY_DIR}/generated/fpu-runtime/fpu.cpp"
            --state-header "${SONIC_LINUX_SDK}/include/katana/runtime/native_aot_state.hpp"
            --units-file "${fpu_cache_dir}/units.txt"
        DEPENDS "${SONIC_ROOT}/tools/prepare-fpu-register-cache.py" "${SONIC_ROOT}/src/sonic_fpu_register_cache.hpp"
            "${CMAKE_BINARY_DIR}/generated/fpu-runtime/fpu.cpp"
            "${SONIC_LINUX_SDK}/include/katana/runtime/native_aot_state.hpp"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts"
            "${fpu_cache_dir}/units.txt" ${fpu_cache_inputs}
        VERBATIM)
endif()
