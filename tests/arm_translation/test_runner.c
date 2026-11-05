#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dlfcn.h>

#include "../../src/translation/atl_arm_translate.h"

int main(int argc, char **argv) {
    printf("ARM Translation Test Runner\n");
    printf("============================\n\n");
    
    if (argc < 2) {
        printf("Usage: %s <arm_library.so>\n", argv[0]);
        return 1;
    }
    
    const char *lib_path = argv[1];
    printf("Testing library: %s\n", lib_path);
    
    /* Initialize translation engine */
    atl_translate_config_t config = {
        .enable_tier1_jit = true,
        .enable_tier2_traces = true,
        .enable_game_opts = true,
        .tier1_max_mb = 256,
        .tier2_max_mb = 512,
        .enable_disk_cache = false,
        .enable_profiling = false,
        .cache_dir = "/tmp/atl_cache"
    };
    
    if (!atl_arm_translate_init(&config)) {
        printf("ERROR: Failed to initialize ARM translation engine\n");
        return 1;
    }
    printf("Translation engine initialized\n");
    
    uintptr_t translated_base = 0;
    size_t translated_size = 0;
    
    /* Translate the ARM library */
    printf("Translating library...\n");
    atl_translation_handle_t *handle = atl_arm_translate_library(
        lib_path,
        0,
        &translated_base,
        &translated_size
    );
    
    if (!handle || !handle->is_translated) {
        printf("ERROR: Translation failed\n");
        atl_arm_translate_shutdown();
        return 1;
    }
    
    printf("Translation successful!\n");
    printf("  Original base: %p (size: %zu bytes)\n", 
           handle->original_base, handle->original_size);
    printf("  Translated base: %p (size: %zu bytes)\n", 
           (void*)handle->translated_base, handle->translated_size);
    printf("  Library hash: 0x%lx\n", handle->library_hash);
    
    /* Get statistics */
    uint64_t total_translated, total_code_bytes, cache_hits, cache_misses;
    atl_arm_get_stats(&total_translated, &total_code_bytes, &cache_hits, &cache_misses);
    printf("\nStatistics:\n");
    printf("  Total translated: %lu\n", total_translated);
    printf("  Total code bytes: %lu\n", total_code_bytes);
    printf("  Cache hits: %lu\n", cache_hits);
    printf("  Cache misses: %lu\n", cache_misses);
    
    /* Cleanup */
    atl_arm_translate_shutdown();
    printf("\nTest completed successfully\n");
    
    return 0;
}
