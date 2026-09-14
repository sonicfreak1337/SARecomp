set(sonic_linux_movie "${CMAKE_BINARY_DIR}/generated/linux-movie/native_port_movie.cpp")
add_custom_command(OUTPUT "${sonic_linux_movie}"
    COMMAND "${Python3_EXECUTABLE}" "${SONIC_ROOT}/tools/prepare-linux-movie.py"
        "${SONIC_LINUX_SDK}/src/runtime/native_port_movie.cpp" "${sonic_linux_movie}"
    DEPENDS "${SONIC_ROOT}/tools/prepare-linux-movie.py" "${SONIC_LINUX_SDK}/src/runtime/native_port_movie.cpp" VERBATIM)
set(sonic_linux_ffmpeg "${SONIC_ROOT}/.local/linux-deps/ffmpeg/ffmpeg-n8.1.2-52-g5a03dfa0f6-linux64-lgpl-shared-8.1")
foreach(component IN ITEMS avformat avcodec avutil swresample swscale)
    file(GLOB libraries "${sonic_linux_ffmpeg}/lib/lib${component}.so.*")
    list(SORT libraries COMPARE NATURAL)
    list(GET libraries 0 library)
    add_library(sonic_linux_${component} SHARED IMPORTED)
    set_target_properties(sonic_linux_${component} PROPERTIES IMPORTED_LOCATION "${library}")
endforeach()
add_library(sonic_linux_movie STATIC "${sonic_linux_movie}"
    "${SONIC_LINUX_SDK}/src/runtime/native_port_ffmpeg_codec.cpp"
    "${SONIC_LINUX_SDK}/src/runtime/native_port_codec.cpp")
target_include_directories(sonic_linux_movie PRIVATE "${SONIC_ROOT}/src/linux"
    "${SONIC_LINUX_SDK}/src/runtime" "${sonic_linux_ffmpeg}/include")
target_compile_options(sonic_linux_movie PRIVATE -O2 -g0 -ffunction-sections -fdata-sections)
target_link_libraries(sonic_linux_movie PUBLIC sonic_linux_audio sonic_linux_platform
    sonic_linux_avformat sonic_linux_avcodec sonic_linux_avutil sonic_linux_swresample sonic_linux_swscale)
add_executable(sonic-linux-media-check EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/check_media_decoding.cpp")
target_compile_options(sonic-linux-media-check PRIVATE -O2 -g0)
target_link_libraries(sonic-linux-media-check PRIVATE sonic_linux_movie sonic_linux_sdk_headers)
set_target_properties(sonic-linux-media-check PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/lib")
