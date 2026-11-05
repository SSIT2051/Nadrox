#include "arm_decode.h"
#include "mir.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * ARM Instruction Decoder Implementation
 * ============================================================================
 * Decodes ARMv4/v7 A32/T32/Thumb-2 instructions into MIR.
 * ARMv4 support is first-class.
 * ============================================================================ */

/* ARMv4 mode: ARMv4 has different shift-by-register semantics (shift amount
 * >= 32 is defined differently than ARMv6+/ARMv7). Default on since the module
 * target defaults to is_armv4=true. */
static bool g_armv4_mode = true;

void atl_arm_set_armv4_mode(bool armv4) {
    g_armv4_mode = armv4;
}

/* Field extraction helpers */
#define ARM_BIT(inst, n)      (((inst) >> (n)) & 1u)
#define ARM_BITS(inst, hi, lo) (((inst) >> (lo)) & ((1u << ((hi) - (lo) + 1)) - 1u))

#define ARM_COND(inst)   ARM_BITS(inst, 31, 28)
#define ARM_OP(inst)     ARM_BITS(inst, 27, 26)
#define ARM_DP_OPCODE(inst) ARM_BITS(inst, 24, 21)   /* real 4-bit data-processing opcode */
#define ARM_S_BIT(inst)  ARM_BIT(inst, 20)
#define ARM_RN(inst)     ARM_BITS(inst, 19, 16)
#define ARM_RD(inst)     ARM_BITS(inst, 15, 12)
#define ARM_OPERAND2(inst) ARM_BITS(inst, 11, 0)
#define ARM_IMM(inst)    ARM_BITS(inst, 11, 0)
#define ARM_OFFSET(inst) ARM_BITS(inst, 11, 0)
#define ARM_IMM24(inst)  ARM_BITS(inst, 23, 0)
#define ARM_IMM8(inst)   ARM_BITS(inst, 7, 0)
#define ARM_ROTATE(inst) ARM_BITS(inst, 11, 8)
#define ARM_IMM5(inst)   ARM_BITS(inst, 11, 7)
#define ARM_SHR5(inst)   ARM_BITS(inst, 6, 5)
#define ARM_SHIFT_TYPE(inst) ARM_BITS(inst, 6, 5)
#define ARM_RS(inst)     ARM_BITS(inst, 11, 8)

/* Thumb-2 field extraction */
#define THUMB_OPCODE(inst16) ARM_BITS(inst16, 15, 11)
#define THUMB_RN(inst16)     ARM_BITS(inst16, 10, 8)
#define THUMB_RD(inst16)     ARM_BITS(inst16, 7, 0)
#define THUMB_IMM5(inst16)   ARM_BITS(inst16, 10, 6)
#define THUMB_IMM3(inst16)   ARM_BITS(inst16, 10, 8)
#define THUMB_IMM8(inst16)   ARM_BITS(inst16, 7, 0)

/* ============================================================================
 * ARM Condition Code Names
 * ============================================================================ */
static const char *arm_cond_names[] = {
    "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
    "hi", "ls", "ge", "lt", "gt", "le", "al", "nv"
};

/* ============================================================================
 * Decode ARM Register List (for LDM/STM)
 * ============================================================================ */
static void decode_register_list(uint16_t reg_list, int *regs, int *count)
{
    *count = 0;
    for (int i = 0; i < 16; i++) {
        if (reg_list & (1u << i)) {
            regs[(*count)++] = i;
        }
    }
}

/* ============================================================================
 * Decode ARM Immediate (rotated immediate)
 * ============================================================================ */
static uint32_t decode_rotated_imm(uint32_t inst)
{
    uint32_t imm = ARM_IMM(inst);
    uint32_t rot = ARM_ROTATE(inst) * 2;
    if (rot == 0) return imm;
    return (imm >> rot) | (imm << (32 - rot));
}

/* ============================================================================
 * Decode ARM Shift Operand (Operand2)
 * ============================================================================ */
typedef struct {
    bool is_immediate;
    uint32_t imm;          /* If immediate */
    uint32_t rm;           /* If register */
    uint32_t shift_type;
    uint32_t shift_amount;
    uint32_t rs;           /* Shift amount register */
} arm_operand2_t;

static void decode_operand2(uint32_t inst, arm_operand2_t *op2)
{
    if (ARM_BIT(inst, 25)) {
        /* Immediate: 8-bit value rotated by even amount */
        op2->is_immediate = true;
        op2->imm = decode_rotated_imm(inst);
        op2->shift_type = ARM_SHIFT_LSL;
        op2->shift_amount = 0;
        op2->rm = 0;
        op2->rs = 0;
    } else {
        /* Register shifted by register or immediate */
        op2->is_immediate = false;
        op2->rm = ARM_BITS(inst, 3, 0);
        op2->shift_type = ARM_SHIFT_TYPE(inst);
        op2->shift_amount = ARM_BITS(inst, 11, 7);
        op2->rs = ARM_RS(inst);

        if (ARM_BIT(inst, 4)) {
            /* Register-specified shift */
            op2->shift_amount = 0; /* Will use rs */
        }
    }
}

/* Apply a shift to a register value (ARMv4 aware). */
static uint32_t apply_shift(uint32_t value, uint32_t shift_type, uint32_t amount)
{
    if (amount == 0) {
        switch (shift_type) {
            case ARM_SHIFT_LSL: return value;
            case ARM_SHIFT_LSR: return 0u;
            case ARM_SHIFT_ASR: return (int32_t)value < 0 ? 0xFFFFFFFFu : 0u;
            case ARM_SHIFT_ROR: return value;
            default: return value;
        }
    }
    amount &= 31u;
    switch (shift_type) {
        case ARM_SHIFT_LSL: return value << amount;
        case ARM_SHIFT_LSR: return value >> amount;
        case ARM_SHIFT_ASR: return (uint32_t)((int32_t)value >> (int)amount);
        case ARM_SHIFT_ROR: return (value >> amount) | (value << (32 - amount));
        default: return value;
    }
}

