cmake_minimum_required(VERSION 3.16)

project(xpa)

##############################
## XPA
##############################

add_library(xpa STATIC
    src/lib.c
    src/backend/backend_win32.cpp
)

target_include_directories(xpa PUBLIC
    include
)

target_include_directories(xpa PRIVATE
    src
)

target_link_libraries(xpa PUBLIC 
    opengl32
)

