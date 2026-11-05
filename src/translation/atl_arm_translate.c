#include "atl_arm_translate.h"
#include "mir.h"
#include "arm_decode.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <elf.h>
#include <link.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* Linker callback registration - declared in linker.h */
extern void atl_linker_set_translate_cb(void *(*cb)(const char *, uintptr_t,
                                                     uintptr_t *, size_t *));

/* ============================================================================
 * Global State
 * ============================================================================ */

bool g_atl_arm_initialized = false;
atl_jit_cache_t *g_atl_jit_cache = NULL;
atl_translate_config_t g_atl_translate_config = {
    .enable_tier1_jit = true,
    .enable_tier2_traces = true,
    .enable_game_opts = true,
    .tier1_max_mb = 64,
    .tier2_max_mb = 256,
    .enable_disk_cache = false,
    .enable_profiling = true,
    .cache_dir = NULL,
};

static atl_mir_context_t *g_mir_ctx = NULL;
static atl_mir_module_t *g_mir_module = NULL;

/* Forward declaration for callback wrapper */
static void *atl_arm_translate_library_cb(const char *library_path, uintptr_t preferred_base,
                                           uintptr_t *translated_base, size_t *translated_size);

/* Statistics */
static uint64_t g_stats_total_translated = 0;
static uint64_t g_stats_total_code_bytes = 0;
static uint64_t g_stats_cache_hits = 0;
static uint64_t g_stats_cache_misses = 0;

/* ============================================================================
 * Library Hash Computation
 * ============================================================================ */

/* Simple hash for cache keying */
static uint64_t compute_file_hash(const uint8_t *data, size_t size)
{
    uint64_t hash = 0x9E3779B97F4A7C15ULL;
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 0x100000001B3ULL;
        hash ^= hash >> 32;
    }
    return hash;
}

uint64_t atl_arm_compute_library_hash(const uint8_t *elf_data, size_t elf_size)
{
    /* Include ISA level and engine version in cache key */
    uint64_t hash = compute_file_hash(elf_data, elf_size);
    hash ^= 0xCAFEBABEULL; /* Engine version */
    hash ^= 0x0001ULL;     /* ARMv4/v7 support */
    return hash;
}

/* ============================================================================
 * ARM Library Detection
 * ============================================================================ */

bool atl_arm_is_library_arm(const uint8_t *elf_header, size_t header_size)
{
    if (!elf_header || header_size < sizeof(ElfW(Ehdr))) return false;

    const ElfW(Ehdr) *ehdr = (const ElfW(Ehdr) *)elf_header;

    /* Check ELF magic */
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        return false;
    }

    /* Check for ARM machine types */
    return (ehdr->e_machine == EM_ARM) || (ehdr->e_machine == EM_AARCH64);
}

/* ============================================================================
 * Translation Engine Init/Shutdown
 * ============================================================================ */

bool atl_arm_translate_init(const atl_translate_config_t *config)
{
    if (g_atl_arm_initialized) return true;

    if (config) {
        g_atl_translate_config = *config;
    }

    /* Create MIR context */
    g_mir_ctx = atl_mir_context_create();
    if (!g_mir_ctx) return false;

    atl_mir_context_set_verify_ssa(g_mir_ctx, true);
    atl_mir_context_set_opt_level(g_mir_ctx, 1);
    atl_mir_context_set_game_opts(g_mir_ctx, g_atl_translate_config.enable_game_opts);

    /* Create MIR module */
    g_mir_module = atl_mir_module_create(g_mir_ctx);
    if (!g_mir_module) {
        atl_mir_context_destroy(g_mir_ctx);
        g_mir_ctx = NULL;
        return false;
    }

    /* Configure target: ARMv4 + ARMv7 + NEON + VFP + Thumb-2 */
    atl_mir_module_set_target(g_mir_module,
        true,  /* is_armv4 */
        true,  /* is_armv7 */
        true,  /* has_neon */
        true,  /* has_vfp */
        true); /* is_thumb2 */

    /* Create JIT cache */
    g_atl_jit_cache = atl_jit_cache_create(
        g_atl_translate_config.tier1_max_mb,
        g_atl_translate_config.tier2_max_mb);
    if (!g_atl_jit_cache) {
        atl_mir_context_destroy(g_mir_ctx);
        g_mir_ctx = NULL;
        return false;
    }

    g_atl_arm_initialized = true;

    /* Register translation callback with the linker */
    atl_linker_set_translate_cb(atl_arm_translate_library_cb);

    return true;
}