/* ============================================================================
 * Decode A32 Data Processing Instructions
 * ============================================================================ */
static bool decode_data_processing(uint32_t inst, atl_arm_decoded_inst_t *d)
{
    d->cond = ARM_COND(inst);
    d->op = ARM_OP(inst);
    d->opcode = ARM_DP_OPCODE(inst);   /* 4-bit opcode bits[24:21] */
    d->set_flags = ARM_S_BIT(inst);
    d->rn = ARM_RN(inst);
    d->rd = ARM_RD(inst);
    d->thumb = false;
    d->neon = false;
    d->inst_class = ARM_INST_DATA_PROCESSING;
    d->mem_size = 4;

    bool imm = ARM_BIT(inst, 25);      /* I bit */

    if (imm) {
        d->imm = decode_rotated_imm(inst);
        d->rm = 0;
        d->shift = ARM_SHIFT_LSL;
        d->shamt = 0;
        d->rs = 0;
    } else {
        d->rm = ARM_BITS(inst, 3, 0);
        d->shift = ARM_BITS(inst, 6, 5);
        if (ARM_BIT(inst, 4)) {
            /* Register-specified shift */
            d->rs = ARM_BITS(inst, 11, 8);
            d->shamt = 0;
        } else {
            d->rs = 0;
            d->shamt = ARM_BITS(inst, 11, 7);
        }
    }

    /* Multiply class: op==00, bit[4]==1 */
    if (ARM_BIT(inst, 4)) {
        uint32_t op23_21 = ARM_BITS(inst, 23, 21);
        if (op23_21 == 0x0) {
            /* Multiply / multiply-accumulate / long multiply */
            d->inst_class = ARM_INST_MULTIPLY;
            d->rm = ARM_BITS(inst, 3, 0);
            d->rs = ARM_BITS(inst, 11, 8);
            d->rn = ARM_RN(inst);
            d->rd = ARM_RD(inst);
            /* Distinguish by bits[7:4] */
            uint32_t b7_4 = ARM_BITS(inst, 7, 4);
            if (b7_4 == 0x1) {
                d->opcode = 0x10; /* MUL */
            } else if (b7_4 == 0x3) {
                d->opcode = 0x11; /* MLA */
            } else {
                /* Long multiply: distinguish UMULL/UMLAL/SMULL/SMLAL */
                d->opcode = 0x12; /* generic long multiply */
            }
            d->set_flags = ARM_S_BIT(inst);
        }
    }

    return true;
}

/* ============================================================================
 * Decode A32 Load/Store Instructions
 * ============================================================================ */
static bool decode_load_store(uint32_t inst, atl_arm_decoded_inst_t *d)
{
    d->cond = ARM_COND(inst);
    d->op = ARM_OP(inst);  /* 01 = load/store */
    d->set_flags = false;
    d->rn = ARM_RN(inst);
    d->rd = ARM_RD(inst);
    d->thumb = false;
    d->neon = false;
    d->inst_class = ARM_INST_LOAD_STORE;
    d->load = ARM_BIT(inst, 20);   /* L bit */
    d->writeback = ARM_BIT(inst, 21); /* W bit */
    d->up = ARM_BIT(inst, 23);     /* U bit */
    d->pre_index = ARM_BIT(inst, 24); /* P bit */
    d->offset = (int32_t)(uint32_t)ARM_OFFSET(inst);

    /* Size: bits[22:21], plus LDRD/STRD bit[22]=1,bit[21]=... */
    bool is_byte  = !ARM_BIT(inst, 22) &&  ARM_BIT(inst, 21);  /* 01 */
    bool is_hword =  ARM_BIT(inst, 22) && !ARM_BIT(inst, 21);  /* 10 */
    bool is_long  =  ARM_BIT(inst, 22) &&  ARM_BIT(inst, 21);  /* 11 -> LDRD/STRD */

    if (is_long) {
        d->mem_size = 8;
        d->inst_class = ARM_INST_LOAD_STORE;
    } else if (is_byte) {
        d->mem_size = 1;
    } else if (is_hword) {
        d->mem_size = 2;
    } else {
        d->mem_size = 4;
    }

    /* Offset: immediate or register */
    if (ARM_BIT(inst, 25)) {
        /* Immediate offset */
        d->rm = 0;
        d->shift = ARM_SHIFT_LSL;
        d->shamt = 0;
        d->rs = 0;
    } else {
        /* Register offset */
        d->rm = ARM_BITS(inst, 3, 0);
        d->shift = ARM_SHIFT_TYPE(inst);
        d->shamt = ARM_BITS(inst, 6, 5);
        d->rs = 0;
    }

    return true;
}

/* ============================================================================
 * Decode A32 Branch Instructions
 * ============================================================================ */
static bool decode_branch(uint32_t inst, atl_arm_decoded_inst_t *d)
{
    d->cond = ARM_COND(inst);
    d->op = ARM_OP(inst);
    d->imm = ARM_IMM24(inst) << 2;  /* Shifted 24-bit offset */
    d->thumb = false;
    d->neon = false;
    d->inst_class = ARM_INST_BRANCH;
    d->mem_size = 0;
    d->link = ARM_BIT(inst, 24);    /* BL bit */

    /* Sign extend 24-bit immediate (now shifted, 26 bits) */
    if (d->imm & (1u << 25)) {
        d->imm |= ~((1u << 26) - 1);
    }

    /* BX / BLX detection: op==00 with bit[4]==1 and specific pattern.
     * BX: cond 00010000 ... 0001 Rm */
    if (ARM_OP(inst) == 0x0 && ARM_BIT(inst, 4)) {
        uint32_t b27_20 = ARM_BITS(inst, 27, 20);
        uint32_t b11_4  = ARM_BITS(inst, 11, 4);
        if (b27_20 == 0x010 && b11_4 == 0x010) {
            d->inst_class = ARM_INST_THUMB_SPECIAL; /* BX/BLX class */
            d->rm = ARM_BITS(inst, 3, 0);
            d->link = false;
        }
    }

    return true;
}

