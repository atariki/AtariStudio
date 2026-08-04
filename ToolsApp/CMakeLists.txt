add_executable(TestApp
    main.cpp
)

target_link_libraries(TestApp
    PRIVATE
        AtariStudio
)

target_include_directories(TestApp
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

ataristudio_copy_asan_runtime(TestApp)
