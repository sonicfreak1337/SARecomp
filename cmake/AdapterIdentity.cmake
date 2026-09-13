# Keep r354's exact component ordering for baseline source compatibility.
set(components native_title_adapter.cpp sonic_private_scenario_launcher.hpp
    sonic_private_scenario_launcher.cpp sonic_private_stage_scenarios.inc
    sonic-native-event-adx-catalog.inc sonic_native_input_policy.hpp
    sonic_frame_completion_contract.hpp sonic_qsound_reverb_medium.hpp
    sonic_qsound_reverb_medium_program.inc sonic_native_sdk_texture_release_plan.hpp sonic_texture_sentinel.hpp
    sonic_native_texture_catalog.hpp sonic_native_texture_catalog.cpp
    sonic_native_save_contract.hpp sonic_presentation.hpp sonic_presentation.cpp
    sonic_render_culling.hpp sonic_render_culling.cpp sonic_sdk_color.hpp sonic_model_uv.hpp renderer/sonic_motion.hpp renderer/sonic_motion_view.hpp
    sonic_language.hpp sonic_language.cpp renderer/renderer_selection.hpp
    sonic_motion_owner.hpp sonic_dispatch_memo.hpp sonic_execution_clock.hpp ../tools/prepare-motion-dispatch.py
    sonic_camera_style.hpp sonic_camera_orbit.hpp sonic_camera_policy.hpp sonic_camera.hpp sonic_camera.cpp
    sonic_camera_collision.hpp sonic_camera_world.hpp sonic_camera_input.hpp
    sonic_startup.hpp sonic_startup.cpp
    sonic_quit_prompt.hpp sonic_quit_prompt.cpp sonic_quit_prompt_image.cpp
    sonic_legacy_video.hpp sonic_legacy_video.cpp
    sonic_settings_fields.inc sonic_input_bindings.hpp sonic_input.hpp sonic_input.cpp sonic_rumble.hpp
    sonic_sony_input.hpp sonic_sony_sdl_api.hpp sonic_sony_input.cpp
    sonic_tutorial_prompt.hpp sonic_tutorial_prompt.cpp sonic_tutorial_prompt_adapter.inc
    sonic_tutorial_art.hpp sonic_tutorial_art.cpp sonic_tutorial_control_spans.inc sonic_tutorial_page_adapter.inc
    sonic_configuration_lock.hpp sonic_audio_settings.hpp sonic_movie_audio.hpp sonic_subtitles.hpp
    sonic_audio_device.hpp sonic_audio_recovery.inc sonic_recovery_status.hpp sonic_host_resume.hpp ../tools/prepare-audio-output.py ../tools/prepare-audio-buses.py
    sonic_menu.hpp sonic_menu.cpp sonic_menu_image.cpp sonic_menu_text.hpp sonic_menu_text.cpp
    sonic_menu_runtime.hpp sonic_menu_runtime.cpp sonic_menu_test_input.hpp sonic_options_display.cpp
    sonic_profiles.hpp sonic_profiles.cpp sonic_restart.hpp sonic_restart.cpp
    sonic_diagnostics.hpp ../launcher/sonic_errors.hpp ../launcher/sonic_options.hpp
    ../tools/prepare-camera-platform.py)
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
