/*
 * ARM EHABI Unwind Table Decoder
 * Parses .ARM.exidx entries for ARM exception handling unwinding
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <elf.h>

#include "ehabi_decode.h"

/* ARM EHABI constants */
#define ARM_EXIDX_CANT_UNWIND   0x00000001
#define ARM_EXIDX_COMPACT       0x80000000

#define ARM_EXTBL_OP_FINISH     0xb0

/* Prel31 decoding - sign-extend 31-bit offset */
static inline int32_t prel31_decode(uint32_t val)
{
    return ((int32_t)val << 1) >> 1;
}

/* Convert prel31 offset to absolute address */
static inline uint32_t prel31_to_addr(uint32_t prel31_addr, uint32_t offset)
{
    int32_t decoded = prel31_decode(offset);
    return prel31_addr + (uint32_t)decoded;
}

/* Decode ARM unwind bytecode */
int ehabi_decode_unwind_program(const uint8_t *program, size_t len, 
                                  struct ehabi_unwind_ops *ops)
{
    if (!program || !ops || len == 0)
        return -1;
    
    memset(ops, 0, sizeof(*ops));
    ops->op_count = 0;
    
    const uint8_t *ptr = program;
    const uint8_t *end = program + len;
    
    while (ptr < end && ops->op_count < 256) {
        uint8_t op = *ptr++;
        struct ehabi_unwind_op *cur = &ops->ops[ops->op_count++];
        
        if ((op & 0xc0) == 0x00) {
            /* vsp = vsp + (xxxxxx << 2) + 4 */
            cur->type = EHABI_OP_DATA_POP;
            cur->data = ((op & 0x3f) << 2) + 4;
        } else if ((op & 0xc0) == 0x40) {
            /* vsp = vsp - (xxxxxx << 2) - 4 */
            cur->type = EHABI_OP_DATA_PUSH;
            cur->data = ((op & 0x3f) << 2) + 4;
        } else if ((op & 0xf0) == 0x80) {
            uint8_t op2 = *ptr++;
            if (op == 0x80 && op2 == 0x00) {
                cur->type = EHABI_OP_REFUSED;
            } else {
                /* Pop r4-r15 according to mask */
                cur->type = EHABI_OP_REG_POP;
                cur->data = ((op & 0x0f) << 8) | op2;
                /* Shift to match bit positions */
                cur->data = cur->data << 4;
            }
        } else if ((op & 0xf0) == 0x90) {
            if (op == 0x9d || op == 0x9f) {
                cur->type = EHABI_OP_RESERVED;
            } else {
                /* vsp = r[nnnn] */
                cur->type = EHABI_OP_REG_TO_SP;
                cur->data = op & 0x0f;
            }
        } else if ((op & 0xf0) == 0xa0) {
            /* Pop r4-r[4+nnn], r14 */
            unsigned end_reg = (op & 0x07);
            cur->data = (1 << (end_reg + 1)) - 1;
            cur->data = cur->data << 4;
            if (op & 0x08) {
                cur->data |= (1 << 14);
            }
            cur->type = EHABI_OP_REG_POP;
        } else if (op == ARM_EXTBL_OP_FINISH) {
            cur->type = EHABI_OP_FINISH;
            break;
        } else if (op == 0xb1) {
            uint8_t op2 = *ptr++;
            if (op2 == 0 || (op2 & 0xf0)) {
                cur->type = EHABI_OP_RESERVED;
            } else {
                /* Pop r0-r3 according to mask */
                cur->type = EHABI_OP_REG_POP;
                cur->data = op2 & 0x0f;
            }
        } else if (op == 0xb2) {
            /* vsp = vsp + 0x204 + (uleb128 << 2) */
            uint32_t offset = 0;
            uint8_t byte;
            uint32_t shift = 0;
            do {
                if (ptr >= end) break;
                byte = *ptr++;
                offset |= (byte & 0x7f) << shift;
                shift += 7;
            } while (byte & 0x80);
            cur->type = EHABI_OP_DATA_POP;
            cur->data = offset * 4 + 0x204;
        } else if (op == 0xb3 || op == 0xc8 || op == 0xc9) {
            if (ptr >= end) break;
            uint8_t op2 = *ptr++;
            cur->type = EHABI_OP_VFP_POP;
            cur->data = op2;
            if (op == 0xc8) {
                cur->data |= (1 << 16); /* VFP_SHIFT_16 */
            }
            if (op != 0xb3) {
                cur->data |= (1 << 17); /* VFP_DOUBLE */
            }
        } else if ((op & 0xf8) == 0xb8 || (op & 0xf8) == 0xd0) {
            cur->type = EHABI_OP_VFP_POP;
            cur->data = 0x80 | (op & 0x07);
            if ((op & 0xf8) == 0xd0) {
                cur->data |= (1 << 17); /* VFP_DOUBLE */
            }
        } else if (op >= 0xc0 && op <= 0xc5) {
            cur->type = EHABI_OP_WREG_POP;
            cur->data = 0xa0 | (op & 0x07);
        } else if (op == 0xc6) {
            if (ptr >= end) break;
            cur->type = EHABI_OP_WREG_POP;
            cur->data = *ptr++;
        } else if (op == 0xc7) {
            if (ptr >= end) break;
            uint8_t op2 = *ptr++;
            if (op2 == 0 || (op2 & 0xf0)) {
                cur->type = EHABI_OP_RESERVED;
            } else {
                cur->type = EHABI_OP_WCGR_POP;
                cur->data = op2 & 0x0f;
            }
        } else {
            cur->type = EHABI_OP_RESERVED;
        }
    }
    
