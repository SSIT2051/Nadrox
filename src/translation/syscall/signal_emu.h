/*
 * Signal Emulation
 * ARM signal frame → x86_64 translation
 */

#ifndef SIGNAL_EMU_H
#define SIGNAL_EMU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <signal.h>
#include <sys/ucontext.h>
#include "syscall_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ARM sigcontext/ucontext definitions (simplified) */
#define ARM_R0 0
#define ARM_R1 1
#define ARM_R2 2
#define ARM_R3 3
#define ARM_R4 4
#define ARM_R5 5
#define ARM_R6 6
#define ARM_R7 7
#define ARM_R8 8
#define ARM_R9 9
#define ARM_R10 10
#define ARM_FP 11
#define ARM_IP 12
#define ARM_SP 13
#define ARM_LR 14
#define ARM_PC 15
#define ARM_CPSR 16

/* Forward declaration for ARM sigcontext */
struct arm_sigcontext;

/* Function declarations */
void arm_to_x86_regs(const struct arm_sigcontext *arm_ctx, ucontext_t *x86_ctx);
void x86_to_arm_regs(const ucontext_t *x86_ctx, struct arm_sigcontext *arm_ctx);
void translate_arm_ucontext_to_x86(const void *arm_ucontext, void *x86_ucontext);
void translate_x86_ucontext_to_arm(const void *x86_ucontext, void *arm_ucontext);
long emu_rt_sigaction(arm_regs_t *regs);
long emu_rt_sigreturn(arm_regs_t *regs);
void emu_cacheflush(uintptr_t addr, size_t size);
void emu___clear_cache(void *start, void *end);
long emu_cacheflush_syscall(arm_regs_t *regs);
void setup_arm_signal_frame(void *frame, int sig, siginfo_t *info, ucontext_t *uc);
void restore_arm_signal_frame(void *frame, ucontext_t *uc);
int install_arm_signal_handler(int signum, const void *handler);

#ifdef __cplusplus
}
#endif

#endif /* SIGNAL_EMU_H */
