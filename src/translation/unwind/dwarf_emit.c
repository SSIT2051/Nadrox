/*
 * x86_64 DWARF CFI Emitter for ARM EHABI Unwinding
 * Converts ARM unwind bytecode to DWARF Call Frame Information
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "dwarf_emit.h"
#include "ehabi_decode.h"

/* DWARF CFI opcode definitions */
#define DW_CFA_ADVANCE_LOC        0x40
#define DW_CFA_OFFSET             0x80
#define DW_CFA_RESTORE            0xc0
#define DW_CFA_EXTENDED           0x00

/* Extended opcodes */
#define DW_CFA_nop                0x00
#define DW_CFA_set_loc            0x01
#define DW_CFA_advance_loc1       0x02
#define DW_CFA_def_cfa            0x0c
#define DW_CFA_def_cfa_register   0x0d
#define DW_CFA_def_cfa_offset     0x0e
#define DW_CFA_def_cfa_expression 0x0f
#define DW_CFA_undefined          0x07
#define DW_CFA_same_value         0x08
#define DW_CFA_offset_extended    0x05
#define DW_CFA_restore_extended   0x06
#define DW_CFA_GNU_args_size      0x2e

/* x86_64 register numbers */
#define DW_REG_RIP                16
#define DW_REG_RSP                7
#define DW_REG_RBP                6
#define DW_REG_RAX                0
#define DW_REG_RDX                1
#define DW_REG_RCX                2
#define DW_REG_RBX                3
#define DW_REG_RSI                4
#define DW_REG_RDI                5
#define DW_REG_R8                 8
#define DW_REG_R9                 9
#define DW_REG_R10                10
#define DW_REG_R11                11
#define DW_REG_R12                12
#define DW_REG_R13                13
#define DW_REG_R14                14
#define DW_REG_R15                15

/* ARM register mapping to x86_64 for unwinding context */
static int arm_to_x86_reg(int arm_reg)
{
    switch (arm_reg) {
        case 0: return DW_REG_RAX; /* r0 -> rax (parameter/return) */
        case 1: return DW_REG_RDX; /* r1 -> rdx */
        case 2: return DW_REG_RCX; /* r2 -> rcx */
        case 3: return DW_REG_RBX; /* r3 -> rbx */
        case 4: return DW_REG_RSI; /* r4 -> rsi */
        case 5: return DW_REG_RDI; /* r5 -> rdi */
        case 6: return DW_REG_RBP; /* r6 -> rbp */
        case 7: return DW_REG_R8;  /* r7 -> r8 */
        case 8: return DW_REG_R9;  /* r8 -> r9 */
        case 9: return DW_REG_R10; /* r9 -> r10 */
        case 10: return DW_REG_R11; /* r10 -> r11 */
        case 11: return DW_REG_R12; /* r11 -> r12 */
        case 12: return DW_REG_R13; /* r12 -> r13 */
        case 13: return DW_REG_RSP; /* sp -> rsp */
        case 14: return DW_REG_R14; /* lr -> r14 */
        case 15: return DW_REG_RIP; /* pc -> rip */
        default: return -1;
    }
}

/* Emit a ULEB128 value */
static size_t emit_uleb128(uint8_t *buf, uint64_t value)
{
    size_t i = 0;
    do {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if (value != 0) byte |= 0x80;
        if (buf) buf[i] = byte;
        i++;
    } while (value != 0);
    return i;
}

/* Emit a SLEB128 value */
static size_t emit_sleb128(uint8_t *buf, int64_t value)
{
    size_t i = 0;
    bool more = true;
    while (more) {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if ((value == 0 && !(byte & 0x40)) || (value == -1 && (byte & 0x40))) {
            more = false;
        } else {
            byte |= 0x80;
        }
        if (buf) buf[i] = byte;
        i++;
    }
    return i;
}

/* Create CIE */
void dwarf_emit_cie(struct dwarf_cie *cie)
{
    if (!cie) return;
    memset(cie, 0, sizeof(*cie));
    cie->version = 1;
    cie->augmentation[0] = 'z';
    cie->augmentation[1] = 'P'; /* Personality routine */
    cie->augmentation[2] = 'R'; /* FDE encoding */
    cie->augmentation[3] = 'L'; /* LSDA encoding */
    cie->augmentation[4] = '\0';
    cie->code_alignment_factor = 1;
    cie->data_alignment_factor = -8; /* x86_64 is 8 bytes, negative */
    cie->return_address_register = DW_REG_RIP;
    cie->personality_routine = 0; /* Will be filled in */
}

/* Create FDE for a function */
void dwarf_emit_fde(struct dwarf_fde *fde, uint32_t func_start, uint32_t func_size)
{
    if (!fde) return;
    memset(fde, 0, sizeof(*fde));
    fde->initial_location = func_start;
    fde->address_range = func_size;
    fde->cie_pointer = 0;
    fde->lsda = 0;
    fde->cie = NULL;
}

