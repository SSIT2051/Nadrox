#include "mir.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

/* ============================================================================
 * MIR Opcode Property Table
 * ============================================================================ */

/* Each entry: opcode, name, properties, min_ops, max_ops, default result kind */
static const atl_mir_opcode_info_t opcode_properties[] = {
    /* Invalid */
    {ATL_MIR_OP_INVALID, "invalid", ATL_MIR_OP_PROP_DEBUG, 0, 0, ATL_MIR_TYPE_VOID},

    /* Arithmetic */
    {ATL_MIR_OP_ADD, "add", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_ASSOCIATIVE | ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_SUB, "sub", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_MUL, "mul", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_ASSOCIATIVE | ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_DIV, "div", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_MOD, "mod", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_NEG, "neg", ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_ABS, "abs", ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_MIN, "min", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_IDEMPOTENT | ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_MAX, "max", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_IDEMPOTENT | ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_FMA, "fma", ATL_MIR_OP_PROP_PURE, 3, 3, ATL_MIR_TYPE_F32},

    /* Bitwise */
    {ATL_MIR_OP_AND, "and", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_ASSOCIATIVE | ATL_MIR_OP_PROP_IDEMPOTENT | ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_OR, "or", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_ASSOCIATIVE | ATL_MIR_OP_PROP_IDEMPOTENT | ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_XOR, "xor", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_ASSOCIATIVE | ATL_MIR_OP_PROP_IDEMPOTENT | ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_NOT, "not", ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_SHL, "shl", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_SHR, "shr", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_SAR, "sar", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_ROL, "rol", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_ROR, "ror", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I32},

    /* Compare */
    {ATL_MIR_OP_CMP_EQ, "cmp_eq", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I1},
    {ATL_MIR_OP_CMP_NE, "cmp_ne", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I1},
    {ATL_MIR_OP_CMP_LT, "cmp_lt", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I1},
    {ATL_MIR_OP_CMP_LE, "cmp_le", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I1},
    {ATL_MIR_OP_CMP_GT, "cmp_gt", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I1},
    {ATL_MIR_OP_CMP_GE, "cmp_ge", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I1},
    {ATL_MIR_OP_CMP_ULT, "cmp_ult", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I1},
    {ATL_MIR_OP_CMP_ULE, "cmp_ule", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I1},
    {ATL_MIR_OP_CMP_UGT, "cmp_ugt", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I1},
    {ATL_MIR_OP_CMP_UGE, "cmp_uge", ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_I1},

    /* Control Flow */
    {ATL_MIR_OP_BR, "br", ATL_MIR_OP_PROP_TERMINATOR | ATL_MIR_OP_PROP_BRANCH, 0, 1, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_BR_IF, "br_if", ATL_MIR_OP_PROP_TERMINATOR | ATL_MIR_OP_PROP_BRANCH, 1, 2, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_SWITCH, "switch", ATL_MIR_OP_PROP_TERMINATOR | ATL_MIR_OP_PROP_BRANCH, 1, 0, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_CALL, "call", ATL_MIR_OP_PROP_CALL | ATL_MIR_OP_PROP_TERMINATOR, 1, ATL_MIR_MAX_OPERANDS, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_INDIRECT_CALL, "indirect_call", ATL_MIR_OP_PROP_CALL | ATL_MIR_OP_PROP_TERMINATOR, 1, ATL_MIR_MAX_OPERANDS, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_RET, "ret", ATL_MIR_OP_PROP_TERMINATOR, 0, 1, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_UNREACHABLE, "unreachable", ATL_MIR_OP_PROP_TERMINATOR, 0, 0, ATL_MIR_TYPE_VOID},

    /* Memory */
    {ATL_MIR_OP_LOAD, "load", ATL_MIR_OP_PROP_MEMORY_READ | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_STORE, "store", ATL_MIR_OP_PROP_MEMORY_WRITE, 2, 2, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_LOAD8, "load8", ATL_MIR_OP_PROP_MEMORY_READ | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I8},
    {ATL_MIR_OP_LOAD16, "load16", ATL_MIR_OP_PROP_MEMORY_READ | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I16},
    {ATL_MIR_OP_LOAD32, "load32", ATL_MIR_OP_PROP_MEMORY_READ | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_LOAD64, "load64", ATL_MIR_OP_PROP_MEMORY_READ | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I64},
    {ATL_MIR_OP_STORE8, "store8", ATL_MIR_OP_PROP_MEMORY_WRITE, 2, 2, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_STORE16, "store16", ATL_MIR_OP_PROP_MEMORY_WRITE, 2, 2, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_STORE32, "store32", ATL_MIR_OP_PROP_MEMORY_WRITE, 2, 2, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_STORE64, "store64", ATL_MIR_OP_PROP_MEMORY_WRITE, 2, 2, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_PREFETCH, "prefetch", ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_MEMORY_READ, 1, 2, ATL_MIR_TYPE_VOID},

    /* Vector/SIMD */
    {ATL_MIR_OP_VADD, "vadd", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 2, 2, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_VSUB, "vsub", ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 2, 2, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_VMUL, "vmul", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 2, 2, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_VDOT, "vdot", ATL_MIR_OP_PROP_COMMUTATIVE | ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 2, 2, ATL_MIR_TYPE_F32},
    {ATL_MIR_OP_VCROSS, "vcross", ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 2, 2, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_VNORMALIZE, "vnormalize", ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 1, 1, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_VSWIZZLE, "vswizzle", ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 1, 2, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_VLOAD, "vload", ATL_MIR_OP_PROP_MEMORY_READ | ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 1, 1, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_VSTORE, "vstore", ATL_MIR_OP_PROP_MEMORY_WRITE | ATL_MIR_OP_PROP_VECTOR, 2, 2, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_VSPLAT, "vsplat", ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 1, 1, ATL_MIR_TYPE_V128},

    /* Conversion */
    {ATL_MIR_OP_I2F, "i2f", ATL_MIR_OP_PROP_CONVERSION | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_F32},
    {ATL_MIR_OP_F2I, "f2i", ATL_MIR_OP_PROP_CONVERSION | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_I2V, "i2v", ATL_MIR_OP_PROP_CONVERSION | ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 1, 1, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_V2I, "v2i", ATL_MIR_OP_PROP_CONVERSION | ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 1, 1, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_ZEXT, "zext", ATL_MIR_OP_PROP_CONVERSION | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_SEXT, "sext", ATL_MIR_OP_PROP_CONVERSION | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_TRUNC, "trunc", ATL_MIR_OP_PROP_CONVERSION | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_BITCAST, "bitcast", ATL_MIR_OP_PROP_CONVERSION | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I32},

    /* Stack/Frame */
    {ATL_MIR_OP_ALLOC, "alloc", ATL_MIR_OP_PROP_STACK, 1, 2, ATL_MIR_TYPE_PTR},
    {ATL_MIR_OP_FREE, "free", ATL_MIR_OP_PROP_STACK, 1, 1, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_FRAME_ADDR, "frame_addr", ATL_MIR_OP_PROP_STACK | ATL_MIR_OP_PROP_PURE, 0, 0, ATL_MIR_TYPE_PTR},
    {ATL_MIR_OP_ARG, "arg", ATL_MIR_OP_PROP_STACK | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_PTR},
    {ATL_MIR_OP_LOCAL, "local", ATL_MIR_OP_PROP_STACK | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_PTR},
    {ATL_MIR_OP_STACK_PTR, "stack_ptr", ATL_MIR_OP_PROP_STACK | ATL_MIR_OP_PROP_PURE, 0, 0, ATL_MIR_TYPE_PTR},

    /* Atomic */
    {ATL_MIR_OP_CAS, "cas", ATL_MIR_OP_PROP_ATOMIC | ATL_MIR_OP_PROP_MEMORY_READ | ATL_MIR_OP_PROP_MEMORY_WRITE, 3, 3, ATL_MIR_TYPE_I1},
    {ATL_MIR_OP_XCHG, "xchg", ATL_MIR_OP_PROP_ATOMIC | ATL_MIR_OP_PROP_MEMORY_READ | ATL_MIR_OP_PROP_MEMORY_WRITE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_FETCH_ADD, "fetch_add", ATL_MIR_OP_PROP_ATOMIC | ATL_MIR_OP_PROP_MEMORY_READ | ATL_MIR_OP_PROP_MEMORY_WRITE, 2, 2, ATL_MIR_TYPE_I32},
    {ATL_MIR_OP_FENCE, "fence", ATL_MIR_OP_PROP_ATOMIC, 0, 0, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_ATOMIC_LOAD, "atomic_load", ATL_MIR_OP_PROP_ATOMIC | ATL_MIR_OP_PROP_MEMORY_READ | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_I32},

    /* Game-Specific */
    {ATL_MIR_OP_MAT4_MUL, "mat4_mul", ATL_MIR_OP_PROP_GAME_SPECIFIC | ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 2, 2, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_MAT4_VEC4, "mat4_vec4", ATL_MIR_OP_PROP_GAME_SPECIFIC | ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 2, 2, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_QUAT_SLERP, "quat_slerp", ATL_MIR_OP_PROP_GAME_SPECIFIC | ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 3, 3, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_TEX_SAMPLE, "tex_sample", ATL_MIR_OP_PROP_GAME_SPECIFIC | ATL_MIR_OP_PROP_PURE | ATL_MIR_OP_PROP_VECTOR, 2, 2, ATL_MIR_TYPE_V128},
    {ATL_MIR_OP_FOG_COMPUTE, "fog_compute", ATL_MIR_OP_PROP_GAME_SPECIFIC | ATL_MIR_OP_PROP_PURE, 2, 2, ATL_MIR_TYPE_F32},
    {ATL_MIR_OP_LERP, "lerp", ATL_MIR_OP_PROP_GAME_SPECIFIC | ATL_MIR_OP_PROP_PURE, 3, 3, ATL_MIR_TYPE_I32},

    /* Syscall/JNI/Intrinsic */
    {ATL_MIR_OP_SYSCALL, "syscall", ATL_MIR_OP_PROP_SYSCALL | ATL_MIR_OP_PROP_CALL, 1, ATL_MIR_MAX_OPERANDS, ATL_MIR_TYPE_I64},
    {ATL_MIR_OP_JNI_CALL, "jni_call", ATL_MIR_OP_PROP_SYSCALL | ATL_MIR_OP_PROP_CALL, 1, ATL_MIR_MAX_OPERANDS, ATL_MIR_TYPE_I64},
    {ATL_MIR_OP_INTRINSIC, "intrinsic", ATL_MIR_OP_PROP_SYSCALL | ATL_MIR_OP_PROP_CALL, 1, ATL_MIR_MAX_OPERANDS, ATL_MIR_TYPE_I64},
    {ATL_MIR_OP_CACHEFLUSH, "cacheflush", ATL_MIR_OP_PROP_SYSCALL, 2, 2, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_TLS_GET, "tls_get", ATL_MIR_OP_PROP_SYSCALL | ATL_MIR_OP_PROP_PURE, 1, 1, ATL_MIR_TYPE_PTR},

    /* Debug/Profiling */
    {ATL_MIR_OP_PROFILE_POINT, "profile_point", ATL_MIR_OP_PROP_DEBUG | ATL_MIR_OP_PROP_SYSCALL, 1, 1, ATL_MIR_TYPE_VOID},
    {ATL_MIR_OP_DEBUG_LOC, "debug_loc", ATL_MIR_OP_PROP_DEBUG | ATL_MIR_OP_PROP_SYSCALL, 0, 2, ATL_MIR_TYPE_VOID},
};

