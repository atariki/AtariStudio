function(require_result actual expected scenario)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(
            FATAL_ERROR
            "${scenario}: expected exit code ${expected}, got ${actual}"
        )
    endif()
endfunction()

function(require_contains text fragment scenario)
    string(
        FIND
        "${text}"
        "${fragment}"
        position
    )

    if(position EQUAL -1)
        message(
            FATAL_ERROR
            "${scenario}: missing output fragment '${fragment}'\n${text}"
        )
    endif()
endfunction()

function(require_not_contains text fragment scenario)
    string(
        FIND
        "${text}"
        "${fragment}"
        position
    )

    if(NOT position EQUAL -1)
        message(
            FATAL_ERROR
            "${scenario}: unexpected output fragment '${fragment}'\n${text}"
        )
    endif()
endfunction()

if(NOT DEFINED TEST_APP OR
   NOT DEFINED INPUT_XEX OR
   NOT DEFINED OUTPUT_CPP OR
   NOT DEFINED MISSING_XEX OR
   NOT DEFINED OUTPUT_DIRECTORY)
    message(FATAL_ERROR "CLI test paths were not configured")
endif()

file(
    MAKE_DIRECTORY
    "${OUTPUT_DIRECTORY}"
)

file(REMOVE "${OUTPUT_CPP}")

execute_process(
    COMMAND
        "${TEST_APP}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

require_result("${result}" "2" "no arguments")
require_contains("${error}" "Usage:" "no arguments")

execute_process(
    COMMAND
        "${TEST_APP}"
        --help
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

require_result("${result}" "0" "help")
require_contains("${output}" "TestApp --help" "help")

execute_process(
    COMMAND
        "${TEST_APP}"
        "${MISSING_XEX}"
        --unknown-option
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

require_result("${result}" "2" "unknown option")
require_contains("${error}" "Unknown option:" "unknown option")
require_not_contains("${error}" "XEX load failed" "unknown option")

execute_process(
    COMMAND
        "${TEST_APP}"
        "${MISSING_XEX}"
        --emit-cpp-only
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

require_result("${result}" "2" "missing output filename")
require_contains(
    "${error}"
    "Missing output filename"
    "missing output filename"
)
require_not_contains(
    "${error}"
    "XEX load failed"
    "missing output filename"
)

execute_process(
    COMMAND
        "${TEST_APP}"
        "${INPUT_XEX}"
        --emit-cpp-only
        "${OUTPUT_CPP}"
        extra
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

require_result("${result}" "2" "extra argument")
require_contains("${error}" "Unexpected extra argument:" "extra argument")

if(EXISTS "${OUTPUT_CPP}")
    message(
        FATAL_ERROR
        "extra argument: output file must not be created"
    )
endif()

file(
    SHA256
    "${INPUT_XEX}"
    input_hash_before
)

execute_process(
    COMMAND
        "${TEST_APP}"
        "${INPUT_XEX}"
        --emit-cpp-only
        "${INPUT_XEX}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

require_result("${result}" "1" "input/output collision")
require_contains(
    "${error}"
    "refer to the same file"
    "input/output collision"
)

file(
    SHA256
    "${INPUT_XEX}"
    input_hash_after
)

if(NOT input_hash_before STREQUAL input_hash_after)
    message(
        FATAL_ERROR
        "input/output collision: input XEX was modified"
    )
endif()

execute_process(
    COMMAND
        "${TEST_APP}"
        "${MISSING_XEX}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

require_result("${result}" "1" "missing input")
require_contains("${error}" "XEX load failed." "missing input")

execute_process(
    COMMAND
        "${TEST_APP}"
        "${INPUT_XEX}"
        --emit-cpp-only
        "${OUTPUT_DIRECTORY}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

require_result("${result}" "1" "unwritable output")
require_contains(
    "${error}"
    "Cannot create translation unit:"
    "unwritable output"
)

execute_process(
    COMMAND
        "${TEST_APP}"
        "${INPUT_XEX}"
        --emit-cpp-only
        "${OUTPUT_CPP}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

require_result("${result}" "0" "C++ emission")
require_contains(
    "${output}"
    "Generated C++ translation unit:"
    "C++ emission"
)

if(NOT EXISTS "${OUTPUT_CPP}")
    message(FATAL_ERROR "C++ emission: output file was not created")
endif()

file(
    READ
    "${OUTPUT_CPP}"
    generated_source
    LIMIT
        65536
)

require_contains(
    "${generated_source}"
    "void initialize_image6502()"
    "C++ emission"
)

set(
    unicode_directory
    "${OUTPUT_DIRECTORY}/path with spaces/日本語"
)

set(
    unicode_input_xex
    "${unicode_directory}/入力 image.xex"
)

set(
    unicode_output_cpp
    "${unicode_directory}/出力 translation.cpp"
)

file(
    REMOVE_RECURSE
    "${unicode_directory}"
)

file(
    MAKE_DIRECTORY
    "${unicode_directory}"
)

configure_file(
    "${INPUT_XEX}"
    "${unicode_input_xex}"
    COPYONLY
)

execute_process(
    COMMAND
        "${TEST_APP}"
        "${unicode_input_xex}"
        --emit-cpp-only
        "${unicode_output_cpp}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

require_result(
    "${result}"
    "0"
    "Unicode and spaced paths"
)

require_contains(
    "${output}"
    "Generated C++ translation unit:"
    "Unicode and spaced paths"
)

if(NOT EXISTS "${unicode_output_cpp}")
    message(
        FATAL_ERROR
        "Unicode and spaced paths: output file was not created"
    )
endif()

file(
    READ
    "${unicode_output_cpp}"
    unicode_generated_source
    LIMIT
        65536
)

require_contains(
    "${unicode_generated_source}"
    "void initialize_image6502()"
    "Unicode and spaced paths"
)

file(
    REMOVE_RECURSE
    "${OUTPUT_DIRECTORY}/path with spaces"
)

file(REMOVE "${OUTPUT_CPP}")
