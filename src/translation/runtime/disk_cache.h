#ifndef ATL_DISK_CACHE_H
#define ATL_DISK_CACHE_H

#include "jit_cache.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ATL_TRANSLATION_VERSION
#define ATL_TRANSLATION_VERSION 1
#endif

#define ATL_DISK_CACHE_MAGIC 0x41544C4443414348ULL /* "ATLDCACH" */

typedef struct {
    uint64_t library_hash;
    uint32_t tier;
    uint32_t isa_level;      /* 4 or 7 */
    uint32_t engine_version;
    uint64_t game_id;
} atl_disk_cache_key_t;

typedef struct {
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t compressed_size;
    uint8_t  compression;    /* 0 none, 1 lz4, 2 zstd */
    uint64_t last_access;
    uint32_t metadata_len;
} atl_disk_cache_entry_t;

/* Global stats reuse jit stats shape via wrapper */
typedef struct {
    uint64_t hits;
    uint64_t misses;
    uint64_t size_bytes;
    uint64_t entries;
} atl_disk_cache_stats_t;

int  atl_disk_cache_init(const char *cache_dir, size_t max_size_mb);
int  atl_disk_cache_lookup(const atl_disk_cache_key_t *key, void **out_code, size_t *out_size);
int  atl_disk_cache_store(const atl_disk_cache_key_t *key, const void *code, size_t size, const void *metadata, size_t metadata_len);
void atl_disk_cache_invalidate(const atl_disk_cache_key_t *key);
void atl_disk_cache_invalidate_library(uint64_t library_hash);
void atl_disk_cache_stats(uint64_t *out_hits, uint64_t *out_misses, uint64_t *out_size);
void atl_disk_cache_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* ATL_DISK_CACHE_H */
