# Keep r354's exact component ordering for baseline source compatibility.
set(components native_title_adapter.cpp sonic_private_scenario_launcher.hpp
    sonic_private_scenario_launcher.cpp sonic_private_stage_scenarios.inc
    sonic-native-event-adx-catalog.inc sonic_native_input_policy.hpp
    sonic_frame_completion_contract.hpp sonic_qsound_reverb_medium.hpp
    sonic_qsound_reverb_medium_program.inc sonic_native_sdk_texture_release_plan.hpp
    sonic_native_texture_catalog.hpp sonic_native_texture_catalog.cpp
    sonic_native_save_contract.hpp)
set(hashes)
foreach(component IN LISTS components)
    set(path "${PROJECT_SOURCE_DIR}/src/${component}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
    file(SHA256 "${path}" sha)
    list(APPEND hashes "${sha}")
endforeach()
list(JOIN hashes ":" composite)
string(SHA256 sonic_native_title_adapter_sha256 "${composite}")
set(path "${PROJECT_SOURCE_DIR}/src/native_latent_texture_dispatch_provider.cpp")
file(SHA256 "${path}" latent)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
string(SHA256 sonic_native_latent_texture_dispatch_provider_composed_sha256
    "${latent}:${sonic_native_title_adapter_sha256}")
set(path "${PROJECT_SOURCE_DIR}/src/native_spg_status_provider.cpp")
file(SHA256 "${path}" sonic_native_spg_status_provider_sha256)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
foreach(header IN ITEMS native_provider_identity native_latent_texture_dispatch_provider_identity native_spg_status_provider_identity)
    configure_file("${PROJECT_SOURCE_DIR}/src/${header}.hpp.in"
        "${CMAKE_BINARY_DIR}/generated/${header}.hpp" @ONLY)
endforeach()
