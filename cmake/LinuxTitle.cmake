# Consume the already reviewed C++ input pack. End users never execute this.
execute_process(COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-linux-title-inputs.py"
    "${SONIC_ROOT}" "${CMAKE_BINARY_DIR}/generated"
    RESULT_VARIABLE title_inputs_result OUTPUT_VARIABLE title_inputs_status ERROR_VARIABLE title_inputs_error)
if(NOT title_inputs_result EQUAL 0)
    message(FATAL_ERROR "${title_inputs_error}")
endif()
message(STATUS "${title_inputs_status}")
include("${SONIC_ROOT}/cmake/AdapterIdentity.cmake")
set(linux_inverse_unit "${CMAKE_BINARY_DIR}/generated/fpu-calls-inverse/unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp")
get_target_property(linux_guest_sources sonic_linux_guest SOURCES)
list(FILTER linux_guest_sources EXCLUDE REGEX "/unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp$")
set_property(TARGET sonic_linux_guest PROPERTY SOURCES ${linux_guest_sources} "${linux_inverse_unit}")
set_source_files_properties("${linux_inverse_unit}" PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/generated/inverse-arithmetic")
get_target_property(linux_runtime_sources sonic_linux_aot_runtime SOURCES)
list(REMOVE_ITEM linux_runtime_sources "${SONIC_LINUX_SDK}/src/runtime/fpu.cpp")
set_property(TARGET sonic_linux_aot_runtime PROPERTY SOURCES ${linux_runtime_sources} "${CMAKE_BINARY_DIR}/generated/fpu-runtime/fpu.cpp")
include("${SONIC_ROOT}/cmake/LinuxHardwareFpu.cmake")

set(linux_title_sources native_title_adapter native_latent_texture_dispatch_provider native_spg_status_provider
    sonic_native_sound_catalog sonic_native_texture_catalog sonic_native_transform_stream sonic_private_scenario_launcher
    sonic_presentation sonic_render_culling sonic_language sonic_camera sonic_quit_prompt sonic_quit_prompt_image
    sonic_legacy_video sonic_options_display sonic_input sonic_menu sonic_menu_image sonic_menu_text sonic_menu_runtime
    sonic_profiles sonic_restart sonic_tutorial_prompt sonic_tutorial_art sonic_palette_lighting sonic_vertex_normals
    sonic_matrix_stack sonic_collision_math sonic_matrix_inverse sonic_triangle_contacts sonic_matrix_vectors
    sonic_atan_math sonic_amy_hammer_effect sonic_collision_candidates sonic_motion_sampling sonic_mesh_plan)
list(TRANSFORM linux_title_sources PREPEND "${SONIC_ROOT}/src/")
list(TRANSFORM linux_title_sources APPEND ".cpp")
add_library(sonic_linux_title STATIC ${linux_title_sources} "${SONIC_ROOT}/src/ui/sonic_raster.cpp")
target_include_directories(sonic_linux_title PRIVATE "${SONIC_WORKING}/generated/include"
    "${CMAKE_BINARY_DIR}/generated" "${CMAKE_BINARY_DIR}/generated/collision-candidates"
    "${CMAKE_BINARY_DIR}/generated/motion-sampling" "${CMAKE_BINARY_DIR}/generated/fpu-body" "${SONIC_ROOT}/.local/ui-deps")
target_compile_options(sonic_linux_title PRIVATE -O2 -g0 -frounding-math -ffp-contract=off -ffunction-sections -fdata-sections)
target_link_libraries(sonic_linux_title PUBLIC sonic_linux_graphics sonic_linux_movie sonic_linux_aot_runtime)

add_executable(sonic-linux-mesh-plan-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_mesh_plan.cpp" "${SONIC_ROOT}/src/sonic_mesh_plan.cpp")
target_include_directories(sonic-linux-mesh-plan-tests PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic-linux-mesh-plan-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
target_link_libraries(sonic-linux-mesh-plan-tests PRIVATE sonic_linux_aot_runtime)
# Research and arithmetic oracles only; none of these sources enters game.
add_executable(sonic-linux-model-projection-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_model_projection.cpp" "${SONIC_ROOT}/src/sonic_model_projection.cpp")
target_include_directories(sonic-linux-model-projection-tests PRIVATE
    "${SONIC_ROOT}/src" "${CMAKE_BINARY_DIR}/generated/fpu-body")
target_compile_options(sonic-linux-model-projection-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
target_link_libraries(sonic-linux-model-projection-tests PRIVATE sonic_linux_aot_runtime)
add_executable(sonic-linux-fpu-body-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_fpu_body.cpp"
    "${SONIC_ROOT}/build-performance/generated/fpu-runtime/fpu-reference.cpp")
target_include_directories(sonic-linux-fpu-body-tests PRIVATE
    "${CMAKE_BINARY_DIR}/generated/fpu-body" "${SONIC_ROOT}/build-performance/generated/fpu-runtime")
target_compile_options(sonic-linux-fpu-body-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
target_link_libraries(sonic-linux-fpu-body-tests PRIVATE sonic_linux_aot_runtime)

set(linux_services_sources native_bringup_coverage native_port_audio_engine native_port_content native_port_cpu_control
    native_port_save native_port_runtime native_port_texture_asset)
list(TRANSFORM linux_services_sources PREPEND "${SONIC_LINUX_SDK}/src/runtime/")
list(TRANSFORM linux_services_sources APPEND ".cpp")
add_library(sonic_linux_services STATIC ${linux_services_sources}
    "${CMAKE_BINARY_DIR}/generated/audio-buses/native_port_sound_bank.cpp")
target_include_directories(sonic_linux_services PRIVATE "${SONIC_ROOT}/src")
target_compile_options(sonic_linux_services PRIVATE -O2 -g0 -ffunction-sections -fdata-sections)
target_link_libraries(sonic_linux_services PUBLIC sonic_linux_graphics sonic_linux_movie sonic_linux_aot_runtime)

add_executable(game "${SONIC_ROOT}/launcher/main.cpp"
    "${CMAKE_BINARY_DIR}/generated/motion-dispatch/native-port-dispatch.cpp"
    "${CMAKE_BINARY_DIR}/generated/minicart/minicart-aot.cpp")
target_include_directories(game PRIVATE "${SONIC_ROOT}/src" "${SONIC_ROOT}/launcher"
    "${SONIC_WORKING}/generated/include" "${CMAKE_BINARY_DIR}/generated" "${CMAKE_BINARY_DIR}/generated/minicart")
target_compile_definitions(game PRIVATE KATANA_NATIVE_PORT_PRODUCT=1 KATANA_PORT_BUILD_PROFILE_NAME="performance"
    KATANA_PORT_PGO_MODE_NAME="off" SARECOMP_LINUX_PRODUCT=1)
target_compile_options(game PRIVATE -O2 -g0 -frounding-math -ffp-contract=off -ffunction-sections -fdata-sections)
include("${SONIC_ROOT}/cmake/SonicInternalDiagnostics.cmake")
target_link_libraries(game PRIVATE sonic_linux_guest sonic_linux_title sonic_linux_services)
include("${SONIC_ROOT}/cmake/LinuxMemory.cmake")
include("${SONIC_ROOT}/cmake/LinuxPreloadedReads.cmake")
include("${SONIC_ROOT}/cmake/LinuxReadGroups.cmake")
target_link_options(game PRIVATE -Wl,--gc-sections -Wl,-z,stack-size=16777216)
set(SARECOMP_LINUX_CODE_LAYOUT "" CACHE FILEPATH "Measured ELF code section order; empty keeps original layout")
if(SARECOMP_LINUX_CODE_LAYOUT)
    if(NOT IS_ABSOLUTE "${SARECOMP_LINUX_CODE_LAYOUT}" OR NOT EXISTS "${SARECOMP_LINUX_CODE_LAYOUT}")
        message(FATAL_ERROR "SARECOMP_LINUX_CODE_LAYOUT requires an existing absolute script")
    endif()
    target_link_options(game PRIVATE "-Wl,-T,${SARECOMP_LINUX_CODE_LAYOUT}")
    set_property(TARGET game APPEND PROPERTY LINK_DEPENDS "${SARECOMP_LINUX_CODE_LAYOUT}")
endif()
set_target_properties(game PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/lib")
