cmake_minimum_required(VERSION 3.21)

project(AtariStudio LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    include(CTest)
endif()

option(
    ATARISTUDIO_ENABLE_ASAN
    "Build AtariStudio executables with AddressSanitizer"
    OFF
)

option(
    ATARISTUDIO_BUILD_TOOLS
    "Build AtariStudio command-line tools"
    ${PROJECT_IS_TOP_LEVEL}
)

option(
    ATARISTUDIO_BUILD_TESTS
    "Build AtariStudio tests when BUILD_TESTING is enabled"
    ${PROJECT_IS_TOP_LEVEL}
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(
        /W4
        /permissive-
    )
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(ATARISTUDIO_ENABLE_ASAN AND
   NOT MSVC AND
   NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(
        FATAL_ERROR
        "ATARISTUDIO_ENABLE_ASAN is not supported by this compiler"
    )
endif()

function(ataristudio_copy_asan_runtime target_name)
    if(NOT ATARISTUDIO_ENABLE_ASAN OR NOT MSVC)
        return()
    endif()

    get_filename_component(
        compiler_directory
        "${CMAKE_CXX_COMPILER}"
        DIRECTORY
    )

    file(
        GLOB
        asan_runtime_dlls
        "${compiler_directory}/clang_rt.asan*_dynamic-*.dll"
    )

    if(NOT asan_runtime_dlls)
        message(
            FATAL_ERROR
            "AddressSanitizer runtime DLLs were not found next to "
            "${CMAKE_CXX_COMPILER}"
        )
    endif()

    foreach(runtime_dll IN LISTS asan_runtime_dlls)
        add_custom_command(
            TARGET
                "${target_name}"
            POST_BUILD
            COMMAND
                "${CMAKE_COMMAND}"
                -E
                copy_if_different
                "${runtime_dll}"
                "$<TARGET_FILE_DIR:${target_name}>"
            VERBATIM
        )
    endforeach()
endfunction()

add_library(AtariStudio STATIC

    src/Core/BinaryReader.cpp
    src/Core/Memory.cpp
    src/Core/Project.cpp

    src/Cpu6502/OpcodeTable.cpp

    src/Disassembler/Disassembler.cpp
    src/Disassembler/Listing.cpp

    src/Formats/XEX/XexFile.cpp
    src/Formats/XEX/XexLoader.cpp
)

target_include_directories(AtariStudio
    PUBLIC
        include
)

target_compile_features(AtariStudio
    PUBLIC
        cxx_std_20
)

if(MSVC)
    target_compile_options(AtariStudio
        PUBLIC
            /EHsc
    )
endif()

if(ATARISTUDIO_ENABLE_ASAN)
    if(MSVC)
        target_compile_options(AtariStudio
            PUBLIC
                /fsanitize=address
                /Zi
        )

        target_link_options(AtariStudio
            INTERFACE
                /INCREMENTAL:NO
                /DEBUG
        )
    else()
        target_compile_options(AtariStudio
            PUBLIC
                -fsanitize=address
                -fno-omit-frame-pointer
        )

        target_link_options(AtariStudio
            INTERFACE
                -fsanitize=address
        )
    endif()
endif()

if(ATARISTUDIO_BUILD_TOOLS OR
   (BUILD_TESTING AND ATARISTUDIO_BUILD_TESTS))
    add_subdirectory(ToolsApp)
endif()

if(BUILD_TESTING AND ATARISTUDIO_BUILD_TESTS)
    add_subdirectory(tests)
endif()
