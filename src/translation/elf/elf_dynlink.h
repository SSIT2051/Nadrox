#ifndef ATL_ELF_DYNLINK_H
#define ATL_ELF_DYNLINK_H

#include <stdint.h>
#include <stddef.h>
#include "elf_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

void *atl_dlopen(const char *path);
void *atl_dlsym(void *handle, const char *name);
int atl_dlclose(void *handle);

#ifdef __cplusplus
}
#endif

#endif
