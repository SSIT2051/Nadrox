#include "elf_loader.h"

atl_elf_log_fn g_elf_logger = NULL;
void *g_elf_logger_ctx = NULL;

void atl_elf_set_logger(atl_elf_log_fn fn, void *ctx) {
    g_elf_logger = fn;
    g_elf_logger_ctx = ctx;
}

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

static int read_file(int fd, void **out_buf, size_t *out_size) {
    struct stat st;
    if (fstat(fd, &st) < 0) return -1;
    size_t sz = (size_t)st.st_size;
    if (sz == 0) return -1;
    void *buf = malloc(sz);
    if (!buf) return -1;
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) { free(buf); return -1; }
    size_t off = 0;
    while (off < sz) {
        ssize_t r = read(fd, (char *)buf + off, sz - off);
        if (r <= 0) { free(buf); return -1; }
        off += (size_t)r;
    }
    *out_buf = buf;
    *out_size = sz;
    return 0;
}

typedef struct atl_seg_map {
    void *addr;
    size_t len;
    uintptr_t vaddr; /* page-aligned p_vaddr */
    size_t voff;     /* p_vaddr - page-aligned p_vaddr */
} atl_seg_map_t;

void *atl_elf_vaddr_to_ptr(const atl_elf_handle_t *handle, uint64_t vaddr) {
    if (!handle) return NULL;
    for (size_t i = 0; i < handle->seg_count; i++) {
        uintptr_t sv = handle->seg_vaddrs ? handle->seg_vaddrs[i] : 0;
        size_t vo = handle->seg_voffs ? handle->seg_voffs[i] : 0;
        size_t len = handle->seg_lens ? handle->seg_lens[i] : 0;
        if ((uintptr_t)vaddr >= sv && (uintptr_t)vaddr < sv + (len > vo ? len - vo : 0)) {
            return (char *)handle->seg_addrs[i] + vo + ((uintptr_t)vaddr - sv);
        }
    }
    /* fallback for legacy/simple case */
    return (void *)((uintptr_t)vaddr + (uintptr_t)handle->base_delta);
}

