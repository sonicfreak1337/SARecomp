find_package(Threads REQUIRED)
set(sdk "${SONIC_BASE}/sdk")
foreach(component IN ITEMS avformat avcodec avutil swresample swscale)
    if(component STREQUAL "avformat" OR component STREQUAL "avcodec")
        set(dll "${component}-62.dll")
    elseif(component STREQUAL "avutil")
        set(dll "avutil-60.dll")
    elseif(component STREQUAL "swresample")
        set(dll "swresample-6.dll")
    else()
        set(dll "swscale-9.dll")
    endif()
    add_library(KatanaFfmpeg::${component} SHARED IMPORTED)
    set_target_properties(KatanaFfmpeg::${component} PROPERTIES
        IMPORTED_IMPLIB "${sdk}/ffmpeg/lib/${component}.lib"
        IMPORTED_LOCATION "${sdk}/ffmpeg/bin/${dll}"
        INTERFACE_INCLUDE_DIRECTORIES "${sdk}/ffmpeg/include")
endforeach()
add_library(KatanaRecomp::KatanaFfmpegBuild INTERFACE IMPORTED)
set_property(TARGET KatanaRecomp::KatanaFfmpegBuild PROPERTY INTERFACE_LINK_LIBRARIES
    "KatanaFfmpeg::avformat;KatanaFfmpeg::avcodec;KatanaFfmpeg::avutil;KatanaFfmpeg::swresample;KatanaFfmpeg::swscale")
foreach(component IN ITEMS aot_runtime native_port_runtime)
    add_library(KatanaRecomp::${component} STATIC IMPORTED)
    set_target_properties(KatanaRecomp::${component} PROPERTIES
        IMPORTED_LOCATION "${sdk}/lib/katana_${component}.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${sdk}/include;${sdk}/generated/include")
endforeach()
set_property(TARGET KatanaRecomp::aot_runtime PROPERTY INTERFACE_LINK_LIBRARIES Threads::Threads)
set_property(TARGET KatanaRecomp::native_port_runtime PROPERTY INTERFACE_LINK_LIBRARIES
    "KatanaRecomp::aot_runtime;KatanaRecomp::KatanaFfmpegBuild;bcrypt;dinput8;dxguid;mfplat;mfreadwrite;mfuuid;ole32;d3d11;dxgi;user32;winmm")
