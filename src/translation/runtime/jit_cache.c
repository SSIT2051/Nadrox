#include "jit_cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * JIT Cache Implementation
 * ============================================================================ */

#define ATL_JIT_HASH_TABLE_SIZE 4096
#define ATL_JIT_DEFAULT_TIER1_MB 64
#define ATL_JIT_DEFAULT_TIER2_MB 256

static uint64_t g_cycle_counter = 0;

static inline uint64_t get_cycle(void) {
    return __builtin_ia32_rdtsc();
}

static inline uint64_t hash_ptr(void *ptr) {
    return (uint64_t)ptr * 0x9E3779B97F4A7C15ULL;
}

atl_jit_cache_t *atl_jit_cache_create(size_t tier1_max_mb, size_t tier2_max_mb)
{
    atl_jit_cache_t *cache = calloc(1, sizeof(atl_jit_cache_t));
    if (!cache) return NULL;

    cache->tier1_max_bytes = (tier1_max_mb ? tier1_max_mb : ATL_JIT_DEFAULT_TIER1_MB) * 1024 * 1024;
    cache->tier2_max_bytes = (tier2_max_mb ? tier2_max_mb : ATL_JIT_DEFAULT_TIER2_MB) * 1024 * 1024;

    cache->hash_table = calloc(ATL_JIT_HASH_TABLE_SIZE, sizeof(atl_jit_entry_t *));
    if (!cache->hash_table) {
        free(cache);
        return NULL;
    }
    cache->hash_table_size = ATL_JIT_HASH_TABLE_SIZE;

    cache->lru_eviction_enabled = true;

    g_cycle_counter = get_cycle();

    return cache;
}

void atl_jit_cache_destroy(atl_jit_cache_t *cache)
{
    if (!cache) return;

    /* Free all entries */
    for (size_t i = 0; i < cache->hash_table_size; i++) {
        atl_jit_entry_t *entry = cache->hash_table[i];
        while (entry) {
            atl_jit_entry_t *next = entry->hash_next;
            if (entry->code) free(entry->code);
            free(entry);
            entry = next;
        }
    }
    free(cache->hash_table);
    free(cache);
}

/* Hash lookup bucket */
static size_t hash_bucket(atl_jit_cache_t *cache, uint64_t key) {
    return atl_jit_hash_key(key) % cache->hash_table_size;
}

/* Lookup entry */
atl_jit_entry_t *atl_jit_cache_lookup(atl_jit_cache_t *cache, uint64_t key, uint32_t tier)
{
    if (!cache) return NULL;

    cache->stats.total_lookups++;

    size_t bucket = hash_bucket(cache, key);
    atl_jit_entry_t *entry = cache->hash_table[bucket];

    while (entry) {
        if (entry->key == key && entry->tier == tier) {
            cache->stats.total_hits++;
            entry->hits++;
            entry->last_access = get_cycle();

            /* Move to front of LRU list */
            if (cache->tier1_head != entry) {
                /* Remove from current position */
                if (entry->lru_prev) entry->lru_prev->lru_next = entry->lru_next;
                if (entry->lru_next) entry->lru_next->lru_prev = entry->lru_prev;
                if (cache->tier1_tail == entry) cache->tier1_tail = entry->lru_prev;

                /* Insert at head */
                entry->lru_prev = NULL;
                entry->lru_next = cache->tier1_head;
                if (cache->tier1_head) cache->tier1_head->lru_prev = entry;
                cache->tier1_head = entry;
                if (!cache->tier1_tail) cache->tier1_tail = entry;
            }

            return entry;
        }
        entry = entry->hash_next;
    }

    cache->stats.total_misses++;
    return NULL;
}

/* Insert entry */
bool atl_jit_cache_insert(atl_jit_cache_t *cache, uint64_t key, uint32_t tier,
                           void *code, size_t code_size)
{
    if (!cache || !code) return false;

    /* Check if already exists */
    atl_jit_entry_t *existing = atl_jit_cache_lookup(cache, key, tier);
    if (existing) {
        /* Update existing entry */
        if (existing->code) free(existing->code);
        existing->code = code;
        existing->code_size = code_size;
        existing->hits = 0;
        existing->last_access = get_cycle();
        return true;
    }

    /* Create new entry */
    atl_jit_entry_t *entry = calloc(1, sizeof(atl_jit_entry_t));
    if (!entry) return false;

    entry->code = code;
    entry->code_size = code_size;
    entry->key = key;
    entry->hash = atl_jit_hash_key(key);
    entry->tier = tier;
    entry->hits = 1;
    entry->last_access = get_cycle();
    entry->memory_usage = sizeof(atl_jit_entry_t) + code_size;

    /* Insert into hash table */
    size_t bucket = hash_bucket(cache, key);
    entry->hash_next = cache->hash_table[bucket];
    cache->hash_table[bucket] = entry;
    cache->hash_table_count++;

    /* Insert into LRU list at head */
    entry->lru_prev = NULL;
    entry->lru_next = cache->tier1_head;
    if (cache->tier1_head) cache->tier1_head->lru_prev = entry;
    cache->tier1_head = entry;
    if (!cache->tier1_tail) cache->tier1_tail = entry;

    /* Update stats */
    if (tier == 1) {
        /* cache->tier1_entries++; */
        cache->tier1_current_bytes += entry->memory_usage;
    } else if (tier == 2) {
        /* cache->tier2_entries++; */
        cache->tier2_current_bytes += entry->memory_usage;
    }

    /* Check memory limits and evict if needed */
    if (cache->lru_eviction_enabled) {
        if (tier == 1 && cache->tier1_current_bytes > cache->tier1_max_bytes) {
            atl_jit_cache_evict_lru(cache, 1);
        } else if (tier == 2 && cache->tier2_current_bytes > cache->tier2_max_bytes) {
            atl_jit_cache_evict_lru(cache, 2);
        }
    }

    return true;
}

