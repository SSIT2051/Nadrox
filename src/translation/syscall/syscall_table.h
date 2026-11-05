/*
 * Syscall Translation Table
 * ARM -> x86_64 syscall number and argument mapping
 */

#ifndef SYSCALL_TABLE_H
#define SYSCALL_TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/syscall.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYSCALL_TABLE_SIZE 512

/* ARM register state */
typedef struct {
    uint32_t r[16];     /* r0-r15 */
    uint32_t cpsr;
} arm_regs_t;

/* x86_64 register state */
typedef struct {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, rflags;
} x86_regs_t;

/* Argument mapping style */
typedef enum {
    ARM_ARG_REGS_STYLE = 0,
    X86_ARG_REGS_STYLE = 1,
    X86_ARG_MMAP_STYLE = 2,
    X86_ARG_FUTEX_STYLE = 3,
    X86_ARG_OPEN_STYLE = 4,
    X86_ARG_SIGACTION_STYLE = 5,
} arg_mapping_style_t;

enum syscall_flags_enum {
    SYSCALL_F_NONE_VAL = 0,
    SYSCALL_F_NEED_EMU_VAL = 1,
    SYSCALL_F_INLINE_VAL = 2,
    SYSCALL_F_SKIP_VAL = 4
};

typedef struct {
    uint32_t arm_num;
    uint32_t x86_num;
    uint32_t num_args;
    arg_mapping_style_t arg_in_style;
    arg_mapping_style_t arg_out_style;
    unsigned int flags;
    bool valid;
} syscall_mapping_t;

/* Function declarations */
void syscall_table_init(void);
syscall_mapping_t *get_syscall_mapping(uint32_t arm_syscall);
uint32_t arm_to_x86_syscall(uint32_t arm_syscall);
void translate_syscall_args(arm_regs_t *arm_regs, x86_regs_t *x86_regs, syscall_mapping_t *mapping);

#ifndef AT_FDCWD
#define AT_FDCWD (-100)
#endif

#ifdef __cplusplus
}
#endif

#endif /* SYSCALL_TABLE_H */
