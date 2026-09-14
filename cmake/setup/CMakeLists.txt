cmake_minimum_required(VERSION 3.25)
project(SonicSetup LANGUAGES C CXX)
get_filename_component(SONIC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(sdk "${SONIC_ROOT}/.local/linux-sdk")
find_package(Threads REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(gdi_source "${CMAKE_BINARY_DIR}/generated/gdi.cpp")
set(disc_source "${CMAKE_BINARY_DIR}/generated/disc.cpp")
add_custom_command(OUTPUT "${disc_source}"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-setup-disc.py"
        "${sdk}/src/runtime/disc.cpp" "${disc_source}"
    DEPENDS "${sdk}/src/runtime/disc.cpp" "${SONIC_ROOT}/tools/prepare-setup-disc.py" VERBATIM)
add_custom_command(OUTPUT "${gdi_source}"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-setup-gdi.py"
        "${sdk}/src/runtime/gdi.cpp" "${gdi_source}"
    DEPENDS "${sdk}/src/runtime/gdi.cpp" "${SONIC_ROOT}/tools/prepare-setup-gdi.py" VERBATIM)
add_library(setup_sdl SHARED IMPORTED)
if(WIN32)
set_target_properties(setup_sdl PROPERTIES
    IMPORTED_LOCATION "${SONIC_ROOT}/third_party/sdl3/lib/x64/SDL3.dll"
    IMPORTED_IMPLIB "${SONIC_ROOT}/.local/ui-deps/SDL3.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${SONIC_ROOT}/third_party/sdl3/include")
else()
set_target_properties(setup_sdl PROPERTIES
    IMPORTED_LOCATION "${SONIC_ROOT}/build-linux/sdl/libSDL3.so.0.4.16"
    INTERFACE_INCLUDE_DIRECTORIES "${SONIC_ROOT}/.local/linux-deps/sdl/SDL3-3.4.16/include")
endif()
add_executable(sonic-setup
    "${SONIC_ROOT}/src/setup/main.cpp" "${SONIC_ROOT}/src/setup/setup_ui.cpp"
    "${SONIC_ROOT}/src/setup/install_engine.cpp" "${SONIC_ROOT}/src/ui/sonic_raster.cpp"
    "${SONIC_ROOT}/src/setup/install_runtime.cpp"
    "${SONIC_ROOT}/src/sonic_presentation.cpp"
    "${gdi_source}" "${disc_source}"
    "${sdk}/src/runtime/iso9660.cpp" "${sdk}/src/io/input_provenance.cpp"
    "${sdk}/src/progress.cpp" "${sdk}/src/io/json_report.cpp")
target_include_directories(sonic-setup PRIVATE "${sdk}/include"
    "${SONIC_ROOT}/.local/baseline/r354/sdk/generated/include"
    "${SONIC_ROOT}/.local/ui-deps" "${SONIC_ROOT}/out/setup-resources")
if(WIN32)
enable_language(RC)
configure_file("${SONIC_ROOT}/cmake/windows-icon.rc.in" "${CMAKE_BINARY_DIR}/sonic-icon.rc" @ONLY)
target_sources(sonic-setup PRIVATE "${CMAKE_BINARY_DIR}/sonic-icon.rc")
target_compile_options(sonic-setup PRIVATE /O2 /Gy /Gw /EHsc /utf-8 /DNOMINMAX)
target_link_options(sonic-setup PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
target_link_libraries(sonic-setup PRIVATE bcrypt user32 shell32 ole32 uuid)
else()
target_compile_options(sonic-setup PRIVATE -O2 -g0 -ffunction-sections -fdata-sections -Wno-nullability-completeness)
target_link_options(sonic-setup PRIVATE -Wl,--gc-sections)
endif()
target_link_libraries(sonic-setup PRIVATE setup_sdl Threads::Threads ${CMAKE_DL_LIBS})
set_target_properties(sonic-setup PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/lib")
