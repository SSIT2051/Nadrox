/*
 * Signal Emulation
 * ARM signal frame → x86_64 translation
 */

#define _GNU_SOURCE
#include "signal_emu.h"
#include "syscall_emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <sys/syscall.h>
#include <errno.h>

/* ARM signal context structures */
struct arm_sigcontext {
    unsigned long trap_no;
    unsigned long error_code;
    unsigned long oldmask;
    unsigned long arm_r0;
    unsigned long arm_r1;
    unsigned long arm_r2;
    unsigned long arm_r3;
    unsigned long arm_r4;
    unsigned long arm_r5;
    unsigned long arm_r6;
    unsigned long arm_r7;
    unsigned long arm_r8;
    unsigned long arm_r9;
    unsigned long arm_r10;
    unsigned long arm_fp;
    unsigned long arm_ip;
    unsigned long arm_sp;
    unsigned long arm_lr;
    unsigned long arm_pc;
    unsigned long arm_cpsr;
    unsigned long fault_address;
};

struct arm_ucontext {
    unsigned long uc_flags;
    struct arm_ucontext *uc_link;
    stack_t uc_stack;
    struct arm_sigcontext uc_mcontext;
    sigset_t uc_sigmask;
};

struct arm_sigframe {
    struct arm_ucontext uc;
    unsigned long retcode[4];
    siginfo_t info;
};

/* Translate ARM registers to x86_64 registers in ucontext */
void arm_to_x86_regs(const struct arm_sigcontext *arm_ctx, ucontext_t *x86_ctx)
{
    if (!arm_ctx || !x86_ctx) return;
    
    greg_t *gregs = x86_ctx->uc_mcontext.gregs;
    
    /* ARM to x86_64 register mapping */
    gregs[REG_RDI] = arm_ctx->arm_r0;
    gregs[REG_RSI] = arm_ctx->arm_r1;
    gregs[REG_RDX] = arm_ctx->arm_r2;
    gregs[REG_RCX] = arm_ctx->arm_r3;
    gregs[REG_R8]  = arm_ctx->arm_r4;
    gregs[REG_R9]  = arm_ctx->arm_r5;
    gregs[REG_R10] = arm_ctx->arm_r6;
    gregs[REG_R11] = arm_ctx->arm_r7;
    gregs[REG_R12] = arm_ctx->arm_r8;
    gregs[REG_R13] = arm_ctx->arm_r9;
    gregs[REG_R14] = arm_ctx->arm_r10;
    gregs[REG_RBP] = arm_ctx->arm_fp;
    gregs[REG_RBX] = arm_ctx->arm_ip;
    gregs[REG_RSP] = arm_ctx->arm_sp;
    gregs[REG_RIP] = arm_ctx->arm_pc;
    
    /* Set flags */
    gregs[REG_EFL] = arm_ctx->arm_cpsr & 0xF0000000;
}

/* Translate x86_64 registers to ARM registers in signal frame */
void x86_to_arm_regs(const ucontext_t *x86_ctx, struct arm_sigcontext *arm_ctx)
{
    if (!x86_ctx || !arm_ctx) return;
    
    const greg_t *gregs = x86_ctx->uc_mcontext.gregs;
    
    arm_ctx->arm_r0 = gregs[REG_RDI];
    arm_ctx->arm_r1 = gregs[REG_RSI];
    arm_ctx->arm_r2 = gregs[REG_RDX];
    arm_ctx->arm_r3 = gregs[REG_RCX];
    arm_ctx->arm_r4 = gregs[REG_R8];
    arm_ctx->arm_r5 = gregs[REG_R9];
    arm_ctx->arm_r6 = gregs[REG_R10];
    arm_ctx->arm_r7 = gregs[REG_R11];
    arm_ctx->arm_r8 = gregs[REG_R12];
    arm_ctx->arm_r9 = gregs[REG_R13];
    arm_ctx->arm_r10 = gregs[REG_R14];
    arm_ctx->arm_fp = gregs[REG_RBP];
    arm_ctx->arm_sp = gregs[REG_RSP];
    arm_ctx->arm_lr = 0;
    arm_ctx->arm_pc = gregs[REG_RIP];
    arm_ctx->arm_cpsr = (unsigned long)(gregs[REG_EFL] & 0xF0000000);
}

