cmake_minimum_required(VERSION 3.16)

project(xpa)

#############################
## RUST BUILD
#############################

find_program(CARGO_EXECUTABLE cargo REQUIRED)

set(RUST_CRATE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rust)
set(RUST_TARGET_DIR ${RUST_CRATE_DIR}/target)
file(GLOB_RECURSE RUST_SOURCES CONFIGURE_DEPENDS
    ${RUST_CRATE_DIR}/src/*.rs
    ${RUST_CRATE_DIR}/include/*.h
)

if(MSVC)
    set(RUST_LIB_PATH ${RUST_TARGET_DIR}/debug/xpa_rust.lib)
else()
    set(RUST_LIB_PATH ${RUST_TARGET_DIR}/debug/libxpa_rust.a)
endif()

add_custom_command(
    OUTPUT ${RUST_LIB_PATH}
    COMMAND ${CARGO_EXECUTABLE} build
    WORKING_DIRECTORY ${RUST_CRATE_DIR}
    DEPENDS
        ${RUST_CRATE_DIR}/Cargo.toml
        ${RUST_CRATE_DIR}/Cargo.lock
        ${RUST_SOURCES}
    COMMENT "Building Rust static library"
)

add_custom_target(xpa_rust_build ALL DEPENDS ${RUST_LIB_PATH})

add_library(xpa_rust STATIC IMPORTED GLOBAL)
set_target_properties(xpa_rust PROPERTIES
    IMPORTED_LOCATION ${RUST_LIB_PATH}
)

add_dependencies(xpa_rust xpa_rust_build)

##############################
## WEB UI BUILD
##############################

find_program(NPM_EXECUTABLE npm REQUIRED)

set(XPA_WEBUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/webui")
set(XPA_WEBUI_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/webui")

file(GLOB_RECURSE XPA_WEBUI_SOURCES CONFIGURE_DEPENDS
    "${XPA_WEBUI_DIR}/src/*.svelte"
    "${XPA_WEBUI_DIR}/src/*.js"
    "${XPA_WEBUI_DIR}/src/*.css"
    "${XPA_WEBUI_DIR}/index.html"
)

add_custom_command(
    OUTPUT "${XPA_WEBUI_OUTPUT}/index.html"
    COMMAND ${NPM_EXECUTABLE} run build -- --outDir "${XPA_WEBUI_OUTPUT}"
    WORKING_DIRECTORY "${XPA_WEBUI_DIR}"
    COMMENT "Building XPA web UI"
    DEPENDS ${XPA_WEBUI_SOURCES}
)

add_custom_target(xpa_webui DEPENDS "${XPA_WEBUI_OUTPUT}/index.html")




##############################
## XPA
##############################

if (WIN32)
    include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
    find_package(unofficial-webview2 CONFIG REQUIRED)
endif()

add_library(xpa STATIC
    src/lib.c
    src/backend/backend_win32.cpp
    src/router/router.c
    "${XPA_WEBUI_EMBED_C}"
)

add_dependencies(xpa xpa_webui)

target_include_directories(xpa PUBLIC
    include
)

target_include_directories(xpa PRIVATE
    rust/include
)

target_link_libraries(xpa PUBLIC 
    xpa_rust
    ws2_32
    userenv
    ntdll
    unofficial::webview2::webview2
    urlmon
    dwmapi
)

##############################
## EMBED WEB UI
##############################

# Embed Resources function courtesy of shir0areed on GitHub
function(embed_resources_output target)
    set(script_path "${CMAKE_CURRENT_BINARY_DIR}/anything_to_c.cmake")
    file(WRITE  "${script_path}" "file(READ \${CMAKE_ARGV3} buf HEX)\n")
    file(APPEND "${script_path}" "string(REGEX REPLACE \"([0-9a-f][0-9a-f])\" \"0x\\\\1, \" buf \${buf})\n")
    file(APPEND "${script_path}" "file(WRITE \${CMAKE_ARGV4} \"const unsigned char \${CMAKE_ARGV5}[] = { \${buf}0x00 };\\n\")\n")
    file(APPEND "${script_path}" "file(APPEND \${CMAKE_ARGV4} \"const unsigned \${CMAKE_ARGV6} = sizeof(\${CMAKE_ARGV5}) - 1;\\n\")\n")
    foreach(res_path ${ARGN})
        string(MAKE_C_IDENTIFIER ${res_path} identifier)
        set(src_path "${CMAKE_CURRENT_BINARY_DIR}/${res_path}")
        set(dst_path "${CMAKE_CURRENT_BINARY_DIR}/${identifier}.c")
        set(anything_to_c ${CMAKE_COMMAND} -P ${script_path} ${src_path} ${dst_path} ${identifier} ${identifier}_size)
        add_custom_command(OUTPUT ${dst_path} COMMAND ${anything_to_c} DEPENDS ${src_path} VERBATIM)
        target_sources(${target} PRIVATE ${src_path} ${dst_path} )
    endforeach()
endfunction()

embed_resources_output(xpa
    webui/index.html
    webui/assets/style.css
    webui/assets/index.js
)