/* ============================================================================
 * Decode A32 Multiply Instructions
 * ============================================================================ */
static bool decode_multiply(uint32_t inst, atl_arm_decoded_inst_t *d)
{
    d->cond = ARM_COND(inst);
    d->set_flags = ARM_S_BIT(inst);
    d->rn = ARM_RN(inst);
    d->rd = ARM_RD(inst);
    d->rs = ARM_RS(inst);
    d->rm = ARM_BITS(inst, 3, 0);
    d->thumb = false;
    d->neon = false;
    d->inst_class = ARM_INST_MULTIPLY;
    d->mem_size = 4;
    d->opcode = ARM_BITS(inst, 21, 20);

    return true;
}

/* ============================================================================
 * Decode A32 Load/Store Multiple (LDM/STM)
 * ============================================================================ */
static bool decode_load_store_multiple(uint32_t inst, atl_arm_decoded_inst_t *d)
{
    d->cond = ARM_COND(inst);
    d->rn = ARM_RN(inst);
    d->writeback = ARM_BIT(inst, 21);
    d->up = ARM_BIT(inst, 23);
    d->pre_index = ARM_BIT(inst, 24);
    d->load = ARM_BIT(inst, 20);
    d->thumb = false;
    d->neon = false;
    d->inst_class = ARM_INST_LOAD_STORE_MULTIPLE;
    d->mem_size = 4;
    d->imm = ARM_BITS(inst, 15, 0);  /* Register list */
    d->offset = (int32_t)(uint32_t)ARM_BITS(inst, 15, 0);

    int regs[16];
    decode_register_list((uint16_t)d->imm, regs, (int *)&d->reg_count);

    return true;
}

/* ============================================================================
 * Decode A32 Coprocessor / SVC (stub)
 * ============================================================================ */
static bool decode_coprocessor(uint32_t inst, atl_arm_decoded_inst_t *d)
{
    (void)inst;
    d->thumb = false;
    d->neon = false;
    d->inst_class = ARM_INST_COPROCESSOR;
    return false; /* stub - not decoded */
}

static bool decode_svc(uint32_t inst, atl_arm_decoded_inst_t *d)
{
    d->cond = ARM_COND(inst);
    d->thumb = false;
    d->neon = false;
    d->inst_class = ARM_INST_SWITCH;  /* SVC/SWI */
    d->imm = ARM_IMM24(inst);
    return true;
}

/* ============================================================================
 * A32 decode dispatch table (opcode masks)
 * ============================================================================ */
typedef bool (*arm_decode_func_t)(uint32_t inst, atl_arm_decoded_inst_t *d);

typedef struct {
    uint32_t mask;
    uint32_t pattern;
    arm_decode_func_t func;
} arm_decode_entry_t;

static const arm_decode_entry_t a32_decode_table[] = {
    /* op[27:26] == 00: data processing / multiply */
    {0x0C000000, 0x00000000, decode_data_processing},
    /* op[27:26] == 01: load/store single */
    {0x0C000000, 0x04000000, decode_load_store},
    /* op[27:26] == 10, bit[25]==0: load/store multiple (LDM/STM) */
    {0x0E000000, 0x08000000, decode_load_store_multiple},
    /* op[27:24] == 1010: branch (B / BL) */
    {0x0F000000, 0x0A000000, decode_branch},
    /* op[27:24] == 1110: coprocessor */
    {0x0F000000, 0x0E000000, decode_coprocessor},
    /* op[27:24] == 1111: SVC / undefined */
    {0x0F000000, 0x0F000000, decode_svc},
};

#define A32_TABLE_SIZE (sizeof(a32_decode_table) / sizeof(a32_decode_table[0]))

/* ============================================================================
 * Decode Thumb Instruction (16-bit)
 * ============================================================================ */
