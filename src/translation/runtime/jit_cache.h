#ifndef ATL_JIT_CACHE_H
#define ATL_JIT_CACHE_H

#include "mir.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Three-Tier JIT Cache System
 * ============================================================================
 * Tier 0: Interpreter (no cache)
 * Tier 1: Basic Block JIT (LRU, 64MB default)
 * Tier 2: Optimized Traces (persistent, 256MB default)
 * ============================================================================ */

/* Cache entry */
typedef struct atl_jit_entry {
    void *code;                 /* x86_64 machine code */
    size_t code_size;           /* Size of code */
    uint64_t key;               /* Hash key (function address + tier) */
    uint64_t hash;              /* Full hash for comparison */
    uint32_t tier;              /* 0=interpreter, 1=basic JIT, 2=optimized */
    uint32_t hits;              /* Hit count for hot detection */
    uint64_t last_access;       /* Last access cycle */
    size_t memory_usage;        /* Total memory used by this entry */
    
    /* LRU list */
    struct atl_jit_entry *lru_prev;
    struct atl_jit_entry *lru_next;
    
    /* Hash chain */
    struct atl_jit_entry *hash_next;
} atl_jit_entry_t;

/* Cache statistics */
typedef struct {
    uint64_t total_lookups;
    uint64_t total_hits;
    uint64_t total_misses;
    uint64_t total_evictions;
    uint64_t tier1_size;        /* Current Tier 1 memory usage */
    uint64_t tier2_size;        /* Current Tier 2 memory usage */
    uint64_t peak_memory;       /* Peak memory usage */
    uint64_t tier1_entries;
    uint64_t tier2_entries;
} atl_jit_stats_t;

/* JIT Cache */
typedef struct atl_jit_cache {
    /* Tier 1 cache (LRU, basic blocks) */
    atl_jit_entry_t *tier1_head;
    atl_jit_entry_t *tier1_tail;
    size_t tier1_capacity;      /* Max entries */
    size_t tier1_max_bytes;     /* Max bytes (64MB default) */
    size_t tier1_current_bytes;
    
    /* Tier 2 cache (optimized traces) */
    atl_jit_entry_t *tier2_head;
    atl_jit_entry_t *tier2_tail;
    size_t tier2_capacity;      /* Max entries */
    size_t tier2_max_bytes;     /* Max bytes (256MB default) */
    size_t tier2_current_bytes;
    
    /* Hash table for fast lookup */
    atl_jit_entry_t **hash_table;
    size_t hash_table_size;
    size_t hash_table_count;
    
    /* Statistics */
    atl_jit_stats_t stats;
    
    /* Configuration */
    bool lru_eviction_enabled;
} atl_jit_cache_t;

/* Create/destroy cache */
atl_jit_cache_t *atl_jit_cache_create(size_t tier1_max_mb, size_t tier2_max_mb);
void atl_jit_cache_destroy(atl_jit_cache_t *cache);

/* Lookup compiled code */
atl_jit_entry_t *atl_jit_cache_lookup(atl_jit_cache_t *cache, uint64_t key, uint32_t tier);

/* Insert compiled code */
bool atl_jit_cache_insert(atl_jit_cache_t *cache, uint64_t key, uint32_t tier,
                           void *code, size_t code_size);

/* Invalidate entry (for self-modifying code) */
void atl_jit_cache_invalidate(atl_jit_cache_t *cache, uint64_t key);

/* Invalidate all entries for a library */
void atl_jit_cache_invalidate_library(atl_jit_cache_t *cache, uint64_t library_hash);

/* Get statistics */
void atl_jit_cache_stats(atl_jit_cache_t *cache, atl_jit_stats_t *out);

/* Evict LRU entries to stay under memory limit */
void atl_jit_cache_evict_lru(atl_jit_cache_t *cache, uint32_t tier);

/* Hash function for cache keys */
static inline uint64_t atl_jit_hash_key(uint64_t key) {
    /* Simple split-mix64 hash */
    key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9ULL;
    key = (key ^ (key >> 27)) * 0x94d049bb133111ebULL;
    key = key ^ (key >> 31);
    return key;
}

#ifdef __cplusplus
}
#endif

#endif /* ATL_JIT_CACHE_H */