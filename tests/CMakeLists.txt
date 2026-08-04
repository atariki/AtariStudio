add_executable(AtariStudioTests
    StructuredPipelineTests.cpp
)

target_link_libraries(AtariStudioTests
    PRIVATE
        AtariStudio
)

ataristudio_copy_asan_runtime(AtariStudioTests)

add_test(
    NAME StructuredPipeline
    COMMAND AtariStudioTests
)

add_executable(BinaryReaderTests
    BinaryReaderTests.cpp
)

target_link_libraries(BinaryReaderTests
    PRIVATE
        AtariStudio
)

ataristudio_copy_asan_runtime(BinaryReaderTests)

add_test(
    NAME BinaryReader
    COMMAND BinaryReaderTests
)

add_executable(CpuTests
    CpuTests.cpp
)

target_link_libraries(CpuTests
    PRIVATE
        AtariStudio
)

ataristudio_copy_asan_runtime(CpuTests)

add_test(
    NAME Cpu6502
    COMMAND CpuTests
)

add_executable(MemoryTests
    MemoryTests.cpp
)

target_link_libraries(MemoryTests
    PRIVATE
        AtariStudio
)

ataristudio_copy_asan_runtime(MemoryTests)

add_test(
    NAME Memory
    COMMAND MemoryTests
)

add_executable(XexLoaderTests
    XexLoaderTests.cpp
)

target_link_libraries(XexLoaderTests
    PRIVATE
        AtariStudio
)

ataristudio_copy_asan_runtime(XexLoaderTests)

add_test(
    NAME XexLoader
    COMMAND XexLoaderTests
)

add_executable(CoreModelTests
    CoreModelTests.cpp
)

target_link_libraries(CoreModelTests
    PRIVATE
        AtariStudio
)

ataristudio_copy_asan_runtime(CoreModelTests)

add_test(
    NAME CoreModels
    COMMAND CoreModelTests
)

add_executable(AnalysisRepeatabilityTests
    AnalysisRepeatabilityTests.cpp
)

target_link_libraries(AnalysisRepeatabilityTests
    PRIVATE
        AtariStudio
)

ataristudio_copy_asan_runtime(
    AnalysisRepeatabilityTests
)

add_test(
    NAME AnalysisRepeatability
    COMMAND
        AnalysisRepeatabilityTests
        "${PROJECT_SOURCE_DIR}/hello.xex"
)

add_executable(AnalysisRobustnessTests
    AnalysisRobustnessTests.cpp
)

target_link_libraries(AnalysisRobustnessTests
    PRIVATE
        AtariStudio
)

ataristudio_copy_asan_runtime(
    AnalysisRobustnessTests
)

add_test(
    NAME AnalysisRobustness
    COMMAND AnalysisRobustnessTests
)

file(
    GLOB_RECURSE
    ATARISTUDIO_PUBLIC_HEADERS
    CONFIGURE_DEPENDS
    RELATIVE
        "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include/AtariStudio/*.h"
)

set(ATARISTUDIO_HEADER_TEST_SOURCES)

foreach(public_header IN LISTS ATARISTUDIO_PUBLIC_HEADERS)
    string(
        REPLACE
        "/"
        "_"
        header_source_name
        "${public_header}"
    )

    set(
        header_source
        "${CMAKE_CURRENT_BINARY_DIR}/header-self-containment/${header_source_name}.cpp"
    )

    file(
        GENERATE
        OUTPUT
            "${header_source}"
        CONTENT
            "#include <${public_header}>\n"
    )

    list(
        APPEND
        ATARISTUDIO_HEADER_TEST_SOURCES
        "${header_source}"
    )
endforeach()

add_library(
    HeaderSelfContainment
    OBJECT
        ${ATARISTUDIO_HEADER_TEST_SOURCES}
)

target_link_libraries(
    HeaderSelfContainment
    PRIVATE
        AtariStudio
)

add_executable(
    HeaderSelfContainmentTests
    HeaderSelfContainmentTests.cpp
    $<TARGET_OBJECTS:HeaderSelfContainment>
)

target_link_libraries(
    HeaderSelfContainmentTests
    PRIVATE
        AtariStudio
)

ataristudio_copy_asan_runtime(
    HeaderSelfContainmentTests
)

add_test(
    NAME HeaderSelfContainment
    COMMAND HeaderSelfContainmentTests
)

set(
    TEST_ARTIFACT_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/artifacts/$<CONFIG>"
)

