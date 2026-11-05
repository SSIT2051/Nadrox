#ifndef ATL_JNI_THUNK_GEN_H
#define ATL_JNI_THUNK_GEN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../syscall/syscall_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Trampoline function type - calls translated ARM function from x86_64 SysV */
typedef void *(*jni_trampoline_t)(arm_regs_t *arm_regs, void *translated_func);

/* Get or create a trampoline for a method signature */
void *atl_jni_get_trampoline(const char *method_signature);

/* Get trampoline for a translated function */
void *atl_jni_create_trampoline(void *translated_arm_func, const char *method_signature);

/* Initialize thunk generator */
bool atl_thunk_gen_init(void);

/* Shutdown thunk generator */
void atl_thunk_gen_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* ATL_JNI_THUNK_GEN_H */
