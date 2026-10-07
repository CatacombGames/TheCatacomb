cmake_minimum_required(VERSION 3.25)

project(TheCatacomb
    VERSION 1.02
    DESCRIPTION "The Catacomb (Catacomb II) on the DOS platform layer"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(dos)

# The game. CAT_PLAY.CPP, OBJECTS.CPP and CATASM.CPP are #included by
# CATACOMB.CPP, as in the original DOS build.
set(CATACOMB_INCLUDED_SOURCES src/CAT_PLAY.CPP src/OBJECTS.CPP src/CATASM.CPP)
set_source_files_properties(${CATACOMB_INCLUDED_SOURCES} PROPERTIES HEADER_FILE_ONLY ON)

add_executable(catacomb
    src/APPINFO.CPP
    src/CATACOMB.CPP
    src/CPANEL.CPP
    src/PCRLIB_A.CPP
    src/PCRLIB_C.CPP
    src/RLEASM.CPP
    src/PCRLIB.H
    src/NGRABCA2.H
    src/SOUNDS.H
    ${CATACOMB_INCLUDED_SOURCES}
)
target_link_libraries(catacomb PRIVATE dos::platform)
dos_borland_compat(catacomb UNSIGNED_CHAR)
# the 1990 code style (switches over part of an enum...) is kept as is
target_compile_options(catacomb PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-switch -Wno-dangling-else -Wno-shift-op-parentheses>)
set_target_properties(catacomb PROPERTIES
    OUTPUT_NAME "catacomb"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data")

if(MSVC)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT catacomb)
endif()

install(TARGETS catacomb
    RUNTIME DESTINATION .)
