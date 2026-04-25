cmake_minimum_required(VERSION 3.20)
project(tricycle-sim VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tricycle-sim
    src/main.cpp
    src/core/app.cpp
    src/core/window.cpp
    src/renderer/shader.cpp
    src/renderer/mesh.cpp
    src/renderer/mesh_builder.cpp
    src/renderer/obj_loader.cpp
    src/renderer/obj_mesh.cpp
    src/tricycle/tricycle_mesh.cpp
    src/tricycle/tricycle_model.cpp
    src/physics/trike_physics.cpp
    src/renderer/font.cpp
    src/renderer/hud.cpp
    vendor/glad/src/glad.c
)

target_include_directories(tricycle-sim PRIVATE
    src
    vendor/glad/include
    vendor/glfw/include
    vendor/glm
)

target_link_libraries(tricycle-sim PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/glfw/lib/libglfw3.a
    opengl32
    gdi32
    user32
    shell32
)