atl_elf_handle_t *atl_elf_load(const char *path) {
    if (!path) return NULL;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;

    atl_elf_handle_t *h = calloc(1, sizeof(*h));
    if (!h) { close(fd); return NULL; }
    h->fd = fd;

    if (read_file(fd, &h->map, &h->map_size) < 0) {
        close(fd); free(h); return NULL;
    }

    unsigned char *e = h->map;
    if (h->map_size < EI_NIDENT || memcmp(e, ELFMAG, SELFMAG) != 0) {
        free(h->map); close(fd); free(h); return NULL;
    }

    if (e[EI_CLASS] == ELFCLASS32) {
        h->is64 = 0;
        if (h->map_size < sizeof(Elf32_Ehdr)) { goto fail; }
        h->ehdr32 = (Elf32_Ehdr *)h->map;
        h->e_machine = h->ehdr32->e_machine;
        h->phnum = h->ehdr32->e_phnum;
        h->phdr32 = (Elf32_Phdr *)((char *)h->map + h->ehdr32->e_phoff);
    } else if (e[EI_CLASS] == ELFCLASS64) {
        h->is64 = 1;
        if (h->map_size < sizeof(Elf64_Ehdr)) { goto fail; }
        h->ehdr64 = (Elf64_Ehdr *)h->map;
        h->e_machine = h->ehdr64->e_machine;
        h->phnum = h->ehdr64->e_phnum;
        h->phdr64 = (Elf64_Phdr *)((char *)h->map + h->ehdr64->e_phoff);
    } else {
        goto fail;
    }

    /* Find PT_LOAD segments and map them into a contiguous reservation
     * so relative offsets between segments (e.g. text->GOT RIP-relative) are preserved. */
    size_t pagesz = 4096;
    atl_seg_map_t *segs = NULL;
    size_t seg_count = 0;
    uintptr_t min_vaddr = 0, max_vaddr = 0;
    int have_expected = 0;
    for (uint16_t i = 0; i < h->phnum; i++) {
        uint64_t ptype, pvaddr, pmemsz;
        if (h->is64) { ptype = h->phdr64[i].p_type; pvaddr = h->phdr64[i].p_vaddr; pmemsz = h->phdr64[i].p_memsz; }
        else { ptype = h->phdr32[i].p_type; pvaddr = h->phdr32[i].p_vaddr; pmemsz = h->phdr32[i].p_memsz; }
        if (ptype != PT_LOAD) continue;
        uintptr_t va0 = (uintptr_t)(pvaddr & ~(uint64_t)(pagesz - 1));
        uintptr_t va1 = (uintptr_t)((pvaddr + pmemsz + pagesz - 1) & ~(uint64_t)(pagesz - 1));
        if (!have_expected || va0 < min_vaddr) min_vaddr = va0;
        if (!have_expected || va1 > max_vaddr) max_vaddr = va1;
        have_expected = 1;
    }
    /* Reserve a contiguous range and map each PT_LOAD at a fixed offset within it.
     * This preserves inter-segment distances required by RIP-relative/PC-relative code. */
    void *reserve = NULL;
    size_t reserve_sz = 0;
    if (have_expected && max_vaddr > min_vaddr) {
        reserve_sz = (size_t)(max_vaddr - min_vaddr);
        reserve = mmap(NULL, reserve_sz, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (reserve == MAP_FAILED) { reserve = NULL; goto fail_segs; }
    }
    for (uint16_t i = 0; i < h->phnum; i++) {
        uint64_t ptype, poffset, pvaddr, pfilesz, pmemsz, pflags;
        if (h->is64) {
            Elf64_Phdr *ph = &h->phdr64[i];
            ptype = ph->p_type; poffset = ph->p_offset; pvaddr = ph->p_vaddr;
            pfilesz = ph->p_filesz; pmemsz = ph->p_memsz; pflags = ph->p_flags;
        } else {
            Elf32_Phdr *ph = &h->phdr32[i];
            ptype = ph->p_type; poffset = ph->p_offset; pvaddr = ph->p_vaddr;
            pfilesz = ph->p_filesz; pmemsz = ph->p_memsz; pflags = ph->p_flags;
        }
        if (ptype != PT_LOAD) continue;
        size_t off = (size_t)(poffset & ~(uint64_t)(pagesz - 1));
        size_t vaddr_page = (size_t)(pvaddr & ~(uint64_t)(pagesz - 1));
        size_t filesz = (size_t)pfilesz + (size_t)(poffset - off);
        size_t memsz = (size_t)pmemsz + (size_t)(pvaddr - vaddr_page);
        if (memsz < filesz) memsz = filesz;
        memsz = (memsz + pagesz - 1) & ~(pagesz - 1);
        int prot = 0;
        if (pflags & PF_R) prot |= PROT_READ;
        if (pflags & PF_W) prot |= PROT_WRITE;
        if (pflags & PF_X) prot |= PROT_EXEC;
        void *target = reserve ? (char *)reserve + (vaddr_page - min_vaddr) : NULL;
        void *m = mmap(target, memsz, prot, MAP_PRIVATE | (target ? MAP_FIXED : 0), fd, (off_t)off);
        if (m == MAP_FAILED) { goto fail_segs; }
        atl_seg_map_t *ns = realloc(segs, (seg_count + 1) * sizeof(*segs));
        if (!ns) { munmap(m, memsz); goto fail_segs; }
        segs = ns;
        segs[seg_count].addr = m;
        segs[seg_count].len = memsz;
        segs[seg_count].vaddr = (uintptr_t)pvaddr;
        segs[seg_count].voff = (size_t)((uintptr_t)pvaddr - vaddr_page);
        seg_count++;
    }
    h->base = reserve ? reserve : NULL;
    h->base_delta = (intptr_t)((uintptr_t)(reserve ? reserve : (void *)0) - (uintptr_t)min_vaddr);
    h->size = reserve_sz;
    h->seg_count = seg_count;
    if (seg_count > 0) {
        h->seg_addrs = calloc(seg_count, sizeof(void *));
        h->seg_lens = calloc(seg_count, sizeof(size_t));
        h->seg_vaddrs = calloc(seg_count, sizeof(uintptr_t));
        h->seg_voffs = calloc(seg_count, sizeof(size_t));
        if (!h->seg_addrs || !h->seg_lens || !h->seg_vaddrs || !h->seg_voffs) {
            free(segs);
            goto fail_segs2;
        }
        for (size_t si = 0; si < seg_count; si++) {
            h->seg_addrs[si] = segs[si].addr;
            h->seg_lens[si] = segs[si].len;
            h->seg_vaddrs[si] = segs[si].vaddr;
            h->seg_voffs[si] = segs[si].voff;
        }
    }
    free(segs);

    /* Parse dynamic section */
    for (uint16_t i = 0; i < h->phnum; i++) {
        int isdyn = h->is64 ? (h->phdr64[i].p_type == PT_DYNAMIC) : (h->phdr32[i].p_type == PT_DYNAMIC);
        if (!isdyn) continue;
        void *dynaddr;
        size_t dynsz;
        if (h->is64) {
            dynaddr = atl_elf_vaddr_to_ptr(h, h->phdr64[i].p_vaddr);
            dynsz = h->phdr64[i].p_memsz;
        } else {
            dynaddr = atl_elf_vaddr_to_ptr(h, h->phdr32[i].p_vaddr);
            dynsz = h->phdr32[i].p_memsz;
        }
        h->dyn_ptr = dynaddr;
        size_t n = dynsz / (h->is64 ? sizeof(Elf64_Dyn) : sizeof(Elf32_Dyn));
        h->dyn_count = n;
        h->dyn = calloc(n + 1, sizeof(atl_elf_dyn_entry_t));
        if (!h->dyn) { goto fail_segs2; }
        for (size_t k = 0; k < n; k++) {
            if (h->is64) {
                Elf64_Dyn *d = (Elf64_Dyn *)dynaddr + k;
                h->dyn[k].tag = d->d_tag;
                h->dyn[k].val = d->d_un.d_val;
                if (d->d_tag == DT_STRTAB) {
                    h->strtab = (char *)atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_STRSZ) {
                    h->strtab_size = d->d_un.d_val;
                }
                if (d->d_tag == DT_INIT) {
                    h->init = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_INIT_ARRAY) {
                    h->init_array = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_INIT_ARRAYSZ) {
                    h->init_arraysz = d->d_un.d_val;
                }
                if (d->d_tag == DT_FINI) {
                    h->fini = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_FINI_ARRAY) {
                    h->fini_array = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_FINI_ARRAYSZ) {
                    h->fini_arraysz = d->d_un.d_val;
                }
                if (d->d_tag == DT_VERSYM) {
                    h->versym = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_VERDEF) {
                    h->verdef = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_VERDEFNUM) {
                    h->verdefsz = (size_t)d->d_un.d_val;
                }
                if (d->d_tag == DT_VERNEED) {
                    h->verneed = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_VERNEEDNUM) {
                    h->verneedsz = (size_t)d->d_un.d_val;
                }
            } else {
                Elf32_Dyn *d = (Elf32_Dyn *)dynaddr + k;
                h->dyn[k].tag = d->d_tag;
                h->dyn[k].val = d->d_un.d_val;
                if (d->d_tag == DT_STRTAB) {
                    h->strtab = (char *)atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_STRSZ) {
                    h->strtab_size = d->d_un.d_val;
                }
                if (d->d_tag == DT_INIT) {
                    h->init = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_INIT_ARRAY) {
                    h->init_array = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_INIT_ARRAYSZ) {
                    h->init_arraysz = d->d_un.d_val;
                }
                if (d->d_tag == DT_FINI) {
                    h->fini = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_FINI_ARRAY) {
                    h->fini_array = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_FINI_ARRAYSZ) {
                    h->fini_arraysz = d->d_un.d_val;
                }
                if (d->d_tag == DT_VERSYM) {
                    h->versym = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_VERDEF) {
                    h->verdef = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_VERDEFNUM) {
                    h->verdefsz = (size_t)d->d_un.d_val;
                }
                if (d->d_tag == DT_VERNEED) {
                    h->verneed = atl_elf_vaddr_to_ptr(h, d->d_un.d_ptr);
                }
                if (d->d_tag == DT_VERNEEDNUM) {
                    h->verneedsz = (size_t)d->d_un.d_val;
                }
            }
        }

    /* SONAME / RPATH / RUNPATH */
    for (size_t k = 0; k < n; k++) {
        uint64_t tag = h->dyn[k].tag;
        if ((tag == DT_SONAME || tag == DT_RPATH || tag == DT_RUNPATH) && h->strtab) {
            if (h->dyn[k].val >= h->strtab_size) continue;
            const char *s = h->strtab + h->dyn[k].val;
            if (tag == DT_SONAME) h->soname = s;
            else if (tag == DT_RPATH) h->rpath = s;
            else h->runpath = s;
        }
    }
    }

    /* Keep header buffer for phdr/ehdr pointers; fd kept for caller context. */
    return h;

fail_segs2:
    if (h->seg_addrs && h->seg_lens) {
        for (size_t si = 0; si < h->seg_count; si++) {
            if (h->seg_addrs[si] && h->seg_lens[si]) munmap(h->seg_addrs[si], h->seg_lens[si]);
        }
    }
    free(h->seg_addrs);
    free(h->seg_lens);
    free(h->seg_vaddrs);
    free(h->seg_voffs);
fail_segs:
    if (reserve && reserve_sz) munmap(reserve, reserve_sz);
    if (segs) {
        for (size_t si = 0; si < seg_count; si++) munmap(segs[si].addr, segs[si].len);
    }
    if (segs) free(segs);
fail:
    if (h->map) free(h->map);
    close(fd);
    free(h->dyn);
    free(h);
    return NULL;
}

void atl_elf_unload(atl_elf_handle_t *handle) {
    if (!handle) return;
    for (size_t si = 0; si < handle->seg_count; si++) {
        if (handle->seg_addrs && handle->seg_lens && handle->seg_addrs[si] && handle->seg_lens[si]) {
            munmap(handle->seg_addrs[si], handle->seg_lens[si]);
        }
    }
    free(handle->seg_addrs);
    free(handle->seg_lens);
    free(handle->seg_vaddrs);
    free(handle->seg_voffs);
    free(handle->map);
    if (handle->fd >= 0) close(handle->fd);
    free(handle->dyn);
    free(handle);
}