const atl_mir_opcode_info_t atl_mir_opcode_table[ATL_MIR_OP_COUNT] = {
#define OP_TABLE_ENTRY(i) opcode_properties[i]
    OP_TABLE_ENTRY(0), OP_TABLE_ENTRY(1), OP_TABLE_ENTRY(2), OP_TABLE_ENTRY(3),
    OP_TABLE_ENTRY(4), OP_TABLE_ENTRY(5), OP_TABLE_ENTRY(6), OP_TABLE_ENTRY(7),
    OP_TABLE_ENTRY(8), OP_TABLE_ENTRY(9), OP_TABLE_ENTRY(10), OP_TABLE_ENTRY(11),
    OP_TABLE_ENTRY(12), OP_TABLE_ENTRY(13), OP_TABLE_ENTRY(14), OP_TABLE_ENTRY(15),
    OP_TABLE_ENTRY(16), OP_TABLE_ENTRY(17), OP_TABLE_ENTRY(18), OP_TABLE_ENTRY(19),
    OP_TABLE_ENTRY(20), OP_TABLE_ENTRY(21), OP_TABLE_ENTRY(22), OP_TABLE_ENTRY(23),
    OP_TABLE_ENTRY(24), OP_TABLE_ENTRY(25), OP_TABLE_ENTRY(26), OP_TABLE_ENTRY(27),
    OP_TABLE_ENTRY(28), OP_TABLE_ENTRY(29), OP_TABLE_ENTRY(30), OP_TABLE_ENTRY(31),
    OP_TABLE_ENTRY(32), OP_TABLE_ENTRY(33), OP_TABLE_ENTRY(34), OP_TABLE_ENTRY(35),
    OP_TABLE_ENTRY(36), OP_TABLE_ENTRY(37), OP_TABLE_ENTRY(38), OP_TABLE_ENTRY(39),
    OP_TABLE_ENTRY(40), OP_TABLE_ENTRY(41), OP_TABLE_ENTRY(42), OP_TABLE_ENTRY(43),
    OP_TABLE_ENTRY(44), OP_TABLE_ENTRY(45), OP_TABLE_ENTRY(46), OP_TABLE_ENTRY(47),
    OP_TABLE_ENTRY(48), OP_TABLE_ENTRY(49), OP_TABLE_ENTRY(50), OP_TABLE_ENTRY(51),
    OP_TABLE_ENTRY(52), OP_TABLE_ENTRY(53), OP_TABLE_ENTRY(54), OP_TABLE_ENTRY(55),
    OP_TABLE_ENTRY(56), OP_TABLE_ENTRY(57), OP_TABLE_ENTRY(58), OP_TABLE_ENTRY(59),
    OP_TABLE_ENTRY(60), OP_TABLE_ENTRY(61), OP_TABLE_ENTRY(62), OP_TABLE_ENTRY(63),
    OP_TABLE_ENTRY(64), OP_TABLE_ENTRY(65), OP_TABLE_ENTRY(66), OP_TABLE_ENTRY(67),
    OP_TABLE_ENTRY(68), OP_TABLE_ENTRY(69), OP_TABLE_ENTRY(70), OP_TABLE_ENTRY(71),
    OP_TABLE_ENTRY(72), OP_TABLE_ENTRY(73), OP_TABLE_ENTRY(74), OP_TABLE_ENTRY(75),
    OP_TABLE_ENTRY(76), OP_TABLE_ENTRY(77), OP_TABLE_ENTRY(78), OP_TABLE_ENTRY(79),
    OP_TABLE_ENTRY(80), OP_TABLE_ENTRY(81), OP_TABLE_ENTRY(82), OP_TABLE_ENTRY(83),
    OP_TABLE_ENTRY(84), OP_TABLE_ENTRY(85), OP_TABLE_ENTRY(86), OP_TABLE_ENTRY(87),
    OP_TABLE_ENTRY(88), OP_TABLE_ENTRY(89), OP_TABLE_ENTRY(90), OP_TABLE_ENTRY(91),
    OP_TABLE_ENTRY(92), OP_TABLE_ENTRY(93), OP_TABLE_ENTRY(94), OP_TABLE_ENTRY(95),
#undef OP_TABLE_ENTRY
};

/* ============================================================================
 * Context Management
 * ============================================================================ */

#define ATL_MIR_ARENA_INITIAL_SIZE (4 * 1024 * 1024) /* 4MB */

atl_mir_context_t *atl_mir_context_create(void)
{
    atl_mir_context_t *ctx = calloc(1, sizeof(atl_mir_context_t));
    if (!ctx) return NULL;

    ctx->arena.capacity = ATL_MIR_ARENA_INITIAL_SIZE;
    ctx->arena.base = malloc(ctx->arena.capacity);
    if (!ctx->arena.base) {
        free(ctx);
        return NULL;
    }
    ctx->arena.offset = 0;

    ctx->options.verify_ssa = true;
    ctx->options.debug_print = false;
    ctx->options.opt_level = 1;
    ctx->options.enable_game_opts = true;

    ctx->last_error = 0;
    ctx->error_msg[0] = 0;

    return ctx;
}

void atl_mir_context_destroy(atl_mir_context_t *ctx)
{
    if (!ctx) return;
    free(ctx->arena.base);
    free(ctx);
}

void atl_mir_context_set_verify_ssa(atl_mir_context_t *ctx, bool enable) {
    if (ctx) ctx->options.verify_ssa = enable;
}

void atl_mir_context_set_debug_print(atl_mir_context_t *ctx, bool enable) {
    if (ctx) ctx->options.debug_print = enable;
}

