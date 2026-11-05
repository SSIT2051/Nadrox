#ifndef ATL_ELF_CONFIG_H
#define ATL_ELF_CONFIG_H

#include <unistd.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tunables */
extern size_t atl_elf_page_size(void);

extern size_t atl_elf_jit_max_window(void);
void atl_elf_jit_set_max_window(size_t v);

extern size_t atl_elf_max_rpath_dirs(void);
void atl_elf_set_max_rpath_dirs(size_t v);

extern size_t atl_elf_symbol_scan_limit(void);
void atl_elf_set_symbol_scan_limit(size_t v);

#ifdef __cplusplus
}
#endif

#endif
