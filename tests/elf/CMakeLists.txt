# ELF loader/dynlink unit tests

add_executable(test_elf_loader
    test_elf_loader.c
)

target_include_directories(test_elf_loader PRIVATE
    ${CMAKE_SOURCE_DIR}/src/translation
    ${CMAKE_SOURCE_DIR}/src/translation/elf
    ${CMAKE_SOURCE_DIR}/src/translation/mir
    ${CMAKE_SOURCE_DIR}/src/translation/mir/arm_decode
    ${CMAKE_SOURCE_DIR}/src/translation/mir/lowering
    ${CMAKE_SOURCE_DIR}/src/translation/runtime
    ${CMAKE_SOURCE_DIR}/src/translation/syscall
    ${CMAKE_SOURCE_DIR}/src/translation/jni
    ${CMAKE_SOURCE_DIR}/src/translation/unwind
)

target_link_libraries(test_elf_loader PRIVATE
    atl::translation
    pthread
    dl
)

add_test(NAME elf_loader_unit_tests COMMAND test_elf_loader)

set_tests_properties(elf_loader_unit_tests PROPERTIES
    ENVIRONMENT "ATL_ELF_TEST_FIXTURE_DIR=/tmp"
    TIMEOUT 120
)
