#ifndef ATL_MIR_TYPES_H
#define ATL_MIR_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MIR Type System
 * ============================================================================
 * SSA-based Medium IR for ARMv4/v7 -> x86_64 translation
 * ~40 opcodes, game-tuned for matrix math, vector ops, texture sampling
 * ============================================================================ */

/* Forward declarations */
typedef struct atl_mir_module atl_mir_module_t;
typedef struct atl_mir_function atl_mir_function_t;
typedef struct atl_mir_block atl_mir_block_t;
typedef struct atl_mir_inst atl_mir_inst_t;
typedef struct atl_mir_value atl_mir_value_t;
typedef struct atl_mir_use atl_mir_use_t;
typedef struct atl_mir_builder atl_mir_builder_t;
typedef struct atl_mir_context atl_mir_context_t;

/* ---------------------------------------------------------------------------
 * MIR Value Types
 * --------------------------------------------------------------------------- */
typedef enum atl_mir_type_kind {
    ATL_MIR_TYPE_VOID    = 0,
    ATL_MIR_TYPE_I1      = 1,   /* Boolean / condition code */
    ATL_MIR_TYPE_I8      = 2,
    ATL_MIR_TYPE_I16     = 3,
    ATL_MIR_TYPE_I32     = 4,
    ATL_MIR_TYPE_I64     = 5,
    ATL_MIR_TYPE_F32     = 6,
    ATL_MIR_TYPE_F64     = 7,
    ATL_MIR_TYPE_V64     = 8,   /* 64-bit vector (NEON D-register) */
    ATL_MIR_TYPE_V128    = 9,   /* 128-bit vector (NEON Q-register) */
    ATL_MIR_TYPE_PTR     = 10,  /* Pointer (64-bit on x86_64) */
    ATL_MIR_TYPE_LABEL   = 11,  /* Block label for control flow */
} atl_mir_type_kind_t;

typedef struct atl_mir_type {
    atl_mir_type_kind_t kind;
    unsigned int lanes;  /* For vector types: number of lanes (2, 4, 8, 16) */
} atl_mir_type_t;

/* ---------------------------------------------------------------------------
 * MIR Value (SSA Value)
 * --------------------------------------------------------------------------- */
struct atl_mir_value {
    /* Identity */
    uint32_t id;                    /* Unique value ID within function */
    atl_mir_type_t type;            /* Value type */
    
    /* Definition */
    atl_mir_inst_t *def;            /* Instruction that defines this value (NULL for args/constants) */
    
    /* Uses (for SSA traversal) */
    atl_mir_use_t *uses;            /* Linked list of uses */
    size_t num_uses;
    
    /* Constant value (if constant) */
    union {
        int64_t  i64;
        uint64_t u64;
        double   f64;
        float    f32;
        void    *ptr;
    } constant;
    bool is_constant;
    
    /* Debug */
    const char *name;               /* Optional debug name */
    
    /* Linkage */
    atl_mir_value_t *next;          /* Next value in function's value list */
};

/* ---------------------------------------------------------------------------
 * MIR Use (for SSA use-def chains)
 * --------------------------------------------------------------------------- */
struct atl_mir_use {
    atl_mir_value_t *value;         /* The value being used */
    atl_mir_inst_t *user;           /* Instruction using this value */
    unsigned int operand_idx;       /* Which operand slot */
    atl_mir_use_t *next;            /* Next use in value's use list */
};

/* ---------------------------------------------------------------------------
 * MIR Opcodes (~40 core opcodes)
 * --------------------------------------------------------------------------- */