void atl_mir_context_set_opt_level(atl_mir_context_t *ctx, uint32_t level) {
    if (ctx) ctx->options.opt_level = level;
}

void atl_mir_context_set_game_opts(atl_mir_context_t *ctx, bool enable) {
    if (ctx) ctx->options.enable_game_opts = enable;
}

int atl_mir_context_last_error(atl_mir_context_t *ctx) {
    return ctx ? ctx->last_error : -1;
}

const char *atl_mir_context_error_message(atl_mir_context_t *ctx) {
    return ctx ? ctx->error_msg : "NULL context";
}

/* Arena allocation helper */
static void *mir_arena_alloc(atl_mir_context_t *ctx, size_t size) {
    if (!ctx || !ctx->arena.base) return NULL;
    
    size = (size + 7) & ~7; /* 8-byte align */
    if (ctx->arena.offset + size > ctx->arena.capacity) {
        /* Grow arena */
        size_t new_cap = ctx->arena.capacity * 2;
        while (new_cap < ctx->arena.offset + size) new_cap *= 2;
        void *new_base = realloc(ctx->arena.base, new_cap);
        if (!new_base) return NULL;
        ctx->arena.base = new_base;
        ctx->arena.capacity = new_cap;
    }
    
    void *ptr = (uint8_t *)ctx->arena.base + ctx->arena.offset;
    ctx->arena.offset += size;
    return ptr;
}

/* ============================================================================
 * Module Management
 * ============================================================================ */

atl_mir_module_t *atl_mir_module_create(atl_mir_context_t *ctx)
{
    atl_mir_module_t *mod = mir_arena_alloc(ctx, sizeof(atl_mir_module_t));
    if (!mod) return NULL;

    mod->first_function = NULL;
    mod->last_function = NULL;
    mod->num_functions = 0;
    mod->next_function_id = 0;
    mod->context = ctx;

    mod->globals.data = NULL;
    mod->globals.size = 0;
    mod->globals.capacity = 0;

    mod->strings.data = NULL;
    mod->strings.size = 0;
    mod->strings.capacity = 0;

    mod->target.is_armv4 = true;
    mod->target.is_armv7 = true;
    mod->target.has_neon = true;
    mod->target.has_vfp = true;
    mod->target.is_thumb2 = true;

    mod->stats.total_insts = 0;
    mod->stats.total_blocks = 0;
    mod->stats.total_functions = 0;

    return mod;
}

void atl_mir_module_destroy(atl_mir_module_t *module)
{
    /* Arena-based: no per-object free needed */
    (void)module;
}

void atl_mir_module_set_target(atl_mir_module_t *module,
                                bool is_armv4, bool is_armv7,
                                bool has_neon, bool has_vfp, bool is_thumb2)
{
    if (!module) return;
    module->target.is_armv4 = is_armv4;
    module->target.is_armv7 = is_armv7;
    module->target.has_neon = has_neon;
    module->target.has_vfp = has_vfp;
    module->target.is_thumb2 = is_thumb2;
}

void *atl_mir_module_add_global(atl_mir_module_t *module, size_t size, size_t alignment)
{
    if (!module) return NULL;
    size = (size + alignment - 1) & ~(alignment - 1);
    void *ptr = mir_arena_alloc(module->context, size);
    return ptr;
}

const char *atl_mir_module_intern_string(atl_mir_module_t *module, const char *str)
{
    if (!module || !str) return NULL;
    size_t len = strlen(str) + 1;
    char *buf = mir_arena_alloc(module->context, len);
    if (!buf) return NULL;
    memcpy(buf, str, len);
    return buf;
}

/* ============================================================================
 * Function Creation
 * ============================================================================ */

atl_mir_function_t *atl_mir_module_add_function(atl_mir_module_t *module,
                                                  const char *name,
                                                  atl_mir_type_t return_type,
                                                  uint32_t num_args,
                                                  const atl_mir_type_t *arg_types)
{
    if (!module) return NULL;

    atl_mir_function_t *func = mir_arena_alloc(module->context, sizeof(atl_mir_function_t));
    if (!func) return NULL;

    memset(func, 0, sizeof(*func));
    func->id = module->next_function_id++;
    func->name = name ? atl_mir_module_intern_string(module, name) : "<anon>";
    func->return_type = return_type;
    func->num_args = num_args;
    func->module = module;
    func->stack_alignment = 16;
    func->tier = 0;

    for (uint32_t i = 0; i < num_args && i < ATL_MIR_MAX_ARGS; i++) {
        func->arg_types[i] = arg_types ? arg_types[i] : atl_mir_type_void();
        func->args[i] = NULL; /* Will be created by builder */
    }

    /* Link into module list */
    func->next = NULL;
    if (module->last_function) {
        module->last_function->next = func;
        module->last_function = func;
    } else {
        module->first_function = func;
        module->last_function = func;
    }
    module->num_functions++;
    module->stats.total_functions++;

    return func;
}

/* ============================================================================
 * Builder Management
 * ============================================================================ */

struct atl_mir_builder {
    atl_mir_function_t *func;
    atl_mir_block_t *cur_block;
    atl_mir_context_t *ctx;
};

atl_mir_builder_t *atl_mir_builder_create(atl_mir_function_t *func)
{
    if (!func || !func->module) return NULL;

    atl_mir_builder_t *b = calloc(1, sizeof(atl_mir_builder_t));
    if (!b) return NULL;

    b->func = func;
    b->cur_block = NULL;
    b->ctx = NULL; /* Will set when context available */

    return b;
}

void atl_mir_builder_destroy(atl_mir_builder_t *b)
{
    free(b);
}

atl_mir_block_t *atl_mir_builder_create_block(atl_mir_builder_t *b, const char *name)
{
    if (!b || !b->func) return NULL;

    atl_mir_block_t *block = calloc(1, sizeof(atl_mir_block_t));
    if (!block) return NULL;

    block->id = b->func->next_block_id++;
    block->first_inst = NULL;
    block->last_inst = NULL;
    block->num_insts = 0;
    block->preds = NULL;
    block->num_preds = 0;
    block->pred_capacity = 0;
    block->succs = NULL;
    block->num_succs = 0;
    block->succ_capacity = 0;
    block->loop_depth = 0;
    block->is_loop_header = false;
    block->exec_count = 0;
    block->last_exec_cycle = 0;
    block->prev = NULL;
    block->next = NULL;
    block->idom = NULL;
    block->dom_children = NULL;
    block->num_dom_children = 0;
    block->worklist_flags = 0;

    /* Link into function block list */
    if (b->func->last_block) {
        b->func->last_block->next = block;
        block->prev = b->func->last_block;
        b->func->last_block = block;
    } else {
        b->func->first_block = block;
        b->func->last_block = block;
        b->func->entry_block = block;
    }
    b->func->num_blocks++;

    (void)name; /* TODO: store block name for debug */
    return block;
}

void atl_mir_builder_set_block(atl_mir_builder_t *b, atl_mir_block_t *block)
{
    if (b) b->cur_block = block;
}

atl_mir_block_t *atl_mir_builder_current_block(atl_mir_builder_t *b)
{
    return b ? b->cur_block : NULL;
}

void atl_mir_builder_seal_block(atl_mir_builder_t *b, atl_mir_block_t *block)
{
    /* Mark block as complete (no more predecessors can be added) */
    (void)b; (void)block;
    /* No-op for now; sealing is a hint for optimizations */
}

/* ============================================================================
 * Branch/Control Flow
 * ============================================================================ */

void atl_mir_builder_br(atl_mir_builder_t *b, atl_mir_block_t *target)
{
    if (!b || !b->cur_block || !target) return;

    atl_mir_inst_t *inst = calloc(1, sizeof(atl_mir_inst_t));
    if (!inst) return;

    inst->id = b->func->next_inst_id++;
    inst->opcode = ATL_MIR_OP_BR;
    inst->result_type = atl_mir_type_void();
    inst->num_operands = 1;
    inst->operands[0] = NULL; /* Target is a block, not a value */
    inst->result = NULL;
    inst->succ[0] = target;
    inst->num_succ = 1;
    inst->block = b->cur_block;
    inst->flags = ATL_MIR_OP_PROP_TERMINATOR | ATL_MIR_OP_PROP_BRANCH;
    inst->prev = b->cur_block->last_inst;
    inst->next = NULL;

    if (b->cur_block->last_inst) {
        b->cur_block->last_inst->next = inst;
    } else {
        b->cur_block->first_inst = inst;
    }
    b->cur_block->last_inst = inst;
    b->cur_block->num_insts++;

    /* Add successor edge */
    if (target->num_succs < target->succ_capacity) {
        /* grow */
    }
    /* Simplified: just store in inst->succ */
    (void)target;
}

