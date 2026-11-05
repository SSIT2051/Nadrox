#ifndef ATL_MIR_OPCODES_H
#define ATL_MIR_OPCODES_H

#include "mir_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MIR Opcode Definitions and Properties
 * ============================================================================
 * This file contains the opcode property tables and helper functions.
 * Generated from the opcode definitions in mir_types.h.
 * ============================================================================ */

/* Opcode property flags */
#define ATL_MIR_OP_PROP_COMMUTATIVE      (1u << 0)  /* a op b == b op a */
#define ATL_MIR_OP_PROP_ASSOCIATIVE      (1u << 1)  /* (a op b) op c == a op (b op c) */
#define ATL_MIR_OP_PROP_PURE             (1u << 2)  /* No side effects, result depends only on inputs */
#define ATL_MIR_OP_PROP_IDEMPOTENT       (1u << 3)  /* a op a == a */
#define ATL_MIR_OP_PROP_NO_DUPLICATE     (1u << 4)  /* Don't CSE this instruction */
#define ATL_MIR_OP_PROP_TERMINATOR       (1u << 5)  /* Block terminator (branch, return) */
#define ATL_MIR_OP_PROP_BRANCH           (1u << 6)  /* Control flow branch */
#define ATL_MIR_OP_PROP_CALL             (1u << 7)  /* Function call */
#define ATL_MIR_OP_PROP_MEMORY_READ      (1u << 8)  /* Reads memory */
#define ATL_MIR_OP_PROP_MEMORY_WRITE     (1u << 9)  /* Writes memory */
#define ATL_MIR_OP_PROP_VOLATILE         (1u << 10) /* Volatile memory access */
#define ATL_MIR_OP_PROP_VECTOR           (1u << 11) /* Vector/SIMD operation */
#define ATL_MIR_OP_PROP_GAME_SPECIFIC    (1u << 12) /* Game-specific optimization opcode */
#define ATL_MIR_OP_PROP_CONVERSION       (1u << 13) /* Type conversion */
#define ATL_MIR_OP_PROP_STACK            (1u << 14) /* Stack/frame operation */
#define ATL_MIR_OP_PROP_ATOMIC           (1u << 15) /* Atomic operation */
#define ATL_MIR_OP_PROP_SYSCALL          (1u << 16) /* Syscall/JNI/intrinsic */
#define ATL_MIR_OP_PROP_DEBUG            (1u << 17) /* Debug/profiling */

/* Opcode property table entry */
typedef struct {
    atl_mir_opcode_t opcode;
    const char *name;
    uint32_t properties;
    uint32_t min_operands;
    uint32_t max_operands;
    atl_mir_type_kind_t result_type;  /* Default result type kind */
} atl_mir_opcode_info_t;

/* Opcode property table */
extern const atl_mir_opcode_info_t atl_mir_opcode_table[ATL_MIR_OP_COUNT];

/* ---------------------------------------------------------------------------
 * Opcode Property Query Functions
 * --------------------------------------------------------------------------- */

static inline const atl_mir_opcode_info_t *atl_mir_opcode_info(atl_mir_opcode_t op) {
    if (op < ATL_MIR_OP_COUNT) {
        return &atl_mir_opcode_table[op];
    }
    return &atl_mir_opcode_table[ATL_MIR_OP_INVALID];
}

static inline const char *atl_mir_opcode_name(atl_mir_opcode_t op) {
    return atl_mir_opcode_info(op)->name;
}

static inline bool atl_mir_opcode_is_commutative(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_COMMUTATIVE) != 0;
}

static inline bool atl_mir_opcode_is_associative(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_ASSOCIATIVE) != 0;
}

static inline bool atl_mir_opcode_is_pure(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_PURE) != 0;
}

static inline bool atl_mir_opcode_is_idempotent(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_IDEMPOTENT) != 0;
}

static inline bool atl_mir_opcode_is_terminator(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_TERMINATOR) != 0;
}

static inline bool atl_mir_opcode_is_branch(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_BRANCH) != 0;
}

static inline bool atl_mir_opcode_is_call(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_CALL) != 0;
}

