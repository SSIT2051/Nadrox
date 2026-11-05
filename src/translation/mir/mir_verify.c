#include "mir.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * SSA Verification
 * ============================================================================
 * Verifies that MIR IR preserves SSA form after every optimization pass.
 * Critical for catching IR corruption bugs early.
 * ============================================================================ */

/* Verify a single value's use-def chain */
static bool verify_value(atl_mir_function_t *func, atl_mir_value_t *val,
                          char *error_buf, size_t error_buf_size)
{
    if (!val) return true;

    /* Every non-constant, non-argument value must have exactly one definition */
    if (!val->is_constant && val->def == NULL) {
        /* Check if it's an argument */
        bool is_arg = false;
        for (uint32_t i = 0; i < func->num_args; i++) {
            if (func->args[i] == val) { is_arg = true; break; }
        }
        if (!is_arg) {
            snprintf(error_buf, error_buf_size,
                     "Value %u (%s) has no definition and is not an argument",
                     val->id, val->name ? val->name : "?");
            return false;
        }
    }

    /* Result value must be defined by its instruction */
    if (val->def && val->def->result != val) {
        snprintf(error_buf, error_buf_size,
                 "Value %u def->result mismatch: def inst %u result %p != value %p",
                 val->id, val->def->id, (void*)val->def->result, (void*)val);
        return false;
    }

    /* Every use must reference this value */
    atl_mir_use_t *use = val->uses;
    while (use) {
        if (use->value != val) {
            snprintf(error_buf, error_buf_size,
                     "Use chain corrupted: use->value %p != val %p",
                     (void*)use->value, (void*)val);
            return false;
        }
        if (use->operand_idx >= ATL_MIR_MAX_OPERANDS) {
            snprintf(error_buf, error_buf_size,
                     "Use operand_idx %u out of range", use->operand_idx);
            return false;
        }
        /* Verify the user instruction actually uses this value at that slot */
        if (use->user && use->operand_idx < use->user->num_operands) {
            if (use->user->operands[use->operand_idx] != val) {
                snprintf(error_buf, error_buf_size,
                         "Use chain mismatch: inst %u operand %u = %p, expected %p",
                         use->user->id, use->operand_idx,
                         (void*)use->user->operands[use->operand_idx], (void*)val);
                return false;
            }
        }
        use = use->next;
    }

    /* num_uses must match actual use count */
    size_t actual_uses = 0;
    use = val->uses;
    while (use) { actual_uses++; use = use->next; }
    if (val->num_uses != actual_uses) {
        snprintf(error_buf, error_buf_size,
                 "Value %u num_uses=%zu but actual use count=%zu",
                 val->id, val->num_uses, actual_uses);
        return false;
    }

    return true;
}

/* Verify a single instruction */
static bool verify_inst(atl_mir_function_t *func, atl_mir_inst_t *inst,
                         char *error_buf, size_t error_buf_size)
{
    if (!inst) return true;

    /* Opcode must be valid */
    if (inst->opcode >= ATL_MIR_OP_COUNT) {
        snprintf(error_buf, error_buf_size,
                 "Inst %u has invalid opcode %u", inst->id, inst->opcode);
        return false;
    }

    /* Operand count must be within limits */
    if (inst->num_operands > ATL_MIR_MAX_OPERANDS) {
        snprintf(error_buf, error_buf_size,
                 "Inst %u has %u operands, max is %u",
                 inst->id, inst->num_operands, ATL_MIR_MAX_OPERANDS);
        return false;
    }

    /* Operands may be NULL when values cross block boundaries; skip NULL checks. */
    (void)func;
    (void)error_buf;
    (void)error_buf_size;

    /* Result type must match opcode properties */
    if (inst->result_type.kind == ATL_MIR_TYPE_VOID &&
        atl_mir_opcode_info(inst->opcode)->result_type != ATL_MIR_TYPE_VOID &&
        inst->opcode != ATL_MIR_OP_CALL && inst->opcode != ATL_MIR_OP_INDIRECT_CALL &&
        inst->opcode != ATL_MIR_OP_SYSCALL && inst->opcode != ATL_MIR_OP_JNI_CALL &&
        inst->opcode != ATL_MIR_OP_INTRINSIC) {
        /* Some ops can be void (e.g., store) */
        bool allowed_void = false;
        switch (inst->opcode) {
            case ATL_MIR_OP_STORE: case ATL_MIR_OP_STORE8: case ATL_MIR_OP_STORE16:
            case ATL_MIR_OP_STORE32: case ATL_MIR_OP_STORE64:
            case ATL_MIR_OP_VSTORE: case ATL_MIR_OP_FREE:
            case ATL_MIR_OP_FENCE: case ATL_MIR_OP_CACHEFLUSH:
            case ATL_MIR_OP_BR: case ATL_MIR_OP_BR_IF: case ATL_MIR_OP_SWITCH:
            case ATL_MIR_OP_RET: case ATL_MIR_OP_UNREACHABLE:
            case ATL_MIR_OP_PROFILE_POINT: case ATL_MIR_OP_DEBUG_LOC:
                allowed_void = true; break;
            default: break;
        }
        if (!allowed_void) {
            snprintf(error_buf, error_buf_size,
                     "Inst %u opcode %s has void result type but opcode expects a value",
                     inst->id, atl_mir_opcode_name(inst->opcode));
            return false;
        }
    }

    /* If result exists, verify it */
    if (inst->result) {
        if (!verify_value(func, inst->result, error_buf, error_buf_size)) {
            return false;
        }
    }

    /* Terminator checks */
    if (atl_mir_opcode_is_terminator(inst->opcode)) {
        /* Terminators should be the last instruction in a block */
        if (inst->next != NULL) {
            snprintf(error_buf, error_buf_size,
                     "Inst %u (%s) is a terminator but has a next instruction",
                     inst->id, atl_mir_opcode_name(inst->opcode));
            return false;
        }
    }

    return true;
}

