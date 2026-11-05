#ifndef EHABI_DECODE_H
#define EHABI_DECODE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ARM EHABI unwind operation types */
typedef enum {
    EHABI_OP_DATA_POP,
    EHABI_OP_DATA_PUSH,
    EHABI_OP_REG_POP,
    EHABI_OP_REG_TO_SP,
    EHABI_OP_VFP_POP,
    EHABI_OP_WREG_POP,
    EHABI_OP_WCGR_POP,
    EHABI_OP_FINISH,
    EHABI_OP_REFUSED,
    EHABI_OP_RESERVED
} ehabi_op_type_t;

/* Decoded unwind operation */
struct ehabi_unwind_op {
    ehabi_op_type_t type;
    uint32_t data;
};

/* Collection of unwind operations */
struct ehabi_unwind_ops {
    struct ehabi_unwind_op ops[256];
    int op_count;
};

/* Parsed exidx entry */
struct ehabi_entry {
    uint32_t function_addr;
    uint32_t entry_addr;
    bool cannot_unwind;
    bool is_compact;
    uint32_t personality;
    uint32_t personality_addr;
    uint8_t unwind_program[32];
    size_t unwind_program_len;
};

/* Function declarations */
int ehabi_decode_unwind_program(const uint8_t *program, size_t len,
                                  struct ehabi_unwind_ops *ops);
int ehabi_parse_exidx(uint32_t *exidx_base, size_t exidx_count,
                      struct ehabi_entry *entries);
struct ehabi_entry *ehabi_lookup_entry(struct ehabi_entry *entries,
                                       size_t count, uint32_t pc);

#endif /* EHABI_DECODE_H */
