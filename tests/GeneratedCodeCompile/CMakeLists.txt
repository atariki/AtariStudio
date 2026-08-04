cmake_minimum_required(VERSION 3.21)

project(AtariStudioGeneratedCodeCompile LANGUAGES CXX)

include(CTest)

if(NOT DEFINED GENERATED_SOURCE)
    message(FATAL_ERROR "GENERATED_SOURCE is required")
endif()

if(NOT EXISTS "${GENERATED_SOURCE}")
    message(FATAL_ERROR
        "Generated source does not exist: ${GENERATED_SOURCE}")
endif()

if(NOT DEFINED OPCODE_SOURCE)
    message(FATAL_ERROR "OPCODE_SOURCE is required")
endif()

if(NOT EXISTS "${OPCODE_SOURCE}")
    message(FATAL_ERROR
        "Opcode source does not exist: ${OPCODE_SOURCE}")
endif()

add_library(GeneratedCode OBJECT
    "${GENERATED_SOURCE}"
)

add_library(OpcodeGeneratedCode OBJECT
    "${OPCODE_SOURCE}"
)

target_compile_features(GeneratedCode
    PRIVATE
        cxx_std_20
)

target_compile_features(OpcodeGeneratedCode
    PRIVATE
        cxx_std_20
)

if(MSVC)
    target_compile_options(GeneratedCode
        PRIVATE
            /W4
            /permissive-
    )

    target_compile_options(OpcodeGeneratedCode
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(GeneratedCode
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )

    target_compile_options(OpcodeGeneratedCode
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

configure_file(
    RuntimeSmoke.cpp.in
    RuntimeSmoke.cpp
    @ONLY
)

add_executable(GeneratedCodeRuntime
    "${CMAKE_CURRENT_BINARY_DIR}/RuntimeSmoke.cpp"
)

target_compile_features(GeneratedCodeRuntime
    PRIVATE
        cxx_std_20
)

if(MSVC)
    target_compile_options(GeneratedCodeRuntime
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(GeneratedCodeRuntime
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_test(
    NAME GeneratedCodeRuntimeSmoke
    COMMAND GeneratedCodeRuntime
)
