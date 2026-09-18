set(movement_contact_dir "${CMAKE_BINARY_DIR}/generated/movement-contact")
set(movement_contact_outputs "${movement_contact_dir}/contact-identities.inc"
    "${movement_contact_dir}/contact-switch.inc" "${movement_contact_dir}/contact-members.inc"
    "${movement_contact_dir}/contact-epochs.inc")
file(READ "${SONIC_ROOT}/tools/movement-contact-owners.json" movement_contact_inventory)
string(JSON movement_contact_count LENGTH "${movement_contact_inventory}")
math(EXPR movement_contact_last "${movement_contact_count}-1")
foreach(i RANGE 0 ${movement_contact_last})
    string(JSON contact_entry GET "${movement_contact_inventory}" ${i} entry)
    math(EXPR contact_hex "${contact_entry}" OUTPUT_FORMAT HEXADECIMAL)
    string(SUBSTRING "${contact_hex}" 2 -1 contact_hex)
    string(TOUPPER "${contact_hex}" contact_hex)
    list(APPEND movement_contact_outputs "${movement_contact_dir}/contact-owner_${contact_hex}.inc")
endforeach()
add_custom_command(OUTPUT ${movement_contact_outputs}
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-movement-contact.py"
        --ram "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
        --output "${movement_contact_dir}" --source-root "${SONIC_WORKING}/generated"
    DEPENDS "${SONIC_ROOT}/tools/prepare-movement-contact.py" "${SONIC_ROOT}/tools/movement-contact-owners.json"
        "${SONIC_ROOT}/tools/prepare-render-hierarchy.py" "${SONIC_ROOT}/tools/prepare-render-hierarchy-bridge.py" "${SONIC_ROOT}/tools/prepare-collision-world.py"
        "${SONIC_ROOT}/tools/prepare_collision_candidates.py" "${SONIC_ROOT}/tools/prepare_motion_sampling.py"
        "${SONIC_ROOT}/tools/prepare-movement-resolver.py" "${SONIC_ROOT}/tools/prepare_near_collision.py"
        "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
    VERBATIM)
target_sources(${animation_title} PRIVATE "${SONIC_ROOT}/src/sonic_movement_contact.cpp" ${movement_contact_outputs})
target_include_directories(${animation_title} PRIVATE "${movement_contact_dir}")
add_executable(sonic-movement-contact-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_movement_contact.cpp"
    "${SONIC_ROOT}/src/sonic_movement_contact.cpp" ${movement_contact_outputs} "${animation_reference}")
target_include_directories(sonic-movement-contact-tests PRIVATE "${SONIC_ROOT}/src" "${movement_contact_dir}")
target_link_libraries(sonic-movement-contact-tests PRIVATE sonic_palette_batch)
if(TARGET sonic_linux_title)
    # Use the game's already qualified bounded/glibc comparison bridge. The
    # oracle compares all 16 MiB at every callback; Zig's bytewise bcmp otherwise
    # dominates TCG test time without adding coverage or a stricter comparison.
    target_sources(sonic-movement-contact-tests PRIVATE $<TARGET_OBJECTS:sonic_linux_memory>)
    target_sources(sonic-movement-contact-tests PRIVATE "${SONIC_LINUX_SDK}/src/decoder/decoder.cpp" "${SONIC_LINUX_SDK}/src/decoder/instruction_metadata.cpp")
    target_compile_options(sonic-movement-contact-tests PRIVATE -O2 -g0 -frounding-math -ffp-contract=off)
    target_link_options(sonic-movement-contact-tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(sonic-movement-contact-tests PRIVATE sonic_linux_services)
else()
    if(SARECOMP_NATIVE_MEMORY_CAPABILITY)
        get_target_property(movement_contact_libraries sonic-movement-contact-tests LINK_LIBRARIES)
        set_property(TARGET sonic-movement-contact-tests PROPERTY LINK_LIBRARIES sonic_internal_diagnostics ${movement_contact_libraries})
    endif()
    target_include_directories(sonic-movement-contact-tests PRIVATE "${CMAKE_BINARY_DIR}/cull-test-sdk/include")
    target_compile_options(sonic-movement-contact-tests PRIVATE /EHsc /utf-8 /fp:strict)
    target_link_libraries(sonic-movement-contact-tests PRIVATE "${SONIC_ROOT}/.local/toolchain/katana_core.lib"
        "${SONIC_ROOT}/.local/toolchain/katana_runtime_core.lib" KatanaRecomp::native_port_runtime)
endif()