typedef enum atl_mir_opcode {
    /* Invalid/placeholder */
    ATL_MIR_OP_INVALID = 0,
    
    /* --- Arithmetic (10) --- */
    ATL_MIR_OP_ADD,
    ATL_MIR_OP_SUB,
    ATL_MIR_OP_MUL,
    ATL_MIR_OP_DIV,
    ATL_MIR_OP_MOD,
    ATL_MIR_OP_NEG,
    ATL_MIR_OP_ABS,
    ATL_MIR_OP_MIN,
    ATL_MIR_OP_MAX,
    ATL_MIR_OP_FMA,           /* Fused multiply-add */
    
    /* --- Bitwise (9) --- */
    ATL_MIR_OP_AND,
    ATL_MIR_OP_OR,
    ATL_MIR_OP_XOR,
    ATL_MIR_OP_NOT,
    ATL_MIR_OP_SHL,
    ATL_MIR_OP_SHR,           /* Logical shift right */
    ATL_MIR_OP_SAR,           /* Arithmetic shift right */
    ATL_MIR_OP_ROL,
    ATL_MIR_OP_ROR,
    
    /* --- Compare (12 signed/unsigned variants) --- */
    ATL_MIR_OP_CMP_EQ,
    ATL_MIR_OP_CMP_NE,
    ATL_MIR_OP_CMP_LT,
    ATL_MIR_OP_CMP_LE,
    ATL_MIR_OP_CMP_GT,
    ATL_MIR_OP_CMP_GE,
    ATL_MIR_OP_CMP_ULT,       /* Unsigned less than */
    ATL_MIR_OP_CMP_ULE,       /* Unsigned less or equal */
    ATL_MIR_OP_CMP_UGT,       /* Unsigned greater than */
    ATL_MIR_OP_CMP_UGE,       /* Unsigned greater or equal */
    
    /* --- Control Flow (7) --- */
    ATL_MIR_OP_BR,            /* Unconditional branch */
    ATL_MIR_OP_BR_IF,         /* Conditional branch */
    ATL_MIR_OP_SWITCH,        /* Switch/jump table */
    ATL_MIR_OP_CALL,          /* Direct call */
    ATL_MIR_OP_INDIRECT_CALL, /* Indirect call */
    ATL_MIR_OP_RET,           /* Return */
    ATL_MIR_OP_UNREACHABLE,   /* Unreachable code marker */
    
    /* --- Memory (10) --- */
    ATL_MIR_OP_LOAD,
    ATL_MIR_OP_STORE,
    ATL_MIR_OP_LOAD8,
    ATL_MIR_OP_LOAD16,
    ATL_MIR_OP_LOAD32,
    ATL_MIR_OP_LOAD64,
    ATL_MIR_OP_STORE8,
    ATL_MIR_OP_STORE16,
    ATL_MIR_OP_STORE32,
    ATL_MIR_OP_STORE64,
    ATL_MIR_OP_PREFETCH,
    
    /* --- Vector/SIMD (10) --- */
    ATL_MIR_OP_VADD,
    ATL_MIR_OP_VSUB,
    ATL_MIR_OP_VMUL,
    ATL_MIR_OP_VDOT,          /* Vector dot product */
    ATL_MIR_OP_VCROSS,        /* Vector cross product (3D) */
    ATL_MIR_OP_VNORMALIZE,    /* Vector normalize */
    ATL_MIR_OP_VSWIZZLE,      /* Vector swizzle/shuffle */
    ATL_MIR_OP_VLOAD,
    ATL_MIR_OP_VSTORE,
    ATL_MIR_OP_VSPLAT,        /* Vector splat (broadcast scalar) */
    
    /* --- Conversion (8) --- */
    ATL_MIR_OP_I2F,           /* Int to float */
    ATL_MIR_OP_F2I,           /* Float to int */
    ATL_MIR_OP_I2V,           /* Int to vector */
    ATL_MIR_OP_V2I,           /* Vector to int */
    ATL_MIR_OP_ZEXT,          /* Zero extend */
    ATL_MIR_OP_SEXT,          /* Sign extend */
    ATL_MIR_OP_TRUNC,         /* Truncate */
    ATL_MIR_OP_BITCAST,       /* Bitwise cast (no conversion) */
    
    /* --- Stack/Frame (6) --- */
    ATL_MIR_OP_ALLOC,         /* Stack allocation */
    ATL_MIR_OP_FREE,          /* Stack free (for alloca) */
    ATL_MIR_OP_FRAME_ADDR,    /* Frame address */
    ATL_MIR_OP_ARG,           /* Function argument */
    ATL_MIR_OP_LOCAL,         /* Local variable (stack slot) */
    ATL_MIR_OP_STACK_PTR,     /* Stack pointer */
    
    /* --- Atomic (5) --- */
    ATL_MIR_OP_CAS,           /* Compare-and-swap */
    ATL_MIR_OP_XCHG,          /* Atomic exchange */
    ATL_MIR_OP_FETCH_ADD,     /* Atomic fetch-add */
    ATL_MIR_OP_FENCE,         /* Memory fence */
    ATL_MIR_OP_ATOMIC_LOAD,   /* Atomic load */
    
    /* --- Game-Specific (6) --- */
    ATL_MIR_OP_MAT4_MUL,      /* 4x4 matrix multiplication */
    ATL_MIR_OP_MAT4_VEC4,     /* 4x4 matrix * vec4 */
    ATL_MIR_OP_QUAT_SLERP,    /* Quaternion spherical linear interpolation */
    ATL_MIR_OP_TEX_SAMPLE,    /* Texture sampling */
    ATL_MIR_OP_FOG_COMPUTE,   /* Fog computation */
    ATL_MIR_OP_LERP,          /* Linear interpolation (common in games) */
    
    /* --- Syscall/JNI/Intrinsic (5) --- */
    ATL_MIR_OP_SYSCALL,       /* System call */
    ATL_MIR_OP_JNI_CALL,      /* JNI method call */
    ATL_MIR_OP_INTRINSIC,     /* Compiler intrinsic */
    ATL_MIR_OP_CACHEFLUSH,    /* Cache flush (self-modifying code) */
    ATL_MIR_OP_TLS_GET,       /* Thread-local storage get */
    
    /* --- Debug/Profiling (2) --- */
    ATL_MIR_OP_PROFILE_POINT, /* Profile counter increment */
    ATL_MIR_OP_DEBUG_LOC,     /* Debug location info */
    
    /* Sentinel */
    ATL_MIR_OP_COUNT
} atl_mir_opcode_t;

