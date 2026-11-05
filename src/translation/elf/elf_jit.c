#include "elf_jit.h"
#include "elf_config.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../unwind/personality.h"

struct atl_elf_jit {
    atl_elf_handle_t *handle;
    atl_mir_context_t *mir_ctx;
    atl_mir_module_t *mir_module;
    atl_jit_cache_t *cache;
    void *fail_stub;   /* executable `ret` stub used when translation fails */
};

static atl_elf_jit_t *g_atl_elf_jit = NULL;
static pthread_mutex_t g_atl_elf_jit_mutex = PTHREAD_MUTEX_INITIALIZER;

static void atl_elf_jit_destroy_locked(atl_elf_jit_t *jit)
{
    if (!jit) return;

    if (jit->fail_stub) {
        munmap(jit->fail_stub, atl_elf_page_size());
        jit->fail_stub = NULL;
    }

    if (jit->cache) {
        atl_jit_cache_destroy(jit->cache);
        jit->cache = NULL;
    }

    if (jit->mir_module) {
        atl_mir_module_destroy(jit->mir_module);
        jit->mir_module = NULL;
    }

    if (jit->mir_ctx) {
        atl_mir_context_destroy(jit->mir_ctx);
        jit->mir_ctx = NULL;
    }

    jit->handle = NULL;
    free(jit);
}