static bool decode_thumb16(uint16_t inst16, atl_arm_decoded_inst_t *d)
{
    uint32_t opcode = THUMB_OPCODE(inst16);
    d->thumb = true;
    d->neon = false;
    d->cond = ARM_COND_AL;  /* Thumb instructions are unconditional */
    d->mem_size = 4;
    d->rn = ARM_REG_R0;
    d->rd = ARM_REG_R0;
    d->rm = ARM_REG_R0;

    /* ---- Special Thumb instructions (direct bitmask checks) ---- */

    if ((inst16 & 0xFF80) == 0x4000) {
        /* BX: 010000 00000000 Rm */
        d->inst_class = ARM_INST_THUMB_SPECIAL;
        d->opcode = 0x30;
        d->rm = ARM_BITS(inst16, 2, 0);
        return true;
    }
    if ((inst16 & 0xFF00) == 0xDF00) {
        /* SWI: 11011111 imm8 */
        d->inst_class = ARM_INST_THUMB_SVC_BKPT;
        d->opcode = 0x60;
        d->imm = ARM_BITS(inst16, 7, 0);
        return true;
    }
    if ((inst16 & 0xFF00) == 0xBE00) {
        /* BKPT: 10111110 imm8 */
        d->inst_class = ARM_INST_THUMB_SVC_BKPT;
        d->opcode = 0x61;
        d->imm = ARM_BITS(inst16, 7, 0);
        return true;
    }
    if ((inst16 & 0xF000) == 0xD000) {
        /* Conditional branch: 1101 cond imm8 */
        d->inst_class = ARM_INST_THUMB_COND_BRANCH;
        d->cond = ARM_BITS(inst16, 10, 8);
        d->imm = (int32_t)(uint32_t)ARM_BITS(inst16, 7, 0) << 1;
        d->opcode = 0x61;
        return true;
    }

    /* ---- Regular Thumb-2 16-bit encodings (switch on bits[15:11]) ---- */
    switch (opcode) {
        case 0x00: case 0x01: case 0x02: /* Shift immediate: 0000x imm5 Rd Rs */
        {
            d->inst_class = ARM_INST_THUMB_DATA_PROC;
            uint32_t st = (opcode == 0x00) ? ARM_SHIFT_LSL :
                          (opcode == 0x01) ? ARM_SHIFT_LSR : ARM_SHIFT_ASR;
            d->shift = st;
            d->shamt = ARM_BITS(inst16, 10, 6);
            d->rd = ARM_BITS(inst16, 5, 3);
            d->rm = ARM_BITS(inst16, 2, 0);
            d->opcode = (opcode == 0x00) ? 0x20 :
                        (opcode == 0x01) ? 0x21 : 0x22;
            break;
        }
        case 0x03: /* ADD/SUB (register): 00011 ... */
        {
            d->inst_class = ARM_INST_THUMB_DATA_PROC;
            d->opcode = ARM_BIT(inst16, 10) ? 0x02 : 0x04; /* SUB / ADD */
            d->rd = ARM_BITS(inst16, 6, 4) | (ARM_BIT(inst16, 10) << 3);
            d->rn = ARM_BITS(inst16, 2, 0);
            d->rm = ARM_BITS(inst16, 2, 0);
            d->imm = ARM_BITS(inst16, 9, 6);
            break;
        }
        case 0x04: case 0x05: /* ADD/SUB (immediate): 0100x ... */
        {
            d->inst_class = ARM_INST_THUMB_DATA_PROC;
            d->opcode = ARM_BIT(inst16, 10) ? 0x02 : 0x04; /* SUB / ADD */
            d->rn = ARM_BITS(inst16, 5, 3);
            d->rd = ARM_BITS(inst16, 2, 0);
            d->imm = ARM_BITS(inst16, 9, 6);
            break;
        }
        case 0x06: case 0x07: /* ALU / high-register: 01001x / 01000x ... */
        {
            d->inst_class = ARM_INST_THUMB_DATA_PROC;
            d->opcode = 0x04; /* ADD */
            d->rd = ARM_BITS(inst16, 6, 4);
            d->rn = ARM_BITS(inst16, 2, 0);
            break;
        }
        case 0x08: case 0x09: /* LDR/STR register / PC-relative load: 0100x */
        {
            d->inst_class = ARM_INST_THUMB_LOAD_STORE;
            d->load = ARM_BIT(inst16, 10);
            d->mem_size = ARM_BIT(inst16, 9) ? 1 : 4;
            d->opcode = d->load ? 0x40 : 0x41;
            d->rd = ARM_BITS(inst16, 6, 4) | (ARM_BIT(inst16, 7) << 3);
            d->rn = ARM_BITS(inst16, 2, 0);
            d->imm = ARM_BITS(inst16, 7, 0);
            break;
        }
        case 0x0A: case 0x0B: /* LDR/STR immediate: 0101x ... */
        {
            d->inst_class = ARM_INST_THUMB_LOAD_STORE;
            d->load = ARM_BIT(inst16, 10);
            d->mem_size = ARM_BIT(inst16, 9) ? 1 : 4;
            d->opcode = d->load ? 0x40 : 0x41;
            d->rn = ARM_BITS(inst16, 5, 3);
            d->rd = ARM_BITS(inst16, 2, 0);
            d->imm = ARM_BITS(inst16, 8, 6);
            break;
        }
        case 0x0C: case 0x0D: /* LDRH/STRH / LDRSB/LDRSH: 01001x ... */
        {
            d->inst_class = ARM_INST_THUMB_LOAD_STORE;
            d->load = ARM_BIT(inst16, 10);
            d->mem_size = ARM_BIT(inst16, 9) ? 2 : 1;
            d->opcode = d->load ? (d->mem_size == 2 ? 0x42 : 0x45) :
                                 (d->mem_size == 2 ? 0x43 : 0x41);
            d->rn = ARM_BITS(inst16, 5, 3);
            d->rd = ARM_BITS(inst16, 2, 0);
            d->imm = ARM_BITS(inst16, 8, 6);
            break;
        }
        case 0x0E: case 0x0F: /* LDM/STM: 01100x ... */
        {
            d->inst_class = ARM_INST_THUMB_LOAD_STORE_MULTI;
            d->load = ARM_BIT(inst16, 10);
            d->writeback = ARM_BIT(inst16, 9);
            d->rn = ARM_BITS(inst16, 7, 3);
            d->imm = ARM_BITS(inst16, 7, 0);
            d->mem_size = 4;
            d->opcode = d->load ? 0x50 : 0x51;
            break;
        }
        case 0x10: case 0x11: case 0x12: case 0x13:
        case 0x14: case 0x15: case 0x16: case 0x17:
        {
            /* Unconditional / other branch encodings */
            d->inst_class = ARM_INST_THUMB_BRANCH;
            d->opcode = 0x70;
            d->imm = (int32_t)(uint32_t)ARM_BITS(inst16, 10, 0) << 1;
            break;
        }
        default:
            d->inst_class = ARM_INST_UNDEFINED;
            return false;
    }

    return true;
}