static inline bool atl_mir_opcode_reads_memory(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_MEMORY_READ) != 0;
}

static inline bool atl_mir_opcode_writes_memory(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_MEMORY_WRITE) != 0;
}

static inline bool atl_mir_opcode_is_vector(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_VECTOR) != 0;
}

static inline bool atl_mir_opcode_is_game_specific(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_GAME_SPECIFIC) != 0;
}

static inline bool atl_mir_opcode_is_conversion(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_CONVERSION) != 0;
}

static inline bool atl_mir_opcode_is_stack(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_STACK) != 0;
}

static inline bool atl_mir_opcode_is_atomic(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_ATOMIC) != 0;
}

static inline bool atl_mir_opcode_is_syscall(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_SYSCALL) != 0;
}

static inline bool atl_mir_opcode_is_debug(atl_mir_opcode_t op) {
    return (atl_mir_opcode_info(op)->properties & ATL_MIR_OP_PROP_DEBUG) != 0;
}

static inline uint32_t atl_mir_opcode_min_operands(atl_mir_opcode_t op) {
    return atl_mir_opcode_info(op)->min_operands;
}

static inline uint32_t atl_mir_opcode_max_operands(atl_mir_opcode_t op) {
    return atl_mir_opcode_info(op)->max_operands;
}

/* ---------------------------------------------------------------------------
 * Operand Type Inference
 * --------------------------------------------------------------------------- */

/* For binary ops, infer result type from operand types */
atl_mir_type_t atl_mir_infer_binary_result_type(atl_mir_opcode_t op, atl_mir_type_t lhs, atl_mir_type_t rhs);

/* For unary ops, infer result type from operand type */
atl_mir_type_t atl_mir_infer_unary_result_type(atl_mir_opcode_t op, atl_mir_type_t operand);

/* For conversion ops, get the target type from the opcode */
atl_mir_type_kind_t atl_mir_conversion_target_type(atl_mir_opcode_t op);

/* ---------------------------------------------------------------------------
 * Opcode Categories (for pass scheduling)
 * --------------------------------------------------------------------------- */

typedef enum atl_mir_opcode_category {
    ATL_MIR_CAT_INVALID = 0,
    ATL_MIR_CAT_ARITHMETIC,
    ATL_MIR_CAT_BITWISE,
    ATL_MIR_CAT_COMPARE,
    ATL_MIR_CAT_CONTROL_FLOW,
    ATL_MIR_CAT_MEMORY,
    ATL_MIR_CAT_VECTOR,
    ATL_MIR_CAT_CONVERSION,
    ATL_MIR_CAT_STACK,
    ATL_MIR_CAT_ATOMIC,
    ATL_MIR_CAT_GAME_SPECIFIC,
    ATL_MIR_CAT_SYSCALL,
    ATL_MIR_CAT_DEBUG,
} atl_mir_opcode_category_t;

atl_mir_opcode_category_t atl_mir_opcode_category(atl_mir_opcode_t op);

/* ---------------------------------------------------------------------------
 * ARM Instruction to MIR Opcode Mapping
 * --------------------------------------------------------------------------- */

/* Maps ARM A32/T32 opcode to MIR opcode(s) */
typedef struct {
    uint32_t arm_opcode;       /* ARM instruction encoding (top bits) */
    uint32_t arm_mask;         /* Mask for matching */
    atl_mir_opcode_t mir_opcode;
    bool is_thumb;             /* Thumb-2 vs A32 */
    bool is_neon;              /* NEON/VFP instruction */
} atl_arm_to_mir_mapping_t;

/* ARM -> MIR mapping table (populated in arm_decode/arm_to_mir.c) */
extern const atl_arm_to_mir_mapping_t atl_arm_to_mir_table[];
extern const size_t atl_arm_to_mir_table_size;

/* Lookup MIR opcode for ARM instruction */
atl_mir_opcode_t atl_arm_decode_to_mir(uint32_t arm_inst, bool is_thumb, bool is_neon);

#ifdef __cplusplus
}
#endif

#endif /* ATL_MIR_OPCODES_H */