void atl_mir_builder_br_if(atl_mir_builder_t *b, atl_mir_value_t *cond,
                            atl_mir_block_t *then_block, atl_mir_block_t *else_block)
{
    if (!b || !b->cur_block || !cond) return;

    atl_mir_inst_t *inst = calloc(1, sizeof(atl_mir_inst_t));
    if (!inst) return;

    inst->id = b->func->next_inst_id++;
    inst->opcode = ATL_MIR_OP_BR_IF;
    inst->result_type = atl_mir_type_void();
    inst->num_operands = 1;
    inst->operands[0] = cond;
    inst->result = NULL;
    inst->succ[0] = then_block;
    inst->succ[1] = else_block;
    inst->num_succ = 2;
    inst->block = b->cur_block;
    inst->flags = ATL_MIR_OP_PROP_TERMINATOR | ATL_MIR_OP_PROP_BRANCH;
    inst->prev = b->cur_block->last_inst;
    inst->next = NULL;

    if (b->cur_block->last_inst) {
        b->cur_block->last_inst->next = inst;
    } else {
        b->cur_block->first_inst = inst;
    }
    b->cur_block->last_inst = inst;
    b->cur_block->num_insts++;

    /* Track cond value use */
    cond->num_uses++;
    atl_mir_use_t *use = calloc(1, sizeof(atl_mir_use_t));
    if (use) {
        use->value = cond;
        use->user = inst;
        use->operand_idx = 0;
        use->next = cond->uses;
        cond->uses = use;
    }
}

void atl_mir_builder_switch(atl_mir_builder_t *b, atl_mir_value_t *value,
                             atl_mir_block_t *default_block,
                             uint32_t num_cases, const uint64_t *case_values,
                             atl_mir_block_t *const *case_blocks)
{
    if (!b || !b->cur_block || !value) return;

    atl_mir_inst_t *inst = calloc(1, sizeof(atl_mir_inst_t));
    if (!inst) return;

    inst->id = b->func->next_inst_id++;
    inst->opcode = ATL_MIR_OP_SWITCH;
    inst->result_type = atl_mir_type_void();
    inst->num_operands = 1;
    inst->operands[0] = value;
    inst->result = NULL;
    inst->succ[0] = default_block;
    inst->num_succ = 1 + num_cases; /* Simplified */
    inst->block = b->cur_block;
    inst->flags = ATL_MIR_OP_PROP_TERMINATOR | ATL_MIR_OP_PROP_BRANCH;
    inst->prev = b->cur_block->last_inst;
    inst->next = NULL;

    if (b->cur_block->last_inst) {
        b->cur_block->last_inst->next = inst;
    } else {
        b->cur_block->first_inst = inst;
    }
    b->cur_block->last_inst = inst;
    b->cur_block->num_insts++;

    value->num_uses++;
    atl_mir_use_t *use = calloc(1, sizeof(atl_mir_use_t));
    if (use) {
        use->value = value;
        use->user = inst;
        use->operand_idx = 0;
        use->next = value->uses;
        value->uses = use;
    }

    (void)default_block; (void)num_cases; (void)case_values; (void)case_blocks;
}

void atl_mir_builder_ret(atl_mir_builder_t *b, atl_mir_value_t *value)
{
    if (!b || !b->cur_block) return;

    atl_mir_inst_t *inst = calloc(1, sizeof(atl_mir_inst_t));
    if (!inst) return;

    inst->id = b->func->next_inst_id++;
    inst->opcode = ATL_MIR_OP_RET;
    inst->result_type = b->func->return_type;
    inst->num_operands = value ? 1 : 0;
    if (value) inst->operands[0] = value;
    inst->result = NULL;
    inst->num_succ = 0;
    inst->block = b->cur_block;
    inst->flags = ATL_MIR_OP_PROP_TERMINATOR;
    inst->prev = b->cur_block->last_inst;
    inst->next = NULL;

    if (b->cur_block->last_inst) {
        b->cur_block->last_inst->next = inst;
    } else {
        b->cur_block->first_inst = inst;
    }
    b->cur_block->last_inst = inst;
    b->cur_block->num_insts++;

    if (value) {
        value->num_uses++;
        atl_mir_use_t *use = calloc(1, sizeof(atl_mir_use_t));
        if (use) {
            use->value = value;
            use->user = inst;
            use->operand_idx = 0;
            use->next = value->uses;
            value->uses = use;
        }
    }
}

void atl_mir_builder_ret_void(atl_mir_builder_t *b)
{
    atl_mir_builder_ret(b, NULL);
}

void atl_mir_builder_unreachable(atl_mir_builder_t *b)
{
    if (!b || !b->cur_block) return;

    atl_mir_inst_t *inst = calloc(1, sizeof(atl_mir_inst_t));
    if (!inst) return;

    inst->id = b->func->next_inst_id++;
    inst->opcode = ATL_MIR_OP_UNREACHABLE;
    inst->result_type = atl_mir_type_void();
    inst->num_operands = 0;
    inst->result = NULL;
    inst->num_succ = 0;
    inst->block = b->cur_block;
    inst->flags = ATL_MIR_OP_PROP_TERMINATOR;
    inst->prev = b->cur_block->last_inst;
    inst->next = NULL;

    if (b->cur_block->last_inst) {
        b->cur_block->last_inst->next = inst;
    } else {
        b->cur_block->first_inst = inst;
    }
    b->cur_block->last_inst = inst;
    b->cur_block->num_insts++;
}

/* ============================================================================
 * Value Access
 * ============================================================================ */

atl_mir_value_t *atl_mir_builder_get_arg(atl_mir_builder_t *b, uint32_t index)
{
    if (!b || index >= b->func->num_args || index >= ATL_MIR_MAX_ARGS) return NULL;

    if (!b->func->args[index]) {
        /* Create argument value */
        atl_mir_value_t *arg = calloc(1, sizeof(atl_mir_value_t));
        if (!arg) return NULL;

        arg->id = b->func->next_value_id++;
        arg->type = b->func->arg_types[index];
        arg->def = NULL;
        arg->uses = NULL;
        arg->num_uses = 0;
        arg->is_constant = false;
        arg->name = NULL;
        arg->next = NULL;

        /* Link into value list */
        if (b->func->last_value) {
            b->func->last_value->next = arg;
        } else {
            b->func->first_value = arg;
        }
        b->func->last_value = arg;
        b->func->num_values++;

        b->func->args[index] = arg;
    }
    return b->func->args[index];
}

/* ============================================================================
 * Constants
 * ============================================================================ */

atl_mir_value_t *atl_mir_builder_const_int(atl_mir_builder_t *b, atl_mir_type_t type, int64_t value)
{
    if (!b || !b->func) return NULL;

    atl_mir_value_t *c = calloc(1, sizeof(atl_mir_value_t));
    if (!c) return NULL;

    c->id = b->func->next_value_id++;
    c->type = type;
    c->def = NULL;
    c->uses = NULL;
    c->num_uses = 0;
    c->constant.i64 = value;
    c->is_constant = true;
    c->name = NULL;
    c->next = NULL;

    /* Link into value list */
    if (b->func->last_value) {
        b->func->last_value->next = c;
    } else {
        b->func->first_value = c;
    }
    b->func->last_value = c;
    b->func->num_values++;

    return c;
}

atl_mir_value_t *atl_mir_builder_const_float(atl_mir_builder_t *b, atl_mir_type_t type, double value)
{
    if (!b || !b->func) return NULL;

    atl_mir_value_t *c = calloc(1, sizeof(atl_mir_value_t));
    if (!c) return NULL;

    c->id = b->func->next_value_id++;
    c->type = type;
    c->def = NULL;
    c->uses = NULL;
    c->num_uses = 0;
    c->constant.f64 = value;
    c->is_constant = true;
    c->name = NULL;
    c->next = NULL;

    if (b->func->last_value) {
        b->func->last_value->next = c;
    } else {
        b->func->first_value = c;
    }
    b->func->last_value = c;
    b->func->num_values++;

    return c;
}

atl_mir_value_t *atl_mir_builder_const_ptr(atl_mir_builder_t *b, void *ptr)
{
    if (!b || !b->func) return NULL;

    atl_mir_value_t *c = calloc(1, sizeof(atl_mir_value_t));
    if (!c) return NULL;

    c->id = b->func->next_value_id++;
    c->type = atl_mir_type_ptr();
    c->def = NULL;
    c->uses = NULL;
    c->num_uses = 0;
    c->constant.ptr = ptr;
    c->is_constant = true;
    c->name = NULL;
    c->next = NULL;

    if (b->func->last_value) {
        b->func->last_value->next = c;
    } else {
        b->func->first_value = c;
    }
    b->func->last_value = c;
    b->func->num_values++;

    return c;
}