/* Translate ARM ucontext to x86_64 ucontext */
void translate_arm_ucontext_to_x86(const void *arm_ucontext, void *x86_ucontext)
{
    if (!arm_ucontext || !x86_ucontext) return;
    
    const struct arm_ucontext *arm_uc = (const struct arm_ucontext *)arm_ucontext;
    ucontext_t *x86_uc = (ucontext_t *)x86_ucontext;
    
    /* Translate signal mask */
    sigemptyset(&x86_uc->uc_sigmask);
    memcpy(&x86_uc->uc_sigmask, &arm_uc->uc_sigmask, sizeof(sigset_t));
    
    /* Translate stack */
    x86_uc->uc_stack = arm_uc->uc_stack;
    
    /* Translate register context */
    arm_to_x86_regs(&arm_uc->uc_mcontext, x86_uc);
}

/* Translate x86_64 ucontext to ARM ucontext */
void translate_x86_ucontext_to_arm(const void *x86_ucontext, void *arm_ucontext)
{
    if (!x86_ucontext || !arm_ucontext) return;
    
    const ucontext_t *x86_uc = (const ucontext_t *)x86_ucontext;
    struct arm_ucontext *arm_uc = (struct arm_ucontext *)arm_ucontext;
    
    /* Translate signal mask */
    memcpy(&arm_uc->uc_sigmask, &x86_uc->uc_sigmask, sizeof(sigset_t));
    
    /* Translate stack */
    arm_uc->uc_stack = x86_uc->uc_stack;
    
    /* Translate register context */
    x86_to_arm_regs(x86_uc, &arm_uc->uc_mcontext);
}

/* Emulate rt_sigaction */
long emu_rt_sigaction(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    
    /* ARM: r0=signum, r1=act, r2=oldact, r3=sigsetsize */
    int signum = (int)regs->r[0];
    struct sigaction *act = (struct sigaction *)(regs->r[1] ? translate_addr_arm_to_host(regs->r[1]) : 0);
    struct sigaction *oldact = (struct sigaction *)(regs->r[2] ? translate_addr_arm_to_host(regs->r[2]) : 0);
    size_t sigsetsize = (size_t)regs->r[3];
    
    long ret = syscall(SYS_rt_sigaction, signum, act, oldact, sigsetsize);
    if (ret < 0) return -errno;
    return ret;
}

/* Emulate rt_sigreturn */
long emu_rt_sigreturn(arm_regs_t *regs)
{
    (void)regs;
    long ret = syscall(SYS_rt_sigreturn);
    if (ret < 0) return -errno;
    return ret;
}

/* Intercept __clear_cache / cacheflush */
void emu_cacheflush(uintptr_t addr, size_t size)
{
    extern void atl_arm_invalidate_cache_all(uintptr_t addr, size_t len);
    if (addr && size) {
        __builtin___clear_cache((char *)addr, (char *)addr + size);
        /* Also notify JIT */
        atl_arm_invalidate_cache_all(addr, size);
    }
}

void emu___clear_cache(void *start, void *end)
{
    if (start && end && (char *)end >= (char *)start) {
        emu_cacheflush((uintptr_t)start, (size_t)((char *)end - (char *)start));
    }
}

/* Handle ARM cacheflush syscall */
long emu_cacheflush_syscall(arm_regs_t *regs)
{
    if (!regs) return -EINVAL;
    
    /* ARM cacheflush: r0=start, r1=end, r2=flags */
    uintptr_t start = regs->r[0];
    uintptr_t end = regs->r[1];
    
    if (end >= start) {
        emu_cacheflush(start, end - start);
    }
    return 0;
}

/* Setup signal frame for ARM */
void setup_arm_signal_frame(void *frame, int sig, siginfo_t *info, ucontext_t *uc)
{
    if (!frame) return;
    
    struct arm_sigframe *sf = (struct arm_sigframe *)frame;
    
    /* Set up ucontext */
    translate_x86_ucontext_to_arm(uc, &sf->uc);
    
    /* Set up siginfo */
    if (info) {
        memcpy(&sf->info, info, sizeof(siginfo_t));
    }
    
    memset(sf->retcode, 0, sizeof(sf->retcode));
}

/* Restore signal frame */
void restore_arm_signal_frame(void *frame, ucontext_t *uc)
{
    if (!frame || !uc) return;
    
    struct arm_sigframe *sf = (struct arm_sigframe *)frame;
    
    /* Restore context */
    translate_arm_ucontext_to_x86(&sf->uc, uc);
}

/* Signal handler wrapper */
static void arm_signal_handler_wrapper(int sig, siginfo_t *info, void *ucontext)
{
    (void)sig;
    (void)info;
    (void)ucontext;
}

/* Install ARM signal handler */
int install_arm_signal_handler(int signum, const void *handler)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = arm_signal_handler_wrapper;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    (void)handler;
    
    return sigaction(signum, &sa, NULL);
}
