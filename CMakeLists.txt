cmake_minimum_required(VERSION 3.16)

project(xpa)

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

add_library(xpa STATIC
    src/lib.c
    src/backend/backend_win32.cpp
)

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
)
