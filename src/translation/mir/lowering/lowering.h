#ifndef ATL_MIR_LOWERING_H
#define ATL_MIR_LOWERING_H

#include "../mir.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/mman.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * x86_64 Register Definitions
 * ============================================================================ */

typedef enum {
    X86_REG_RAX = 0,
    X86_REG_RCX = 1,
    X86_REG_RDX = 2,
    X86_REG_RBX = 3,
    X86_REG_RSP = 4,
    X86_REG_RBP = 5,
    X86_REG_RSI = 6,
    X86_REG_RDI = 7,
    X86_REG_R8  = 8,
    X86_REG_R9  = 9,
    X86_REG_R10 = 10,
    X86_REG_R11 = 11,
    X86_REG_R12 = 12,
    X86_REG_R13 = 13,
    X86_REG_R14 = 14,
    X86_REG_R15 = 15,
} x86_reg_t;

/* Physical registers available for allocation */
#define X86_NUM_PHYS_REGS 16

/* Caller-saved and callee-saved conventions */
static const bool x86_reg_caller_saved[X86_NUM_PHYS_REGS] = {
    true,  /* RAX */
    true,  /* RCX */
    true,  /* RDX */
    false, /* RBX - callee saved */
    false, /* RSP */
    false, /* RBP */
    true,  /* RSI */
    true,  /* RDI */
    true,  /* R8 */
    true,  /* R9 */
    true,  /* R10 */
    true,  /* R11 */
    false, /* R12 - callee saved */
    false, /* R13 - callee saved */
    false, /* R14 - callee saved */
    false, /* R15 - callee saved */
};

/* Register names for debugging */
const char *x86_reg_name(x86_reg_t reg);

/* ============================================================================
 * Assembler Emitter (asm_emit.c)
 * ============================================================================ */

typedef struct atl_asm_emitter {
    uint8_t *code;
    size_t capacity;
    size_t size;
    size_t pos;  /* current position */
    
    /* Label fixup table */
    struct {
        char **labels;
        int32_t *positions;  /* position where label is defined, -1 if not yet defined */
        int32_t **fixups;    /* list of positions that need fixup */
        size_t *fixup_counts;
        size_t count;
        size_t capacity;
    } label_table;
    
    /* Jump fixups */
    struct {
        size_t *inst_pos;    /* position of instruction needing fixup */
        int32_t *label_id;   /* label to fixup to */
        int32_t *rel_offset; /* relative offset size/type */
        size_t count;
        size_t capacity;
    } jumps;
    
    /* Memory allocation context - coexist with bionic linker allocator */
    void *(*alloc_func)(size_t);
    void (*free_func)(void *);
    
    /* Support for self-modifying code invalidation */
    bool smc_enabled;
    void (*invalidate_icache)(void *addr, size_t size);
} atl_asm_emitter_t;

atl_asm_emitter_t *atl_asm_emitter_create(size_t initial_capacity);
void atl_asm_emitter_destroy(atl_asm_emitter_t *emit);
void atl_asm_emitter_reset(atl_asm_emitter_t *emit);

/* Label management */
int atl_asm_new_label(atl_asm_emitter_t *emit);
void atl_asm_bind_label(atl_asm_emitter_t *emit, int label_id);
void atl_asm_fixup_jumps(atl_asm_emitter_t *emit);

/* Raw instruction emission */
void atl_asm_emit_byte(atl_asm_emitter_t *emit, uint8_t b);
void atl_asm_emit_dword(atl_asm_emitter_t *emit, uint32_t d);
void atl_asm_emit_qword(atl_asm_emitter_t *emit, uint64_t q);
void atl_asm_emit_rex(atl_asm_emitter_t *emit, bool w, bool r, bool x, bool b);
void atl_asm_emit_modrm(atl_asm_emitter_t *emit, uint8_t mod, uint8_t reg, uint8_t rm);
void atl_asm_emit_sib(atl_asm_emitter_t *emit, uint8_t scale, uint8_t index, uint8_t base);

/* Common x86_64 instructions */
void atl_asm_emit_mov_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64);
void atl_asm_emit_mov_reg_imm64(atl_asm_emitter_t *emit, x86_reg_t dst, uint64_t imm);
void atl_asm_emit_mov_reg_mem(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t base, int32_t disp, int size);
void atl_asm_emit_mov_mem_reg(atl_asm_emitter_t *emit, x86_reg_t base, int32_t disp, x86_reg_t src, int size);
void atl_asm_emit_add_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64);
void atl_asm_emit_sub_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64);
void atl_asm_emit_sub_reg_imm(atl_asm_emitter_t *emit, x86_reg_t dst, int32_t imm, bool is64);
void atl_asm_emit_and_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64);
void atl_asm_emit_or_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64);
void atl_asm_emit_xor_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64);
void atl_asm_emit_shl_reg_cl(atl_asm_emitter_t *emit, x86_reg_t dst, bool is64);
void atl_asm_emit_shr_reg_cl(atl_asm_emitter_t *emit, x86_reg_t dst, bool is64);
void atl_asm_emit_sar_reg_cl(atl_asm_emitter_t *emit, x86_reg_t dst, bool is64);
void atl_asm_emit_imul_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64);
void atl_asm_emit_div_reg(atl_asm_emitter_t *emit, x86_reg_t src, bool is64);
void atl_asm_emit_idiv_reg(atl_asm_emitter_t *emit, x86_reg_t src, bool is64);
void atl_asm_emit_cmp_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64);
void atl_asm_emit_test_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64);
void atl_asm_emit_jmp(atl_asm_emitter_t *emit, int label_id);
void atl_asm_emit_jcc(atl_asm_emitter_t *emit, int cc, int label_id); /* cc: 0=O,1=NO,... */
void atl_asm_emit_call_reg(atl_asm_emitter_t *emit, x86_reg_t reg);
void atl_asm_emit_call_imm(atl_asm_emitter_t *emit, int32_t rel32);
void atl_asm_emit_ret(atl_asm_emitter_t *emit);
void atl_asm_emit_push_reg(atl_asm_emitter_t *emit, x86_reg_t reg);
void atl_asm_emit_pop_reg(atl_asm_emitter_t *emit, x86_reg_t reg);
void atl_asm_emit_lea(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t base, int32_t disp);
void atl_asm_emit_nop(atl_asm_emitter_t *emit);