add_test(
    NAME TestArtifactDirectory
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        make_directory
        "${TEST_ARTIFACT_DIRECTORY}"
)

set_tests_properties(
    TestArtifactDirectory
    PROPERTIES
        FIXTURES_SETUP TestArtifacts
)

set(
    TEST_TEMPORARY_ENVIRONMENT
    "TMP=${TEST_ARTIFACT_DIRECTORY}"
    "TEMP=${TEST_ARTIFACT_DIRECTORY}"
    "TMPDIR=${TEST_ARTIFACT_DIRECTORY}"
)

set_tests_properties(
    StructuredPipeline
    BinaryReader
    XexLoader
    PROPERTIES
        FIXTURES_REQUIRED TestArtifacts
        ENVIRONMENT "${TEST_TEMPORARY_ENVIRONMENT}"
)

add_test(
    NAME CliBehavior
    COMMAND
        "${CMAKE_COMMAND}"
        "-DTEST_APP=$<TARGET_FILE:TestApp>"
        "-DINPUT_XEX=${PROJECT_SOURCE_DIR}/hello.xex"
        "-DOUTPUT_CPP=${TEST_ARTIFACT_DIRECTORY}/cli.generated.cpp"
        "-DMISSING_XEX=${TEST_ARTIFACT_DIRECTORY}/missing.xex"
        "-DOUTPUT_DIRECTORY=${TEST_ARTIFACT_DIRECTORY}"
        -P
        "${CMAKE_CURRENT_SOURCE_DIR}/CliBehavior.cmake"
)

set_tests_properties(
    CliBehavior
    PROPERTIES
        FIXTURES_REQUIRED TestArtifacts
)

add_executable(OpcodeTranslationEmitter
    OpcodeTranslationEmitter.cpp
)

target_link_libraries(OpcodeTranslationEmitter
    PRIVATE
        AtariStudio
)

ataristudio_copy_asan_runtime(OpcodeTranslationEmitter)

set(GENERATED_CODE_SOURCE
    "${TEST_ARTIFACT_DIRECTORY}/hello.generated.cpp"
)

set(GENERATED_CODE_BUILD_DIR
    "${TEST_ARTIFACT_DIRECTORY}/generated-code-compile"
)

set(OPCODE_CODE_SOURCE
    "${TEST_ARTIFACT_DIRECTORY}/opcode-matrix.generated.cpp"
)

set(
    NESTED_CMAKE_GENERATOR_ARGUMENTS
    -G
    "${CMAKE_GENERATOR}"
)

if(CMAKE_GENERATOR_PLATFORM)
    list(
        APPEND
        NESTED_CMAKE_GENERATOR_ARGUMENTS
        -A
        "${CMAKE_GENERATOR_PLATFORM}"
    )
endif()

if(CMAKE_GENERATOR_TOOLSET)
    list(
        APPEND
        NESTED_CMAKE_GENERATOR_ARGUMENTS
        -T
        "${CMAKE_GENERATOR_TOOLSET}"
    )
endif()

if(CMAKE_GENERATOR_INSTANCE)
    list(
        APPEND
        NESTED_CMAKE_GENERATOR_ARGUMENTS
        "-DCMAKE_GENERATOR_INSTANCE=${CMAKE_GENERATOR_INSTANCE}"
    )
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND
   CMAKE_BUILD_TYPE)
    list(
        APPEND
        NESTED_CMAKE_GENERATOR_ARGUMENTS
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    )
endif()

add_test(
    NAME GeneratedCodeEmission
    COMMAND
        $<TARGET_FILE:TestApp>
        "${PROJECT_SOURCE_DIR}/hello.xex"
        --emit-cpp-only
        "${GENERATED_CODE_SOURCE}"
)

set_tests_properties(
    GeneratedCodeEmission
    PROPERTIES
        FIXTURES_REQUIRED TestArtifacts
        FIXTURES_SETUP GeneratedCodeSource
)

add_test(
    NAME OpcodeCodeEmission
    COMMAND
        $<TARGET_FILE:OpcodeTranslationEmitter>
        "${OPCODE_CODE_SOURCE}"
)

set_tests_properties(
    OpcodeCodeEmission
    PROPERTIES
        FIXTURES_REQUIRED TestArtifacts
        FIXTURES_SETUP OpcodeCodeSource
)

