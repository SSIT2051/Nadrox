#ifndef ATL_ELF_RELOC_H
#define ATL_ELF_RELOC_H

#include <stdint.h>
#include <stddef.h>
#include "elf_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

int atl_elf_reloc_process(atl_elf_handle_t *h, void *base, const char *symname_resolver(void*, const char*, int), void *ctx);

#ifdef __cplusplus
}
#endif

#endif
