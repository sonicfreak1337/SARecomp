# Both matched stage pairs improve throughput modestly. Keep a build-time
# control for future hardware comparisons; guest semantics are unchanged.
# No guest unit or pinned runtime source is rebuilt for this binding change.
option(SARECOMP_LINUX_MEMORY_COMPARE "Optimize host buffer comparisons" ON)
add_library(sonic_linux_memory OBJECT "${SONIC_ROOT}/src/linux/sonic_memory_compare.c")
target_compile_options(sonic_linux_memory PRIVATE -O2 -g0 -fno-builtin)
if(SARECOMP_LINUX_MEMORY_COMPARE)
    target_sources(game PRIVATE $<TARGET_OBJECTS:sonic_linux_memory>)
endif()

add_executable(sonic-linux-memory-tests EXCLUDE_FROM_ALL
    "${SONIC_ROOT}/tools/test_linux_memory.c" $<TARGET_OBJECTS:sonic_linux_memory>)
add_executable(sonic-linux-memory-control EXCLUDE_FROM_ALL "${SONIC_ROOT}/tools/test_linux_memory.c")
foreach(target IN ITEMS sonic-linux-memory-tests sonic-linux-memory-control)
    target_compile_options(${target} PRIVATE -O2 -g0 -fno-builtin)
    target_link_libraries(${target} PRIVATE ${CMAKE_DL_LIBS})
endforeach()
