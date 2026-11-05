#ifndef ATL_ARM_TRANSLATE_H
#define ATL_ARM_TRANSLATE_H

#include "mir.h"
#include "jit_cache.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <elf.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * ATL ARM Translation Engine - Main Entry Point
 * ============================================================================
 * Hooks into the bionic linker at FLAG_ARM_LIBRARY to translate ARM
 * libraries to x86_64 using the custom MIR-based translation engine.
 * ============================================================================ */

/* Translation result handle */
typedef struct atl_translation_handle {
    void *translated_base;    /* x86_64 base address */
    size_t translated_size;   /* x86_64 code size */
    void *original_base;      /* Original ARM base address */
    size_t original_size;     /* Original ARM code size */
    uint64_t library_hash;    /* Hash of original ARM library */
    atl_jit_cache_t *jit_cache; /* JIT cache for this library */
    bool is_translated;       /* Whether translation succeeded */
} atl_translation_handle_t;

/* Translation configuration */
typedef struct atl_translate_config {
    bool enable_tier1_jit;          /* Enable Tier 1 basic block JIT */
    bool enable_tier2_traces;       /* Enable Tier 2 optimized traces */
    bool enable_game_opts;          /* Enable game-specific optimizations */
    size_t tier1_max_mb;            /* Tier 1 cache max size in MB */
    size_t tier2_max_mb;            /* Tier 2 cache max size in MB */
    bool enable_disk_cache;         /* Enable persistent disk cache */
    bool enable_profiling;          /* Enable profiling */
    const char *cache_dir;          /* Disk cache directory */
} atl_translate_config_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/* Initialize translation engine */
bool atl_arm_translate_init(const atl_translate_config_t *config);

/* Shutdown translation engine */
void atl_arm_translate_shutdown(void);

/* Translate an ARM library to x86_64.
 * Called from the linker when FLAG_ARM_LIBRARY is set.
 *
 * Parameters:
 *   library_path - Path to the ARM .so file
 *   preferred_base - Preferred load address (from linker)
 *   out_translated_base - OUT: x86_64 base address
 *   out_translated_size - OUT: x86_64 code size
 *
 * Returns:
 *   Translation handle on success, NULL on failure.
 */
atl_translation_handle_t *atl_arm_translate_library(
    const char *library_path,
    uintptr_t preferred_base,
    uintptr_t *out_translated_base,
    size_t *out_translated_size);

/* Translate a single ARM function (for JIT compilation) */
void *atl_arm_translate_function(
    atl_translation_handle_t *handle,
    uint32_t arm_pc,
    size_t *out_code_size);

/* Invalidate JIT cache for self-modifying code (__clear_cache/cacheflush) */
void atl_arm_invalidate_cache(
    atl_translation_handle_t *handle,
    uintptr_t addr,
    size_t len);

/* Get translation statistics */
void atl_arm_get_stats(uint64_t *total_translated, uint64_t *total_code_bytes,
                        uint64_t *cache_hits, uint64_t *cache_misses);

/* Check if a library is ARM (e_machine == EM_ARM or EM_AARCH64) */
bool atl_arm_is_library_arm(const uint8_t *elf_header, size_t header_size);

/* Compute library hash for cache keying */
uint64_t atl_arm_compute_library_hash(const uint8_t *elf_data, size_t elf_size);

/* ============================================================================
 * Syscall/JNI Integration (forward declarations)
 * ============================================================================ */

/* Get syscall number for ARM syscall */
uint32_t atl_arm_syscall_arm_to_host(uint32_t arm_syscall);

/* Get JNI trampoline for method signature */
void *atl_arm_jni_get_trampoline(const char *method_signature);

/* ============================================================================
 * Global state
 * ============================================================================ */

/* Whether the translation engine is initialized */
extern bool g_atl_arm_initialized;

/* Global JIT cache */
extern atl_jit_cache_t *g_atl_jit_cache;

/* Global configuration */
extern atl_translate_config_t g_atl_translate_config;

#ifdef __cplusplus
}
#endif

#endif /* ATL_ARM_TRANSLATE_H */
/* ============================================================================
 * Exception Unwinding Support
 * ============================================================================ */

/* Register unwind information from ARM library */
void atl_arm_register_unwind_info(void *exidx_base, size_t exidx_count, uint32_t text_base);

