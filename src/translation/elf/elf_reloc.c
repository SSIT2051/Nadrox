#include "elf_reloc.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

typedef const char *(*atl_sym_resolver_fn)(void *, const char *, int);

extern atl_elf_log_fn g_elf_logger;
extern void *g_elf_logger_ctx;

static void atl_reloc_diag_unresolved(const char *name, const char *what) {
    if (g_elf_logger) {
        if (name && name[0] != '\0')
            g_elf_logger(g_elf_logger_ctx, 1, "elf_reloc: unresolved symbol '%s' (%s)", name, what);
        else
            g_elf_logger(g_elf_logger_ctx, 1, "elf_reloc: unresolved unnamed symbol (%s)", what);
        return;
    }
    if (name && name[0] != '\0') {
        fprintf(stderr, "elf_reloc: unresolved symbol '%s' (%s)\n", name, what);
    } else {
        fprintf(stderr, "elf_reloc: unresolved unnamed symbol (%s)\n", what);
    }
}

static uintptr_t atl_resolve_undef(atl_sym_resolver_fn resolver, void *ctx,
                                   const char *name, int is_func,
                                   const char *what) {
    if (!resolver) {
        atl_reloc_diag_unresolved(name, what);
        return 0;
    }
    const char *r = resolver(ctx, name, is_func);
    if (!r) {
        atl_reloc_diag_unresolved(name, what);
        return 0;
    }
    return (uintptr_t)r;
}

static const char *atl_sym_name64(const Elf64_Sym *s, uintptr_t strtab_addr,
                                  size_t strtab_size) {
    if (!s || !strtab_addr) return "";
    size_t off = (size_t)s->st_name;
    if (strtab_size && off >= strtab_size) return "";
    return (const char *)(strtab_addr + off);
}

static const char *atl_sym_name32(const Elf32_Sym *s, uintptr_t strtab_addr,
                                  size_t strtab_size) {
    if (!s || !strtab_addr) return "";
    size_t off = (size_t)s->st_name;
    if (strtab_size && off >= strtab_size) return "";
    return (const char *)(strtab_addr + off);
}

static int atl_reloc_count_ok(uint64_t entsz, uint64_t elem_sz, size_t *out_cnt) {
    if (!entsz || !elem_sz) return 0;
    if (entsz % elem_sz != 0) return 0;
    uint64_t cnt = entsz / elem_sz;
    if (cnt > (uint64_t)(SIZE_MAX / 4)) return 0; /* sanity cap */
    *out_cnt = (size_t)cnt;
    return 1;
}

static void *atl_reloc_table_ptr(atl_elf_handle_t *h, uint64_t vaddr, uint64_t entsz, uint64_t elem_sz, size_t *out_cnt) {
    size_t cnt = 0;
    if (!h || !vaddr) return NULL;
    if (!atl_reloc_count_ok(entsz, elem_sz, &cnt)) return NULL;
    void *p = atl_elf_vaddr_to_ptr(h, vaddr);
    if (!p) return NULL;
    *out_cnt = cnt;
    return p;
}

