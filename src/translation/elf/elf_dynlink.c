#define _GNU_SOURCE
#include "elf_config.h"

/* around line 285? */
#include "elf_reloc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <limits.h>

typedef struct atl_lib {
    atl_elf_handle_t *elf;
    char *path;
    int refcount;
    struct atl_lib *next;
} atl_lib_t;

static atl_lib_t *registry = NULL;
static pthread_mutex_t registry_mutex;
static pthread_once_t registry_mutex_once = PTHREAD_ONCE_INIT;

static void registry_mutex_init(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&registry_mutex, &attr);
    pthread_mutexattr_destroy(&attr);
}

static atl_lib_t *find_lib(const char *path) {
    for (atl_lib_t *l = registry; l; l = l->next) {
        if (strcmp(l->path, path) == 0) return l;
    }
    return NULL;
}

static void atl_parse_version_tags(atl_elf_handle_t *h) {
    if (!h) return;
    for (size_t i = 0; i < h->dyn_count; i++) {
        uint64_t tag = h->dyn[i].tag;
        uint64_t val = h->dyn[i].val;
        switch (tag) {
            case DT_VERSYM:
                h->versym = atl_elf_vaddr_to_ptr(h, val);
                break;
            case DT_VERDEF:
                h->verdef = atl_elf_vaddr_to_ptr(h, val);
                break;
            case DT_VERDEFNUM:
                h->verdefsz = (size_t)val;
                break;
            case DT_VERNEED:
                h->verneed = atl_elf_vaddr_to_ptr(h, val);
                break;
            case DT_VERNEEDNUM:
                h->verneedsz = (size_t)val;
                break;
            default:
                break;
        }
    }
}