/* ============================================================================
 * Decode Thumb-2 32-bit Instruction
 * ============================================================================ */
static bool decode_thumb32(uint32_t inst32, atl_arm_decoded_inst_t *d)
{
    uint16_t inst16 = (uint16_t)(inst32 >> 16);
    uint32_t op1 = ARM_BITS(inst16, 15, 11); /* bits[27:23] of 32-bit */
    d->thumb = true;
    d->neon = false;
    d->cond = ARM_COND_AL;
    d->mem_size = 4;
    d->rn = ARM_BITS(inst32, 19, 16);
    d->rd = ARM_BITS(inst32, 15, 12);

    if (op1 == 0x1E || op1 == 0x1F) {
        /* 32-bit Thumb-2 */
        uint32_t op2 = ARM_BITS(inst16, 10, 9);
        uint32_t opcode = ARM_BITS(inst32, 20, 16);

        d->opcode = opcode;

        if (op1 == 0x1F && op2 == 0x0) {
            /* Data processing (shifted register / immediate) */
            d->inst_class = ARM_INST_THUMB_DATA_PROC;
            d->rm = ARM_BITS(inst32, 3, 0);
            d->imm = ARM_BITS(inst32, 11, 0);
            d->shift = ARM_BITS(inst32, 6, 5);
            d->shamt = ARM_BITS(inst32, 11, 7);
        } else if (op1 == 0x1F && op2 == 0x1) {
            /* Load/store */
            d->inst_class = ARM_INST_THUMB_LOAD_STORE;
            d->load = ARM_BIT(inst32, 20);
            d->mem_size = ARM_BIT(inst32, 22) ? (ARM_BIT(inst32, 21) ? 8 : 2) : (ARM_BIT(inst32, 21) ? 1 : 4);
            d->writeback = ARM_BIT(inst32, 21);
            d->up = ARM_BIT(inst32, 23);
            d->pre_index = ARM_BIT(inst32, 24);
            d->imm = ARM_BITS(inst32, 11, 0);
            d->rn = ARM_BITS(inst32, 19, 16);
            d->rd = ARM_BITS(inst32, 15, 12);
        } else if (op1 == 0x1E && op2 == 0x0) {
            /* Branch */
            d->inst_class = ARM_INST_BRANCH;
            d->imm = ARM_BITS(inst32, 23, 0) << 2;
            d->link = ARM_BIT(inst32, 26);
            d->opcode = 0x70;
        } else if (op1 == 0x1E && op2 == 0x1) {
            /* Coprocessor */
            d->inst_class = ARM_INST_COPROCESSOR;
        } else {
            d->inst_class = ARM_INST_UNDEFINED;
            return false;
        }
    } else {
        d->inst_class = ARM_INST_UNDEFINED;
        return false;
    }

    return true;
}

/* ============================================================================
 * NEON/VFP Decoder (stub)
 * ============================================================================ */
static bool decode_neon(uint32_t inst, atl_arm_decoded_inst_t *d)
{
    memset(d, 0, sizeof(*d));
    d->neon = true;
    d->thumb = false;
    d->inst_class = ARM_INST_NEON_DATA_PROC;

    /* NEON/VFP coprocessor encoding: 1111 1110 ... */
    if ((inst & 0x0F000000) == 0x0E000000) {
        uint32_t op = ARM_BITS(inst, 23, 20);
        switch (op) {
            case 0x0: d->opcode = 0x80; d->mem_size = 16; break; /* VADD */
            case 0x1: d->opcode = 0x81; d->mem_size = 16; break; /* VSUB */
            case 0x2: d->opcode = 0x82; d->mem_size = 16; break; /* VMUL */
            case 0x3: d->opcode = 0x83; d->mem_size = 16; break; /* VLDM */
            case 0x4: d->opcode = 0x84; d->mem_size = 16; break; /* VSTM */
            default: d->opcode = 0x8F; break;
        }
        d->rd = ARM_RD(inst);
        d->rn = ARM_RN(inst);
        return true;
    }

    return false;
}

/* ============================================================================
 * Main A32 Decoder
 * ============================================================================ */
bool atl_arm_decode_a32(uint32_t inst, atl_arm_decoded_inst_t *out)
{
    if (!out) return false;

    memset(out, 0, sizeof(*out));
    out->cond = ARM_COND(inst);

    /* Unconditional (cond == 1111) special handling */
    if (ARM_COND(inst) == ARM_COND_NV) {
        out->cond = ARM_COND_AL;
    }

    /* Dispatch via decode table (opcode masks) */
    for (size_t i = 0; i < A32_TABLE_SIZE; i++) {
        const arm_decode_entry_t *e = &a32_decode_table[i];
        if ((inst & e->mask) == e->pattern) {
            return e->func(inst, out);
        }
    }

    out->inst_class = ARM_INST_UNDEFINED;
    return false;
}

/* ============================================================================
 * Thumb Decoder
 * ============================================================================ */
bool atl_arm_decode_thumb(uint16_t inst16, uint32_t inst32, bool is_32bit,
                          atl_arm_decoded_inst_t *out)
{
    if (!out) return false;

    memset(out, 0, sizeof(*out));

    if (is_32bit) {
        return decode_thumb32(inst32, out);
    } else {
        return decode_thumb16(inst16, out);
    }
}

/* ============================================================================
 * NEON/VFP Decoder (public stub)
 * ============================================================================ */
bool atl_arm_decode_neon(uint32_t inst, atl_arm_decoded_inst_t *out)
{
    if (!out) return false;
    return decode_neon(inst, out);
}

/* ============================================================================
 * ARM -> MIR mapping table (opcode masks)
 * ============================================================================ */

