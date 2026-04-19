cmake_minimum_required(VERSION 3.16)

add_library(glad STATIC
    glad/glad.c
)

target_include_directories(glad PUBLIC
    glad/include
)

# Disable optional FreeType dependencies we don't need
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
add_subdirectory(freetype)