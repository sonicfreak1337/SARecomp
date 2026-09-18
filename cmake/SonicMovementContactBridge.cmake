foreach(contact_unit IN ITEMS unit-v8C073018-8C073018-457a60bdbd02a56b.cpp
        unit-v8C074214-8C075154-1bc56e9d4bd882fb.cpp
        unit-v8C033122-8C0342E0-7edcb8468a2b4534.cpp)
    set(contact_bridge "${CMAKE_BINARY_DIR}/generated/movement-contact-bridge/${contact_unit}")
    if(TARGET sonic_linux_guest)
        set(contact_target sonic_linux_guest)
    elseif(TARGET sonic_ram_regions)
        set(contact_target sonic_ram_regions)
    else()
        set(contact_target sonic_dispatch)
    endif()
    if(contact_target STREQUAL "sonic_dispatch")
        if(NOT SARECOMP_COMPACT_AOT_EXPERIMENT STREQUAL "OFF" OR
           (DEFINED SARECOMP_RAM_READ_EXPERIMENT AND NOT SARECOMP_RAM_READ_EXPERIMENT STREQUAL "OFF"))
            message(FATAL_ERROR "Movement contact requires retained Windows units")
        endif()
    endif()
        get_target_property(contact_sources ${contact_target} SOURCES)
        set(contact_matches)
        foreach(source IN LISTS contact_sources)
            get_filename_component(name "${source}" NAME)
            if(name STREQUAL contact_unit)
                list(APPEND contact_matches "${source}")
            endif()
        endforeach()
        list(LENGTH contact_matches count)
        if(count EQUAL 0 AND contact_target STREQUAL "sonic_dispatch")
            set(contact_input "${SONIC_WORKING}/generated/code/${contact_unit}")
            target_sources(sonic_dispatch PRIVATE "${contact_bridge}")
        elseif(count EQUAL 1)
            list(GET contact_matches 0 contact_input)
            list(REMOVE_ITEM contact_sources "${contact_input}")
            set_property(TARGET ${contact_target} PROPERTY SOURCES ${contact_sources} "${contact_bridge}")
        else()
            message(FATAL_ERROR "Movement contact unit must have exactly one active owner")
        endif()
        get_source_file_property(contact_includes "${contact_input}" INCLUDE_DIRECTORIES)
        if(contact_includes)
            set_source_files_properties("${contact_bridge}" PROPERTIES INCLUDE_DIRECTORIES "${contact_includes};${SONIC_ROOT}/src")
        else()
            set_source_files_properties("${contact_bridge}" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
        endif()
    if(NOT TARGET sonic_linux_guest)
        set_source_files_properties("${contact_bridge}" PROPERTIES COMPILE_OPTIONS "/fp:strict;/bigobj")
    endif()
    add_custom_command(OUTPUT "${contact_bridge}"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-movement-contact-bridge.py"
            --source-root "${SONIC_WORKING}/generated" --input "${contact_input}" --output "${contact_bridge}"
            --ram "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
        DEPENDS "${SONIC_ROOT}/tools/prepare-movement-contact-bridge.py" "${contact_input}"
            "${SONIC_ROOT}/tools/prepare-movement-contact.py" "${SONIC_ROOT}/tools/movement-contact-owners.json"
            "${SONIC_ROOT}/tools/object-contact-owners.json"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts" VERBATIM)
    list(APPEND contact_bridges "${contact_bridge}")
endforeach()


set(contact_original "${CMAKE_BINARY_DIR}/generated/movement-contact-bridge/private-original.cpp")
add_custom_command(OUTPUT "${contact_original}"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-movement-contact-bridge.py"
        --source-root "${SONIC_WORKING}/generated" --output "${contact_original}"
        --ram "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
    DEPENDS "${SONIC_ROOT}/tools/prepare-movement-contact-bridge.py" "${SONIC_ROOT}/tools/prepare-movement-contact.py" "${SONIC_ROOT}/tools/movement-contact-owners.json"
        "${SONIC_ROOT}/tools/object-contact-owners.json"
        "${SONIC_WORKING}/generated/.katana-generated-artifacts" VERBATIM)
target_sources(${contact_target} PRIVATE "${contact_original}")
set_source_files_properties("${contact_original}" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src;${SONIC_WORKING}/generated/include")
if(NOT TARGET sonic_linux_guest)
    set_source_files_properties("${contact_original}" PROPERTIES COMPILE_OPTIONS "/fp:strict;/bigobj")
endif()
get_target_property(contact_test_sources sonic-movement-contact-tests SOURCES)
list(REMOVE_ITEM contact_test_sources "${SONIC_ROOT}/tools/test_movement_contact.cpp")
add_executable(sonic-movement-contact-aot-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_movement_contact_aot.cpp"
    ${contact_test_sources} "${contact_original}")
foreach(property IN ITEMS INCLUDE_DIRECTORIES COMPILE_OPTIONS COMPILE_DEFINITIONS LINK_LIBRARIES LINK_OPTIONS)
    get_target_property(value sonic-movement-contact-tests ${property})
    if(value)
        set_property(TARGET sonic-movement-contact-aot-tests PROPERTY ${property} "${value}")
    endif()
endforeach()
target_include_directories(sonic-movement-contact-aot-tests PRIVATE "${SONIC_WORKING}/generated/include"
    "${CMAKE_BINARY_DIR}/generated/movement-contact-bridge")
add_executable(sonic-object-contact-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_object_contact.cpp"
    ${contact_test_sources} "${contact_original}")
foreach(property IN ITEMS INCLUDE_DIRECTORIES COMPILE_OPTIONS COMPILE_DEFINITIONS LINK_LIBRARIES LINK_OPTIONS)
    get_target_property(value sonic-movement-contact-aot-tests ${property})
    if(value)
        set_property(TARGET sonic-object-contact-tests PROPERTY ${property} "${value}")
    endif()
endforeach()
