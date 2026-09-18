foreach(hierarchy_unit IN ITEMS unit-v8C0400A0-8C04124E-c3a8c709f8ba2806.cpp
        unit-v8C0412C8-8C0425A0-1c2be1678b040d69.cpp
        unit-v8C036BC0-8C037C3C-aa2f5ddfed3d4270.cpp
        unit-v8C050BE4-8C051E00-44b823a416a623f6.cpp
        unit-v8C0FD05A-8C0FE340-9f120c53ca8c2b89.cpp)
    set(hierarchy_bridge "${CMAKE_BINARY_DIR}/generated/render-hierarchy-bridge/${hierarchy_unit}")
    if(TARGET sonic_linux_guest)
        set(hierarchy_target sonic_linux_guest)
    elseif(TARGET sonic_ram_regions)
        set(hierarchy_target sonic_ram_regions)
    else()
        set(hierarchy_target sonic_dispatch)
    endif()
    if(hierarchy_target STREQUAL "sonic_dispatch")
        if(NOT SARECOMP_COMPACT_AOT_EXPERIMENT STREQUAL "OFF" OR
           (DEFINED SARECOMP_RAM_READ_EXPERIMENT AND NOT SARECOMP_RAM_READ_EXPERIMENT STREQUAL "OFF"))
            message(FATAL_ERROR "Render hierarchy requires retained Windows units")
        endif()
    endif()
        get_target_property(hierarchy_sources ${hierarchy_target} SOURCES)
        set(hierarchy_matches)
        foreach(source IN LISTS hierarchy_sources)
            get_filename_component(name "${source}" NAME)
            if(name STREQUAL hierarchy_unit)
                list(APPEND hierarchy_matches "${source}")
            endif()
        endforeach()
        list(LENGTH hierarchy_matches count)
        if(count EQUAL 0 AND hierarchy_target STREQUAL "sonic_dispatch")
            set(hierarchy_input "${SONIC_WORKING}/generated/code/${hierarchy_unit}")
            target_sources(sonic_dispatch PRIVATE "${hierarchy_bridge}")
        elseif(count EQUAL 1)
            list(GET hierarchy_matches 0 hierarchy_input)
            list(REMOVE_ITEM hierarchy_sources "${hierarchy_input}")
            set_property(TARGET ${hierarchy_target} PROPERTY SOURCES ${hierarchy_sources} "${hierarchy_bridge}")
        else()
            message(FATAL_ERROR "Render hierarchy unit must have exactly one active owner")
        endif()
        get_source_file_property(hierarchy_includes "${hierarchy_input}" INCLUDE_DIRECTORIES)
        if(hierarchy_includes)
            set_source_files_properties("${hierarchy_bridge}" PROPERTIES INCLUDE_DIRECTORIES "${hierarchy_includes};${SONIC_ROOT}/src")
        else()
            set_source_files_properties("${hierarchy_bridge}" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src")
        endif()
    if(NOT TARGET sonic_linux_guest)
        set_source_files_properties("${hierarchy_bridge}" PROPERTIES COMPILE_OPTIONS "/fp:strict;/bigobj")
    endif()
    add_custom_command(OUTPUT "${hierarchy_bridge}"
        COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-render-hierarchy-bridge.py"
            --source-root "${SONIC_WORKING}/generated" --input "${hierarchy_input}" --output "${hierarchy_bridge}"
            --ram "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
        DEPENDS "${SONIC_ROOT}/tools/prepare-render-hierarchy-bridge.py" "${hierarchy_input}"
            "${SONIC_ROOT}/tools/land-render-owners.json"
            "${SONIC_ROOT}/tools/actor-operation-owners.json"
            "${SONIC_ROOT}/tools/prepare-render-hierarchy.py"
            "${SONIC_WORKING}/generated/.katana-generated-artifacts" VERBATIM)
    list(APPEND hierarchy_bridges "${hierarchy_bridge}")
endforeach()


set(hierarchy_original "${CMAKE_BINARY_DIR}/generated/render-hierarchy-bridge/private-original.cpp")
add_custom_command(OUTPUT "${hierarchy_original}"
    BYPRODUCTS "${CMAKE_BINARY_DIR}/generated/render-hierarchy-bridge/hierarchy-test-externals.inc"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-render-hierarchy-bridge.py"
        --source-root "${SONIC_WORKING}/generated" --output "${hierarchy_original}"
        --ram "${SONIC_ROOT}/.local/baseline/r354/native-content/postpal-main-ram-native-ready.bin"
    DEPENDS "${SONIC_ROOT}/tools/prepare-render-hierarchy-bridge.py" "${SONIC_ROOT}/tools/prepare-render-hierarchy.py"
        "${SONIC_ROOT}/tools/land-render-owners.json"
        "${SONIC_ROOT}/tools/actor-operation-owners.json"
        "${SONIC_WORKING}/generated/.katana-generated-artifacts" VERBATIM)
target_sources(${hierarchy_target} PRIVATE "${hierarchy_original}")
set_source_files_properties("${hierarchy_original}" PROPERTIES INCLUDE_DIRECTORIES "${SONIC_ROOT}/src;${SONIC_WORKING}/generated/include")
if(NOT TARGET sonic_linux_guest)
    set_source_files_properties("${hierarchy_original}" PROPERTIES COMPILE_OPTIONS "/fp:strict;/bigobj")
endif()
get_target_property(hierarchy_test_sources sonic-render-hierarchy-tests SOURCES)
list(REMOVE_ITEM hierarchy_test_sources "${SONIC_ROOT}/tools/test_render_hierarchy.cpp")
add_executable(sonic-render-hierarchy-aot-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_render_hierarchy_aot.cpp"
    ${hierarchy_test_sources} "${hierarchy_original}")
foreach(property IN ITEMS INCLUDE_DIRECTORIES COMPILE_OPTIONS COMPILE_DEFINITIONS LINK_LIBRARIES LINK_OPTIONS)
    get_target_property(value sonic-render-hierarchy-tests ${property})
    if(value)
        set_property(TARGET sonic-render-hierarchy-aot-tests PROPERTY ${property} "${value}")
    endif()
endforeach()
target_include_directories(sonic-render-hierarchy-aot-tests PRIVATE "${SONIC_WORKING}/generated/include" "${CMAKE_BINARY_DIR}/generated/render-hierarchy-bridge")
add_executable(sonic-land-render-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_land_render.cpp"
    ${hierarchy_test_sources} "${hierarchy_original}")
foreach(property IN ITEMS INCLUDE_DIRECTORIES COMPILE_OPTIONS COMPILE_DEFINITIONS LINK_LIBRARIES LINK_OPTIONS)
    get_target_property(value sonic-render-hierarchy-aot-tests ${property})
    if(value)
        set_property(TARGET sonic-land-render-tests PROPERTY ${property} "${value}")
    endif()
endforeach()

add_executable(sonic-actor-operation-tests EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_actor_operation.cpp"
    ${hierarchy_test_sources} "${hierarchy_original}")
foreach(property IN ITEMS INCLUDE_DIRECTORIES COMPILE_OPTIONS COMPILE_DEFINITIONS LINK_LIBRARIES LINK_OPTIONS)
    get_target_property(value sonic-render-hierarchy-aot-tests ${property})
    if(value)
        set_property(TARGET sonic-actor-operation-tests PROPERTY ${property} "${value}")
    endif()
endforeach()
