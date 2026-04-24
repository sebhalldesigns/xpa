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
    src/workbench/workbench.cpp
    src/menubar/menubar.cpp
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