add_test(
    NAME GeneratedCodeBuildDirectoryReset
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        rm
        -rf
        "${GENERATED_CODE_BUILD_DIR}"
)

set_tests_properties(
    GeneratedCodeBuildDirectoryReset
    PROPERTIES
        FIXTURES_REQUIRED
            "GeneratedCodeSource;OpcodeCodeSource"
        FIXTURES_SETUP GeneratedCodeBuildDirectory
)

add_test(
    NAME GeneratedCodeConfigure
    COMMAND
        "${CMAKE_COMMAND}"
        -S
        "${CMAKE_CURRENT_SOURCE_DIR}/GeneratedCodeCompile"
        -B
        "${GENERATED_CODE_BUILD_DIR}"
        ${NESTED_CMAKE_GENERATOR_ARGUMENTS}
        "-DGENERATED_SOURCE=${GENERATED_CODE_SOURCE}"
        "-DOPCODE_SOURCE=${OPCODE_CODE_SOURCE}"
)

set_tests_properties(
    GeneratedCodeConfigure
    PROPERTIES
        FIXTURES_REQUIRED GeneratedCodeBuildDirectory
        FIXTURES_SETUP GeneratedCodeBuild
)

add_test(
    NAME GeneratedCodeCompile
    COMMAND
        "${CMAKE_COMMAND}"
        --build
        "${GENERATED_CODE_BUILD_DIR}"
        --config
        "$<CONFIG>"
)

set_tests_properties(
    GeneratedCodeCompile
    PROPERTIES
        FIXTURES_REQUIRED GeneratedCodeBuild
        FIXTURES_SETUP GeneratedCodeExecutable
)

add_test(
    NAME GeneratedCodeRuntime
    COMMAND
        "${CMAKE_CTEST_COMMAND}"
        --test-dir
        "${GENERATED_CODE_BUILD_DIR}"
        -C
        "$<CONFIG>"
        --output-on-failure
)

set_tests_properties(
    GeneratedCodeRuntime
    PROPERTIES
        FIXTURES_REQUIRED GeneratedCodeExecutable
)

set(
    EMBEDDED_CONSUMER_BUILD_DIR
    "${TEST_ARTIFACT_DIRECTORY}/embedded-consumer"
)

add_test(
    NAME EmbeddedConsumerBuildDirectoryReset
    COMMAND
        "${CMAKE_COMMAND}"
        -E
        rm
        -rf
        "${EMBEDDED_CONSUMER_BUILD_DIR}"
)

set_tests_properties(
    EmbeddedConsumerBuildDirectoryReset
    PROPERTIES
        FIXTURES_REQUIRED TestArtifacts
        FIXTURES_SETUP EmbeddedConsumerBuildDirectory
)

add_test(
    NAME EmbeddedConsumerConfigure
    COMMAND
        "${CMAKE_COMMAND}"
        -S
        "${CMAKE_CURRENT_SOURCE_DIR}/EmbeddedConsumer"
        -B
        "${EMBEDDED_CONSUMER_BUILD_DIR}"
        ${NESTED_CMAKE_GENERATOR_ARGUMENTS}
        "-DATARISTUDIO_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DATARISTUDIO_ENABLE_ASAN=${ATARISTUDIO_ENABLE_ASAN}"
)

set_tests_properties(
    EmbeddedConsumerConfigure
    PROPERTIES
        FIXTURES_REQUIRED EmbeddedConsumerBuildDirectory
        FIXTURES_SETUP EmbeddedConsumerBuild
)

add_test(
    NAME EmbeddedConsumerCompile
    COMMAND
        "${CMAKE_COMMAND}"
        --build
        "${EMBEDDED_CONSUMER_BUILD_DIR}"
        --config
        "$<CONFIG>"
        --target
        AtariStudioEmbeddedConsumer
)

set_tests_properties(
    EmbeddedConsumerCompile
    PROPERTIES
        FIXTURES_REQUIRED EmbeddedConsumerBuild
        FIXTURES_SETUP EmbeddedConsumerExecutable
)

add_test(
    NAME EmbeddedConsumerRuntime
    COMMAND
        "${CMAKE_CTEST_COMMAND}"
        --test-dir
        "${EMBEDDED_CONSUMER_BUILD_DIR}"
        -C
        "$<CONFIG>"
        --output-on-failure
)

set_tests_properties(
    EmbeddedConsumerRuntime
    PROPERTIES
        FIXTURES_REQUIRED EmbeddedConsumerExecutable
)
