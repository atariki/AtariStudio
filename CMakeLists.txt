cmake_minimum_required(VERSION 3.21)

project(
    AtariStudio
    VERSION 0.1.0
    DESCRIPTION "Atari 8-bit Reverse Engineering IDE"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(AtariStudio
    src/main.cpp
 "include/AtariStudio/Core/Types.h" "include/AtariStudio/Core/Segment.h" "include/AtariStudio/Core/Memory.h" "include/AtariStudio/Core/Memory.cpp" "include/AtariStudio/Core/Project.h" "include/AtariStudio/Core/Project.cpp")

target_include_directories(AtariStudio
    PRIVATE
        include
)

set_target_properties(AtariStudio PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)

add_executable(AtariStudio
    src/main.cpp

    core/Memory.cpp
    core/Project.cpp
)

target_include_directories(AtariStudio
    PRIVATE
        include
        core
)