/* ---------------------------------------------------------------------------
 * MIR Instruction
 * --------------------------------------------------------------------------- */
#define ATL_MIR_MAX_OPERANDS 8

struct atl_mir_inst {
    /* Identity */
    uint32_t id;                    /* Unique instruction ID within function */
    atl_mir_opcode_t opcode;        /* Operation */
    atl_mir_type_t result_type;     /* Result type (VOID for no result) */
    
    /* Operands (SSA values) */
    uint32_t num_operands;
    atl_mir_value_t *operands[ATL_MIR_MAX_OPERANDS];
    
    /* Result value (for non-void instructions) */
    atl_mir_value_t *result;
    
    /* Control flow successors (for branch instructions) */
    atl_mir_block_t *succ[2];       /* Up to 2 successors (br_if, switch) */
    uint32_t num_succ;
    
    /* Block membership */
    atl_mir_block_t *block;         /* Containing block */
    
    /* Linkage in block */
    atl_mir_inst_t *prev;
    atl_mir_inst_t *next;
    
    /* Optimization/metadata */
    uint32_t flags;
#define ATL_MIR_INST_FLAG_COMMUTATIVE     (1u << 0)
#define ATL_MIR_INST_FLAG_PURE            (1u << 1)  /* No side effects */
#define ATL_MIR_INST_FLAG_NO_DUPLICATE    (1u << 2)  /* Don't CSE this */
#define ATL_MIR_INST_FLAG_VOLATILE        (1u << 3)  /* Volatile memory access */
#define ATL_MIR_INST_FLAG_REMATERIALIZABLE (1u << 4) /* Can be rematerialized */
    
    /* Debug location */
    uint32_t arm_pc;                /* Original ARM PC for this instruction */
    const char *debug_file;
    uint32_t debug_line;
};

/* ---------------------------------------------------------------------------
 * MIR Basic Block
 * --------------------------------------------------------------------------- */
struct atl_mir_block {
    /* Identity */
    uint32_t id;                    /* Unique block ID within function */
    
    /* Instructions */
    atl_mir_inst_t *first_inst;
    atl_mir_inst_t *last_inst;
    size_t num_insts;
    
    /* Predecessors/Successors (CFG) */
    atl_mir_block_t **preds;
    size_t num_preds;
    size_t pred_capacity;
    atl_mir_block_t **succs;
    size_t num_succs;
    size_t succ_capacity;
    
