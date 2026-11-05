# =============================================================================
# ATL Translation Engine - Custom ARMv4/v7 -> x86_64 Translation
# =============================================================================

add_library(atl_translation STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/atl_arm_translate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mir/mir_builder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mir/mir_verify.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mir/arm_decode/arm_decode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/jit_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime/disk_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mir/lowering/asm_emit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mir/lowering/reg_alloc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mir/lowering/lower_x86_64.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syscall/syscall_table.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syscall/syscall_emu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/syscall/signal_emu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/thunk_gen.c
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/jni_table.c
    ${CMAKE_CURRENT_SOURCE_DIR}/jni/jni_mir.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unwind/ehabi_decode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unwind/dwarf_emit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/unwind/personality.c
    ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_loader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_reloc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_dynlink.c
    ${CMAKE_CURRENT_SOURCE_DIR}/elf/elf_jit.c
)

target_include_directories(atl_translation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/mir
    ${CMAKE_CURRENT_SOURCE_DIR}/mir/arm_decode
    ${CMAKE_CURRENT_SOURCE_DIR}/mir/lowering
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/syscall
    ${CMAKE_CURRENT_SOURCE_DIR}/jni
    ${CMAKE_CURRENT_SOURCE_DIR}/unwind
    ${CMAKE_CURRENT_SOURCE_DIR}/elf
)

target_compile_definitions(atl_translation PRIVATE
    ATL_TRANSLATION_ENABLED
)

# Link dependencies - translation engine depends on bionic linker types
target_link_libraries(atl_translation PRIVATE
    bionic::dl_bio
    bionic::c_bio
    pthread
)

# Make available globally
add_library(atl::translation ALIAS atl_translation)

# Export for dependent targets
install(TARGETS atl_translation EXPORT atl-targets
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)
export(TARGETS atl_translation FILE "${CMAKE_CURRENT_BINARY_DIR}/atl_translation-targets.cmake" NAMESPACE atl::)

message(STATUS "ATL: Custom ARM translation engine enabled")
