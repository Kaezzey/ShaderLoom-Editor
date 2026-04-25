cmake_minimum_required(VERSION 3.24)

project(GrainradOfflineEditor
    VERSION 0.1.0
    LANGUAGES CXX
)

option(GRAINRAD_BUILD_GUI "Build the GLFW/OpenGL/ImGui editor shell" OFF)

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

add_library(grainrad_core
    src/grainrad/Image.cpp
    src/grainrad/Processing.cpp
    src/grainrad/effects/AsciiEffect.cpp
    src/grainrad/effects/DitherEffect.cpp
    src/grainrad/effects/PixelSortEffect.cpp
)

target_include_directories(grainrad_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${STB_INCLUDE_DIR}
)

if(MSVC)
    target_compile_options(grainrad_core PRIVATE /W4 /permissive-)
    target_compile_definitions(grainrad_core PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(grainrad_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(grainrad_cli
    tools/grainrad_cli.cpp
)

target_link_libraries(grainrad_cli PRIVATE grainrad_core)

if(GRAINRAD_BUILD_GUI)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)

    add_executable(grainrad_editor
        src/app/main.cpp
    )

    target_link_libraries(grainrad_editor
        PRIVATE
            grainrad_core
            OpenGL::GL
            glfw
            imgui::imgui
    )
endif()
