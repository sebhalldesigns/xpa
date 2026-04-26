cmake_minimum_required(VERSION 3.16)

project(xpa)

add_subdirectory(extern)

##############################
## XPA
##############################

add_library(xpa STATIC
    src/lib.c
    src/backend/nuklear_impl.c
    src/backend/backend_win32.cpp
    
    src/draw/draw.c
    src/control/control.c
    src/workbench/workbench.c
    src/menubar/menubar.c
    src/statusbar/statusbar.c
    src/dock/dock.c
)

target_include_directories(xpa PUBLIC
    include
)

target_include_directories(xpa PRIVATE
    src
    extern
)

target_link_libraries(xpa PUBLIC 
    opengl32
    dwmapi
    glad
    freetype
)


# Embed Resources function courtesy of shir0areed on GitHub
function(embed_resources target)
    set(script_path "${CMAKE_CURRENT_BINARY_DIR}/anything_to_c.cmake")
    file(WRITE  "${script_path}" "file(READ \${CMAKE_ARGV3} buf HEX)\n")
    file(APPEND "${script_path}" "string(REGEX REPLACE \"([0-9a-f][0-9a-f])\" \"0x\\\\1, \" buf \${buf})\n")
    file(APPEND "${script_path}" "file(WRITE \${CMAKE_ARGV4} \"const unsigned char \${CMAKE_ARGV5}[] = { \${buf}0x00 };\\n\")\n")
    file(APPEND "${script_path}" "file(APPEND \${CMAKE_ARGV4} \"const unsigned \${CMAKE_ARGV6} = sizeof(\${CMAKE_ARGV5}) - 1;\\n\")\n")
    foreach(res_path ${ARGN})
        string(MAKE_C_IDENTIFIER ${res_path} identifier)
        set(src_path "${CMAKE_CURRENT_SOURCE_DIR}/${res_path}")
        set(dst_path "${CMAKE_CURRENT_BINARY_DIR}/${identifier}.c")
        set(anything_to_c ${CMAKE_COMMAND} -P ${script_path} ${src_path} ${dst_path} ${identifier} ${identifier}_size)
        add_custom_command(OUTPUT ${dst_path} COMMAND ${anything_to_c} DEPENDS ${src_path} VERBATIM)
        target_sources(${target} PRIVATE ${src_path} ${dst_path} )
    endforeach()
endfunction()

embed_resources(xpa
    assets/fonts/Font-Awesome-7-Free-Regular-400.otf
    assets/fonts/Font-Awesome-7-Free-Solid-900.otf
)
