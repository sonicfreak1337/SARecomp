set(sonic_internal_diagnostics_dir "${CMAKE_BINARY_DIR}/generated/internal-diagnostics")
add_custom_command(OUTPUT
    "${sonic_internal_diagnostics_dir}/native_bringup_dispatch.cpp"
    "${sonic_internal_diagnostics_dir}/native_port_runtime.cpp"
    "${sonic_internal_diagnostics_dir}/provenance.json"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-internal-diagnostics.py"
        --sdk "${SONIC_BASE}/katana-source-178448be.zip"
        --output-dir "${sonic_internal_diagnostics_dir}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-internal-diagnostics.py"
        "${SONIC_BASE}/katana-source-178448be.zip" VERBATIM)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Replace each source at its original archive position. Prepending a new
    # runtime archive moves hot code and needlessly enlarges binary patches.
    foreach(pair IN ITEMS "sonic_linux_aot_runtime|native_bringup_dispatch" "sonic_linux_services|native_port_runtime")
        string(REPLACE "|" ";" parts "${pair}")
        list(GET parts 0 owner)
        list(GET parts 1 stem)
        get_target_property(sources ${owner} SOURCES)
        list(TRANSFORM sources REPLACE "${SONIC_LINUX_SDK}/src/runtime/${stem}\\.cpp$"
            "${sonic_internal_diagnostics_dir}/${stem}.cpp")
        set_property(TARGET ${owner} PROPERTY SOURCES ${sources})
        set_source_files_properties("${sonic_internal_diagnostics_dir}/${stem}.cpp" PROPERTIES
            INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
    endforeach()
else()
    add_library(sonic_internal_diagnostics STATIC
        "${sonic_internal_diagnostics_dir}/native_bringup_dispatch.cpp"
        "${sonic_internal_diagnostics_dir}/native_port_runtime.cpp")
    target_include_directories(sonic_internal_diagnostics PRIVATE "${SONIC_ROOT}/src")
    target_compile_options(sonic_internal_diagnostics PRIVATE /O2 /EHsc /utf-8)
    target_link_libraries(sonic_internal_diagnostics PRIVATE KatanaRecomp::native_port_runtime)
    target_link_libraries(game PRIVATE sonic_internal_diagnostics)
endif()
add_executable(sonic_internal_policy_tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_internal_diagnostics.cpp")
target_include_directories(sonic_internal_policy_tests PRIVATE "${SONIC_ROOT}/src")