/* Evict LRU entries */
void atl_jit_cache_evict_lru(atl_jit_cache_t *cache, uint32_t tier)
{
    if (!cache || !cache->lru_eviction_enabled) return;

    size_t *current_bytes = (tier == 1) ? &cache->tier1_current_bytes : &cache->tier2_current_bytes;
    size_t max_bytes = (tier == 1) ? cache->tier1_max_bytes : cache->tier2_max_bytes;
    size_t *entry_count = NULL; /* (tier == 1) ? &cache->tier1_entries : &cache->tier2_entries; */

    while (*current_bytes > max_bytes && cache->tier1_tail) {
        atl_jit_entry_t *entry = cache->tier1_tail;

        /* Only evict entries of the requested tier */
        if (entry->tier != tier) {
            /* Skip to previous */
            cache->tier1_tail = entry->lru_prev;
            continue;
        }

        /* Remove from LRU */
        if (entry->lru_prev) entry->lru_prev->lru_next = NULL;
        cache->tier1_tail = entry->lru_prev;

        /* Remove from hash table */
        size_t bucket = entry->hash % cache->hash_table_size;
        atl_jit_entry_t **head = &cache->hash_table[bucket];
        atl_jit_entry_t *prev = NULL;
        atl_jit_entry_t *curr = *head;
        while (curr) {
            if (curr == entry) {
                if (prev) prev->hash_next = curr->hash_next;
                else *head = curr->hash_next;
                break;
            }
            prev = curr;
            curr = curr->hash_next;
        }

        /* Free memory */
        if (entry->code) free(entry->code);
        *current_bytes -= entry->memory_usage;
    /* update count */
        cache->stats.total_evictions++;

        free(entry);
    }
}

/* Invalidate entry */
void atl_jit_cache_invalidate(atl_jit_cache_t *cache, uint64_t key)
{
    if (!cache) return;

    size_t bucket = hash_bucket(cache, key);
    atl_jit_entry_t **head = &cache->hash_table[bucket];
    atl_jit_entry_t *prev = NULL;
    atl_jit_entry_t *curr = *head;

    while (curr) {
        if (curr->key == key) {
            /* Remove from hash */
            if (prev) prev->hash_next = curr->hash_next;
            else *head = curr->hash_next;
            cache->hash_table_count--;

            /* Remove from LRU */
            if (curr->lru_prev) curr->lru_prev->lru_next = curr->lru_next;
            if (curr->lru_next) curr->lru_next->lru_prev = curr->lru_prev;
            if (cache->tier1_head == curr) cache->tier1_head = curr->lru_next;
            if (cache->tier1_tail == curr) cache->tier1_tail = curr->lru_prev;

            /* Update stats */
            if (curr->tier == 1) {
                cache->tier1_current_bytes -= curr->memory_usage;
                /* cache->tier1_entries--; */
            } else if (curr->tier == 2) {
                cache->tier2_current_bytes -= curr->memory_usage;
                /* cache->tier2_entries--; */
            }

            if (curr->code) free(curr->code);
            free(curr);
            cache->stats.total_evictions++;
            return;
        }
        prev = curr;
        curr = curr->hash_next;
    }
}

/* Invalidate all entries for a library */
void atl_jit_cache_invalidate_library(atl_jit_cache_t *cache, uint64_t library_hash)
{
    if (!cache) return;

    /* In a full implementation, entries would be tagged with library hash.
     * For now, invalidate all entries (simplest correct approach).
     */
    for (size_t i = 0; i < cache->hash_table_size; i++) {
        atl_jit_entry_t *entry = cache->hash_table[i];
        while (entry) {
            atl_jit_entry_t *next = entry->hash_next;
            if (entry->code) free(entry->code);
            free(entry);
            entry = next;
        }
        cache->hash_table[i] = NULL;
    }
    cache->hash_table_count = 0;
    cache->tier1_head = NULL;
    cache->tier1_tail = NULL;
    cache->tier1_current_bytes = 0;
    cache->tier2_current_bytes = 0;
}

/* Get statistics */
void atl_jit_cache_stats(atl_jit_cache_t *cache, atl_jit_stats_t *out)
{
    if (!cache || !out) return;
    *out = cache->stats;
    out->tier1_size = cache->tier1_current_bytes;
    out->tier2_size = cache->tier2_current_bytes;
    /* Count entries by walking if needed */
    /* For simplicity just return stats values */
    /* Peak memory */
    uint64_t total = cache->tier1_current_bytes + cache->tier2_current_bytes;
    if (total > cache->stats.peak_memory) {
        cache->stats.peak_memory = total;
    }
    out->peak_memory = cache->stats.peak_memory;
}