/* Convert ARM EHABI unwind ops to DWARF CFI instructions */
int dwarf_convert_ehabi_to_cfi(const struct ehabi_unwind_ops *ehabi_ops,
                                uint8_t *cfi_buf, size_t cfi_buf_size)
{
    if (!ehabi_ops || !cfi_buf)
        return -1;
    
    size_t pos = 0;
    uint32_t cfa_offset = 0;
    int current_cfa_reg = DW_REG_RSP;
    
    /* Initial state: CFA = RSP + 8 (return address pushed) */
    /* Emit DW_CFA_def_cfa RSP, 8 */
    if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_EXTENDED;
    if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_def_cfa;
    pos += emit_uleb128(cfi_buf ? &cfi_buf[pos] : NULL, DW_REG_RSP);
    pos += emit_uleb128(cfi_buf ? &cfi_buf[pos] : NULL, 8);
    
    for (int i = 0; i < ehabi_ops->op_count; i++) {
        const struct ehabi_unwind_op *op = &ehabi_ops->ops[i];
        
        switch (op->type) {
            case EHABI_OP_DATA_PUSH:
                /* Stack grows down, push means decrease SP/CFA */
                cfa_offset += op->data;
                /* Update CFA: CFA = current_cfa_reg + cfa_offset */
                if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_EXTENDED;
                if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_def_cfa_offset;
                pos += emit_uleb128(cfi_buf ? &cfi_buf[pos] : NULL, cfa_offset);
                break;
                
            case EHABI_OP_DATA_POP:
                /* Pop means increase SP/CFA */
                if (op->data <= cfa_offset)
                    cfa_offset -= op->data;
                /* Update CFA */
                if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_EXTENDED;
                if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_def_cfa_offset;
                pos += emit_uleb128(cfi_buf ? &cfi_buf[pos] : NULL, cfa_offset);
                break;
                
            case EHABI_OP_REG_TO_SP:
                /* vsp = r[n] - change CFA register */
                {
                    int new_reg = arm_to_x86_reg(op->data);
                    if (new_reg >= 0) {
                        current_cfa_reg = new_reg;
                        if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_EXTENDED;
                        if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_def_cfa_register;
                        pos += emit_uleb128(cfi_buf ? &cfi_buf[pos] : NULL, current_cfa_reg);
                    }
                }
                break;
                
            case EHABI_OP_REG_POP:
                /* Pop registers - record their locations relative to CFA */
                for (int r = 0; r < 16; r++) {
                    if (op->data & (1 << r)) {
                        int x86_reg = arm_to_x86_reg(r);
                        if (x86_reg >= 0 && x86_reg != current_cfa_reg) {
                            /* DW_CFA_offset: register stored at CFA - offset */
                            if (x86_reg < 64) {
                                if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_OFFSET | x86_reg;
                            } else {
                                if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_EXTENDED;
                                if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_offset_extended;
                                pos += emit_uleb128(cfi_buf ? &cfi_buf[pos] : NULL, x86_reg);
                            }
                            /* offset in DWARF units (8 bytes for x86_64) */
                            pos += emit_uleb128(cfi_buf ? &cfi_buf[pos] : NULL, (cfa_offset / 8));
                        }
                    }
                }
                break;
                
            case EHABI_OP_FINISH:
                /* End of unwind program */
                goto done;
                
            case EHABI_OP_REFUSED:
            case EHABI_OP_RESERVED:
                /* Cannot unwind or reserved */
                return -1;
                
            default:
                /* Skip VFP/WREG/WCGR for now */
                break;
        }
    }
    
done:
    /* Pad with NOP if needed */
    while (pos < cfi_buf_size && pos % 4 != 0) {
        if (pos < cfi_buf_size) cfi_buf[pos++] = DW_CFA_nop;
    }
    
    return (int)pos;
}

/* Emit complete .eh_frame section */
size_t dwarf_emit_eh_frame(struct ehabi_entry *entries, size_t entry_count,
                            uint8_t *out_buf, size_t out_buf_size,
                            uint64_t text_base)
{
    if (!entries || !out_buf)
        return 0;
    
    size_t pos = 0;
    struct dwarf_cie cie;
    dwarf_emit_cie(&cie);
    
    /* For each entry, create CIE and FDE */
    for (size_t i = 0; i < entry_count; i++) {
        if (entries[i].cannot_unwind)
            continue;
        
        /* Create FDE */
        struct dwarf_fde fde;
        dwarf_emit_fde(&fde, entries[i].function_addr, 4); /* Assume 4 bytes per entry estimate */
        fde.cie = &cie;
        
        /* Convert unwind program */
        struct ehabi_unwind_ops ops;
        if (entries[i].unwind_program_len > 0) {
            if (ehabi_decode_unwind_program(entries[i].unwind_program,
                                           entries[i].unwind_program_len, &ops) < 0)
                continue;
        }
        
        /* Emit FDE */
        /* FDE format: length(4), CIE pointer(4), initial location(8), address range(8), aug len(1), LSDA encoding, LSDA ptr, CFI instructions */
        size_t fde_start = pos;
        uint32_t fde_len_pos = pos;
        pos += 4; /* length placeholder */
        
        /* CIE pointer (0 in .eh_frame for CIE reference) */
        *(uint32_t *)(out_buf + pos) = 0;
        pos += 4;
        
        /* Initial location (PC-relative) */
        *(uint64_t *)(out_buf + pos) = entries[i].function_addr - text_base;
        pos += 8;
        
        /* Address range */
        *(uint64_t *)(out_buf + pos) = fde.address_range;
        pos += 8;
        
        /* Augmentation length */
        uint8_t aug_len_pos = pos;
        pos += 1;
        size_t aug_data_start = pos;
        
        /* LSDA pointer (if present) */
        /* For now, no LSDA */
        out_buf[aug_len_pos] = 0;
        
        /* CFI instructions */
        size_t cfi_start = pos;
        pos += dwarf_convert_ehabi_to_cfi(&ops, out_buf ? &out_buf[pos] : NULL, 
                                         out_buf_size > pos ? out_buf_size - pos : 0);
        
        /* Fill in FDE length */
        uint32_t fde_len = (uint32_t)(pos - fde_start - 4);
        *(uint32_t *)(out_buf + fde_len_pos) = fde_len;
    }
    
    return pos;
}