    /* Loop analysis */
    uint32_t loop_depth;
    bool is_loop_header;
    
    /* Profile data */
    uint64_t exec_count;            /* Execution count (for PGO) */
    uint64_t last_exec_cycle;       /* Last execution cycle (for hot detection) */
    
    /* Linkage in function */
    atl_mir_block_t *prev;
    atl_mir_block_t *next;
    
    /* Dominator tree */
    atl_mir_block_t *idom;          /* Immediate dominator */
    atl_mir_block_t **dom_children;
    size_t num_dom_children;
    
    /* Worklist flags */
    uint32_t worklist_flags;
};

/* ---------------------------------------------------------------------------
 * MIR Function
 * --------------------------------------------------------------------------- */
#define ATL_MIR_MAX_ARGS 16
#define ATL_MIR_MAX_LOCALS 256

struct atl_mir_function {
    /* Identity */
    uint32_t id;                    /* Unique function ID within module */
    const char *name;               /* Function name */
    
    /* Signature */
    atl_mir_type_t return_type;
    uint32_t num_args;
    atl_mir_value_t *args[ATL_MIR_MAX_ARGS];
    atl_mir_type_t arg_types[ATL_MIR_MAX_ARGS];
    
    /* Blocks */
    atl_mir_block_t *entry_block;
    atl_mir_block_t *first_block;
    atl_mir_block_t *last_block;
    size_t num_blocks;
    
    /* Values (all SSA values in this function) */
    atl_mir_value_t *first_value;
    atl_mir_value_t *last_value;
    size_t num_values;
    uint32_t next_value_id;
    uint32_t next_inst_id;
    uint32_t next_block_id;
    
    /* Locals (stack slots) */
    uint32_t num_locals;
    size_t local_sizes[ATL_MIR_MAX_LOCALS];
    atl_mir_type_t local_types[ATL_MIR_MAX_LOCALS];
    
    /* Calling convention info */
    bool is_variadic;
    bool is_noreturn;
    uint32_t stack_alignment;
    
    /* ARM metadata */
    uint32_t arm_start_pc;          /* ARM function entry PC */
    uint32_t arm_end_pc;            /* ARM function end PC */
    bool is_thumb;                  /* Thumb mode */
    
    /* JIT compilation state */
    void *compiled_code;            /* x86_64 machine code */
    size_t compiled_size;
    uint32_t tier;                  /* 0=interpreter, 1=basic JIT, 2=optimized */
    uint64_t compile_time_cycles;   /* Time spent compiling */
    
    /* Module reference */
    atl_mir_module_t *module;
    
    /* Linkage in module */
    struct atl_mir_function *next;
    struct atl_mir_function *prev;
};

/* ---------------------------------------------------------------------------
 * MIR Module (container for all functions)
 * --------------------------------------------------------------------------- */
struct atl_mir_module {
    /* Functions */
    atl_mir_function_t *first_function;
    atl_mir_function_t *last_function;
    size_t num_functions;
    uint32_t next_function_id;

    /* Context reference */
    atl_mir_context_t *context;
    
    /* Global data */
    struct {
        void *data;
        size_t size;
        size_t capacity;
    } globals;
    
    /* String table (for debug names) */
    struct {
        char *data;
        size_t size;
        size_t capacity;
    } strings;
    
    /* Target info */
    struct {
        bool is_armv4;
        bool is_armv7;
        bool has_neon;
        bool has_vfp;
        bool is_thumb2;
    } target;
    
    /* Statistics */
    struct {
        uint64_t total_insts;
        uint64_t total_blocks;
        uint64_t total_functions;
    } stats;
};

/* ---------------------------------------------------------------------------
 * MIR Context (global state for builder, passes, etc.)
 * --------------------------------------------------------------------------- */
struct atl_mir_context {
    /* Memory arena for allocations */
    struct {
        void *base;
        size_t offset;
        size_t capacity;
    } arena;
    
    /* Error handling */
    int last_error;
    char error_msg[256];
    
    /* Options */
    struct {
        bool verify_ssa;            /* Run SSA verification after each pass */
        bool debug_print;           /* Print IR after each pass */
        uint32_t opt_level;         /* 0=none, 1=basic, 2=aggressive */
        bool enable_game_opts;      /* Enable game-specific optimizations */
    } options;
    