atl_mir_value_t *atl_mir_builder_const_vector(atl_mir_builder_t *b, atl_mir_type_t type, const void *data)
{
    if (!b || !b->func) return NULL;

    atl_mir_value_t *c = calloc(1, sizeof(atl_mir_value_t));
    if (!c) return NULL;

    c->id = b->func->next_value_id++;
    c->type = type;
    c->def = NULL;
    c->uses = NULL;
    c->num_uses = 0;
    c->constant.ptr = (void *)data;
    c->is_constant = true;
    c->name = NULL;
    c->next = NULL;

    if (b->func->last_value) {
        b->func->last_value->next = c;
    } else {
        b->func->first_value = c;
    }
    b->func->last_value = c;
    b->func->num_values++;

    return c;
}

/* ============================================================================
 * Local Variables (Stack Slots)
 * ============================================================================ */

atl_mir_value_t *atl_mir_builder_create_local(atl_mir_builder_t *b, atl_mir_type_t type, size_t size)
{
    if (!b || !b->func) return NULL;

    atl_mir_value_t *local = calloc(1, sizeof(atl_mir_value_t));
    if (!local) return NULL;

    local->id = b->func->next_value_id++;
    local->type = type;
    local->def = NULL;
    local->uses = NULL;
    local->num_uses = 0;
    local->is_constant = false;
    local->name = NULL;
    local->next = NULL;

    /* Register local slot */
    uint32_t slot = b->func->num_locals++;
    if (slot >= ATL_MIR_MAX_LOCALS) {
        free(local);
        return NULL;
    }
    b->func->local_sizes[slot] = size;
    b->func->local_types[slot] = type;

    /* Link into value list */
    if (b->func->last_value) {
        b->func->last_value->next = local;
    } else {
        b->func->first_value = local;
    }
    b->func->last_value = local;
    b->func->num_values++;

    return local;
}

atl_mir_value_t *atl_mir_builder_load_local(atl_mir_builder_t *b, atl_mir_value_t *local)
{
    if (!b || !local) return NULL;
    return atl_mir_builder_load(b, local->type, local, 4);
}

void atl_mir_builder_store_local(atl_mir_builder_t *b, atl_mir_value_t *local, atl_mir_value_t *value)
{
    if (!b || !local || !value) return;
    atl_mir_builder_store(b, local, value, 4);
}

/* ============================================================================
 * Generic Instruction Builder
 * ============================================================================ */

atl_mir_value_t *atl_mir_builder_inst(atl_mir_builder_t *b, atl_mir_opcode_t op,
                                       atl_mir_type_t result_type,
                                       uint32_t num_operands, atl_mir_value_t **operands)
{
    if (!b || !b->cur_block) return NULL;

    atl_mir_inst_t *inst = calloc(1, sizeof(atl_mir_inst_t));
    if (!inst) return NULL;

    inst->id = b->func->next_inst_id++;
    inst->opcode = op;
    inst->result_type = result_type;
    inst->num_operands = num_operands;
    for (uint32_t i = 0; i < num_operands && i < ATL_MIR_MAX_OPERANDS; i++) {
        inst->operands[i] = operands ? operands[i] : NULL;
    }
    inst->result = NULL;
    inst->block = b->cur_block;
    inst->flags = atl_mir_opcode_info(op)->properties;
    inst->prev = b->cur_block->last_inst;
    inst->next = NULL;

    if (b->cur_block->last_inst) {
        b->cur_block->last_inst->next = inst;
    } else {
        b->cur_block->first_inst = inst;
    }
    b->cur_block->last_inst = inst;
    b->cur_block->num_insts++;

    /* Create result value if not void */
    atl_mir_value_t *result = NULL;
    if (result_type.kind != ATL_MIR_TYPE_VOID) {
        result = calloc(1, sizeof(atl_mir_value_t));
        if (result) {
            result->id = b->func->next_value_id++;
            result->type = result_type;
            result->def = inst;
            result->uses = NULL;
            result->num_uses = 0;
            result->is_constant = false;
            result->name = NULL;
            result->next = NULL;

            if (b->func->last_value) {
                b->func->last_value->next = result;
            } else {
                b->func->first_value = result;
            }
            b->func->last_value = result;
            b->func->num_values++;
        }
        inst->result = result;
    }

    /* Track operand uses */
    for (uint32_t i = 0; i < num_operands && i < ATL_MIR_MAX_OPERANDS; i++) {
        if (operands && operands[i]) {
            operands[i]->num_uses++;
            atl_mir_use_t *use = calloc(1, sizeof(atl_mir_use_t));
            if (use) {
                use->value = operands[i];
                use->user = inst;
                use->operand_idx = i;
                use->next = operands[i]->uses;
                operands[i]->uses = use;
            }
        }
    }

    return result;
}

/* ============================================================================
 * Arithmetic Builders
 * ============================================================================ */

#define BUILD_BINARY(name, OP) \
atl_mir_value_t *atl_mir_builder_##name(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val) { \
    if (!b || !b->cur_block || !a || !b_val) return NULL; \
    atl_mir_value_t *ops[] = { a, b_val }; \
    return atl_mir_builder_inst(b, ATL_MIR_OP_##OP, a->type, 2, ops); \
}

BUILD_BINARY(add, ADD)
BUILD_BINARY(sub, SUB)
BUILD_BINARY(mul, MUL)
BUILD_BINARY(div, DIV)
BUILD_BINARY(mod, MOD)
BUILD_BINARY(min, MIN)
BUILD_BINARY(max, MAX)

#undef BUILD_BINARY

atl_mir_value_t *atl_mir_builder_neg(atl_mir_builder_t *b, atl_mir_value_t *a)
{
    if (!b || !b->cur_block || !a) return NULL;
    atl_mir_value_t *ops[] = { a };
    return atl_mir_builder_inst(b, ATL_MIR_OP_NEG, a->type, 1, ops);
}

atl_mir_value_t *atl_mir_builder_abs(atl_mir_builder_t *b, atl_mir_value_t *a)
{
    if (!b || !b->cur_block || !a) return NULL;
    atl_mir_value_t *ops[] = { a };
    return atl_mir_builder_inst(b, ATL_MIR_OP_ABS, a->type, 1, ops);
}

atl_mir_value_t *atl_mir_builder_fma(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val, atl_mir_value_t *c)
{
    if (!b || !b->cur_block || !a || !b_val || !c) return NULL;
    atl_mir_value_t *ops[] = { a, b_val, c };
    return atl_mir_builder_inst(b, ATL_MIR_OP_FMA, a->type, 3, ops);
}

/* ============================================================================
 * Bitwise Builders
 * ============================================================================ */

#define BUILD_BITWISE(name, OP) \
atl_mir_value_t *atl_mir_builder_##name(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val) { \
    if (!b || !b->cur_block || !a || !b_val) return NULL; \
    atl_mir_value_t *ops[] = { a, b_val }; \
    return atl_mir_builder_inst(b, ATL_MIR_OP_##OP, a->type, 2, ops); \
}

BUILD_BITWISE(and, AND)
BUILD_BITWISE(or, OR)
BUILD_BITWISE(xor, XOR)
BUILD_BITWISE(shl, SHL)
BUILD_BITWISE(shr, SHR)
BUILD_BITWISE(sar, SAR)
BUILD_BITWISE(rol, ROL)
BUILD_BITWISE(ror, ROR)

#undef BUILD_BITWISE

atl_mir_value_t *atl_mir_builder_not(atl_mir_builder_t *b, atl_mir_value_t *a)
{
    if (!b || !b->cur_block || !a) return NULL;
    atl_mir_value_t *ops[] = { a };
    return atl_mir_builder_inst(b, ATL_MIR_OP_NOT, a->type, 1, ops);
}

/* ============================================================================
 * Compare Builders
 * ============================================================================ */

#define BUILD_CMP(name, OP) \
atl_mir_value_t *atl_mir_builder_##name(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val) { \
    if (!b || !b->cur_block || !a || !b_val) return NULL; \
    atl_mir_value_t *ops[] = { a, b_val }; \
    return atl_mir_builder_inst(b, ATL_MIR_OP_##OP, atl_mir_type_int(1), 2, ops); \
}