static const atl_arm_to_mir_mapping_t arm_to_mir_table[] = {
    /* ---- A32 Data Processing (mask ignores I bit and cond) ---- */
    {0x0DE00000, 0x00000000, ATL_MIR_OP_AND,  false, false},  /* AND */
    {0x0DE00000, 0x00200000, ATL_MIR_OP_XOR,  false, false},  /* EOR */
    {0x0DE00000, 0x00400000, ATL_MIR_OP_SUB,  false, false},  /* SUB */
    {0x0DE00000, 0x00600000, ATL_MIR_OP_SUB,  false, false},  /* RSB */
    {0x0DE00000, 0x00800000, ATL_MIR_OP_ADD,  false, false},  /* ADD */
    {0x0DE00000, 0x00A00000, ATL_MIR_OP_ADD,  false, false},  /* ADC */
    {0x0DE00000, 0x00C00000, ATL_MIR_OP_SUB,  false, false},  /* SBC */
    {0x0DE00000, 0x00E00000, ATL_MIR_OP_SUB,  false, false},  /* RSC */
    {0x0DE00000, 0x01000000, ATL_MIR_OP_AND,  false, false},  /* TST */
    {0x0DE00000, 0x01200000, ATL_MIR_OP_XOR,  false, false},  /* TEQ */
    {0x0DE00000, 0x01400000, ATL_MIR_OP_CMP_EQ, false, false},/* CMP */
    {0x0DE00000, 0x01600000, ATL_MIR_OP_ADD,  false, false},  /* CMN */
    {0x0DE00000, 0x01800000, ATL_MIR_OP_OR,   false, false},  /* ORR */
    {0x0DE00000, 0x01A00000, ATL_MIR_OP_ADD,  false, false},  /* MOV */
    {0x0DE00000, 0x01C00000, ATL_MIR_OP_AND,  false, false},  /* BIC */
    {0x0DE00000, 0x01E00000, ATL_MIR_OP_NOT,  false, false},  /* MVN */

    /* ---- A32 Multiply ---- */
    {0x0CE000F0, 0x00000010, ATL_MIR_OP_MUL,  false, false},  /* MUL / MLA / long mul */

    /* ---- A32 Load/Store ---- */
    {0x0C700000, 0x04100000, ATL_MIR_OP_LOAD32,  false, false}, /* LDR word */
    {0x0C700000, 0x04000000, ATL_MIR_OP_STORE32, false, false}, /* STR word */
    {0x0C700000, 0x05100000, ATL_MIR_OP_LOAD8,   false, false}, /* LDRB */
    {0x0C700000, 0x05000000, ATL_MIR_OP_STORE8,  false, false}, /* STRB */
    {0x0C700000, 0x06100000, ATL_MIR_OP_LOAD16,  false, false}, /* LDRH */
    {0x0C700000, 0x06000000, ATL_MIR_OP_STORE16, false, false}, /* STRH */
    {0x0C700000, 0x07100000, ATL_MIR_OP_LOAD64,  false, false}, /* LDRD */
    {0x0C700000, 0x07000000, ATL_MIR_OP_STORE64, false, false}, /* STRD */

    /* ---- A32 Branch ---- */
    {0x0F000000, 0x0A000000, ATL_MIR_OP_BR,        false, false}, /* B */
    {0x0F000000, 0x0B000000, ATL_MIR_OP_BR,        false, false}, /* BL */

    /* ---- A32 Load/Store Multiple ---- */
    {0x0CE00000, 0x08000000, ATL_MIR_OP_LOAD32,  false, false}, /* LDM */
    {0x0CE00000, 0x08000000, ATL_MIR_OP_STORE32, false, false}, /* STM (load bit decides) */

    /* ---- A32 SVC/SWI ---- */
    {0x0F000000, 0x0F000000, ATL_MIR_OP_SYSCALL,  false, false},

    /* ---- Thumb-2 16-bit / 32-bit (class-based, matched in decoder) ---- */
    {0xF800, 0x0000, ATL_MIR_OP_SHL,  true, false},  /* Thumb LSL */
    {0xF800, 0x0800, ATL_MIR_OP_SHR,  true, false},  /* Thumb LSR */
    {0xF800, 0x1000, ATL_MIR_OP_SAR,  true, false},  /* Thumb ASR */
    {0xF000, 0x1000, ATL_MIR_OP_BR,   true, false},  /* Thumb B (cond branch) */
    {0xF800, 0x4000, ATL_MIR_OP_ADD,  true, false},  /* Thumb ADD imm */
    {0xF800, 0x5000, ATL_MIR_OP_SUB,  true, false},  /* Thumb SUB imm */
    {0xFF00, 0xDF00, ATL_MIR_OP_SYSCALL,true, false}, /* Thumb SWI */
    {0xFFF0, 0x4700, ATL_MIR_OP_INDIRECT_CALL, true, false}, /* Thumb BX */

    /* ---- NEON/VECTOR ---- */
    {0x0F0000F0, 0x0E000000, ATL_MIR_OP_VADD,  false, true},
    {0x0F0000F0, 0x0E000010, ATL_MIR_OP_VSUB,  false, true},
    {0x0F0000F0, 0x0E000020, ATL_MIR_OP_VMUL,  false, true},
    {0x0F0000F0, 0x0E000030, ATL_MIR_OP_VLOAD, false, true},
    {0x0F0000F0, 0x0E000040, ATL_MIR_OP_VSTORE,false, true},
};

static const size_t arm_to_mir_table_size =
    sizeof(arm_to_mir_table) / sizeof(arm_to_mir_table[0]);

