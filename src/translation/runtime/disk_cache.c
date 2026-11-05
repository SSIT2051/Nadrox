#include "disk_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#define MAX_ENTRIES 65536

typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint64_t max_size;
    uint64_t current_size;
} disk_cache_header_t;

static char g_cache_dir[4096];
static size_t g_max_size = 0;
static int g_fd = -1;
static void *g_map = NULL;
static size_t g_map_size = 0;
static disk_cache_header_t *g_hdr = NULL;
static atl_disk_cache_entry_t *g_index = NULL;
static atl_disk_cache_key_t *g_keys = NULL;
static uint64_t g_hits = 0, g_misses = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_flush_thread;
static volatile int g_running = 0;

static uint64_t key_hash(const atl_disk_cache_key_t *k) {
    uint64_t h = k->library_hash;
    h ^= (uint64_t)k->tier * 0x9e3779b97f4a7c15ULL;
    h ^= (uint64_t)k->isa_level * 0xc2b2ae3d27d4eb4fULL;
    h ^= (uint64_t)k->engine_version * 0x1505ULL;
    h ^= k->game_id * 0x5bd1e995ULL;
    return h;
}

static int find_entry(const atl_disk_cache_key_t *key) {
    uint64_t kh = key_hash(key);
    for (uint32_t i = 0; i < g_hdr->entry_count; i++) {
        if (key_hash(&g_keys[i]) == kh &&
            memcmp(&g_keys[i], key, sizeof(*key)) == 0) return (int)i;
    }
    return -1;
}

static void *flush_thread_fn(void *arg) {
    (void)arg;
    while (g_running) {
        usleep(5000000);
        if (g_map) msync(g_map, g_map_size, MS_SYNC);
    }
    return NULL;
}

int atl_disk_cache_init(const char *cache_dir, size_t max_size_mb) {
    snprintf(g_cache_dir, sizeof(g_cache_dir), "%s", cache_dir);
    g_max_size = max_size_mb * 1024ULL * 1024ULL;
    if (g_max_size == 0) g_max_size = 1536ULL * 1024 * 1024;
    mkdir(cache_dir, 0755);
    char path[4096];
    snprintf(path, sizeof(path), "%s/disk_cache.bin", cache_dir);
    g_fd = open(path, O_RDWR | O_CREAT, 0644);
    if (g_fd < 0) return -1;
    size_t init_size = 64 * 1024 * 1024;
    struct stat st;
    fstat(g_fd, &st);
    if (st.st_size < (off_t)init_size) ftruncate(g_fd, init_size);
    g_map_size = init_size;
    g_map = mmap(NULL, g_map_size, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);
    if (g_map == MAP_FAILED) { close(g_fd); g_fd=-1; return -1; }
    g_hdr = (disk_cache_header_t *)g_map;
    if (g_hdr->magic != ATL_DISK_CACHE_MAGIC) {
        memset(g_map, 0, g_map_size);
        g_hdr->magic = ATL_DISK_CACHE_MAGIC;
        g_hdr->version = 1;
        g_hdr->entry_count = 0;
        g_hdr->max_size = g_max_size;
        g_hdr->current_size = 0;
    }
    g_index = (atl_disk_cache_entry_t *)((char *)g_map + sizeof(disk_cache_header_t));
    g_keys = (atl_disk_cache_key_t *)((char *)g_map + sizeof(disk_cache_header_t) + MAX_ENTRIES * sizeof(atl_disk_cache_entry_t));
    g_running = 1;
    pthread_create(&g_flush_thread, NULL, flush_thread_fn, NULL);
    return 0;
}

int atl_disk_cache_lookup(const atl_disk_cache_key_t *key, void **out_code, size_t *out_size) {
    pthread_mutex_lock(&g_lock);
    int idx = find_entry(key);
    if (idx < 0) { g_misses++; pthread_mutex_unlock(&g_lock); return -1; }
    g_hits++;
    atl_disk_cache_entry_t *e = &g_index[idx];
    e->last_access = (uint64_t)time(NULL);
    void *code = malloc(e->code_size);
    memcpy(code, (char *)g_map + e->code_offset, e->code_size);
    *out_code = code;
    *out_size = e->code_size;
    pthread_mutex_unlock(&g_lock);
    return 0;
}