BUILD_CMP(cmp_eq, CMP_EQ)
BUILD_CMP(cmp_ne, CMP_NE)
BUILD_CMP(cmp_lt, CMP_LT)
BUILD_CMP(cmp_le, CMP_LE)
BUILD_CMP(cmp_gt, CMP_GT)
BUILD_CMP(cmp_ge, CMP_GE)
BUILD_CMP(cmp_ult, CMP_ULT)
BUILD_CMP(cmp_ule, CMP_ULE)
BUILD_CMP(cmp_ugt, CMP_UGT)
BUILD_CMP(cmp_uge, CMP_UGE)

#undef BUILD_CMP

/* ============================================================================
 * Memory Builders
 * ============================================================================ */

atl_mir_value_t *atl_mir_builder_load(atl_mir_builder_t *b, atl_mir_type_t type, atl_mir_value_t *addr, uint32_t align)
{
    if (!b || !b->cur_block || !addr) return NULL;
    atl_mir_value_t *ops[] = { addr };
    atl_mir_value_t *result = atl_mir_builder_inst(b, ATL_MIR_OP_LOAD, type, 1, ops);
    (void)align;
    return result;
}

atl_mir_value_t *atl_mir_builder_load8(atl_mir_builder_t *b, atl_mir_value_t *addr)
{
    if (!b || !b->cur_block || !addr) return NULL;
    atl_mir_value_t *ops[] = { addr };
    return atl_mir_builder_inst(b, ATL_MIR_OP_LOAD8, atl_mir_type_int(8), 1, ops);
}

atl_mir_value_t *atl_mir_builder_load16(atl_mir_builder_t *b, atl_mir_value_t *addr)
{
    if (!b || !b->cur_block || !addr) return NULL;
    atl_mir_value_t *ops[] = { addr };
    return atl_mir_builder_inst(b, ATL_MIR_OP_LOAD16, atl_mir_type_int(16), 1, ops);
}

atl_mir_value_t *atl_mir_builder_load32(atl_mir_builder_t *b, atl_mir_value_t *addr)
{
    if (!b || !b->cur_block || !addr) return NULL;
    atl_mir_value_t *ops[] = { addr };
    return atl_mir_builder_inst(b, ATL_MIR_OP_LOAD32, atl_mir_type_int(32), 1, ops);
}

atl_mir_value_t *atl_mir_builder_load64(atl_mir_builder_t *b, atl_mir_value_t *addr)
{
    if (!b || !b->cur_block || !addr) return NULL;
    atl_mir_value_t *ops[] = { addr };
    return atl_mir_builder_inst(b, ATL_MIR_OP_LOAD64, atl_mir_type_int(64), 1, ops);
}

void atl_mir_builder_store(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value, uint32_t align)
{
    if (!b || !b->cur_block || !addr || !value) return;
    atl_mir_value_t *ops[] = { addr, value };
    atl_mir_builder_inst(b, ATL_MIR_OP_STORE, atl_mir_type_void(), 2, ops);
    (void)align;
}

void atl_mir_builder_store8(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value)
{
    if (!b || !b->cur_block || !addr || !value) return;
    atl_mir_value_t *ops[] = { addr, value };
    atl_mir_builder_inst(b, ATL_MIR_OP_STORE8, atl_mir_type_void(), 2, ops);
}

void atl_mir_builder_store16(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value)
{
    if (!b || !b->cur_block || !addr || !value) return;
    atl_mir_value_t *ops[] = { addr, value };
    atl_mir_builder_inst(b, ATL_MIR_OP_STORE16, atl_mir_type_void(), 2, ops);
}

void atl_mir_builder_store32(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value)
{
    if (!b || !b->cur_block || !addr || !value) return;
    atl_mir_value_t *ops[] = { addr, value };
    atl_mir_builder_inst(b, ATL_MIR_OP_STORE32, atl_mir_type_void(), 2, ops);
}

void atl_mir_builder_store64(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value)
{
    if (!b || !b->cur_block || !addr || !value) return;
    atl_mir_value_t *ops[] = { addr, value };
    atl_mir_builder_inst(b, ATL_MIR_OP_STORE64, atl_mir_type_void(), 2, ops);
}

void atl_mir_builder_prefetch(atl_mir_builder_t *b, atl_mir_value_t *addr, int rw, int locality)
{
    if (!b || !b->cur_block || !addr) return;
    atl_mir_value_t *ops[] = { addr };
    atl_mir_builder_inst(b, ATL_MIR_OP_PREFETCH, atl_mir_type_void(), 1, ops);
    (void)rw; (void)locality;
}

/* ============================================================================
 * Vector Builders
 * ============================================================================ */

#define BUILD_VECTOR(name, OP) \
atl_mir_value_t *atl_mir_builder_##name(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val) { \
    if (!b || !b->cur_block || !a || !b_val) return NULL; \
    atl_mir_value_t *ops[] = { a, b_val }; \
    return atl_mir_builder_inst(b, ATL_MIR_OP_##OP, a->type, 2, ops); \
}

BUILD_VECTOR(vadd, VADD)
BUILD_VECTOR(vsub, VSUB)
BUILD_VECTOR(vmul, VMUL)
BUILD_VECTOR(vdot, VDOT)
BUILD_VECTOR(vcross, VCROSS)

#undef BUILD_VECTOR

atl_mir_value_t *atl_mir_builder_vnormalize(atl_mir_builder_t *b, atl_mir_value_t *a)
{
    if (!b || !b->cur_block || !a) return NULL;
    atl_mir_value_t *ops[] = { a };
    return atl_mir_builder_inst(b, ATL_MIR_OP_VNORMALIZE, a->type, 1, ops);
}

atl_mir_value_t *atl_mir_builder_vswizzle(atl_mir_builder_t *b, atl_mir_value_t *a, uint32_t mask)
{
    if (!b || !b->cur_block || !a) return NULL;
    atl_mir_value_t *ops[] = { a };
    atl_mir_value_t *result = atl_mir_builder_inst(b, ATL_MIR_OP_VSWIZZLE, a->type, 1, ops);
    /* Store mask somewhere for lowering */
    (void)mask;
    return result;
}

atl_mir_value_t *atl_mir_builder_vload(atl_mir_builder_t *b, atl_mir_type_t type, atl_mir_value_t *addr, uint32_t align)
{
    if (!b || !b->cur_block || !addr) return NULL;
    atl_mir_value_t *ops[] = { addr };
    return atl_mir_builder_inst(b, ATL_MIR_OP_VLOAD, type, 1, ops);
    (void)align;
}

void atl_mir_builder_vstore(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value, uint32_t align)
{
    if (!b || !b->cur_block || !addr || !value) return;
    atl_mir_value_t *ops[] = { addr, value };
    atl_mir_builder_inst(b, ATL_MIR_OP_VSTORE, atl_mir_type_void(), 2, ops);
    (void)align;
}

atl_mir_value_t *atl_mir_builder_vsplat(atl_mir_builder_t *b, atl_mir_type_t type, atl_mir_value_t *scalar)
{
    if (!b || !b->cur_block || !scalar) return NULL;
    atl_mir_value_t *ops[] = { scalar };
    return atl_mir_builder_inst(b, ATL_MIR_OP_VSPLAT, type, 1, ops);
}

/* ============================================================================
 * Conversion Builders
 * ============================================================================ */

atl_mir_value_t *atl_mir_builder_i2f(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from)
{
    if (!b || !b->cur_block || !from) return NULL;
    atl_mir_value_t *ops[] = { from };
    return atl_mir_builder_inst(b, ATL_MIR_OP_I2F, to_type, 1, ops);
}

atl_mir_value_t *atl_mir_builder_f2i(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from)
{
    if (!b || !b->cur_block || !from) return NULL;
    atl_mir_value_t *ops[] = { from };
    return atl_mir_builder_inst(b, ATL_MIR_OP_F2I, to_type, 1, ops);
}

atl_mir_value_t *atl_mir_builder_zext(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from)
{
    if (!b || !b->cur_block || !from) return NULL;
    atl_mir_value_t *ops[] = { from };
    return atl_mir_builder_inst(b, ATL_MIR_OP_ZEXT, to_type, 1, ops);
}

atl_mir_value_t *atl_mir_builder_sext(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from)
{
    if (!b || !b->cur_block || !from) return NULL;
    atl_mir_value_t *ops[] = { from };
    return atl_mir_builder_inst(b, ATL_MIR_OP_SEXT, to_type, 1, ops);
}

atl_mir_value_t *atl_mir_builder_trunc(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from)
{
    if (!b || !b->cur_block || !from) return NULL;
    atl_mir_value_t *ops[] = { from };
    return atl_mir_builder_inst(b, ATL_MIR_OP_TRUNC, to_type, 1, ops);
}

