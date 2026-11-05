#ifndef ATL_ELF_JIT_H
#define ATL_ELF_JIT_H

#include "elf_loader.h"
#include "../mir/arm_decode/arm_decode.h"
#include "../mir/mir.h"
#include "../mir/lowering/lowering.h"
#include "../runtime/jit_cache.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque JIT context bound to a single loaded ELF handle.
 * Owns a MIR context/module and a JIT cache. */
typedef struct atl_elf_jit atl_elf_jit_t;

/* Create a JIT context for the given loaded ELF handle. */
atl_elf_jit_t *atl_elf_jit_create(atl_elf_handle_t *handle);

/* Destroy a JIT context and free all cached translated code. */
void atl_elf_jit_destroy(atl_elf_jit_t *jit);

/* Translate the ARM function at func_addr (a VA inside handle) to x86_64.
 * Looks up the JIT cache first; on a miss decodes ARM -> MIR via
 * atl_arm_decode_function() and lowers MIR -> x86_64 via atl_mir_lower_function(),
 * then emits the result into the jit_cache and returns an executable pointer.
 * Returns NULL on failure. */
void *atl_elf_jit_translate(atl_elf_jit_t *jit, uintptr_t func_addr);

/* Invalidate any cached translation overlapping [addr, addr+len). */
void atl_elf_jit_invalidate(atl_elf_jit_t *jit, uintptr_t addr, size_t len);

/* PLT trampoline: returns an executable stub that, on first call, triggers
 * atl_elf_jit_translate() for func_addr, patches itself to jump directly to
 * the translated code on subsequent calls, and transfers control.
 * Returns NULL on failure. */
void *atl_elf_plt_trampoline(atl_elf_jit_t *jit, uintptr_t func_addr);

/* Process-global JIT context (convenience; lazily created on first call). */
atl_elf_jit_t *atl_elf_jit_global(void);

#ifdef __cplusplus
}
#endif

#endif /* ATL_ELF_JIT_H */
