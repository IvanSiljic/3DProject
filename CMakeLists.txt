cmake_minimum_required(VERSION 3.27)
project(3DProject)

set(CMAKE_CXX_STANDARD 17)

set(OpenGlLinkers -lglfw3 -lGL -lX11 -lpthread -lXrandr -lXi -ldl)

add_executable(3DProject main.cpp
        glad.c
        imgui/imgui.cpp
        imgui/imgui_demo.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_opengl3.cpp
)

target_link_libraries(3DProject ${OpenGlLinkers})