atl_elf_jit_t *atl_elf_jit_create(atl_elf_handle_t *handle)
{
    atl_elf_jit_t *jit = calloc(1, sizeof(*jit));
    if (!jit) return NULL;

    jit->handle = handle;

    jit->mir_ctx = atl_mir_context_create();
    if (!jit->mir_ctx) goto fail;
    atl_mir_context_set_verify_ssa(jit->mir_ctx, true);
    atl_mir_context_set_opt_level(jit->mir_ctx, 1);

    jit->mir_module = atl_mir_module_create(jit->mir_ctx);
    if (!jit->mir_module) goto fail;
    atl_mir_module_set_target(jit->mir_module,
                              true,  /* is_armv4 */
                              true,  /* is_armv7 */
                              true,  /* has_neon */
                              true,  /* has_vfp */
                              true); /* is_thumb2 */

    jit->cache = atl_jit_cache_create(64, 256);
    if (!jit->cache) goto fail;

    /* C++/EH skeleton: register ARM->x86_64 DWARF/EHABI personality integration. */
    atl_elf_jit_register_eh_personality();

    /* Executable `ret` stub returned when a translation fails. */
    jit->fail_stub = mmap(NULL, atl_elf_page_size(), PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (jit->fail_stub == MAP_FAILED) goto fail;
    *(uint8_t *)jit->fail_stub = 0xC3; /* ret */
    __builtin___clear_cache(jit->fail_stub,
                            (uint8_t *)jit->fail_stub + 1);
    if (mprotect(jit->fail_stub, atl_elf_page_size(), PROT_READ | PROT_EXEC) != 0) {
        munmap(jit->fail_stub, atl_elf_page_size());
        jit->fail_stub = NULL;
        goto fail;
    }

    return jit;

fail:
    atl_elf_jit_destroy_locked(jit);
    return NULL;
}

void atl_elf_jit_destroy(atl_elf_jit_t *jit)
{
    atl_elf_jit_destroy_locked(jit);
}

void *atl_elf_jit_translate(atl_elf_jit_t *jit, uintptr_t func_addr)
{
    if (!jit || !jit->cache) return NULL;

    /* JIT cache hit. */
    {
        atl_jit_entry_t *entry = atl_jit_cache_lookup(jit->cache,
                                                      (uint64_t)func_addr, 1);
        if (entry) return entry->code;
    }

    /* Global JIT may be created without a bound ELF handle. */
    if (!jit->handle) return NULL;

    /* Resolve the ARM code bytes from the mapped ELF segments. */
    uintptr_t base = (uintptr_t)jit->handle->base;
    uintptr_t end = base + jit->handle->size;
    if (func_addr < base || func_addr >= end) return NULL;

    bool is_thumb = (func_addr & 1u) != 0;
    uint32_t start_pc = (uint32_t)(func_addr & ~1u);
    const uint8_t *code = (const uint8_t *)func_addr;
    size_t window = (size_t)(end - func_addr);
    if (window > atl_elf_jit_max_window()) window = atl_elf_jit_max_window();
    if (window == 0) return NULL;

    /* ARM -> MIR. */
    atl_mir_function_t *func = atl_arm_decode_function(jit->mir_module,
                                                       code, window,
                                                       start_pc, is_thumb);
    if (!func) return NULL;

    /* MIR -> x86_64 (returns executable memory). */
    size_t code_size = 0;
    void *x86_code = atl_mir_lower_function(func, &code_size);
    if (!x86_code || code_size == 0) return NULL;

    /* Cache the result; still return the usable code if caching fails. */
    (void)atl_jit_cache_insert(jit->cache, (uint64_t)func_addr, 1,
                               x86_code, code_size);
    return x86_code;
}

void atl_elf_jit_invalidate(atl_elf_jit_t *jit, uintptr_t addr, size_t len)
{
    if (!jit || !jit->cache || len == 0) return;
    uintptr_t end = addr + len;
    /* Collect overlapping keys */
    size_t cap = 1024, nkeys = 0;
    uint64_t *keys = malloc(cap * sizeof(*keys));
    if (!keys) return;
    atl_jit_cache_t *c = jit->cache;
    for (size_t b = 0; b < c->hash_table_size; b++) {
        for (atl_jit_entry_t *e = c->hash_table[b]; e; e = e->hash_next) {
            if (e->key >= addr && e->key < end) {
                if (nkeys == cap) {
                    cap *= 2;
                    uint64_t *nk = realloc(keys, cap * sizeof(*keys));
                    if (!nk) { free(keys); return; }
                    keys = nk;
                }
                keys[nkeys++] = e->key;
            }
        }
    }
    for (size_t i = 0; i < nkeys; i++) {
        atl_jit_cache_invalidate(c, keys[i]);
    }
    free(keys);
}

/* Resolver invoked by the PLT trampoline on first call.
 * Translates func_addr, patches slot to point at the translated code,
 * and returns the executable entry point (or fail_stub on failure). */
static void *atl_elf_plt_resolver(uintptr_t func_addr,
                                  atl_elf_jit_t *jit,
                                  void **slot)
{
    void *code = atl_elf_jit_translate(jit, func_addr);
    if (code) {
        *slot = code;
        return code;
    }
    *slot = jit->fail_stub ? jit->fail_stub : (void *)atl_elf_plt_resolver;
    return jit->fail_stub ? jit->fail_stub : (void *)atl_elf_plt_resolver;
}

/* x86_64 PLT trampoline template (named instruction layout for maintainability).
 * Layout:
 *   0x00: jmp *slot(%rip)           ; FF 25 00 00 00 00
 *   0x06: slot (8 bytes, absolute pointer patched to resolved target)
 *   0x0e: resolver stub:
 *           movabs $func_addr,%rdi  ; 48 BF imm64
 *           movabs $jit,%rsi        ; 48 BE imm64
 *           movabs $slot_addr,%rdx  ; 48 BA imm64
 *           call rel32 resolver     ; E8 rel32
 *           jmp *%rax               ; FF E0
 */
enum {
    ATL_PLT_JMP_SLOT_OFF      = 0x00,
    ATL_PLT_SLOT_OFF          = 0x06,
    ATL_PLT_STUB_OFF          = 0x0e,
    ATL_PLT_MOV_RDI_OFF       = 0x0e,
    ATL_PLT_MOV_RDI_IMM_OFF   = 0x10,
    ATL_PLT_MOV_RSI_OFF       = 0x18,
    ATL_PLT_MOV_RSI_IMM_OFF   = 0x1a,
    ATL_PLT_MOV_RDX_OFF       = 0x22,
    ATL_PLT_MOV_RDX_IMM_OFF   = 0x24,
    ATL_PLT_CALL_OFF          = 0x2c,
    ATL_PLT_CALL_REL_OFF      = 0x2d,
    ATL_PLT_JMP_RAX_OFF       = 0x31,
    ATL_PLT_SIZE              = 0x33
};

static const uint8_t atl_plt_template[ATL_PLT_SIZE] = {
    /* 0x00 */ 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,             /* jmp *slot(%rip) */
    /* 0x06 */ 0,0,0,0,0,0,0,0,                                 /* slot pointer */
    /* 0x0e */ 0x48, 0xBF, 0,0,0,0,0,0,0,0,                    /* movabs imm64,%rdi */
    /* 0x18 */ 0x48, 0xBE, 0,0,0,0,0,0,0,0,                    /* movabs imm64,%rsi */
    /* 0x22 */ 0x48, 0xBA, 0,0,0,0,0,0,0,0,                    /* movabs imm64,%rdx */
    /* 0x2c */ 0xE8, 0,0,0,0,                                   /* call rel32 */
    /* 0x31 */ 0xFF, 0xE0                                      /* jmp *%rax */
};

static void *atl_elf_plt_build_trampoline(void *page, atl_elf_jit_t *jit,
                                          uintptr_t func_addr)
{
    uint8_t *p = (uint8_t *)page;
    memcpy(p, atl_plt_template, ATL_PLT_SIZE);

    void *slot_addr = p + ATL_PLT_SLOT_OFF;
    void *stub_addr = p + ATL_PLT_STUB_OFF;
    void *resolver = (void *)(uintptr_t)atl_elf_plt_resolver;

    *(void **)slot_addr = stub_addr; /* first call -> resolver stub */
    *(uint64_t *)(p + ATL_PLT_MOV_RDI_IMM_OFF) = (uint64_t)func_addr;
    *(uint64_t *)(p + ATL_PLT_MOV_RSI_IMM_OFF) = (uint64_t)(uintptr_t)jit;
    *(uint64_t *)(p + ATL_PLT_MOV_RDX_IMM_OFF) = (uint64_t)(uintptr_t)slot_addr;

    int32_t rel = (int32_t)((uintptr_t)resolver -
                            ((uintptr_t)p + ATL_PLT_JMP_RAX_OFF));
    *(int32_t *)(p + ATL_PLT_CALL_REL_OFF) = rel;
    return page;
}

void *atl_elf_plt_trampoline(atl_elf_jit_t *jit, uintptr_t func_addr)
{
    if (!jit || !jit->cache) return NULL;

    void *page = mmap(NULL, atl_elf_page_size(), PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) return NULL;

    atl_elf_plt_build_trampoline(page, jit, func_addr);

    if (mprotect(page, atl_elf_page_size(), PROT_READ | PROT_EXEC) != 0) {
        munmap(page, atl_elf_page_size());
        return NULL;
    }
    __builtin___clear_cache(page, (uint8_t *)page + atl_elf_page_size());

    return page;
}

atl_elf_jit_t *atl_elf_jit_global(void)
{
    if (g_atl_elf_jit) return g_atl_elf_jit;

    pthread_mutex_lock(&g_atl_elf_jit_mutex);
    if (!g_atl_elf_jit) {
        g_atl_elf_jit = atl_elf_jit_create(NULL);
    }
    pthread_mutex_unlock(&g_atl_elf_jit_mutex);
    return g_atl_elf_jit;
}