/* Provide the symbols expected by mir_opcodes.h (mir_builder.c stub removed). */
const atl_arm_to_mir_mapping_t atl_arm_to_mir_table[] = {
    {0, 0, ATL_MIR_OP_INVALID, false, false}
};
const size_t atl_arm_to_mir_table_size = 1;

/* ============================================================================
 * atl_arm_decode_to_mir: map ARM instruction to MIR opcode
 * ============================================================================ */

static atl_mir_opcode_t map_dp_opcode(uint32_t opcode)
{
    switch (opcode) {
        case 0x0: return ATL_MIR_OP_AND;   /* AND */
        case 0x1: return ATL_MIR_OP_XOR;   /* EOR */
        case 0x2: return ATL_MIR_OP_SUB;   /* SUB */
        case 0x3: return ATL_MIR_OP_SUB;   /* RSB */
        case 0x4: return ATL_MIR_OP_ADD;   /* ADD */
        case 0x5: return ATL_MIR_OP_ADD;   /* ADC */
        case 0x6: return ATL_MIR_OP_SUB;   /* SBC */
        case 0x7: return ATL_MIR_OP_SUB;   /* RSC */
        case 0x8: return ATL_MIR_OP_AND;   /* TST */
        case 0x9: return ATL_MIR_OP_XOR;   /* TEQ */
        case 0xA: return ATL_MIR_OP_CMP_EQ;/* CMP */
        case 0xB: return ATL_MIR_OP_ADD;   /* CMN */
        case 0xC: return ATL_MIR_OP_OR;    /* ORR */
        case 0xD: return ATL_MIR_OP_ADD;   /* MOV */
        case 0xE: return ATL_MIR_OP_AND;   /* BIC */
        case 0xF: return ATL_MIR_OP_NOT;   /* MVN */
        default:  return ATL_MIR_OP_INVALID;
    }
}

static atl_mir_opcode_t map_load_store(bool load, uint32_t mem_size)
{
    if (load) {
        switch (mem_size) {
            case 1: return ATL_MIR_OP_LOAD8;
            case 2: return ATL_MIR_OP_LOAD16;
            case 4: return ATL_MIR_OP_LOAD32;
            case 8: return ATL_MIR_OP_LOAD64;
        }
    } else {
        switch (mem_size) {
            case 1: return ATL_MIR_OP_STORE8;
            case 2: return ATL_MIR_OP_STORE16;
            case 4: return ATL_MIR_OP_STORE32;
            case 8: return ATL_MIR_OP_STORE64;
        }
    }
    return ATL_MIR_OP_INVALID;
}

static atl_mir_opcode_t map_decoded_to_mir(const atl_arm_decoded_inst_t *d)
{
    if (d->neon) {
        switch (d->opcode) {
            case 0x80: return ATL_MIR_OP_VADD;
            case 0x81: return ATL_MIR_OP_VSUB;
            case 0x82: return ATL_MIR_OP_VMUL;
            case 0x83: return ATL_MIR_OP_VLOAD;
            case 0x84: return ATL_MIR_OP_VSTORE;
            default:   return ATL_MIR_OP_INVALID;
        }
    }

    switch (d->inst_class) {
        case ARM_INST_DATA_PROCESSING:
            return map_dp_opcode(d->opcode & 0xF);
        case ARM_INST_MULTIPLY:
            return ATL_MIR_OP_MUL;
        case ARM_INST_LOAD_STORE:
            return map_load_store(d->load, d->mem_size);
        case ARM_INST_BRANCH:
            if (d->link) return ATL_MIR_OP_BR;
            return (d->inst_class == ARM_INST_THUMB_SPECIAL) ?
                   ATL_MIR_OP_INDIRECT_CALL : ATL_MIR_OP_BR;
        case ARM_INST_LOAD_STORE_MULTIPLE:
            return map_load_store(d->load, 4);
        case ARM_INST_SWITCH:
            return ATL_MIR_OP_SYSCALL;
        case ARM_INST_COPROCESSOR:
            return ATL_MIR_OP_INVALID;
        case ARM_INST_THUMB_DATA_PROC:
            return map_dp_opcode(d->opcode & 0xF);
        case ARM_INST_THUMB_SPECIAL:
            return ATL_MIR_OP_INDIRECT_CALL; /* BX */
        case ARM_INST_THUMB_BRANCH:
            return ATL_MIR_OP_BR;
        case ARM_INST_THUMB_LOAD_STORE:
            return map_load_store(d->load, d->mem_size);
        case ARM_INST_THUMB_LOAD_STORE_MULTI:
            return map_load_store(d->load, 4);
        case ARM_INST_THUMB_COND_BRANCH:
            return ATL_MIR_OP_BR_IF;
        case ARM_INST_THUMB_SVC_BKPT:
            return ATL_MIR_OP_SYSCALL;
        case ARM_INST_NEON_DATA_PROC:
        case ARM_INST_NEON_LOAD_STORE:
            return ATL_MIR_OP_INVALID; /* stub */
        default:
            return ATL_MIR_OP_INVALID;
    }
}

atl_mir_opcode_t atl_arm_decode_to_mir(uint32_t arm_inst, bool is_thumb, bool is_neon)
{
    /* NEON instructions */
    if (is_neon) {
        atl_arm_decoded_inst_t d;
        if (atl_arm_decode_neon(arm_inst, &d)) {
            return map_decoded_to_mir(&d);
        }
        return ATL_MIR_OP_INVALID;
    }

    if (is_thumb) {
        /* Distinguish 16-bit vs 32-bit Thumb-2 */
        bool is_32bit = (arm_inst & 0xF800) == 0xF800 ||
                        (arm_inst & 0xF800) == 0xE800;
        uint16_t inst16 = (uint16_t)arm_inst;
        atl_arm_decoded_inst_t d;
        if (!atl_arm_decode_thumb(inst16, arm_inst, is_32bit, &d))
            return ATL_MIR_OP_INVALID;
        return map_decoded_to_mir(&d);
    }

    /* A32 */
    atl_arm_decoded_inst_t d;
    if (!atl_arm_decode_a32(arm_inst, &d))
        return ATL_MIR_OP_INVALID;
    return map_decoded_to_mir(&d);
}