/* Callback wrapper for the linker - matches the expected signature */
static void *atl_arm_translate_library_cb(const char *library_path, uintptr_t preferred_base,
                                           uintptr_t *translated_base, size_t *translated_size)
{
    return atl_arm_translate_library(library_path, preferred_base,
                                     translated_base, translated_size);
}

void atl_arm_translate_shutdown(void)
{
    if (!g_atl_arm_initialized) return;

    if (g_atl_jit_cache) {
        atl_jit_cache_destroy(g_atl_jit_cache);
        g_atl_jit_cache = NULL;
    }

    if (g_mir_ctx) {
        atl_mir_context_destroy(g_mir_ctx);
        g_mir_ctx = NULL;
    }

    g_mir_module = NULL;
    g_atl_arm_initialized = false;
}

/* ============================================================================
 * Main Translation Entry Point
 * ============================================================================ */

atl_translation_handle_t *atl_arm_translate_library(
    const char *library_path,
    uintptr_t preferred_base,
    uintptr_t *out_translated_base,
    size_t *out_translated_size)
{
    if (!library_path || !g_atl_arm_initialized) return NULL;

    /* Open and read the library */
    int fd = open(library_path, O_RDONLY);
    if (fd < 0) return NULL;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return NULL;
    }

    size_t file_size = st.st_size;
    uint8_t *file_data = malloc(file_size);
    if (!file_data) {
        close(fd);
        return NULL;
    }

    ssize_t bytes_read = read(fd, file_data, file_size);
    close(fd);

    if (bytes_read < 0 || (size_t)bytes_read != file_size) {
        free(file_data);
        return NULL;
    }

    /* Verify it's an ARM library */
    if (!atl_arm_is_library_arm(file_data, file_size)) {
        free(file_data);
        return NULL;
    }

    /* Compute library hash for cache keying */
    uint64_t lib_hash = atl_arm_compute_library_hash(file_data, file_size);

    /* Check disk cache (if enabled) */
    /* TODO: Implement disk cache lookup */

    /* Parse ELF header */
    ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *)file_data;

    /* Find .text section */
    ElfW(Shdr) *shdrs = (ElfW(Shdr) *)(file_data + ehdr->e_shoff);
    const char *strtab = (const char *)(file_data + shdrs[ehdr->e_shstrndx].sh_offset);

    size_t text_size = 0;
    uintptr_t text_addr = 0;
    uint8_t *text_data = NULL;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char *name = strtab + shdrs[i].sh_name;
        if (strcmp(name, ".text") == 0) {
            text_size = shdrs[i].sh_size;
            text_addr = shdrs[i].sh_addr;
            text_data = file_data + shdrs[i].sh_offset;
            break;
        }
    }

    if (!text_data || text_size == 0) {
        /* No .text section - nothing to translate */
        free(file_data);
        return NULL;
    }

    /* Create translation handle */
    atl_translation_handle_t *handle = calloc(1, sizeof(atl_translation_handle_t));
    if (!handle) {
        free(file_data);
        return NULL;
    }

    handle->library_hash = lib_hash;
    handle->original_base = (void *)text_addr;
    handle->original_size = text_size;
    handle->is_translated = false;

    /* Allocate memory for translated code at compatible virtual address */
    /* Use mmap with MAP_FIXED if preferred_base is specified, otherwise let kernel choose */
    void *translated_base = NULL;
    if (preferred_base) {
        translated_base = mmap((void *)preferred_base, text_size * 4,
                               PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (translated_base == MAP_FAILED) {
            /* Try without fixed address */
            translated_base = mmap(NULL, text_size * 4,
                                   PROT_READ | PROT_WRITE | PROT_EXEC,
                                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        }
    } else {
        translated_base = mmap(NULL, text_size * 4,
                               PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }

    if (translated_base == MAP_FAILED) {
        free(handle);
        free(file_data);
        return NULL;
    }

    handle->translated_base = translated_base;
    handle->translated_size = text_size * 4; /* Estimate: x86_64 is typically larger */

    /* Decode ARM code to MIR and lower to x86_64 */
    /* For now, we do basic block-by-block translation */
    uint32_t pc = text_addr;
    uint32_t end_pc = text_addr + text_size;

    while (pc < end_pc) {
        /* Read instruction */
        uint32_t arm_inst;
        bool is_thumb = false;

        if (pc & 1) {
            /* Thumb mode - read 16-bit instruction */
            if (pc + 2 > end_pc) break;
            arm_inst = *(uint16_t *)(file_data + (pc - text_addr));
            is_thumb = true;
            pc += 2;
        } else {
            /* ARM mode - read 32-bit instruction */
            if (pc + 4 > end_pc) break;
            arm_inst = *(uint32_t *)(file_data + (pc - text_addr));
            is_thumb = false;
            pc += 4;
        }

        /* Decode ARM instruction */
        atl_arm_decoded_inst_t decoded;
        bool decode_ok = is_thumb ?
            atl_arm_decode_thumb((uint16_t)arm_inst, arm_inst, false, &decoded) :
            atl_arm_decode_a32(arm_inst, &decoded);

        if (!decode_ok) {
            /* Skip undecodable instruction */
            continue;
        }

        /* TODO: Convert decoded instruction to MIR and lower to x86_64 */
    }

    /* Mark as translated */
    handle->is_translated = true;

    /* Update statistics */
    g_stats_total_translated++;
    g_stats_total_code_bytes += handle->translated_size;

    /* Return outputs */
    if (out_translated_base) *out_translated_base = (uintptr_t)translated_base;
    if (out_translated_size) *out_translated_size = handle->translated_size;

    /* Keep file_data for now (TODO: free after translation complete) */
    /* free(file_data); */

    return handle;
}

/* ============================================================================
 * Single Function Translation (for JIT)
 * ============================================================================ */

void *atl_arm_translate_function(
    atl_translation_handle_t *handle,
    uint32_t arm_pc,
    size_t *out_code_size)
{
    if (!handle || !g_atl_arm_initialized || !out_code_size) return NULL;

    *out_code_size = 0;

    /* Check JIT cache first */
    uint64_t cache_key = handle->library_hash ^ ((uint64_t)arm_pc << 32);
    atl_jit_entry_t *entry = atl_jit_cache_lookup(g_atl_jit_cache, cache_key, 1);
    if (entry) {
        *out_code_size = entry->code_size;
        return entry->code;
    }

    /* TODO: Translate single function from ARM to x86_64 */

    return NULL;
}

/* ============================================================================
 * Cache Invalidation (for self-modifying code)
 * ============================================================================ */

void atl_arm_invalidate_cache(
    atl_translation_handle_t *handle,
    uintptr_t addr,
    size_t len)
{
    if (!handle || !g_atl_jit_cache) return;

    /* Invalidate entries overlapping with [addr, addr+len) */
    /* In a full implementation, we'd track address ranges per entry */
    /* For now, invalidate the whole library cache */
    atl_jit_cache_invalidate_library(g_atl_jit_cache, handle->library_hash);
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

void atl_arm_get_stats(uint64_t *total_translated, uint64_t *total_code_bytes,
                        uint64_t *cache_hits, uint64_t *cache_misses)
{
    if (total_translated) *total_translated = g_stats_total_translated;
    if (total_code_bytes) *total_code_bytes = g_stats_total_code_bytes;
    if (cache_hits) *cache_hits = g_stats_cache_hits;
    if (cache_misses) *cache_misses = g_stats_cache_misses;

    /* Also get JIT cache stats */
    if (g_atl_jit_cache) {
        atl_jit_stats_t jit_stats;
        atl_jit_cache_stats(g_atl_jit_cache, &jit_stats);
        if (cache_hits) *cache_hits += jit_stats.total_hits;
        if (cache_misses) *cache_misses += jit_stats.total_misses;
    }
}

/* ============================================================================
 * Syscall Translation (stub)
 * ============================================================================ */

uint32_t atl_arm_syscall_arm_to_host(uint32_t arm_syscall)
{
    /* ARM -> x86_64 syscall number mapping */
    /* See plan file for full mapping table */
    switch (arm_syscall) {
        case 3:  return 0;   /* read */
        case 4:  return 1;   /* write */
        case 5:  return 257; /* open -> openat */
        case 192: return 9;  /* mmap */
        case 191: return 11; /* munmap */
        case 120: return 56; /* clone */
        case 240: return 202;/* futex */
        case 174: return 13; /* rt_sigaction */
        case 173: return 15; /* rt_sigreturn */
        default:  return arm_syscall; /* Pass through if unknown */
    }
}

/* ============================================================================
 * JNI Trampoline (stub)
 * ============================================================================ */

void *atl_arm_jni_get_trampoline(const char *method_signature)
{
    (void)method_signature;
    /* TODO: Generate or look up JNI trampoline */
    return NULL;
}


/* ============================================================================
 * Exception Unwinding Integration
 * ============================================================================ */

#include "unwind/ehabi_decode.h"
#include "unwind/dwarf_emit.h"
#include "unwind/personality.h"

void atl_arm_register_unwind_info(void *exidx_base, size_t exidx_count, uint32_t text_base)
{
    atl_arm_register_exidx((uint32_t *)exidx_base, exidx_count, text_base);
    atl_arm_register_personality();
}
