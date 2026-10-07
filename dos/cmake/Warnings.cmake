include_guard(GLOBAL)

# Warnings for code written for this project (not for ported DOS sources).
function(dos_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /wd4100)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()
endfunction()

# Compiler settings that match the Borland C/C++ compilers DOS programs were
# built with: unsigned char (-K) and no struct padding (-a-) are opted into
# per target with these helpers.
function(dos_borland_compat target)
    cmake_parse_arguments(ARG "UNSIGNED_CHAR" "" "" ${ARGN})
    if(ARG_UNSIGNED_CHAR)
        if(MSVC)
            target_compile_options(${target} PRIVATE /J)
        else()
            target_compile_options(${target} PRIVATE -funsigned-char)
        endif()
    endif()
    if(MSVC)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
    endif()
endfunction()