atl_mir_value_t *atl_mir_builder_bitcast(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from)
{
    if (!b || !b->cur_block || !from) return NULL;
    atl_mir_value_t *ops[] = { from };
    return atl_mir_builder_inst(b, ATL_MIR_OP_BITCAST, to_type, 1, ops);
}

/* ============================================================================
 * Stack/Frame Builders
 * ============================================================================ */

atl_mir_value_t *atl_mir_builder_alloc(atl_mir_builder_t *b, size_t size, uint32_t align)
{
    if (!b || !b->cur_block) return NULL;
    atl_mir_value_t *ops[] = { NULL };
    ops[0] = atl_mir_builder_const_int(b, atl_mir_type_int(64), (int64_t)size);
    return atl_mir_builder_inst(b, ATL_MIR_OP_ALLOC, atl_mir_type_ptr(), 1, ops);
    (void)align;
}

void atl_mir_builder_free(atl_mir_builder_t *b, atl_mir_value_t *ptr)
{
    if (!b || !b->cur_block || !ptr) return;
    atl_mir_value_t *ops[] = { ptr };
    atl_mir_builder_inst(b, ATL_MIR_OP_FREE, atl_mir_type_void(), 1, ops);
}

atl_mir_value_t *atl_mir_builder_frame_addr(atl_mir_builder_t *b)
{
    if (!b || !b->cur_block) return NULL;
    return atl_mir_builder_inst(b, ATL_MIR_OP_FRAME_ADDR, atl_mir_type_ptr(), 0, NULL);
}

atl_mir_value_t *atl_mir_builder_stack_ptr(atl_mir_builder_t *b)
{
    if (!b || !b->cur_block) return NULL;
    return atl_mir_builder_inst(b, ATL_MIR_OP_STACK_PTR, atl_mir_type_ptr(), 0, NULL);
}

/* ============================================================================
 * Atomic Builders
 * ============================================================================ */

atl_mir_value_t *atl_mir_builder_cas(atl_mir_builder_t *b, atl_mir_type_t type,
                                      atl_mir_value_t *addr, atl_mir_value_t *expected, atl_mir_value_t *desired)
{
    if (!b || !b->cur_block || !addr || !expected || !desired) return NULL;
    atl_mir_value_t *ops[] = { addr, expected, desired };
    return atl_mir_builder_inst(b, ATL_MIR_OP_CAS, atl_mir_type_int(1), 3, ops);
}

atl_mir_value_t *atl_mir_builder_xchg(atl_mir_builder_t *b, atl_mir_type_t type,
                                       atl_mir_value_t *addr, atl_mir_value_t *value)
{
    if (!b || !b->cur_block || !addr || !value) return NULL;
    atl_mir_value_t *ops[] = { addr, value };
    return atl_mir_builder_inst(b, ATL_MIR_OP_XCHG, type, 2, ops);
}

atl_mir_value_t *atl_mir_builder_fetch_add(atl_mir_builder_t *b, atl_mir_type_t type,
                                            atl_mir_value_t *addr, atl_mir_value_t *value)
{
    if (!b || !b->cur_block || !addr || !value) return NULL;
    atl_mir_value_t *ops[] = { addr, value };
    return atl_mir_builder_inst(b, ATL_MIR_OP_FETCH_ADD, type, 2, ops);
}

void atl_mir_builder_fence(atl_mir_builder_t *b)
{
    if (!b || !b->cur_block) return;
    atl_mir_builder_inst(b, ATL_MIR_OP_FENCE, atl_mir_type_void(), 0, NULL);
}

atl_mir_value_t *atl_mir_builder_atomic_load(atl_mir_builder_t *b, atl_mir_type_t type, atl_mir_value_t *addr)
{
    if (!b || !b->cur_block || !addr) return NULL;
    atl_mir_value_t *ops[] = { addr };
    return atl_mir_builder_inst(b, ATL_MIR_OP_ATOMIC_LOAD, type, 1, ops);
}

/* ============================================================================
 * Game-Specific Builders
 * ============================================================================ */

atl_mir_value_t *atl_mir_builder_mat4_mul(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val)
{
    if (!b || !b->cur_block || !a || !b_val) return NULL;
    atl_mir_value_t *ops[] = { a, b_val };
    return atl_mir_builder_inst(b, ATL_MIR_OP_MAT4_MUL, atl_mir_type_vector(128, 4), 2, ops);
}

atl_mir_value_t *atl_mir_builder_mat4_vec4(atl_mir_builder_t *b, atl_mir_value_t *mat, atl_mir_value_t *vec)
{
    if (!b || !b->cur_block || !mat || !vec) return NULL;
    atl_mir_value_t *ops[] = { mat, vec };
    return atl_mir_builder_inst(b, ATL_MIR_OP_MAT4_VEC4, atl_mir_type_vector(128, 4), 2, ops);
}

atl_mir_value_t *atl_mir_builder_quat_slerp(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val, atl_mir_value_t *t)
{
    if (!b || !b->cur_block || !a || !b_val || !t) return NULL;
    atl_mir_value_t *ops[] = { a, b_val, t };
    return atl_mir_builder_inst(b, ATL_MIR_OP_QUAT_SLERP, atl_mir_type_vector(128, 4), 3, ops);
}

atl_mir_value_t *atl_mir_builder_tex_sample(atl_mir_builder_t *b, atl_mir_value_t *sampler, atl_mir_value_t *coord)
{
    if (!b || !b->cur_block || !sampler || !coord) return NULL;
    atl_mir_value_t *ops[] = { sampler, coord };
    return atl_mir_builder_inst(b, ATL_MIR_OP_TEX_SAMPLE, atl_mir_type_vector(128, 4), 2, ops);
}

atl_mir_value_t *atl_mir_builder_fog_compute(atl_mir_builder_t *b, atl_mir_value_t *dist, atl_mir_value_t *density)
{
    if (!b || !b->cur_block || !dist || !density) return NULL;
    atl_mir_value_t *ops[] = { dist, density };
    return atl_mir_builder_inst(b, ATL_MIR_OP_FOG_COMPUTE, atl_mir_type_float(32), 2, ops);
}

atl_mir_value_t *atl_mir_builder_lerp(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val, atl_mir_value_t *t)
{
    if (!b || !b->cur_block || !a || !b_val || !t) return NULL;
    atl_mir_value_t *ops[] = { a, b_val, t };
    return atl_mir_builder_inst(b, ATL_MIR_OP_LERP, a->type, 3, ops);
}

/* ============================================================================
 * Syscall/JNI/Intrinsic Builders
 * ============================================================================ */

atl_mir_value_t *atl_mir_builder_syscall(atl_mir_builder_t *b, uint32_t syscall_num,
                                          uint32_t num_args, atl_mir_value_t **args)
{
    if (!b || !b->cur_block) return NULL;
    atl_mir_value_t **ops = malloc(sizeof(atl_mir_value_t *) * (1 + num_args));
    if (!ops) return NULL;
    ops[0] = atl_mir_builder_const_int(b, atl_mir_type_int(32), (int64_t)syscall_num);
    for (uint32_t i = 0; i < num_args; i++) ops[1 + i] = args ? args[i] : NULL;
    atl_mir_value_t *result = atl_mir_builder_inst(b, ATL_MIR_OP_SYSCALL, atl_mir_type_int(64), 1 + num_args, ops);
    free(ops);
    return result;
}

atl_mir_value_t *atl_mir_builder_jni_call(atl_mir_builder_t *b, const char *method_name,
                                           uint32_t num_args, atl_mir_value_t **args)
{
    if (!b || !b->cur_block) return NULL;
    atl_mir_value_t **ops = malloc(sizeof(atl_mir_value_t *) * (1 + num_args));
    if (!ops) return NULL;
    ops[0] = NULL; /* Method name passed separately */
    for (uint32_t i = 0; i < num_args; i++) ops[1 + i] = args ? args[i] : NULL;
    atl_mir_value_t *result = atl_mir_builder_inst(b, ATL_MIR_OP_JNI_CALL, atl_mir_type_int(64), 1 + num_args, ops);
    free(ops);
    (void)method_name;
    return result;
}

atl_mir_value_t *atl_mir_builder_intrinsic(atl_mir_builder_t *b, const char *name,
                                            uint32_t num_args, atl_mir_value_t **args)
{
    if (!b || !b->cur_block) return NULL;
    atl_mir_value_t **ops = malloc(sizeof(atl_mir_value_t *) * num_args);
    if (!ops) return NULL;
    for (uint32_t i = 0; i < num_args; i++) ops[i] = args ? args[i] : NULL;
    atl_mir_value_t *result = atl_mir_builder_inst(b, ATL_MIR_OP_INTRINSIC, atl_mir_type_int(64), num_args, ops);
    free(ops);
    (void)name;
    return result;
}

