# Provides SDL3::SDL3: an installed SDL3 if one is found, otherwise a pinned
# release built from source alongside the project (static).

include_guard(GLOBAL)

option(DOS_VENDORED_SDL3 "Always build SDL3 from source instead of using an installed one" OFF)

set(DOS_SDL3_VERSION 3.4.18 CACHE STRING "SDL3 release built when no installed SDL3 is used")
set(DOS_SDL3_SHA256 9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
    CACHE STRING "SHA256 of the SDL3 source archive")

if(NOT TARGET SDL3::SDL3 AND NOT DOS_VENDORED_SDL3)
    find_package(SDL3 3.2 CONFIG QUIET)
endif()

if(NOT TARGET SDL3::SDL3)
    message(STATUS "Building SDL ${DOS_SDL3_VERSION} from source")
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        URL https://github.com/libsdl-org/SDL/releases/download/release-${DOS_SDL3_VERSION}/SDL3-${DOS_SDL3_VERSION}.tar.gz
        URL_HASH SHA256=${DOS_SDL3_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        EXCLUDE_FROM_ALL
        SYSTEM)
    FetchContent_MakeAvailable(SDL3)
endif()
