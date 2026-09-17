# Qualified whole collision-world group; internal zero override remains.
foreach(world_unit IN ITEMS
    unit-v8C0273A2-8C028EC2-3c3ba866ec5c4265.cpp
    unit-v8C02CA00-8C02DE20-1e7e2fdd391d9bd8.cpp
    unit-v8C051E56-8C053338-ce4429c39e6969de.cpp)
    set(world_bridge "${CMAKE_BINARY_DIR}/generated/world-bridge/${world_unit}")
    if(TARGET sonic_linux_guest)
        set(world_target sonic_linux_guest)
    elseif(TARGET sonic_ram_regions)
        set(world_target sonic_ram_regions)
    else()
        set(world_target sonic_dispatch)
    endif()
    if(world_target STREQUAL "sonic_dispatch")
        if(NOT SARECOMP_COMPACT_AOT_EXPERIMENT STREQUAL "OFF" OR
           (DEFINED SARECOMP_RAM_READ_EXPERIMENT AND NOT SARECOMP_RAM_READ_EXPERIMENT STREQUAL "OFF"))
            message(FATAL_ERROR "Native world requires retained Windows units")
        endif()
        set(world_input "${SONIC_WORKING}/generated/code/${world_unit}")
        target_sources(sonic_dispatch PRIVATE "${world_bridge}")
    else()
        get_target_property(world_sources ${world_target} SOURCES)
        set(world_matches)
        foreach(source IN LISTS world_sources)
            get_filename_component(name "${source}" NAME)
            if(name STREQUAL world_unit)
                list(APPEND world_matches "${source}")
            endif()
        endforeach()
        list(LENGTH world_matches count)
        if(NOT count EQUAL 1)
            message(FATAL_ERROR "Collision world unit must have exactly one active owner")
        endif()
        list(GET world_matches 0 world_input)
        list(REMOVE_ITEM world_sources "${world_input}")
        set_property(TARGET ${world_target} PROPERTY SOURCES ${world_sources} "${world_bridge}")
        get_source_file_property(world_includes "${world_input}" INCLUDE_DIRECTORIES)
        if(world_includes)
            set_source_files_properties("${world_bridge}" PROPERTIES INCLUDE_DIRECTORIES "${world_includes};${SONIC_ROOT}/src")
        else()
            set_source_files_properties("${world_bridge}" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
        endif()
    endif()
    if(NOT TARGET sonic_linux_guest)
        set_source_files_properties("${world_bridge}" PROPERTIES COMPILE_OPTIONS "/fp:strict;/bigobj")
    endif()
    add_custom_command(OUTPUT "${world_bridge}"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-collision-world-bridge.py"
            --source-root "${SONIC_WORKING}/generated" --input "${world_input}" --output "${world_bridge}"
            --ram "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
        DEPENDS "${SONIC_ROOT}/tools/prepare-collision-world-bridge.py" "${world_input}"
            "${SONIC_ROOT}/tools/prepare-collision-world.py"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts" VERBATIM)
    list(APPEND world_bridges "${world_bridge}")
endforeach()

get_target_property(world_test_sources sonic-collision-world-tests SOURCES)
list(REMOVE_ITEM world_test_sources "${SONIC_ROOT}/tools/test_collision_world.cpp")
add_executable(sonic-collision-world-aot-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_collision_world_aot.cpp" ${world_test_sources} ${world_bridges})
foreach(property IN ITEMS INCLUDE_DIRECTORIES COMPILE_OPTIONS COMPILE_DEFINITIONS LINK_LIBRARIES LINK_OPTIONS)
    get_target_property(value sonic-collision-world-tests ${property})
    if(value)
        set_property(TARGET sonic-collision-world-aot-tests PROPERTY ${property} "${value}")
    endif()
endforeach()
target_compile_definitions(sonic-collision-world-aot-tests PRIVATE SARECOMP_WORLD_AOT_TEST=1)
target_include_directories(sonic-collision-world-aot-tests PRIVATE "${SONIC_WORKING}/generated/include")
