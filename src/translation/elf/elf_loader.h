#ifndef ATL_ELF_LOADER_H
#define ATL_ELF_LOADER_H

#include <stdint.h>
#include <stddef.h>
#include <elf.h>

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*atl_elf_log_fn)(void *ctx, int level, const char *fmt, ...);

void atl_elf_set_logger(atl_elf_log_fn fn, void *ctx);

/* Opaque handle returned by atl_elf_load */
typedef struct atl_elf_handle atl_elf_handle_t;

/* Dynamic section entry snapshot */
typedef struct {
    uint64_t tag;
    uint64_t val;
} atl_elf_dyn_entry_t;

struct atl_elf_handle {
    int is64;                  /* 1 for ELF64, 0 for ELF32 */
    uint16_t e_machine;        /* ELF e_machine */
    int fd;                    /* file descriptor */
    void *map;                 /* retained file header buffer (for parsing) */
    size_t map_size;           /* size of retained header buffer */
    void *base;                /* actual lowest mapped segment base */
    intptr_t base_delta;       /* actual_base - expected_base (legacy; prefer vaddr translation) */
    size_t size;               /* total mapped size (span) */
    void **seg_addrs;          /* mapped segment addresses (page-aligned mapping base) */
    size_t *seg_lens;          /* mapped segment lengths */
    uintptr_t *seg_vaddrs;     /* mapped segment vaddrs (ph->p_vaddr, unaligned) */
    size_t *seg_voffs;         /* p_vaddr - page-aligned p_vaddr */
    size_t seg_count;          /* number of mapped segments */
    Elf64_Ehdr *ehdr64;        /* ELF64 header (if applicable) */
    Elf32_Ehdr *ehdr32;        /* ELF32 header (if applicable) */
    Elf64_Phdr *phdr64;        /* program headers */
    Elf32_Phdr *phdr32;
    uint16_t phnum;
    void *dyn_ptr;             /* pointer to dynamic section in memory */
    size_t dyn_count;          /* number of dynamic entries */
    atl_elf_dyn_entry_t *dyn;  /* parsed dynamic entries */
    const char *soname;        /* SONAME string (borrowed from strtab) */
    const char *rpath;         /* RPATH (borrowed) */
    const char *runpath;       /* RUNPATH (borrowed) */
    char *strtab;              /* dynamic string table */
    size_t strtab_size;
    void *init;                /* DT_INIT function pointer */
    void *init_array;          /* DT_INIT_ARRAY base */
    size_t init_arraysz;       /* DT_INIT_ARRAYSZ (bytes) */
    void *fini;                /* DT_FINI function pointer */
    void *fini_array;          /* DT_FINI_ARRAY base */
    size_t fini_arraysz;       /* DT_FINI_ARRAYSZ (bytes) */
    /* Symbol versioning (parsed, not enforced) */
    void *versym;              /* DT_VERSYM */
    void *verdef;              /* DT_VERDEF */
    size_t verdefsz;           /* number of DT_VERDEF entries (DT_VERDEFNUM) */
    void *verneed;             /* DT_VERNEED */
    size_t verneedsz;          /* number of DT_VERNEED entries (DT_VERNEEDNUM) */
};

atl_elf_handle_t *atl_elf_load(const char *path);
void atl_elf_unload(atl_elf_handle_t *handle);
void *atl_elf_vaddr_to_ptr(const atl_elf_handle_t *handle, uint64_t vaddr);

#ifdef __cplusplus
}
#endif

#endif /* ATL_ELF_LOADER_H */
