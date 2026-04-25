cmake_minimum_required(VERSION 3.24)

project(ShaderLoomOfflineEditor
    VERSION 0.1.0
    LANGUAGES CXX
)

option(ShaderLoom_BUILD_GUI "Build the GLFW/OpenGL/ImGui editor shell" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_path(STB_INCLUDE_DIR
    NAMES stb_image.h stb_image_write.h
    HINTS
        "C:/dev/vcpkg/installed/x64-windows/include"
)

if(NOT STB_INCLUDE_DIR)
    message(FATAL_ERROR "stb headers were not found. Install stb or pass -DSTB_INCLUDE_DIR=<path>.")
endif()

add_library(ShaderLoom_core
    src/ShaderLoom/Image.cpp
    src/ShaderLoom/Processing.cpp
    src/ShaderLoom/effects/AsciiEffect.cpp
    src/ShaderLoom/effects/DitherEffect.cpp
    src/ShaderLoom/effects/PixelSortEffect.cpp
)

target_include_directories(ShaderLoom_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${STB_INCLUDE_DIR}
)

if(MSVC)
    target_compile_options(ShaderLoom_core PRIVATE /W4 /permissive-)
    target_compile_definitions(ShaderLoom_core PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(ShaderLoom_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ShaderLoom_cli
    tools/ShaderLoom_cli.cpp
)

target_link_libraries(ShaderLoom_cli PRIVATE ShaderLoom_core)

if(ShaderLoom_BUILD_GUI)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)

    add_executable(ShaderLoom_editor
        src/app/main.cpp
    )

    target_link_libraries(ShaderLoom_editor
        PRIVATE
            ShaderLoom_core
            OpenGL::GL
            glfw
            imgui::imgui
    )

    if(WIN32)
        target_link_libraries(ShaderLoom_editor PRIVATE Comdlg32)
    endif()
endif()
