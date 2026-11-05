#ifndef PERSONALITY_H
#define PERSONALITY_H

#include <stdint.h>
#include <unwind.h>

/* Function declarations */
void atl_arm_register_exidx(uint32_t *exidx_base, size_t exidx_count,
                             uint32_t text_base);

_Unwind_Reason_Code atl_arm_personality(int version,
                                        _Unwind_Action actions,
                                        uint64_t exceptionClass,
                                        struct _Unwind_Exception *exceptionObject,
                                        struct _Unwind_Context *context);

void atl_arm_register_personality(void);
void atl_arm_set_cleanup_handler(void (*handler)(void *));

/* Skeleton registration hook for ELF/JIT integration. */
void atl_elf_jit_register_eh_personality(void);

#endif /* PERSONALITY_H */