int atl_disk_cache_store(const atl_disk_cache_key_t *key, const void *code, size_t size, const void *metadata, size_t metadata_len) {
    pthread_mutex_lock(&g_lock);
    /* evict if needed */
    while (g_hdr->current_size + size > g_max_size && g_hdr->entry_count > 0) {
        /* evict oldest */
        uint32_t oldest = 0; uint64_t oldest_t = g_index[0].last_access;
        for (uint32_t i=1;i<g_hdr->entry_count;i++) if (g_index[i].last_access < oldest_t){oldest_t=g_index[i].last_access;oldest=i;}
        /* shift */
        memmove(&g_index[oldest], &g_index[oldest+1], (g_hdr->entry_count-oldest-1)*sizeof(atl_disk_cache_entry_t));
        memmove(&g_keys[oldest], &g_keys[oldest+1], (g_hdr->entry_count-oldest-1)*sizeof(atl_disk_cache_key_t));
        g_hdr->entry_count--;
        g_hdr->current_size -= g_index[oldest].code_size; /* approximate */
    }
    if (g_hdr->entry_count >= MAX_ENTRIES) { pthread_mutex_unlock(&g_lock); return -1; }
    uint32_t off = sizeof(disk_cache_header_t) + MAX_ENTRIES*sizeof(atl_disk_cache_entry_t) + MAX_ENTRIES*sizeof(atl_disk_cache_key_t);
    /* simple: append at current_size offset region after header area; reuse offset tracking via current_size added to base data area */
    size_t data_base = sizeof(disk_cache_header_t) + MAX_ENTRIES*(sizeof(atl_disk_cache_entry_t)+sizeof(atl_disk_cache_key_t));
    size_t offset = data_base + g_hdr->current_size;
    if (offset + size > g_map_size) { pthread_mutex_unlock(&g_lock); return -1; }
    memcpy((char *)g_map + offset, code, size);
    uint32_t idx = g_hdr->entry_count++;
    g_keys[idx] = *key;
    g_index[idx].code_offset = (uint32_t)offset;
    g_index[idx].code_size = (uint32_t)size;
    g_index[idx].compressed_size = (uint32_t)size;
    g_index[idx].compression = 0;
    g_index[idx].last_access = (uint64_t)time(NULL);
    g_index[idx].metadata_len = (uint32_t)metadata_len;
    g_hdr->current_size += size;
    (void)metadata;
    pthread_mutex_unlock(&g_lock);
    return 0;
}

void atl_disk_cache_invalidate(const atl_disk_cache_key_t *key) {
    pthread_mutex_lock(&g_lock);
    int idx = find_entry(key);
    if (idx >= 0) {
        memmove(&g_index[idx], &g_index[idx+1], (g_hdr->entry_count-idx-1)*sizeof(atl_disk_cache_entry_t));
        memmove(&g_keys[idx], &g_keys[idx+1], (g_hdr->entry_count-idx-1)*sizeof(atl_disk_cache_key_t));
        g_hdr->entry_count--;
    }
    pthread_mutex_unlock(&g_lock);
}

void atl_disk_cache_invalidate_library(uint64_t library_hash) {
    pthread_mutex_lock(&g_lock);
    uint32_t i = 0;
    while (i < g_hdr->entry_count) {
        if (g_keys[i].library_hash == library_hash) {
            memmove(&g_index[i], &g_index[i+1], (g_hdr->entry_count-i-1)*sizeof(atl_disk_cache_entry_t));
            memmove(&g_keys[i], &g_keys[i+1], (g_hdr->entry_count-i-1)*sizeof(atl_disk_cache_key_t));
            g_hdr->entry_count--;
        } else i++;
    }
    pthread_mutex_unlock(&g_lock);
}

void atl_disk_cache_stats(uint64_t *out_hits, uint64_t *out_misses, uint64_t *out_size) {
    if (out_hits) *out_hits = g_hits;
    if (out_misses) *out_misses = g_misses;
    if (out_size) *out_size = g_hdr ? g_hdr->current_size : 0;
}

void atl_disk_cache_shutdown(void) {
    g_running = 0;
    pthread_join(g_flush_thread, NULL);
    if (g_map) msync(g_map, g_map_size, MS_SYNC);
    if (g_map) munmap(g_map, g_map_size);
    if (g_fd >= 0) close(g_fd);
}