/* Self-modifying code invalidation */
void atl_asm_invalidate_code(atl_asm_emitter_t *emit, void *addr, size_t size);

/* ============================================================================
 * Register Allocator (reg_alloc.c)
 * ============================================================================ */

typedef struct atl_live_interval {
    atl_mir_value_t *value;
    int start;
    int end;
    x86_reg_t reg;      /* assigned register, or -1 if spilled */
    int spill_slot;     /* spill slot index, or -1 */
    bool fixed;
    struct atl_live_interval *next;
} atl_live_interval_t;

typedef struct atl_reg_alloc {
    atl_mir_function_t *func;
    
    /* Live intervals */
    atl_live_interval_t *intervals;
    size_t num_intervals;
    
    /* Physical register pool */
    bool phys_reg_used[X86_NUM_PHYS_REGS];
    atl_live_interval_t *phys_reg_interval[X86_NUM_PHYS_REGS];
    
    /* Spill slots */
    size_t next_spill_slot;
    
    /* Free list */
    atl_live_interval_t *free_intervals;
    
    /* Coexist with bionic linker memory allocator */
    void *(*alloc_func)(size_t);
    void (*free_func)(void *);
} atl_reg_alloc_t;

atl_reg_alloc_t *atl_reg_alloc_create(atl_mir_function_t *func);
void atl_reg_alloc_destroy(atl_reg_alloc_t *ra);

/* Linear scan register allocation */
bool atl_reg_alloc_run(atl_reg_alloc_t *ra);

/* Get assignment for a value */
x86_reg_t atl_reg_alloc_get_reg(atl_reg_alloc_t *ra, atl_mir_value_t *value);
int atl_reg_alloc_get_spill_slot(atl_reg_alloc_t *ra, atl_mir_value_t *value);
bool atl_reg_alloc_is_spilled(atl_reg_alloc_t *ra, atl_mir_value_t *value);

/* ============================================================================
 * x86_64 Lowering (lower_x86_64.c)
 * ============================================================================ */

typedef struct atl_lower_context {
    atl_mir_function_t *func;
    atl_asm_emitter_t *emit;
    atl_reg_alloc_t *ra;
    
    /* Current instruction index for linear scan */
    int inst_index;
    
    /* Basic block mapping */
    int *block_to_label;  /* block id -> label id */
    size_t block_count;
    
    /* Value to register/spill mapping (from regalloc) */
    
    /* Stack frame layout */
    size_t spill_base_offset;
    size_t local_base_offset;
    size_t frame_size;
    
    /* ARM register mapping (for translated ARM code) */
    /* ARM r0-r15 map to stack slots or registers */
    int arm_reg_spill_slot[16];
    
    /* Coexist with bionic linker memory allocator */
    void *(*alloc_func)(size_t);
    void (*free_func)(void *);
    
    /* Single address space - translated code at same VA as ARM */
    bool same_va_space;
    uintptr_t arm_base_va;
    uintptr_t x86_base_va;
    
    /* Self-modifying code invalidation support */
    bool smc_invalidation;
    
    /* ARMv4 first-class support */
    bool armv4_compat;
} atl_lower_context_t;

atl_lower_context_t *atl_lower_context_create(atl_mir_function_t *func);
void atl_lower_context_destroy(atl_lower_context_t *ctx);

/* Lower a single MIR instruction to x86_64 */
bool atl_lower_instruction(atl_lower_context_t *ctx, atl_mir_inst_t *inst);

/* Lower entire MIR function to x86_64 machine code */
void *atl_mir_lower_function(atl_mir_function_t *func, size_t *out_size);
bool atl_mir_lower_module(atl_mir_module_t *module);

/* Get register for MIR value */
x86_reg_t atl_lower_get_reg(atl_lower_context_t *ctx, atl_mir_value_t *val);
void atl_lower_spill_to_mem(atl_lower_context_t *ctx, x86_reg_t reg, atl_mir_value_t *val);
void atl_lower_reload_from_mem(atl_lower_context_t *ctx, x86_reg_t reg, atl_mir_value_t *val);

/* SMC invalidation */
void atl_lower_invalidate_smc(atl_lower_context_t *ctx, void *addr, size_t size);

/* Register helpers */
static inline bool x86_needs_rex(x86_reg_t reg) {
    return reg >= X86_REG_R8;
}

static inline uint8_t x86_reg_low(x86_reg_t reg) {
    if (reg >= X86_REG_R8) return (uint8_t)(reg - X86_REG_R8);
    return (uint8_t)(reg & 7);
}

#ifdef __cplusplus
}
#endif

#endif /* ATL_MIR_LOWERING_H */