static unsigned long atl_elf_hash(const char *name) {
    unsigned long h = 0, g;
    while (*name) {
        h = (h << 4) + (unsigned char)*name++;
        g = h & 0xf0000000UL;
        if (g) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

static uint32_t atl_gnu_hash(const char *name) {
    uint32_t h = 5381u;
    while (*name)
        h = (h * 33u) + (unsigned char)*name++;
    return h;
}

/* Look up 'name' using the classic DT_HASH (SYSV) bucket+chains table.
 * Returns a translated symbol address, or NULL if not found. */
static void *resolve_symbol_sysv_hash(atl_elf_handle_t *h, uintptr_t symtab_addr,
                                      uintptr_t strtab_addr, uint64_t syment,
                                      uint64_t hash_vaddr, const char *name) {
    uint32_t *hashp = (uint32_t *)atl_elf_vaddr_to_ptr(h, hash_vaddr);
    if (!hashp) return NULL;
    uint32_t nbucket = hashp[0];
    if (!nbucket) return NULL;
    uint32_t *buckets = hashp + 2;
    uint32_t *chains = buckets + nbucket;
    unsigned long hv = atl_elf_hash(name);
    uint32_t sym = buckets[hv % nbucket];
    while (sym != 0) {
        if (h->is64) {
            Elf64_Sym *s = (Elf64_Sym *)(symtab_addr + (uintptr_t)sym * syment);
            const char *sn = NULL;
            if (s->st_name < (h->strtab_size ? h->strtab_size : 4096u))
                sn = (const char *)(strtab_addr + s->st_name);
            if (sn && strcmp(sn, name) == 0)
                return atl_elf_vaddr_to_ptr(h, s->st_value);
        } else {
            Elf32_Sym *s = (Elf32_Sym *)(symtab_addr + (uintptr_t)sym * syment);
            const char *sn = NULL;
            if (s->st_name < (h->strtab_size ? h->strtab_size : 4096u))
                sn = (const char *)(strtab_addr + s->st_name);
            if (sn && strcmp(sn, name) == 0)
                return atl_elf_vaddr_to_ptr(h, s->st_value);
        }
        sym = chains[sym];
    }
    return NULL;
}

/* Look up 'name' using the DT_GNU_HASH bloom filter + buckets + chains table.
 * Returns a translated symbol address, or NULL if not found. */
static void *resolve_symbol_gnu_hash(atl_elf_handle_t *h, uintptr_t symtab_addr,
                                     uintptr_t strtab_addr, uint64_t syment,
                                     uint64_t gnu_hash_vaddr, const char *name) {
    uint32_t *gh = (uint32_t *)atl_elf_vaddr_to_ptr(h, gnu_hash_vaddr);
    if (!gh) return NULL;
    uint32_t nbuckets = gh[0];
    uint32_t symoffset = gh[1];
    uint32_t bloom_size = gh[2];
    uint32_t bloom_shift = gh[3];
    if (!nbuckets) return NULL;
    size_t bits_per_word = h->is64 ? 64u : 32u;
    const uint8_t *bloom = (const uint8_t *)(gh + 4);
    const uint32_t *buckets = (const uint32_t *)(bloom + (size_t)bloom_size * (h->is64 ? 8u : 4u));
    const uint32_t *chains = buckets + nbuckets;

    uint32_t hashv = atl_gnu_hash(name);
    /* Bloom filter check: both bits must be set. */
    size_t word_idx = (hashv / bits_per_word) % (size_t)bloom_size;
    uint64_t word = 0;
    memcpy(&word, bloom + word_idx * (h->is64 ? 8u : 4u), h->is64 ? 8u : 4u);
    uint64_t bit1 = (uint64_t)1u << (hashv % bits_per_word);
    uint64_t bit2 = (uint64_t)1u << ((hashv >> bloom_shift) % bits_per_word);
    if ((word & bit1) == 0 || (word & bit2) == 0)
        return NULL;

    uint32_t symndx = buckets[hashv % nbuckets];
    if (symndx < symoffset || symndx == 0)
        return NULL;

    if (h->is64) {
        Elf64_Sym *sym = (Elf64_Sym *)(symtab_addr + (uintptr_t)symndx * syment);
        while (1) {
            uint32_t hval = chains[symndx - symoffset];
            int is_last = hval & 1u;
            uint32_t sym_hash = hval & ~1u;
            if (sym_hash == (hashv & ~1u)) {
                const char *sn = NULL;
                if (sym->st_name < (h->strtab_size ? h->strtab_size : 4096u))
                    sn = (const char *)(strtab_addr + sym->st_name);
                if (sn && strcmp(sn, name) == 0)
                    return atl_elf_vaddr_to_ptr(h, sym->st_value);
            }
            if (is_last) break;
            symndx++;
            sym = (Elf64_Sym *)((uintptr_t)sym + syment);
        }
    } else {
        Elf32_Sym *sym = (Elf32_Sym *)(symtab_addr + (uintptr_t)symndx * syment);
        while (1) {
            uint32_t hval = chains[symndx - symoffset];
            int is_last = hval & 1u;
            uint32_t sym_hash = hval & ~1u;
            if (sym_hash == (hashv & ~1u)) {
                const char *sn = NULL;
                if (sym->st_name < (h->strtab_size ? h->strtab_size : 4096u))
                    sn = (const char *)(strtab_addr + sym->st_name);
                if (sn && strcmp(sn, name) == 0)
                    return atl_elf_vaddr_to_ptr(h, sym->st_value);
            }
            if (is_last) break;
            symndx++;
            sym = (Elf32_Sym *)((uintptr_t)sym + syment);
        }
    }
    return NULL;
}

/* Symbol versioning helpers (DT_VERSYM/DT_VERDEF/DT_VERNEED). */
static uint16_t atl_versym_index(const atl_elf_handle_t *h, uint32_t sym_index) {
    if (!h || !h->versym) return 0;
    const Elf64_Versym *vs = (const Elf64_Versym *)h->versym; /* Elf32/64 Versym is Half */
    (void)h->is64;
    return (uint16_t)(vs[sym_index] & 0x7FFFu);
}

static int atl_versym_hidden(const atl_elf_handle_t *h, uint32_t sym_index) {
    if (!h || !h->versym) return 0;
    const Elf64_Versym *vs = (const Elf64_Versym *)h->versym;
    (void)h->is64;
    return (vs[sym_index] & 0x8000u) != 0;
}

static const char *atl_verdef_name(const atl_elf_handle_t *h, uint16_t ndx) {
    if (!h || !h->verdef || ndx == 0) return NULL;
    const uint8_t *p = (const uint8_t *)h->verdef;
    for (size_t i = 0; i < (h->verdefsz ? h->verdefsz : 4096u); i++) {
        const Elf64_Verdef *vd = (const Elf64_Verdef *)p;
        if (vd->vd_ndx == ndx) {
            if (vd->vd_cnt < 1) return NULL;
            const Elf64_Verdaux *vda = (const Elf64_Verdaux *)(p + vd->vd_aux);
            if (h->strtab && vda->vda_name < h->strtab_size)
                return h->strtab + vda->vda_name;
            return NULL;
        }
        if (vd->vd_next == 0) break;
        p += vd->vd_next;
    }
    return NULL;
}

static int atl_version_matches(const char *required, const char *provided) {
    if (!required || !required[0]) return 1;
    if (!provided) return 0;
    return strcmp(required, provided) == 0;
}

/* Find a required version name for (dep_soname, sym_name) by walking DT_VERNEED
 * entries that reference dep_soname. Returns the version string or NULL. */
static __attribute__((unused)) const char *atl_verneed_for_dep(const atl_elf_handle_t *req_h,
                                       const char *dep_soname,
                                       const char *sym_name) {
    (void)sym_name; /* version is per-reference via versym in the referrer; best-effort */
    if (!req_h || !req_h->verneed || !dep_soname) return NULL;
    const uint8_t *p = (const uint8_t *)req_h->verneed;
    for (size_t i = 0; i < (req_h->verneedsz ? req_h->verneedsz : 4096u); i++) {
        const Elf64_Verneed *vn = (const Elf64_Verneed *)p;
        const char *file = NULL;
        if (req_h->strtab && vn->vn_file < req_h->strtab_size)
            file = req_h->strtab + vn->vn_file;
        if (file && strcmp(file, dep_soname) == 0) {
            /* Return the first non-weak auxiliary as a coarse requirement when
             * per-symbol mapping is unavailable in this lookup path. */
            const uint8_t *auxp = p + vn->vn_aux;
            for (uint16_t a = 0; a < vn->vn_cnt; a++) {
                const Elf64_Vernaux *vna = (const Elf64_Vernaux *)auxp;
                if (req_h->strtab && vna->vna_name < req_h->strtab_size)
                    return req_h->strtab + vna->vna_name;
                if (vna->vna_next == 0) break;
                auxp += vna->vna_next;
            }
        }
        if (vn->vn_next == 0) break;
        p += vn->vn_next;
    }
    return NULL;
}

static int atl_symbol_version_ok(const atl_elf_handle_t *provider,
                                 uint32_t sym_index,
                                 const char *required_ver) {
    if (!required_ver || !required_ver[0]) return 1;
    if (!provider || !provider->versym) return 1; /* no version info: accept */
    uint16_t ndx = atl_versym_index(provider, sym_index);
    if (ndx == VER_NDX_LOCAL) return 0;
    if (ndx == VER_NDX_GLOBAL) return 1; /* global/base; accept if requirement is weak */
    /* Hidden definitions are not matched by default versioned lookups. */
    if (atl_versym_hidden(provider, sym_index)) return 0;
    const char *prov = atl_verdef_name(provider, ndx);
    return atl_version_matches(required_ver, prov);
}

static void *resolve_symbol_in_range(atl_lib_t *lib, const char *name,
                                     const char *required_ver) {
    atl_elf_handle_t *h = lib->elf;
    if (!h) return NULL;
    uint64_t symtab = 0, strtab = 0, syment = 0, hash = 0, gnu_hash = 0;
    for (size_t i = 0; i < h->dyn_count; i++) {
        switch (h->dyn[i].tag) {
            case DT_SYMTAB: symtab = h->dyn[i].val; break;
            case DT_STRTAB: strtab = h->dyn[i].val; break;
            case DT_SYMENT: syment = h->dyn[i].val; break;
            case DT_HASH: hash = h->dyn[i].val; break;
            case DT_GNU_HASH: gnu_hash = h->dyn[i].val; break;
            default: break;
        }
    }
    if (!strtab && h->strtab) strtab = (uint64_t)(uintptr_t)h->strtab;
    if (!symtab) return NULL;

    if (!syment) syment = h->is64 ? sizeof(Elf64_Sym) : sizeof(Elf32_Sym);
    uintptr_t symtab_addr = (uintptr_t)atl_elf_vaddr_to_ptr(h, symtab);
    uintptr_t strtab_addr = h->strtab ? (uintptr_t)h->strtab : (strtab ? (uintptr_t)atl_elf_vaddr_to_ptr(h, strtab) : 0);

    /* Fast path: no required version and provider has no versym constraints to
     * filter by — use hash lookup. If a version is required, fall through to
     * the version-aware scan (hash helpers don't report symbol indices). */
    if (!required_ver && (!h->versym || !h->verneed)) {
        if (gnu_hash) {
            void *found = resolve_symbol_gnu_hash(h, symtab_addr, strtab_addr, syment, gnu_hash, name);
            if (found) return found;
        }
        if (hash) {
            void *found = resolve_symbol_sysv_hash(h, symtab_addr, strtab_addr, syment, hash, name);
            if (found) return found;
        }
    }

    /* Version-aware scan: checks DT_VERSYM index for each matching name and
     * skips symbols whose version does not satisfy required_ver. */
    size_t i = 0;
    int zero_run = 0;
    for (;; i++) {
        if (h->is64) {
            Elf64_Sym *s = (Elf64_Sym *)(symtab_addr + i * syment);
            const char *sn = NULL;
            if (s->st_name < (h->strtab_size ? h->strtab_size : 4096u))
                sn = (const char *)(strtab_addr + s->st_name);
            if (sn && strcmp(sn, name) == 0) {
                if (!atl_symbol_version_ok(h, (uint32_t)i, required_ver))
                    continue; /* version mismatch: skip symbol */
                return atl_elf_vaddr_to_ptr(h, s->st_value);
            }
            if (s->st_name == 0 && s->st_value == 0) zero_run++; else zero_run = 0;
        } else {
            Elf32_Sym *s = (Elf32_Sym *)(symtab_addr + i * syment);
            const char *sn = NULL;
            if (s->st_name < (h->strtab_size ? h->strtab_size : 4096u))
                sn = (const char *)(strtab_addr + s->st_name);
            if (sn && strcmp(sn, name) == 0) {
                if (!atl_symbol_version_ok(h, (uint32_t)i, required_ver))
                    continue;
                return atl_elf_vaddr_to_ptr(h, s->st_value);
            }
            if (s->st_name == 0 && s->st_value == 0) zero_run++; else zero_run = 0;
        }
        if (zero_run >= 3) break;
    }
    return NULL;
}

static void *resolve_symbol(atl_lib_t *lib, const char *name) {
    return resolve_symbol_in_range(lib, name, NULL);
}

void *atl_dlopen(const char *path) {
    if (!path) return NULL;

    pthread_once(&registry_mutex_once, registry_mutex_init);
    pthread_mutex_lock(&registry_mutex);
    atl_lib_t *existing = find_lib(path);
    if (existing) {
        existing->refcount++;
        pthread_mutex_unlock(&registry_mutex);
        return existing;
    }
    pthread_mutex_unlock(&registry_mutex);

    atl_elf_handle_t *h = atl_elf_load(path);
    if (!h) return NULL;

    atl_lib_t *lib = calloc(1, sizeof(*lib));
    if (!lib) {
        atl_elf_unload(h);
        return NULL;
    }
    lib->elf = h;
    lib->path = strdup(path);
    if (!lib->path) {
        atl_elf_unload(h);
        free(lib);
        return NULL;
    }
    lib->refcount = 1;

    /* Parse symbol version tags (DT_VERSYM/VERDEF/VERNEED); store pointers only. */
    atl_parse_version_tags(h);

    /* Process DT_NEEDED dependencies */
    /* Directory of the loading library (for same-dir search and $ORIGIN). */
    char origin_dir[4096];
    snprintf(origin_dir, sizeof(origin_dir), "%s", path);
    char *oslash = strrchr(origin_dir, '/');
    if (oslash) *oslash = '\0'; else origin_dir[0] = '\0';

    /* Collect RPATH/RUNPATH (prefer values parsed by loader; fall back to dyn). */
    const char *rpath_str = h->rpath;
    const char *runpath_str = h->runpath;
    char rpath_dyn[4096] = {0}, runpath_dyn[4096] = {0};
    if (!rpath_str || !runpath_str) {
        for (size_t i = 0; i < h->dyn_count; i++) {
            if (!rpath_str && h->dyn[i].tag == DT_RPATH && h->strtab)
                snprintf(rpath_dyn, sizeof(rpath_dyn), "%s", h->strtab + h->dyn[i].val);
            if (!runpath_str && h->dyn[i].tag == DT_RUNPATH && h->strtab)
                snprintf(runpath_dyn, sizeof(runpath_dyn), "%s", h->strtab + h->dyn[i].val);
        }
        if (!rpath_str && rpath_dyn[0]) rpath_str = rpath_dyn;
        if (!runpath_str && runpath_dyn[0]) runpath_str = runpath_dyn;
    }

    /* Non-destructive ':' split into a fixed array of directory pointers. */
    char rpath_copy[4096] = {0}, runpath_copy[4096] = {0};
    size_t ATL_MAX_RPATH_DIRS = atl_elf_max_rpath_dirs();
    char *rpath_dirs[ATL_MAX_RPATH_DIRS]; /* VLA not allowed? C99 VLA okay */
    /* Actually VLA with variable size; okay */
    char *runpath_dirs[ATL_MAX_RPATH_DIRS];
    size_t rpath_ndirs = 0, runpath_ndirs = 0;
    if (rpath_str && rpath_str[0]) {
        snprintf(rpath_copy, sizeof(rpath_copy), "%s", rpath_str);
        char *p = rpath_copy;
        while (*p && rpath_ndirs < ATL_MAX_RPATH_DIRS) {
            char *start = p;
            while (*p && *p != ':') p++;
            if (*p == ':') *p++ = '\0';
            rpath_dirs[rpath_ndirs++] = start;
        }
    }
    if (runpath_str && runpath_str[0]) {
        snprintf(runpath_copy, sizeof(runpath_copy), "%s", runpath_str);
        char *p = runpath_copy;
        while (*p && runpath_ndirs < ATL_MAX_RPATH_DIRS) {
            char *start = p;
            while (*p && *p != ':') p++;
            if (*p == ':') *p++ = '\0';
            runpath_dirs[runpath_ndirs++] = start;
        }
    }

    static const char *const sys_paths[] = { "/lib", "/usr/lib", "/lib64", "/usr/lib64", NULL };

    for (size_t i = 0; i < h->dyn_count; i++) {
        if (h->dyn[i].tag != DT_NEEDED) continue;
        const char *depname = h->strtab ? h->strtab + h->dyn[i].val : NULL;
        if (!depname || !depname[0]) continue;

        /* Absolute path: try as-is only. */
        if (depname[0] == '/') {
            (void)atl_dlopen(depname);
            continue;
        }

        char cand[8192];
        int loaded = 0;

        /* 1) Same directory as the loading library. */
        if (origin_dir[0]) {
            snprintf(cand, sizeof(cand), "%s/%s", origin_dir, depname);
            if (atl_dlopen(cand)) loaded = 1;
        }

        /* Helper macro-like expansion of $ORIGIN and directory search. */
        /* 2) RPATH dirs, 3) RUNPATH dirs, 4) system paths. */
        char *const *dir_lists[2] = { rpath_dirs, runpath_dirs };
        const size_t dir_counts[2] = { rpath_ndirs, runpath_ndirs };
        for (int li = 0; li < 2 && !loaded; li++) {
            for (size_t di = 0; di < dir_counts[li] && !loaded; di++) {
                const char *d = dir_lists[li][di];
                if (!d || !d[0]) continue;
                char expanded[4096];
                /* $ORIGIN substitution (expand to directory of the loading library). */
                const char *origin_tag = "$ORIGIN";
                const char *pos = strstr(d, origin_tag);
                if (pos && origin_dir[0]) {
                    size_t before = (size_t)(pos - d);
                    if (before >= sizeof(expanded)) before = sizeof(expanded) - 1;
                    memcpy(expanded, d, before);
                    expanded[before] = '\0';
                    snprintf(expanded + before, sizeof(expanded) - before, "%s%s", origin_dir, pos + 7);
                } else {
                    snprintf(expanded, sizeof(expanded), "%s", d);
                }
                snprintf(cand, sizeof(cand), "%s/%s", expanded, depname);
                if (atl_dlopen(cand)) loaded = 1;
            }
        }
        for (size_t si = 0; sys_paths[si] && !loaded; si++) {
            snprintf(cand, sizeof(cand), "%s/%s", sys_paths[si], depname);
            if (atl_dlopen(cand)) loaded = 1;
        }
        (void)loaded;
    }

    /* GOT/PLT setup via relocations */
    atl_elf_reloc_process(h, h->base, NULL, NULL);

    /* Run init functions only for host-native images; guest-arch (e.g. ARM on x86_64)
     * init code cannot execute on this host and is handled by the translation/JIT path. */
    int run_native_init = 0;
#if defined(__x86_64__)
    run_native_init = (h->e_machine == EM_X86_64);
#elif defined(__i386__)
    run_native_init = (h->e_machine == EM_386);
#elif defined(__aarch64__)
    run_native_init = (h->e_machine == EM_AARCH64);
#elif defined(__arm__)
    run_native_init = (h->e_machine == EM_ARM);
#endif
    if (run_native_init) {
        if (h->init) ((void (*)(void))h->init)();
        if (h->init_array) {
            size_t n = h->init_arraysz / sizeof(void *);
            for (size_t i = 0; i < n; i++) {
                void (*f)(void) = ((void (**)(void))h->init_array)[i];
                if (f) f();
            }
        }
    }

    pthread_mutex_lock(&registry_mutex);
    /* Re-check in case another thread loaded it while we were loading. */
    existing = find_lib(path);
    if (existing) {
        existing->refcount++;
        pthread_mutex_unlock(&registry_mutex);
        if (run_native_init) {
            /* cannot undo init safely; keep as loaded */
        }
        atl_elf_unload(h);
        free(lib->path);
        free(lib);
        return existing;
    }
    lib->next = registry;
    registry = lib;
    pthread_mutex_unlock(&registry_mutex);
    return lib;
}

void *atl_dlsym(void *handle, const char *name) {
    if (!handle || !name) return NULL;

    pthread_once(&registry_mutex_once, registry_mutex_init);
    pthread_mutex_lock(&registry_mutex);
    /* search this and dependencies (simple: search registry in order) */
    for (atl_lib_t *l = registry; l; l = l->next) {
        void *v = resolve_symbol(l, name);
        if (v) {
            pthread_mutex_unlock(&registry_mutex);
            return v;
        }
    }
    pthread_mutex_unlock(&registry_mutex);
    return NULL;
}

int atl_dlclose(void *handle) {
    if (!handle) return -1;

    pthread_once(&registry_mutex_once, registry_mutex_init);
    pthread_mutex_lock(&registry_mutex);
    atl_lib_t *lib = (atl_lib_t *)handle;
    if (--lib->refcount > 0) {
        pthread_mutex_unlock(&registry_mutex);
        return 0;
    }
    /* remove from registry */
    atl_lib_t **pp = &registry;
    while (*pp && *pp != lib) pp = &(*pp)->next;
    if (*pp) *pp = lib->next;
    pthread_mutex_unlock(&registry_mutex);

    atl_elf_handle_t *h = lib->elf;
    if (h) {
        int run_native_fini = 0;
#if defined(__x86_64__)
        run_native_fini = (h->e_machine == EM_X86_64);
#elif defined(__i386__)
        run_native_fini = (h->e_machine == EM_386);
#elif defined(__aarch64__)
        run_native_fini = (h->e_machine == EM_AARCH64);
#elif defined(__arm__)
        run_native_fini = (h->e_machine == EM_ARM);
#endif
        if (run_native_fini) {
            if (h->fini_array) {
                size_t n = h->fini_arraysz / sizeof(void *);
                for (size_t i = n; i > 0; i--) {
                    void (*f)(void) = ((void (**)(void))h->fini_array)[i - 1];
                    if (f) f();
                }
            }
            if (h->fini) ((void (*)(void))h->fini)();
        }
    }

    atl_elf_unload(lib->elf);
    free(lib->path);
    free(lib);
    return 0;
}
