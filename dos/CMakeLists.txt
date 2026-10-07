# DOS platform layer
#
#   dos::core      SDL-free emulated PC (memory, ports, VGA, PIT, BIOS...) and
#                  the Borland C runtime compatibility (dos/borland.h)
#   dos::platform  dos::core + the SDL3 host and the program entry point;
#                  link a DOS program against this and define dos_app_info

include(${CMAKE_CURRENT_LIST_DIR}/cmake/Warnings.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/SDL3.cmake)

find_package(Threads REQUIRED)

add_library(dos_core STATIC
    include/dos/app.hpp
    include/dos/borland.h
    include/dos/machine.hpp
    include/dos/runtime.hpp
    src/bios.cpp
    src/borland.cpp
    src/files.cpp
    src/input.cpp
    src/internal.hpp
    src/keyboard.cpp
    src/machine.cpp
    src/pit.cpp
    src/runtime.cpp
    src/vga.cpp
)
add_library(dos::core ALIAS dos_core)
target_include_directories(dos_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(dos_core PUBLIC cxx_std_23)
target_link_libraries(dos_core PUBLIC Threads::Threads)
dos_set_warnings(dos_core)

add_library(dos_sdl STATIC
    sdl/frontend.cpp
    sdl/frontend.hpp
)
target_link_libraries(dos_sdl PUBLIC dos_core SDL3::SDL3)
dos_set_warnings(dos_sdl)

# The entry point is compiled into each program so SDL can provide WinMain.
add_library(dos_platform INTERFACE)
add_library(dos::platform ALIAS dos_platform)
target_sources(dos_platform INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/sdl/entry.cpp)
target_link_libraries(dos_platform INTERFACE dos_sdl)