    /* Pass manager */
    void *pass_manager;             /* Opaque pass manager */
};

/* ---------------------------------------------------------------------------
 * Type Utilities
 * --------------------------------------------------------------------------- */
static inline atl_mir_type_t atl_mir_type_void(void) {
    return (atl_mir_type_t){ATL_MIR_TYPE_VOID, 0};
}

static inline atl_mir_type_t atl_mir_type_int(unsigned bits) {
    switch (bits) {
        case 1:  return (atl_mir_type_t){ATL_MIR_TYPE_I1, 0};
        case 8:  return (atl_mir_type_t){ATL_MIR_TYPE_I8, 0};
        case 16: return (atl_mir_type_t){ATL_MIR_TYPE_I16, 0};
        case 32: return (atl_mir_type_t){ATL_MIR_TYPE_I32, 0};
        case 64: return (atl_mir_type_t){ATL_MIR_TYPE_I64, 0};
        default: return (atl_mir_type_t){ATL_MIR_TYPE_I32, 0};
    }
}

static inline atl_mir_type_t atl_mir_type_float(unsigned bits) {
    return (bits == 32) ? (atl_mir_type_t){ATL_MIR_TYPE_F32, 0} : (atl_mir_type_t){ATL_MIR_TYPE_F64, 0};
}

static inline atl_mir_type_t atl_mir_type_vector(unsigned bits, unsigned lanes) {
    return (bits == 64) ? (atl_mir_type_t){ATL_MIR_TYPE_V64, lanes} : (atl_mir_type_t){ATL_MIR_TYPE_V128, lanes};
}

static inline atl_mir_type_t atl_mir_type_ptr(void) {
    return (atl_mir_type_t){ATL_MIR_TYPE_PTR, 0};
}

static inline atl_mir_type_t atl_mir_type_label(void) {
    return (atl_mir_type_t){ATL_MIR_TYPE_LABEL, 0};
}

static inline bool atl_mir_type_is_int(atl_mir_type_t t) {
    return t.kind >= ATL_MIR_TYPE_I1 && t.kind <= ATL_MIR_TYPE_I64;
}

static inline bool atl_mir_type_is_float(atl_mir_type_t t) {
    return t.kind == ATL_MIR_TYPE_F32 || t.kind == ATL_MIR_TYPE_F64;
}

static inline bool atl_mir_type_is_vector(atl_mir_type_t t) {
    return t.kind == ATL_MIR_TYPE_V64 || t.kind == ATL_MIR_TYPE_V128;
}

static inline bool atl_mir_type_is_scalar(atl_mir_type_t t) {
    return atl_mir_type_is_int(t) || atl_mir_type_is_float(t) || t.kind == ATL_MIR_TYPE_PTR;
}

static inline unsigned atl_mir_type_bits(atl_mir_type_t t) {
    switch (t.kind) {
        case ATL_MIR_TYPE_I1: return 1;
        case ATL_MIR_TYPE_I8: return 8;
        case ATL_MIR_TYPE_I16: return 16;
        case ATL_MIR_TYPE_I32: return 32;
        case ATL_MIR_TYPE_I64: return 64;
        case ATL_MIR_TYPE_F32: return 32;
        case ATL_MIR_TYPE_F64: return 64;
        case ATL_MIR_TYPE_V64: return 64;
        case ATL_MIR_TYPE_V128: return 128;
        case ATL_MIR_TYPE_PTR: return 64;
        default: return 0;
    }
}

static inline unsigned atl_mir_type_bytes(atl_mir_type_t t) {
    return (atl_mir_type_bits(t) + 7) / 8;
}

/* ---------------------------------------------------------------------------
 * Instruction Builders (declaration - implemented in mir_builder.c)
 * --------------------------------------------------------------------------- */
atl_mir_inst_t *atl_mir_build_inst(atl_mir_builder_t *b, atl_mir_opcode_t op, ...);
atl_mir_inst_t *atl_mir_build_inst_v(atl_mir_builder_t *b, atl_mir_opcode_t op, va_list args);

#ifdef __cplusplus
}
#endif

#endif /* ATL_MIR_TYPES_H */