void atl_mir_builder_cacheflush(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *len)
{
    if (!b || !b->cur_block || !addr || !len) return;
    atl_mir_value_t *ops[] = { addr, len };
    atl_mir_builder_inst(b, ATL_MIR_OP_CACHEFLUSH, atl_mir_type_void(), 2, ops);
}

atl_mir_value_t *atl_mir_builder_tls_get(atl_mir_builder_t *b, uint32_t tls_index)
{
    if (!b || !b->cur_block) return NULL;
    atl_mir_value_t *ops[] = { atl_mir_builder_const_int(b, atl_mir_type_int(32), (int64_t)tls_index) };
    return atl_mir_builder_inst(b, ATL_MIR_OP_TLS_GET, atl_mir_type_ptr(), 1, ops);
}

/* ============================================================================
 * Debug/Profiling Builders
 * ============================================================================ */

void atl_mir_builder_profile_point(atl_mir_builder_t *b, uint32_t point_id)
{
    if (!b || !b->cur_block) return;
    atl_mir_value_t *ops[] = { atl_mir_builder_const_int(b, atl_mir_type_int(32), (int64_t)point_id) };
    atl_mir_builder_inst(b, ATL_MIR_OP_PROFILE_POINT, atl_mir_type_void(), 1, ops);
}

void atl_mir_builder_debug_loc(atl_mir_builder_t *b, const char *file, uint32_t line, uint32_t arm_pc)
{
    if (!b || !b->cur_block) return;
    atl_mir_value_t *ops[2] = { NULL, NULL };
    if (file) ops[0] = NULL; /* String constant - TODO */
    if (arm_pc) ops[1] = atl_mir_builder_const_int(b, atl_mir_type_int(32), (int64_t)arm_pc);
    atl_mir_builder_inst(b, ATL_MIR_OP_DEBUG_LOC, atl_mir_type_void(), 2, ops);
    (void)file; (void)line;
}

/* ============================================================================
 * Type Inference Helpers
 * ============================================================================ */

atl_mir_type_t atl_mir_infer_binary_result_type(atl_mir_opcode_t op, atl_mir_type_t lhs, atl_mir_type_t rhs)
{
    /* For arithmetic/bitwise, result type matches operand type */
    return lhs;
}

atl_mir_type_t atl_mir_infer_unary_result_type(atl_mir_opcode_t op, atl_mir_type_t operand)
{
    return operand;
}

atl_mir_type_kind_t atl_mir_conversion_target_type(atl_mir_opcode_t op)
{
    switch (op) {
        case ATL_MIR_OP_I2F: return ATL_MIR_TYPE_F32;
        case ATL_MIR_OP_F2I: return ATL_MIR_TYPE_I32;
        case ATL_MIR_OP_I2V: return ATL_MIR_TYPE_V128;
        case ATL_MIR_OP_V2I: return ATL_MIR_TYPE_I32;
        case ATL_MIR_OP_ZEXT: return ATL_MIR_TYPE_I32;
        case ATL_MIR_OP_SEXT: return ATL_MIR_TYPE_I32;
        case ATL_MIR_OP_TRUNC: return ATL_MIR_TYPE_I32;
        case ATL_MIR_OP_BITCAST: return ATL_MIR_TYPE_I32;
        default: return ATL_MIR_TYPE_VOID;
    }
}

/* ============================================================================
 * Opcode Category
 * ============================================================================ */

atl_mir_opcode_category_t atl_mir_opcode_category(atl_mir_opcode_t op)
{
    switch (op) {
        case ATL_MIR_OP_ADD: case ATL_MIR_OP_SUB: case ATL_MIR_OP_MUL:
        case ATL_MIR_OP_DIV: case ATL_MIR_OP_MOD: case ATL_MIR_OP_NEG:
        case ATL_MIR_OP_ABS: case ATL_MIR_OP_MIN: case ATL_MIR_OP_MAX:
        case ATL_MIR_OP_FMA: return ATL_MIR_CAT_ARITHMETIC;
        case ATL_MIR_OP_AND: case ATL_MIR_OP_OR: case ATL_MIR_OP_XOR:
        case ATL_MIR_OP_NOT: case ATL_MIR_OP_SHL: case ATL_MIR_OP_SHR:
        case ATL_MIR_OP_SAR: case ATL_MIR_OP_ROL: case ATL_MIR_OP_ROR:
            return ATL_MIR_CAT_BITWISE;
        case ATL_MIR_OP_CMP_EQ: case ATL_MIR_OP_CMP_NE: case ATL_MIR_OP_CMP_LT:
        case ATL_MIR_OP_CMP_LE: case ATL_MIR_OP_CMP_GT: case ATL_MIR_OP_CMP_GE:
        case ATL_MIR_OP_CMP_ULT: case ATL_MIR_OP_CMP_ULE: case ATL_MIR_OP_CMP_UGT:
        case ATL_MIR_OP_CMP_UGE: return ATL_MIR_CAT_COMPARE;
        case ATL_MIR_OP_BR: case ATL_MIR_OP_BR_IF: case ATL_MIR_OP_SWITCH:
        case ATL_MIR_OP_CALL: case ATL_MIR_OP_INDIRECT_CALL: case ATL_MIR_OP_RET:
        case ATL_MIR_OP_UNREACHABLE: return ATL_MIR_CAT_CONTROL_FLOW;
        case ATL_MIR_OP_LOAD: case ATL_MIR_OP_STORE: case ATL_MIR_OP_LOAD8:
        case ATL_MIR_OP_LOAD16: case ATL_MIR_OP_LOAD32: case ATL_MIR_OP_LOAD64:
        case ATL_MIR_OP_STORE8: case ATL_MIR_OP_STORE16: case ATL_MIR_OP_STORE32:
        case ATL_MIR_OP_STORE64: case ATL_MIR_OP_PREFETCH: return ATL_MIR_CAT_MEMORY;
        case ATL_MIR_OP_VADD: case ATL_MIR_OP_VSUB: case ATL_MIR_OP_VMUL:
        case ATL_MIR_OP_VDOT: case ATL_MIR_OP_VCROSS: case ATL_MIR_OP_VNORMALIZE:
        case ATL_MIR_OP_VSWIZZLE: case ATL_MIR_OP_VLOAD: case ATL_MIR_OP_VSTORE:
        case ATL_MIR_OP_VSPLAT: return ATL_MIR_CAT_VECTOR;
        case ATL_MIR_OP_I2F: case ATL_MIR_OP_F2I: case ATL_MIR_OP_I2V:
        case ATL_MIR_OP_V2I: case ATL_MIR_OP_ZEXT: case ATL_MIR_OP_SEXT:
        case ATL_MIR_OP_TRUNC: case ATL_MIR_OP_BITCAST: return ATL_MIR_CAT_CONVERSION;
        case ATL_MIR_OP_ALLOC: case ATL_MIR_OP_FREE: case ATL_MIR_OP_FRAME_ADDR:
        case ATL_MIR_OP_ARG: case ATL_MIR_OP_LOCAL: case ATL_MIR_OP_STACK_PTR:
            return ATL_MIR_CAT_STACK;
        case ATL_MIR_OP_CAS: case ATL_MIR_OP_XCHG: case ATL_MIR_OP_FETCH_ADD:
        case ATL_MIR_OP_FENCE: case ATL_MIR_OP_ATOMIC_LOAD: return ATL_MIR_CAT_ATOMIC;
        case ATL_MIR_OP_MAT4_MUL: case ATL_MIR_OP_MAT4_VEC4: case ATL_MIR_OP_QUAT_SLERP:
        case ATL_MIR_OP_TEX_SAMPLE: case ATL_MIR_OP_FOG_COMPUTE: case ATL_MIR_OP_LERP:
            return ATL_MIR_CAT_GAME_SPECIFIC;
        case ATL_MIR_OP_SYSCALL: case ATL_MIR_OP_JNI_CALL: case ATL_MIR_OP_INTRINSIC:
        case ATL_MIR_OP_CACHEFLUSH: case ATL_MIR_OP_TLS_GET: return ATL_MIR_CAT_SYSCALL;
        case ATL_MIR_OP_PROFILE_POINT: case ATL_MIR_OP_DEBUG_LOC: return ATL_MIR_CAT_DEBUG;
        default: return ATL_MIR_CAT_INVALID;
    }
}

/* ============================================================================
 * ARM to MIR Mapping (defined in arm_decode/arm_decode.c)
 * ============================================================================ */
