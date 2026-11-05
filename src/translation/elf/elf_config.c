#include "elf_config.h"

#include <stdlib.h>
#include <unistd.h>

static size_t g_page_size = 0;
static size_t g_jit_max_window = 64 * 1024;
static size_t g_max_rpath_dirs = 64;
static size_t g_symbol_scan_limit = 1 << 20;

size_t atl_elf_page_size(void) {
    if (!g_page_size) g_page_size = (size_t)sysconf(_SC_PAGESIZE);
    if (!g_page_size) g_page_size = 4096;
    return g_page_size;
}

size_t atl_elf_jit_max_window(void) { return g_jit_max_window; }
void atl_elf_jit_set_max_window(size_t v) { if (v) g_jit_max_window = v; }

size_t atl_elf_max_rpath_dirs(void) { return g_max_rpath_dirs; }
void atl_elf_set_max_rpath_dirs(size_t v) { if (v) g_max_rpath_dirs = v; }

size_t atl_elf_symbol_scan_limit(void) { return g_symbol_scan_limit; }
void atl_elf_set_symbol_scan_limit(size_t v) { if (v) g_symbol_scan_limit = v; }
