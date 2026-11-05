/*
 * Syscall Emulation
 * Emulation stubs for syscalls that need translation
 */

#ifndef SYSCALL_EMU_H
#define SYSCALL_EMU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <signal.h>
#include "syscall_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Clone arguments */
typedef struct {
    unsigned long flags;
    void *stack;
    int (*fn)(void *);
    void *arg;
    pid_t *ptid;
    pid_t *ctid;
    unsigned long tls;
} clone_args_t;

/* Function declarations */
long emu_mmap(arm_regs_t *regs);
long emu_munmap(arm_regs_t *regs);
long emu_mprotect(arm_regs_t *regs);
long emu_clone(arm_regs_t *regs);
long emu_futex(arm_regs_t *regs);
long emu_ioctl(arm_regs_t *regs);
long emu_fcntl(arm_regs_t *regs);
long emu_poll(arm_regs_t *regs);
long emu_epoll_create(arm_regs_t *regs);
long emu_epoll_ctl(arm_regs_t *regs);
long emu_epoll_wait(arm_regs_t *regs);
long syscall_emulate(uint32_t arm_syscall, arm_regs_t *regs);
void syscall_emu_set_single_addr_space(bool enable);
uintptr_t translate_addr_arm_to_host(uintptr_t arm_addr);
uintptr_t translate_addr_host_to_arm(uintptr_t host_addr);

#ifdef __cplusplus
}
#endif

#endif /* SYSCALL_EMU_H */