/* ============================================================================
 * Convert decoded ARM instruction to MIR block
 * ============================================================================ */

atl_mir_block_t *atl_arm_to_mir_block(atl_mir_function_t *func,
                                       const atl_arm_decoded_inst_t *decoded,
                                       uint32_t pc)
{
    if (!func || !decoded) return NULL;

    atl_mir_context_t *ctx = atl_mir_context_create();
    atl_mir_builder_t *b = atl_mir_builder_create(func);

    atl_mir_opcode_t mir_op = map_decoded_to_mir(decoded);
    if (mir_op == ATL_MIR_OP_INVALID) {
        atl_mir_builder_destroy(b);
        atl_mir_context_destroy(ctx);
        return NULL;
    }

    atl_mir_block_t *block = atl_mir_builder_create_block(b, "block");
    atl_mir_builder_set_block(b, block);

    atl_mir_type_t i32 = atl_mir_type_int(32);

    /* Build operands: rn, rd, rm, immediate */
    atl_mir_value_t *ops[ATL_MIR_MAX_OPERANDS];
    uint32_t nops = 0;

    /* Source: rn (if valid) */
    if (decoded->rn != 0 && decoded->rn != 15) {
        ops[nops++] = atl_mir_builder_const_int(b, i32, (int64_t)decoded->rn);
    }
    if (decoded->rm != 0) {
        ops[nops++] = atl_mir_builder_const_int(b, i32, (int64_t)decoded->rm);
    }
    if (decoded->imm != 0 || decoded->shamt != 0) {
        ops[nops++] = atl_mir_builder_const_int(b, i32,
            (int64_t)(decoded->imm ? decoded->imm : decoded->shamt));
    }

    /* Branch targets */
    if (mir_op == ATL_MIR_OP_BR && decoded->inst_class == ARM_INST_BRANCH) {
        uint32_t target = pc + 4 + decoded->imm;
        atl_mir_value_t *tgt = atl_mir_builder_const_int(b, i32, (int64_t)target);
        ops[nops++] = tgt;
    }

    atl_mir_builder_inst(b, mir_op, i32, nops, ops);
    atl_mir_inst_t *last = block->last_inst;
    if (last) last->arm_pc = pc;

    /* Terminator for branches */
    if (atl_mir_opcode_is_terminator(mir_op)) {
        /* already a terminator via builder */
    } else {
        /* fall-through handled by caller */
    }

    atl_mir_builder_destroy(b);
    atl_mir_context_destroy(ctx);
    return block;
}

/* ============================================================================
 * ARM Function Decoder
 * ============================================================================ */
atl_mir_function_t *atl_arm_decode_function(atl_mir_module_t *module,
                                             const uint8_t *code, size_t code_size,
                                             uint32_t start_pc, bool is_thumb)
{
    if (!module || !code || code_size == 0) return NULL;

    atl_mir_context_t *ctx = atl_mir_context_create();
    atl_mir_function_t *func = atl_mir_module_add_function(module, "<arm_func>",
        atl_mir_type_int(32), 0, NULL);
    if (!func) {
        atl_mir_context_destroy(ctx);
        return NULL;
    }

    func->is_thumb = is_thumb;
    func->arm_start_pc = start_pc;
    func->arm_end_pc = start_pc + (uint32_t)code_size;

    atl_mir_builder_t *b = atl_mir_builder_create(func);
    atl_mir_block_t *entry = atl_mir_builder_create_block(b, "entry");
    atl_mir_builder_set_block(b, entry);

    uint32_t pc = start_pc;
    uint32_t end_pc = start_pc + (uint32_t)code_size;

    while (pc < end_pc) {
        atl_arm_decoded_inst_t d;
        bool ok;

        if (is_thumb && (pc & 1u)) {
            /* Thumb 16-bit (or first half of 32-bit Thumb-2) */
            if (pc + 2 > end_pc) break;
            uint16_t inst16 = *(const uint16_t *)(code + (pc - start_pc));
            bool is_32bit = (inst16 & 0xF800) == 0xF800 || (inst16 & 0xF800) == 0xE800;
            uint32_t inst32 = inst16;
            if (is_32bit && pc + 4 <= end_pc) {
                inst32 = (uint32_t)inst16 << 16;
                inst32 |= *(const uint16_t *)(code + (pc - start_pc + 2));
            }
            ok = atl_arm_decode_thumb(inst16, inst32, is_32bit, &d);
            pc += is_32bit ? 4 : 2;
        } else {
            /* A32 32-bit */
            if (pc + 4 > end_pc) break;
            uint32_t inst = *(const uint32_t *)(code + (pc - start_pc));
            ok = atl_arm_decode_a32(inst, &d);
            pc += 4;
        }

        if (!ok) continue;

        atl_mir_block_t *block = atl_arm_to_mir_block(func, &d, pc - (is_thumb ? (d.thumb?4:2) : 4));
        if (block) {
            /* link block into function */
            if (!func->first_block) {
                func->first_block = block;
                func->entry_block = block;
            } else {
                func->last_block->next = block;
                block->prev = func->last_block;
            }
            func->last_block = block;
            func->num_blocks++;
        }
    }

    atl_mir_builder_destroy(b);
    atl_mir_context_destroy(ctx);
    return func;
}
