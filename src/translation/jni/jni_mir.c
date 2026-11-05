/*
 * JNI-specific MIR patterns
 * MIR opcode ATL_MIR_OP_JNI_CALL lowering to thunk calls
 * JNIEnv function table access
 * Java object handle management
 */

#define _GNU_SOURCE
#include "jni_mir.h"
#include "jni_table.h"
#include "thunk_gen.h"
#include "../mir/lowering/lowering.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool atl_jni_mir_init(void)
{
    atl_jni_table_init();
    atl_thunk_gen_init();
    return true;
}

/* Access JNIEnv function table */
atl_mir_value_t *atl_jni_mir_get_env_func(atl_mir_builder_t *b,
                                           atl_mir_value_t *env,
                                           uint32_t func_index)
{
    if (!b || !env) return NULL;
    
    /* JNIEnv is a pointer to JNINativeInterface**
     * env points to JNINativeInterface* which has function pointers
     * Get function pointer at func_index
     */
    /* This would be lowered to: load from env + func_index * 8 */
    (void)func_index;
    
    /* Placeholder - would generate MIR for table lookup */
    return atl_mir_builder_load(b, atl_mir_type_ptr(), env, 8);
}

/* Java object handle management */
atl_mir_value_t *atl_jni_mir_get_object_handle(atl_mir_builder_t *b,
                                                atl_mir_value_t *jobject)
{
    if (!b || !jobject) return NULL;
    /* In a real implementation, this would manage local/global references */
    /* For now, just pass through */
    return jobject;
}

void atl_jni_mir_release_object_handle(atl_mir_builder_t *b,
                                       atl_mir_value_t *jobject)
{
    if (!b || !jobject) return;
    /* Would handle reference cleanup */
    (void)jobject;
}

/*
 * Lower JNI_CALL MIR instruction to appropriate code
 */
bool atl_jni_mir_lower_call(atl_lower_context_t *ctx, atl_mir_inst_t *inst)
{
    if (!ctx || !inst) return false;
    
    atl_asm_emitter_t *emit = ctx->emit;
    if (!emit) return false;
    
    /* Get method name/signature if stored in instruction */
    const char *method_name = NULL;
    const char *signature = NULL;
    
    if (inst->num_operands > 0 && inst->operands[0]) {
        /* Method name might be passed differently */
    }
    
    /* Get JNIEnv and method ID from operands */
    /* JNI calls typically: env, obj/class, methodID, args... */
    atl_mir_value_t **ops = inst->operands;
    uint32_t num_ops = inst->num_operands;
    
    if (num_ops < 3) return false;
    
    /* Try to get cached thunk */
    void *thunk = NULL;
    if (signature) {
        thunk = atl_jni_get_trampoline(signature);
    }
    if (!thunk && method_name) {
        /* Create signature from name if needed */
        thunk = atl_jni_get_trampoline(method_name);
    }
    
    /* If we have a thunk, call it */
    if (thunk) {
        /* Set up arguments according to calling convention */
        /* Call trampoline */
        /* Save callee-saved registers as per trampoline expectations */
        atl_asm_emit_push_reg(emit, X86_REG_RBP);
        atl_asm_emit_mov_reg_reg(emit, X86_REG_RBP, X86_REG_RSP, true);
        
        /* Move args to appropriate registers */
        /* This is simplified - real implementation would be more complex */
        
        /* Call thunk */
        uint64_t thunk_addr = (uint64_t)thunk;
        /* Use call with immediate if within range, else indirect */
        /* For now, load into register and call */
        atl_asm_emit_mov_reg_imm64(emit, X86_REG_RAX, thunk_addr);
        atl_asm_emit_call_reg(emit, X86_REG_RAX);
        
        atl_asm_emit_pop_reg(emit, X86_REG_RBP);
        return true;
    }
    
    /* Fallback: direct JNI call through function table */
    /* Load env (first arg) */
    /* Get function pointer from env->funcTable[func_index] */
    /* Call it */
    
    return false;
}
