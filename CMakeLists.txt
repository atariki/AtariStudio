cmake_minimum_required(VERSION 3.21)

project(AtariStudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(AtariStudio STATIC

    src/Core/Memory.cpp
    src/Core/BinaryReader.cpp
    src/Core/Project.cpp

    src/Cpu6502/OpcodeTable.cpp

    src/Disassembler/Disassembler.cpp
    src/Disassembler/Listing.cpp

    src/Formats/XEX/XexFile.cpp
    src/Formats/XEX/XexLoader.cpp
    src/Formats/XEX/XexSegment.cpp
)

target_include_directories(AtariStudio
    PUBLIC
        include
)

add_subdirectory(ToolsApp)