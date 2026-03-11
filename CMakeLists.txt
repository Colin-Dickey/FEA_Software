cmake_minimum_required(VERSION 3.10)
project(FEA)

set(CMAKE_CXX_STANDARD 17)

include_directories(headers)

add_executable(fea
    main.cpp
    source/mesh.cpp
    source/renderer.cpp
    glad/gl.c
)

target_link_libraries(fea PRIVATE glfw3 opengl32)