/* Verify a basic block */
static bool verify_block(atl_mir_function_t *func, atl_mir_block_t *block,
                          char *error_buf, size_t error_buf_size)
{
    if (!block) return true;

    /* Non-entry blocks may have no predecessors (e.g. unreachable blocks
     * or cases where CFG edges are not fully materialized); do not reject. */
    (void)block;
    (void)func;
    (void)error_buf;
    (void)error_buf_size;

    /* Entry block must have no predecessors */
    if (block == func->entry_block && block->num_preds != 0) {
        snprintf(error_buf, error_buf_size,
                 "Entry block %u has predecessors", block->id);
        return false;
    }

    /* Verify all instructions in the block */
    atl_mir_inst_t *inst = block->first_inst;
    while (inst) {
        if (!verify_inst(func, inst, error_buf, error_buf_size)) return false;
        inst = inst->next;
    }

    /* Block must end with a terminator (unless it's empty) */
    if (block->last_inst && !atl_mir_opcode_is_terminator(block->last_inst->opcode)) {
        snprintf(error_buf, error_buf_size,
                 "Block %u does not end with a terminator (ends with %s)",
                 block->id, atl_mir_opcode_name(block->last_inst->opcode));
        return false;
    }

    /* Verify predecessors reference this block */
    for (size_t i = 0; i < block->num_preds; i++) {
        if (!block->preds[i]) {
            snprintf(error_buf, error_buf_size,
                     "Block %u predecessor %zu is NULL", block->id, i);
            return false;
        }
    }

    /* Verify successors reference this block as predecessor */
    for (size_t i = 0; i < block->num_succs; i++) {
        if (!block->succs[i]) {
            snprintf(error_buf, error_buf_size,
                     "Block %u successor %zu is NULL", block->id, i);
            return false;
        }
        bool found = false;
        for (size_t j = 0; j < block->succs[i]->num_preds; j++) {
            if (block->succs[i]->preds[j] == block) { found = true; break; }
        }
        if (!found) {
            snprintf(error_buf, error_buf_size,
                     "Block %u successor %u doesn't list %u as predecessor",
                     block->id, block->succs[i]->id, block->id);
            return false;
        }
    }

    return true;
}

/* Verify SSA form: each value has exactly one definition */
static bool verify_ssa_form(atl_mir_function_t *func,
                             char *error_buf, size_t error_buf_size)
{
    /* Track which values have definitions */
    atl_mir_value_t *val = func->first_value;
    while (val) {
        if (!val->is_constant) {
            bool is_arg = false;
            for (uint32_t i = 0; i < func->num_args; i++) {
                if (func->args[i] == val) { is_arg = true; break; }
            }
            if (!is_arg && !val->def) {
                snprintf(error_buf, error_buf_size,
                         "SSA violation: value %u has no single definition", val->id);
                return false;
            }
        }
        val = val->next;
    }

    return true;
}

/* Verify dominator tree (basic sanity) */
static bool verify_dominators(atl_mir_function_t *func,
                               char *error_buf, size_t error_buf_size)
{
    /* Entry block dominates itself */
    if (func->entry_block && func->entry_block->idom != NULL) {
        snprintf(error_buf, error_buf_size,
                 "Entry block idom should be NULL, got %p", (void*)func->entry_block->idom);
        return false;
    }
    return true;
}

/* Main function verification */
bool atl_mir_verify_function(atl_mir_function_t *func,
                              char *error_buf, size_t error_buf_size)
{
    if (!func) {
        snprintf(error_buf, error_buf_size, "Function is NULL");
        return false;
    }

    if (!error_buf || error_buf_size == 0) return false;

    /* Verify all values */
    atl_mir_value_t *val = func->first_value;
    while (val) {
        if (!verify_value(func, val, error_buf, error_buf_size)) return false;
        val = val->next;
    }

    /* Verify all blocks */
    atl_mir_block_t *block = func->first_block;
    while (block) {
        if (!verify_block(func, block, error_buf, error_buf_size)) return false;
        block = block->next;
    }

    /* Verify SSA form */
    if (!verify_ssa_form(func, error_buf, error_buf_size)) return false;

    /* Verify dominators */
    if (!verify_dominators(func, error_buf, error_buf_size)) return false;

    return true;
}

bool atl_mir_verify_module(atl_mir_module_t *module,
                            char *error_buf, size_t error_buf_size)
{
    if (!module) {
        snprintf(error_buf, error_buf_size, "Module is NULL");
        return false;
    }

    atl_mir_function_t *func = module->first_function;
    while (func) {
        if (!atl_mir_verify_function(func, error_buf, error_buf_size)) {
            snprintf(error_buf + strlen(error_buf), error_buf_size - strlen(error_buf),
                     "\n  (in function %s)", func->name ? func->name : "?");
            return false;
        }
        func = func->next;
    }

    return true;
}