    return 0;
}

/* Parse all exidx entries */
int ehabi_parse_exidx(uint32_t *exidx_base, size_t exidx_count,
                      struct ehabi_entry *entries)
{
    if (!exidx_base || !entries)
        return -1;
    
    for (size_t i = 0; i < exidx_count; i++) {
        uint32_t *entry_ptr = &exidx_base[i * 2]; /* Each entry is 8 bytes = 2 words */
        uint32_t prel31_func = entry_ptr[0];
        uint32_t content = entry_ptr[1];
        
        /* Decode function address */
        entries[i].function_addr = prel31_to_addr((uint32_t)(uintptr_t)prel31_func, prel31_func);
        entries[i].entry_addr = (uint32_t)(uintptr_t)entry_ptr;
        
        if (content == ARM_EXIDX_CANT_UNWIND) {
            /* Cannot unwind */
            entries[i].cannot_unwind = true;
            entries[i].is_compact = false;
            entries[i].personality = 0;
            entries[i].unwind_program_len = 0;
        } else if (content & ARM_EXIDX_COMPACT) {
            /* Compact model */
            entries[i].cannot_unwind = false;
            entries[i].is_compact = true;
            entries[i].personality = (content >> 24) & 0x0f;
            /* Extract unwind bytecode from the entry itself */
            entries[i].unwind_program[0] = (content >> 16) & 0xff;
            entries[i].unwind_program[1] = (content >> 8) & 0xff;
            entries[i].unwind_program[2] = content & 0xff;
            entries[i].unwind_program_len = 3;
        } else {
            /* Personality routine model - content is prel31 offset to exception table */
            entries[i].cannot_unwind = false;
            entries[i].is_compact = false;
            uint32_t extbl_addr = prel31_to_addr(entry_ptr[1], content);
            /* Read from exception table - would need memory access */
            entries[i].personality_addr = extbl_addr;
            /* For now, mark as needing external read */
            entries[i].unwind_program_len = 0;
        }
    }
    
    return (int)exidx_count;
}

/* Lookup exidx entry for a given PC */
struct ehabi_entry *ehabi_lookup_entry(struct ehabi_entry *entries, 
                                       size_t count, uint32_t pc)
{
    /* Binary search - exidx entries are sorted by function address */
    /* Simple linear search for now */
    for (size_t i = 0; i < count; i++) {
        if (pc >= entries[i].function_addr) {
            /* Check if this is the right entry (next entry starts at next function) */
            if (i + 1 < count && pc >= entries[i + 1].function_addr) {
                continue;
            }
            return &entries[i];
        }
    }
    return NULL;
}
