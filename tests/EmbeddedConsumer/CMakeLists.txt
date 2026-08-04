cmake_minimum_required(VERSION 3.21)

project(AtariStudioEmbeddedConsumer LANGUAGES CXX)

if(NOT DEFINED ATARISTUDIO_SOURCE_DIR)
    message(
        FATAL_ERROR
        "ATARISTUDIO_SOURCE_DIR is required"
    )
endif()

set(
    BUILD_TESTING
    OFF
    CACHE BOOL
    "Do not recursively build AtariStudio tests"
    FORCE
)

set(
    ATARISTUDIO_BUILD_TESTS
    OFF
    CACHE BOOL
    "Do not build AtariStudio tests"
    FORCE
)

set(
    ATARISTUDIO_BUILD_TOOLS
    OFF
    CACHE BOOL
    "Do not add AtariStudio tools to the consumer project"
    FORCE
)

add_subdirectory(
    "${ATARISTUDIO_SOURCE_DIR}"
    ataristudio
)

if(TARGET TestApp)
    message(
        FATAL_ERROR
        "AtariStudio must not add TestApp when tools are disabled"
    )
endif()

add_executable(AtariStudioEmbeddedConsumer
    main.cpp
)

target_link_libraries(AtariStudioEmbeddedConsumer
    PRIVATE
        AtariStudio
)

ataristudio_copy_asan_runtime(
    AtariStudioEmbeddedConsumer
)

enable_testing()

add_test(
    NAME EmbeddedConsumerRuntime
    COMMAND AtariStudioEmbeddedConsumer
)
