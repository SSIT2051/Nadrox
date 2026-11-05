#ifndef ATL_ARM_DECODE_H
#define ATL_ARM_DECODE_H

#include "mir.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * ARM Instruction Decoder
 * ============================================================================
 * Decodes ARMv4/v7 A32/T32/Thumb-2 instructions into MIR.
 * ARMv4 support is first-class - cannot assume ARMv7-only semantics.
 * ============================================================================ */

/* ARM instruction fields */
typedef struct {
    uint32_t cond;    /* Condition code (bits 28-31) */
    uint32_t op;      /* Operation type */
    uint32_t opcode;  /* Specific opcode */
    uint32_t rn;      /* Register operand 1 */
    uint32_t rd;      /* Register operand 2 (destination) */
    uint32_t rm;      /* Register operand 3 */
    uint32_t rs;      /* Register operand 4 (shift amount register) */
    uint32_t imm;     /* Immediate value */
    uint32_t shamt;   /* Shift amount */
    uint32_t shift;   /* Shift type */
    bool set_flags;   /* S bit - set condition flags */
    bool writeback;   /* W bit - writeback */
    bool pre_index;   /* P bit - pre/post indexing */
    bool up;          /* U bit - up/down */
    bool load;        /* L bit - load/store */
    bool thumb;       /* Thumb mode instruction */
    bool neon;        /* NEON/VFP instruction */
    bool link;        /* Branch with link (BL/BLX) */
    uint32_t inst_class;   /* atl_arm_inst_class_t classification */
    uint32_t mem_size;     /* Memory access size in bytes (1,2,4,8) */
    int32_t  offset;       /* Signed immediate offset */
    uint32_t reg_count;    /* Register count (LDM/STM list) */
} atl_arm_decoded_inst_t;

/* ARM condition codes */
typedef enum {
    ARM_COND_EQ = 0,  /* Equal */
    ARM_COND_NE = 1,  /* Not equal */
    ARM_COND_CS = 2,  /* Carry set / higher or same */
    ARM_COND_CC = 3,  /* Carry clear / lower */
    ARM_COND_MI = 4,  /* Minus / negative */
    ARM_COND_PL = 5,  /* Plus / positive or zero */
    ARM_COND_VS = 6,  /* Overflow */
    ARM_COND_VC = 7,  /* No overflow */
    ARM_COND_HI = 8,  /* Higher */
    ARM_COND_LS = 9,  /* Lower or same */
    ARM_COND_GE = 10, /* Greater or equal */
    ARM_COND_LT = 11, /* Less than */
    ARM_COND_GT = 12, /* Greater than */
    ARM_COND_LE = 13, /* Less than or equal */
    ARM_COND_AL = 14, /* Always */
    ARM_COND_NV = 15, /* Never / reserved */
} atl_arm_cond_t;

/* ARM register numbers (r0-r15) */
#define ARM_REG_R0  0
#define ARM_REG_R1  1
#define ARM_REG_R2  2
#define ARM_REG_R3  3
#define ARM_REG_R4  4
#define ARM_REG_R5  5
#define ARM_REG_R6  6
#define ARM_REG_R7  7
#define ARM_REG_R8  8
#define ARM_REG_R9  9
#define ARM_REG_R10 10
#define ARM_REG_R11 11
#define ARM_REG_R12 12
#define ARM_REG_SP  13
#define ARM_REG_LR  14
#define ARM_REG_PC  15

/* ARM shift types */
#define ARM_SHIFT_LSL 0
#define ARM_SHIFT_LSR 1
#define ARM_SHIFT_ASR 2
#define ARM_SHIFT_ROR 3
#define ARM_SHIFT_RRX 4

/* ARM instruction classifications */
typedef enum {
    ARM_INST_DATA_PROCESSING,   /* AND, EOR, SUB, RSB, ADD, ADC, SBC, RSC, TST, TEQ, CMP, CMN, ORR, MOV, BIC, MVN */
    ARM_INST_MULTIPLY,          /* MUL, MLA, UMULL, UMLAL, SMULL, SMLAL, MRS, MSR */
    ARM_INST_LOAD_STORE,        /* LDR, STR, LDRB, STRB, LDRH, STRH, LDRSB, LDRSH, LDRD, STRD */
    ARM_INST_BRANCH,            /* B, BL, BX, BLX */
    ARM_INST_LOAD_STORE_MULTIPLE, /* LDM, STM */
    ARM_INST_SWITCH,            /* SWI/SVC */
    ARM_INST_COPROCESSOR,       /* CDP, LDC, STC, MCR, MRC */
    ARM_INST_UNDEFINED,         /* Undefined instruction */
    ARM_INST_THUMB_DATA_PROC,   /* Thumb data processing */
    ARM_INST_THUMB_SPECIAL,     /* Thumb special (BX, BLX) */
    ARM_INST_THUMB_BRANCH,      /* Thumb branch */
    ARM_INST_THUMB_LOAD_STORE,  /* Thumb load/store */
    ARM_INST_THUMB_LOAD_STORE_MULTI, /* Thumb LDM/STM */
    ARM_INST_THUMB_COND_BRANCH, /* Thumb conditional branch */
    ARM_INST_THUMB_SVC_BKPT,    /* Thumb SVC/BKPT */
    ARM_INST_NEON_DATA_PROC,    /* NEON data processing */
    ARM_INST_NEON_LOAD_STORE,   /* NEON load/store */
    ARM_INST_VFP_DATA_PROC,     /* VFP data processing */
} atl_arm_inst_class_t;

/* Decode ARM instruction (A32 mode) */
bool atl_arm_decode_a32(uint32_t inst, atl_arm_decoded_inst_t *out);

/* Set ARMv4 shift semantics mode */
void atl_arm_set_armv4_mode(bool armv4);

/* Decode Thumb-2 instruction (16-bit or 32-bit) */
bool atl_arm_decode_thumb(uint16_t inst16, uint32_t inst32, bool is_32bit,
                           atl_arm_decoded_inst_t *out);

/* Decode NEON/VFP instruction */
bool atl_arm_decode_neon(uint32_t inst, atl_arm_decoded_inst_t *out);

/* Convert decoded ARM instruction to MIR block */
atl_mir_block_t *atl_arm_to_mir_block(atl_mir_function_t *func,
                                       const atl_arm_decoded_inst_t *decoded,
                                       uint32_t pc);

/* Decode an ARM function from binary code */
atl_mir_function_t *atl_arm_decode_function(atl_mir_module_t *module,
                                             const uint8_t *code, size_t code_size,
                                             uint32_t start_pc, bool is_thumb);

/* Get instruction size in bytes */
static inline size_t atl_arm_inst_size(bool is_thumb) {
    return is_thumb ? 2 : 4;
}

/* Check if ARM instruction is a branch */
static inline bool atl_arm_is_branch(const atl_arm_decoded_inst_t *d) {
    return d->thumb ? (d->thumb /* classify later */) : (d->cond == ARM_COND_AL && (d->op & 0x0F000000) == 0x0A000000);
}

/* Check if ARM instruction can fall through */
static inline bool atl_arm_can_fall_through(const atl_arm_decoded_inst_t *d) {
    /* Branches with AL condition don't fall through */
    if (d->cond == ARM_COND_AL && atl_arm_is_branch(d)) return false;
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* ATL_ARM_DECODE_H */