int atl_elf_reloc_process(atl_elf_handle_t *h, void *base,
                          const char *symname_resolver(void *, const char *, int),
                          void *ctx) {
    if (!h) return -1;
    if (!h->dyn_ptr) return 0;

    atl_sym_resolver_fn resolver = (atl_sym_resolver_fn)symname_resolver;

    uint64_t rela = 0, relasz = 0, rel = 0, relsz = 0, jmprel = 0, pltrelsz = 0;
    uint64_t symtab = 0, strtab = 0, pltrel = DT_REL;
    for (size_t i = 0; i < h->dyn_count; i++) {
        switch (h->dyn[i].tag) {
            case DT_RELA: rela = h->dyn[i].val; break;
            case DT_RELASZ: relasz = h->dyn[i].val; break;
            case DT_REL: rel = h->dyn[i].val; break;
            case DT_RELSZ: relsz = h->dyn[i].val; break;
            case DT_JMPREL: jmprel = h->dyn[i].val; break;
            case DT_PLTRELSZ: pltrelsz = h->dyn[i].val; break;
            case DT_PLTREL: pltrel = h->dyn[i].val; break;
            case DT_SYMTAB: symtab = h->dyn[i].val; break;
            case DT_STRTAB: strtab = h->dyn[i].val; break;
            default: break;
        }
    }
    if (!strtab && h->strtab) strtab = (uint64_t)(uintptr_t)h->strtab;

    uintptr_t symtab_addr = symtab ? (uintptr_t)atl_elf_vaddr_to_ptr(h, symtab) : 0;
    uintptr_t strtab_addr = (uintptr_t)h->strtab;
    size_t strtab_size = h->strtab_size;
    if (!strtab_addr && strtab) strtab_addr = (uintptr_t)atl_elf_vaddr_to_ptr(h, strtab);



    if (h->is64) {
        unsigned em = h->ehdr64 ? h->ehdr64->e_machine : 0;
        size_t rela_cnt = 0, rel_cnt = 0, jmp_cnt = 0;
        Elf64_Rela *rela_tbl = (Elf64_Rela *)atl_reloc_table_ptr(h, rela, relasz, sizeof(Elf64_Rela), &rela_cnt);
        Elf64_Rel *rel_tbl = (Elf64_Rel *)atl_reloc_table_ptr(h, rel, relsz, sizeof(Elf64_Rel), &rel_cnt);
        void *jmp_tbl = NULL;
        if (jmprel && pltrelsz) {
            if (pltrel == DT_RELA)
                jmp_tbl = atl_reloc_table_ptr(h, jmprel, pltrelsz, sizeof(Elf64_Rela), &jmp_cnt);
            else
                jmp_tbl = atl_reloc_table_ptr(h, jmprel, pltrelsz, sizeof(Elf64_Rel), &jmp_cnt);
        }
        if (rela_tbl && rela_cnt) {
            for (size_t i = 0; i < rela_cnt; i++) {
                Elf64_Rela *r = &rela_tbl[i];
                unsigned type = ELF64_R_TYPE(r->r_info);
                unsigned sym = ELF64_R_SYM(r->r_info);
                uint64_t *target = (uint64_t *)atl_elf_vaddr_to_ptr(h, r->r_offset);
                if (!target) continue;
                const char *name = "";
                uint64_t symval = 0;
                int is_undef = 0;

                Elf64_Sym *s = NULL;
                if (sym && symtab_addr) {
                    s = (Elf64_Sym *)(symtab_addr + sym * sizeof(Elf64_Sym));
                    name = atl_sym_name64(s, strtab_addr, strtab_size);
                    is_undef = (s->st_shndx == SHN_UNDEF);
                    if (!is_undef) {
                        symval = (uint64_t)atl_elf_vaddr_to_ptr(h, s->st_value);
                    }
                }

                if (is_undef) {
                    symval = (uint64_t)atl_resolve_undef(resolver, ctx, name, 1, "RELA64");
                }

                if (em == EM_X86_64) {
                    switch (type) {
                        case R_X86_64_RELATIVE:
                            *target = (uint64_t)(uintptr_t)base + (uint64_t)r->r_addend;
                            break;
                        case R_X86_64_GLOB_DAT:
                        case R_X86_64_JUMP_SLOT:
                        case R_X86_64_64:
                            *target = symval + (uint64_t)r->r_addend;
                            break;
                        case R_X86_64_32:
                        case R_X86_64_32S:
                            *(uint32_t *)target = (uint32_t)(symval + (uint64_t)r->r_addend);
                            break;
                        case R_X86_64_NONE:
                            break;
                        case R_X86_64_COPY:
                            if (!is_undef && symval && s)
                                memcpy(target, (const void *)(uintptr_t)symval, s->st_size);
                            break;
                        case R_X86_64_TLSDESC:
                        case R_X86_64_TLSDESC_CALL:
                            /* TLS descriptor relocation (TLS not yet implemented). */
                            break;
                        default:
                            break;
                    }
                    continue;
                }
                switch (type) {
                    case R_AARCH64_RELATIVE:
                        *target = (uint64_t)(uintptr_t)base + (uint64_t)r->r_addend;
                        break;
                    case R_AARCH64_GLOB_DAT:
                    case R_AARCH64_JUMP_SLOT:
                        *target = symval + (uint64_t)r->r_addend;
                        break;
                    case R_AARCH64_ABS64:
                        *target = symval + (uint64_t)r->r_addend;
                        if (is_undef && symval == 0)
                            atl_reloc_diag_unresolved(name, "R_AARCH64_ABS64");
                        break;
                    case R_AARCH64_COPY:
                        if (!is_undef && symval && s)
                            memcpy(target, (const void *)(uintptr_t)symval, s->st_size);
                        break;
                    case R_AARCH64_TLSDESC:
                    case R_AARCH64_TLSDESC_LD_PREL19:
                    case R_AARCH64_TLSDESC_ADR_PREL21:
                    case R_AARCH64_TLSDESC_ADR_PAGE21:
                    case R_AARCH64_TLSDESC_LD64_LO12:
                    case R_AARCH64_TLSDESC_ADD_LO12:
                    case R_AARCH64_TLSDESC_OFF_G1:
                    case R_AARCH64_TLSDESC_OFF_G0_NC:
                    case R_AARCH64_TLSDESC_LDR:
                    case R_AARCH64_TLSDESC_ADD:
                    case R_AARCH64_TLSDESC_CALL:
                        /* TLS descriptor relocation (TLS not yet implemented). */
                        break;
                    case R_AARCH64_TLSLE_MOVW_TPREL_G2:
                    case R_AARCH64_TLSLE_MOVW_TPREL_G1:
                    case R_AARCH64_TLSLE_MOVW_TPREL_G1_NC:
                    case R_AARCH64_TLSLE_MOVW_TPREL_G0:
                    case R_AARCH64_TLSLE_MOVW_TPREL_G0_NC:
                    case R_AARCH64_TLSLE_ADD_TPREL_HI12:
                    case R_AARCH64_TLSLE_ADD_TPREL_LO12:
                    case R_AARCH64_TLSLE_ADD_TPREL_LO12_NC:
                    case R_AARCH64_TLSLE_LDST8_TPREL_LO12:
                    case R_AARCH64_TLSLE_LDST8_TPREL_LO12_NC:
                    case R_AARCH64_TLSLE_LDST16_TPREL_LO12:
                    case R_AARCH64_TLSLE_LDST16_TPREL_LO12_NC:
                    case R_AARCH64_TLSLE_LDST32_TPREL_LO12:
                    case R_AARCH64_TLSLE_LDST32_TPREL_LO12_NC:
                    case R_AARCH64_TLSLE_LDST64_TPREL_LO12:
                    case R_AARCH64_TLSLE_LDST64_TPREL_LO12_NC:
                    case R_AARCH64_TLSLE_LDST128_TPREL_LO12:
                    case R_AARCH64_TLSLE_LDST128_TPREL_LO12_NC:
                        /* TLS local-exec relocation (TLS not yet implemented). */
                        break;
                    default:
                        break;
                }
            }
        }
        if (rel_tbl && rel_cnt) {
            for (size_t i = 0; i < rel_cnt; i++) {
                Elf64_Rel *r = &rel_tbl[i];
                unsigned type = ELF64_R_TYPE(r->r_info);
                unsigned sym = ELF64_R_SYM(r->r_info);
                uint64_t *target = (uint64_t *)atl_elf_vaddr_to_ptr(h, r->r_offset);
                if (!target) continue;
                const char *name = "";
                uint64_t symval = 0;
                int is_undef = 0;
                Elf64_Sym *s = NULL;
                if (sym && symtab_addr) {
                    s = (Elf64_Sym *)(symtab_addr + sym * sizeof(Elf64_Sym));
                    name = atl_sym_name64(s, strtab_addr, strtab_size);
                    is_undef = (s->st_shndx == SHN_UNDEF);
                    if (!is_undef) symval = (uint64_t)atl_elf_vaddr_to_ptr(h, s->st_value);
                }
                if (is_undef)
                    symval = (uint64_t)atl_resolve_undef(resolver, ctx, name, 1, "R_AARCH64/REL");
                switch (type) {
                    case R_AARCH64_RELATIVE:
                        *target = (uint64_t)(uintptr_t)base + *target;
                        break;
                    case R_AARCH64_GLOB_DAT:
                    case R_AARCH64_JUMP_SLOT:
                        *target = symval;
                        break;
                    case R_AARCH64_COPY:
                        if (!is_undef && symval && s)
                            memcpy(target, (const void *)(uintptr_t)symval, s->st_size);
                        break;
                    case R_AARCH64_TLSDESC:
                    case R_AARCH64_TLSDESC_LD_PREL19:
                    case R_AARCH64_TLSDESC_ADR_PREL21:
                    case R_AARCH64_TLSDESC_ADR_PAGE21:
                    case R_AARCH64_TLSDESC_LD64_LO12:
                    case R_AARCH64_TLSDESC_ADD_LO12:
                    case R_AARCH64_TLSDESC_OFF_G1:
                    case R_AARCH64_TLSDESC_OFF_G0_NC:
                    case R_AARCH64_TLSDESC_LDR:
                    case R_AARCH64_TLSDESC_ADD:
                    case R_AARCH64_TLSDESC_CALL:
                    case R_AARCH64_TLSLE_MOVW_TPREL_G2:
                    case R_AARCH64_TLSLE_MOVW_TPREL_G1:
                    case R_AARCH64_TLSLE_MOVW_TPREL_G1_NC:
                    case R_AARCH64_TLSLE_MOVW_TPREL_G0:
                    case R_AARCH64_TLSLE_MOVW_TPREL_G0_NC:
                    case R_AARCH64_TLSLE_ADD_TPREL_HI12:
                    case R_AARCH64_TLSLE_ADD_TPREL_LO12:
                    case R_AARCH64_TLSLE_ADD_TPREL_LO12_NC:
                    case R_AARCH64_TLSLE_LDST8_TPREL_LO12:
                    case R_AARCH64_TLSLE_LDST8_TPREL_LO12_NC:
                    case R_AARCH64_TLSLE_LDST16_TPREL_LO12:
                    case R_AARCH64_TLSLE_LDST16_TPREL_LO12_NC:
                    case R_AARCH64_TLSLE_LDST32_TPREL_LO12:
                    case R_AARCH64_TLSLE_LDST32_TPREL_LO12_NC:
                    case R_AARCH64_TLSLE_LDST64_TPREL_LO12:
                    case R_AARCH64_TLSLE_LDST64_TPREL_LO12_NC:
                    case R_AARCH64_TLSLE_LDST128_TPREL_LO12:
                    case R_AARCH64_TLSLE_LDST128_TPREL_LO12_NC:
                        /* TLS relocation (TLS not yet implemented). */
                        break;
                    default:
                        break;
                }
            }
        }
        if (jmp_tbl && jmp_cnt) {
            if (pltrel == DT_RELA) {
                Elf64_Rela *jt = (Elf64_Rela *)jmp_tbl;
                for (size_t i = 0; i < jmp_cnt; i++) {
                    Elf64_Rela *r = &jt[i];
                    unsigned type = ELF64_R_TYPE(r->r_info);
                    unsigned sym = ELF64_R_SYM(r->r_info);
                    uint64_t *target = (uint64_t *)atl_elf_vaddr_to_ptr(h, r->r_offset);
                    if (!target) continue;
                    const char *name = "";
                    uint64_t symval = 0;
                    int is_undef = 0;
                    Elf64_Sym *s = NULL;
                    if (sym && symtab_addr) {
                        s = (Elf64_Sym *)(symtab_addr + sym * sizeof(Elf64_Sym));
                        name = atl_sym_name64(s, strtab_addr, strtab_size);
                        is_undef = (s->st_shndx == SHN_UNDEF);
                        if (!is_undef) symval = (uint64_t)atl_elf_vaddr_to_ptr(h, s->st_value);
                    }
                    if (is_undef)
                        symval = (uint64_t)atl_resolve_undef(resolver, ctx, name, 1, "JUMP_SLOT64");
                    if (em == EM_X86_64) {
                        if (type == R_X86_64_JUMP_SLOT || type == R_X86_64_GLOB_DAT || type == R_X86_64_64)
                            *target = symval + (uint64_t)r->r_addend;
                        else if (type == R_X86_64_RELATIVE)
                            *target = (uint64_t)(uintptr_t)base + (uint64_t)r->r_addend;
                        else if (type == R_X86_64_COPY) {
                            if (!is_undef && symval && s)
                                memcpy(target, (const void *)(uintptr_t)symval, s->st_size);
                        } else if (type == R_X86_64_TLSDESC || type == R_X86_64_TLSDESC_CALL) {
                            /* TLS descriptor relocation (TLS not yet implemented). */
                        }
                        continue;
                    }
                    switch (type) {
                        case R_AARCH64_JUMP_SLOT:
                        case R_AARCH64_GLOB_DAT:
                            *target = symval + (uint64_t)r->r_addend;
                            break;
                        case R_AARCH64_RELATIVE:
                            *target = (uint64_t)(uintptr_t)base + (uint64_t)r->r_addend;
                            break;
                        case R_AARCH64_COPY:
                            if (!is_undef && symval && s)
                                memcpy(target, (const void *)(uintptr_t)symval, s->st_size);
                            break;
                        case R_AARCH64_TLSDESC:
                        case R_AARCH64_TLSDESC_LD_PREL19:
                        case R_AARCH64_TLSDESC_ADR_PREL21:
                        case R_AARCH64_TLSDESC_ADR_PAGE21:
                        case R_AARCH64_TLSDESC_LD64_LO12:
                        case R_AARCH64_TLSDESC_ADD_LO12:
                        case R_AARCH64_TLSDESC_OFF_G1:
                        case R_AARCH64_TLSDESC_OFF_G0_NC:
                        case R_AARCH64_TLSDESC_LDR:
                        case R_AARCH64_TLSDESC_ADD:
                        case R_AARCH64_TLSDESC_CALL:
                        case R_AARCH64_TLSLE_MOVW_TPREL_G2:
                        case R_AARCH64_TLSLE_MOVW_TPREL_G1:
                        case R_AARCH64_TLSLE_MOVW_TPREL_G1_NC:
                        case R_AARCH64_TLSLE_MOVW_TPREL_G0:
                        case R_AARCH64_TLSLE_MOVW_TPREL_G0_NC:
                        case R_AARCH64_TLSLE_ADD_TPREL_HI12:
                        case R_AARCH64_TLSLE_ADD_TPREL_LO12:
                        case R_AARCH64_TLSLE_ADD_TPREL_LO12_NC:
                        case R_AARCH64_TLSLE_LDST8_TPREL_LO12:
                        case R_AARCH64_TLSLE_LDST8_TPREL_LO12_NC:
                        case R_AARCH64_TLSLE_LDST16_TPREL_LO12:
                        case R_AARCH64_TLSLE_LDST16_TPREL_LO12_NC:
                        case R_AARCH64_TLSLE_LDST32_TPREL_LO12:
                        case R_AARCH64_TLSLE_LDST32_TPREL_LO12_NC:
                        case R_AARCH64_TLSLE_LDST64_TPREL_LO12:
                        case R_AARCH64_TLSLE_LDST64_TPREL_LO12_NC:
                        case R_AARCH64_TLSLE_LDST128_TPREL_LO12:
                        case R_AARCH64_TLSLE_LDST128_TPREL_LO12_NC:
                            /* TLS relocation (TLS not yet implemented). */
                            break;
                        default:
                            break;
                    }
                }
            } else {
                Elf64_Rel *jt = (Elf64_Rel *)jmp_tbl;
                for (size_t i = 0; i < jmp_cnt; i++) {
                    Elf64_Rel *r = &jt[i];
                    unsigned type = ELF64_R_TYPE(r->r_info);
                    unsigned sym = ELF64_R_SYM(r->r_info);
                    uint64_t *target = (uint64_t *)atl_elf_vaddr_to_ptr(h, r->r_offset);
                    if (!target) continue;
                    const char *name = "";
                    uint64_t symval = 0;
                    int is_undef = 0;
                    if (sym && symtab_addr) {
                        Elf64_Sym *s = (Elf64_Sym *)(symtab_addr + sym * sizeof(Elf64_Sym));
                        name = atl_sym_name64(s, strtab_addr, strtab_size);
                        is_undef = (s->st_shndx == SHN_UNDEF);
                        if (!is_undef) symval = (uint64_t)atl_elf_vaddr_to_ptr(h, s->st_value);
                    }
                    if (is_undef)
                        symval = (uint64_t)atl_resolve_undef(resolver, ctx, name, 1, "R_AARCH64_JUMP_SLOT");
                    switch (type) {
                        case R_AARCH64_JUMP_SLOT:
                        case R_AARCH64_GLOB_DAT:
                            *target = symval;
                            break;
                        case R_AARCH64_RELATIVE:
                            *target = (uint64_t)(uintptr_t)base + *target;
                            break;
                        case R_AARCH64_COPY:
                            /* Copy relocation; symbol size not available in REL jmprel block. */
                            break;
                        case R_AARCH64_TLSDESC:
                        case R_AARCH64_TLSDESC_LD_PREL19:
                        case R_AARCH64_TLSDESC_ADR_PREL21:
                        case R_AARCH64_TLSDESC_ADR_PAGE21:
                        case R_AARCH64_TLSDESC_LD64_LO12:
                        case R_AARCH64_TLSDESC_ADD_LO12:
                        case R_AARCH64_TLSDESC_OFF_G1:
                        case R_AARCH64_TLSDESC_OFF_G0_NC:
                        case R_AARCH64_TLSDESC_LDR:
                        case R_AARCH64_TLSDESC_ADD:
                        case R_AARCH64_TLSDESC_CALL:
                        case R_AARCH64_TLSLE_MOVW_TPREL_G2:
                        case R_AARCH64_TLSLE_MOVW_TPREL_G1:
                        case R_AARCH64_TLSLE_MOVW_TPREL_G1_NC:
                        case R_AARCH64_TLSLE_MOVW_TPREL_G0:
                        case R_AARCH64_TLSLE_MOVW_TPREL_G0_NC:
                        case R_AARCH64_TLSLE_ADD_TPREL_HI12:
                        case R_AARCH64_TLSLE_ADD_TPREL_LO12:
                        case R_AARCH64_TLSLE_ADD_TPREL_LO12_NC:
                        case R_AARCH64_TLSLE_LDST8_TPREL_LO12:
                        case R_AARCH64_TLSLE_LDST8_TPREL_LO12_NC:
                        case R_AARCH64_TLSLE_LDST16_TPREL_LO12:
                        case R_AARCH64_TLSLE_LDST16_TPREL_LO12_NC:
                        case R_AARCH64_TLSLE_LDST32_TPREL_LO12:
                        case R_AARCH64_TLSLE_LDST32_TPREL_LO12_NC:
                        case R_AARCH64_TLSLE_LDST64_TPREL_LO12:
                        case R_AARCH64_TLSLE_LDST64_TPREL_LO12_NC:
                        case R_AARCH64_TLSLE_LDST128_TPREL_LO12:
                        case R_AARCH64_TLSLE_LDST128_TPREL_LO12_NC:
                            /* TLS relocation (TLS not yet implemented). */
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    } else {
        /* 32-bit ARM */
        size_t rela_cnt = 0, rel_cnt = 0, jmp_cnt = 0;
        Elf32_Rela *rela_tbl = (Elf32_Rela *)atl_reloc_table_ptr(h, rela, relasz, sizeof(Elf32_Rela), &rela_cnt);
        Elf32_Rel *rel_tbl = (Elf32_Rel *)atl_reloc_table_ptr(h, rel, relsz, sizeof(Elf32_Rel), &rel_cnt);
        void *jmp_tbl = NULL;
        if (jmprel && pltrelsz) {
            if (pltrel == DT_RELA)
                jmp_tbl = atl_reloc_table_ptr(h, jmprel, pltrelsz, sizeof(Elf32_Rela), &jmp_cnt);
            else
                jmp_tbl = atl_reloc_table_ptr(h, jmprel, pltrelsz, sizeof(Elf32_Rel), &jmp_cnt);
        }
        if (rel_tbl && rel_cnt) {
            for (size_t i = 0; i < rel_cnt; i++) {
                Elf32_Rel *r = &rel_tbl[i];
                unsigned type = ELF32_R_TYPE(r->r_info);
                unsigned sym = ELF32_R_SYM(r->r_info);
                uint32_t *target = (uint32_t *)atl_elf_vaddr_to_ptr(h, r->r_offset);
                if (!target) continue;
                const char *name = "";
                uint32_t symval = 0;
                int is_undef = 0;

                if (sym && symtab_addr) {
                    Elf32_Sym *s = (Elf32_Sym *)(symtab_addr + sym * sizeof(Elf32_Sym));
                    name = atl_sym_name32(s, strtab_addr, strtab_size);
                    is_undef = (s->st_shndx == SHN_UNDEF);
                    if (!is_undef)
                        symval = (uint32_t)(uintptr_t)atl_elf_vaddr_to_ptr(h, s->st_value);
                }

                if (is_undef)
                    symval = (uint32_t)atl_resolve_undef(resolver, ctx, name, 1, "R_ARM/REL");

                switch (type) {
                    case R_ARM_RELATIVE:
                        /* B + A, where A is the addend stored at the relocation target */
                        *target = (uint32_t)(uintptr_t)base + *target;
                        break;
                    case R_ARM_GLOB_DAT:
                    case R_ARM_JUMP_SLOT:
                        *target = symval;
                        break;
                    case R_ARM_ABS32:
                        *target = symval + *target;
                        if (is_undef && symval == 0)
                            atl_reloc_diag_unresolved(name, "R_ARM_ABS32");
                        break;
                    case R_ARM_COPY:
                        /* Copy relocation; symbol size unavailable in this block. */
                        break;
                    case R_ARM_TLS_DESC:
                    case R_ARM_TLS_DTPMOD32:
                    case R_ARM_TLS_DTPOFF32:
                    case R_ARM_TLS_TPOFF32:
                    case R_ARM_TLS_GD32:
                    case R_ARM_TLS_LDM32:
                    case R_ARM_TLS_LDO32:
                    case R_ARM_TLS_IE32:
                    case R_ARM_TLS_LE32:
                    case R_ARM_TLS_LDO12:
                    case R_ARM_TLS_LE12:
                    case R_ARM_TLS_IE12GP:
                    case R_ARM_TLS_GOTDESC:
                    case R_ARM_TLS_CALL:
                        /* TLS relocation (TLS not yet implemented). */
                        break;
                    case R_ARM_THM_JUMP24:
                    case R_ARM_THM_JUMP19:
                    case R_ARM_THM_JUMP6:
                    case R_ARM_THM_PC22:
                    case R_ARM_THM_PC12:
                    case R_ARM_THM_PC11:
                    case R_ARM_THM_PC9:
                    case R_ARM_THM_PC8:
                        *target = symval;
                        break;
                    case R_ARM_THM_ABS5:
                    case R_ARM_THM_MOVW_ABS_NC:
                    case R_ARM_THM_MOVT_ABS:
                        *target = symval + *target;
                        break;
                    case R_ARM_THM_MOVW_PREL_NC:
                    case R_ARM_THM_MOVT_PREL:
                    case R_ARM_THM_ALU_PREL_11_0:
                    case R_ARM_THM_MOVW_BREL_NC:
                    case R_ARM_THM_MOVT_BREL:
                    case R_ARM_THM_MOVW_BREL:
                    case R_ARM_THM_GOT_BREL12:
                    case R_ARM_THM_RPC22:
                        *target = symval;
                        break;
                    case R_ARM_THM_SWI8:
                    case R_ARM_THM_XPC22:
                    case R_ARM_THM_TLS_CALL:
                    case R_ARM_THM_TLS_DESCSEQ:

                    case R_ARM_THM_TLS_DESCSEQ32:
                        break;
                    default:
                        break;
                }
            }
        }
        if (rela_tbl && rela_cnt) {
            for (size_t i = 0; i < rela_cnt; i++) {
                Elf32_Rela *r = &rela_tbl[i];
                unsigned type = ELF32_R_TYPE(r->r_info);
                unsigned sym = ELF32_R_SYM(r->r_info);
                uint32_t *target = (uint32_t *)atl_elf_vaddr_to_ptr(h, r->r_offset);
                if (!target) continue;
                const char *name = "";
                uint32_t symval = 0;
                int is_undef = 0;
                if (sym && symtab_addr) {
                    Elf32_Sym *s = (Elf32_Sym *)(symtab_addr + sym * sizeof(Elf32_Sym));
                    name = atl_sym_name32(s, strtab_addr, strtab_size);
                    is_undef = (s->st_shndx == SHN_UNDEF);
                    if (!is_undef)
                        symval = (uint32_t)(uintptr_t)atl_elf_vaddr_to_ptr(h, s->st_value);
                }
                if (is_undef)
                    symval = (uint32_t)atl_resolve_undef(resolver, ctx, name, 1, "R_ARM/RELA");
                switch (type) {
                    case R_ARM_RELATIVE:
                        *target = (uint32_t)(uintptr_t)base + (uint32_t)r->r_addend;
                        break;
                    case R_ARM_GLOB_DAT:
                    case R_ARM_JUMP_SLOT:
                        *target = symval + (uint32_t)r->r_addend;
                        break;
                    case R_ARM_COPY:
                        /* Copy relocation; symbol size unavailable in this block. */
                        break;
                    case R_ARM_TLS_DESC:
                    case R_ARM_TLS_DTPMOD32:
                    case R_ARM_TLS_DTPOFF32:
                    case R_ARM_TLS_TPOFF32:
                    case R_ARM_TLS_GD32:
                    case R_ARM_TLS_LDM32:
                    case R_ARM_TLS_LDO32:
                    case R_ARM_TLS_IE32:
                    case R_ARM_TLS_LE32:
                    case R_ARM_TLS_LDO12:
                    case R_ARM_TLS_LE12:
                    case R_ARM_TLS_IE12GP:
                    case R_ARM_TLS_GOTDESC:
                    case R_ARM_TLS_CALL:
                        /* TLS relocation (TLS not yet implemented). */
                        break;
                    case R_ARM_THM_JUMP24:
                    case R_ARM_THM_JUMP19:
                    case R_ARM_THM_JUMP6:
                    case R_ARM_THM_PC22:
                    case R_ARM_THM_PC12:
                    case R_ARM_THM_PC11:
                    case R_ARM_THM_PC9:
                    case R_ARM_THM_PC8:
                        *target = symval;
                        break;
                    case R_ARM_THM_ABS5:
                    case R_ARM_THM_MOVW_ABS_NC:
                    case R_ARM_THM_MOVT_ABS:
                        *target = symval + *target;
                        break;
                    case R_ARM_THM_MOVW_PREL_NC:
                    case R_ARM_THM_MOVT_PREL:
                    case R_ARM_THM_ALU_PREL_11_0:
                    case R_ARM_THM_MOVW_BREL_NC:
                    case R_ARM_THM_MOVT_BREL:
                    case R_ARM_THM_MOVW_BREL:
                    case R_ARM_THM_GOT_BREL12:
                    case R_ARM_THM_RPC22:
                        *target = symval;
                        break;
                    case R_ARM_THM_SWI8:
                    case R_ARM_THM_XPC22:
                    case R_ARM_THM_TLS_CALL:
                    case R_ARM_THM_TLS_DESCSEQ:

                    case R_ARM_THM_TLS_DESCSEQ32:
                        break;
                    default:
                        break;
                }
            }
        }
        if (jmp_tbl && jmp_cnt) {
            if (pltrel == DT_RELA) {
                Elf32_Rela *jt = (Elf32_Rela *)jmp_tbl;
                for (size_t i = 0; i < jmp_cnt; i++) {
                    Elf32_Rela *r = &jt[i];
                    unsigned type = ELF32_R_TYPE(r->r_info);
                    unsigned sym = ELF32_R_SYM(r->r_info);
                    uint32_t *target = (uint32_t *)atl_elf_vaddr_to_ptr(h, r->r_offset);
                    if (!target) continue;
                    const char *name = "";
                    uint32_t symval = 0;
                    int is_undef = 0;
                    if (sym && symtab_addr) {
                        Elf32_Sym *s = (Elf32_Sym *)(symtab_addr + sym * sizeof(Elf32_Sym));
                        name = atl_sym_name32(s, strtab_addr, strtab_size);
                        is_undef = (s->st_shndx == SHN_UNDEF);
                        if (!is_undef)
                            symval = (uint32_t)(uintptr_t)atl_elf_vaddr_to_ptr(h, s->st_value);
                    }
                    if (is_undef)
                        symval = (uint32_t)atl_resolve_undef(resolver, ctx, name, 1, "R_ARM_JUMP_SLOT");
                    switch (type) {
                        case R_ARM_JUMP_SLOT:
                        case R_ARM_GLOB_DAT:
                            *target = symval + (uint32_t)r->r_addend;
                            break;
                        case R_ARM_RELATIVE:
                            *target = (uint32_t)(uintptr_t)base + (uint32_t)r->r_addend;
                            break;
                        case R_ARM_COPY:
                            /* Copy relocation; symbol size unavailable in this block. */
                            break;
                        case R_ARM_TLS_DESC:
                        case R_ARM_TLS_DTPMOD32:
                        case R_ARM_TLS_DTPOFF32:
                        case R_ARM_TLS_TPOFF32:
                        case R_ARM_TLS_GD32:
                        case R_ARM_TLS_LDM32:
                        case R_ARM_TLS_LDO32:
                        case R_ARM_TLS_IE32:
                        case R_ARM_TLS_LE32:
                        case R_ARM_TLS_LDO12:
                        case R_ARM_TLS_LE12:
                        case R_ARM_TLS_IE12GP:
                        case R_ARM_TLS_GOTDESC:
                        case R_ARM_TLS_CALL:
                            /* TLS relocation (TLS not yet implemented). */
                            break;
                        case R_ARM_THM_JUMP24:
                        case R_ARM_THM_JUMP19:
                        case R_ARM_THM_JUMP6:
                        case R_ARM_THM_PC22:
                        case R_ARM_THM_PC12:
                        case R_ARM_THM_PC11:
                        case R_ARM_THM_PC9:
                        case R_ARM_THM_PC8:
                            *target = symval;
                            break;
                        case R_ARM_THM_ABS5:
                        case R_ARM_THM_MOVW_ABS_NC:
                        case R_ARM_THM_MOVT_ABS:
                            *target = symval + *target;
                            break;
                        case R_ARM_THM_MOVW_PREL_NC:
                        case R_ARM_THM_MOVT_PREL:
                        case R_ARM_THM_ALU_PREL_11_0:
                        case R_ARM_THM_MOVW_BREL_NC:
                        case R_ARM_THM_MOVT_BREL:
                        case R_ARM_THM_MOVW_BREL:
                        case R_ARM_THM_GOT_BREL12:
                        case R_ARM_THM_RPC22:
                            *target = symval;
                            break;
                        case R_ARM_THM_SWI8:
                        case R_ARM_THM_XPC22:
                        case R_ARM_THM_TLS_CALL:
                        case R_ARM_THM_TLS_DESCSEQ:
    
                        case R_ARM_THM_TLS_DESCSEQ32:
                            break;
                        default:
                            break;
                    }
                }
            } else {
                Elf32_Rel *jt = (Elf32_Rel *)jmp_tbl;
                for (size_t i = 0; i < jmp_cnt; i++) {
                    Elf32_Rel *r = &jt[i];
                    unsigned type = ELF32_R_TYPE(r->r_info);
                    unsigned sym = ELF32_R_SYM(r->r_info);
                    uint32_t *target = (uint32_t *)atl_elf_vaddr_to_ptr(h, r->r_offset);
                    if (!target) continue;
                    const char *name = "";
                    uint32_t symval = 0;
                    int is_undef = 0;
                    if (sym && symtab_addr) {
                        Elf32_Sym *s = (Elf32_Sym *)(symtab_addr + sym * sizeof(Elf32_Sym));
                        name = atl_sym_name32(s, strtab_addr, strtab_size);
                        is_undef = (s->st_shndx == SHN_UNDEF);
                        if (!is_undef)
                            symval = (uint32_t)(uintptr_t)atl_elf_vaddr_to_ptr(h, s->st_value);
                    }
                    if (is_undef)
                        symval = (uint32_t)atl_resolve_undef(resolver, ctx, name, 1, "R_ARM_JUMP_SLOT");
                    switch (type) {
                        case R_ARM_JUMP_SLOT:
                        case R_ARM_GLOB_DAT:
                            *target = symval;
                            break;
                        case R_ARM_RELATIVE:
                            *target = (uint32_t)(uintptr_t)base + *target;
                            break;
                        case R_ARM_COPY:
                            /* Copy relocation; symbol size unavailable in this block. */
                            break;
                        case R_ARM_TLS_DESC:
                        case R_ARM_TLS_DTPMOD32:
                        case R_ARM_TLS_DTPOFF32:
                        case R_ARM_TLS_TPOFF32:
                        case R_ARM_TLS_GD32:
                        case R_ARM_TLS_LDM32:
                        case R_ARM_TLS_LDO32:
                        case R_ARM_TLS_IE32:
                        case R_ARM_TLS_LE32:
                        case R_ARM_TLS_LDO12:
                        case R_ARM_TLS_LE12:
                        case R_ARM_TLS_IE12GP:
                        case R_ARM_TLS_GOTDESC:
                        case R_ARM_TLS_CALL:
                            /* TLS relocation (TLS not yet implemented). */
                            break;
                        case R_ARM_THM_JUMP24:
                        case R_ARM_THM_JUMP19:
                        case R_ARM_THM_JUMP6:
                        case R_ARM_THM_PC22:
                        case R_ARM_THM_PC12:
                        case R_ARM_THM_PC11:
                        case R_ARM_THM_PC9:
                        case R_ARM_THM_PC8:
                            *target = symval;
                            break;
                        case R_ARM_THM_ABS5:
                        case R_ARM_THM_MOVW_ABS_NC:
                        case R_ARM_THM_MOVT_ABS:
                            *target = symval + *target;
                            break;
                        case R_ARM_THM_MOVW_PREL_NC:
                        case R_ARM_THM_MOVT_PREL:
                        case R_ARM_THM_ALU_PREL_11_0:
                        case R_ARM_THM_MOVW_BREL_NC:
                        case R_ARM_THM_MOVT_BREL:
                        case R_ARM_THM_MOVW_BREL:
                        case R_ARM_THM_GOT_BREL12:
                        case R_ARM_THM_RPC22:
                            *target = symval;
                            break;
                        case R_ARM_THM_SWI8:
                        case R_ARM_THM_XPC22:
                        case R_ARM_THM_TLS_CALL:
                        case R_ARM_THM_TLS_DESCSEQ:
    
                        case R_ARM_THM_TLS_DESCSEQ32:
                            break;
                        default:
                            break;
                    }
                }
            }
        }
    }
    return 0;
}
