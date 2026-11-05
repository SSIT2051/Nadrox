# ARM Translation Engine Unit Tests
cmake_minimum_required(VERSION 3.16)
project(arm_translation_tests LANGUAGES C)

# Add unit tests executable
add_executable(test_arm_translation
    unit_tests.c
)

target_include_directories(test_arm_translation PRIVATE
    ${CMAKE_SOURCE_DIR}/src/translation
    ${CMAKE_SOURCE_DIR}/src/translation/mir
    ${CMAKE_SOURCE_DIR}/src/translation/mir/arm_decode
    ${CMAKE_SOURCE_DIR}/src/translation/runtime
    ${CMAKE_SOURCE_DIR}/src/translation/syscall
    ${CMAKE_SOURCE_DIR}/src/translation/jni
    ${CMAKE_SOURCE_DIR}/src/translation/unwind
)

target_link_libraries(test_arm_translation PRIVATE
    atl::translation
)

# Add test target
add_test(NAME arm_translation_unit_tests COMMAND test_arm_translation)

# Also add the integration test runner
add_executable(test_runner
    test_runner.c
)

target_include_directories(test_runner PRIVATE
    ${CMAKE_SOURCE_DIR}/src/translation
)

target_link_libraries(test_runner PRIVATE
    atl::translation
    dl
)