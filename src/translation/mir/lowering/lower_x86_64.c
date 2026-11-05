#include "lowering.h"
#include "jni_mir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

atl_lower_context_t *atl_lower_context_create(atl_mir_function_t *func)
{
    atl_lower_context_t *ctx = calloc(1, sizeof(atl_lower_context_t));
    if (!ctx) return NULL;
    
    ctx->func = func;
    ctx->emit = atl_asm_emitter_create(64 * 1024);
    ctx->ra = atl_reg_alloc_create(func);
    
    ctx->inst_index = 0;
    ctx->block_count = func ? func->num_blocks : 0;
    ctx->block_to_label = calloc(ctx->block_count + 1, sizeof(int));
    for (size_t i = 0; i <= ctx->block_count; i++) {
        ctx->block_to_label[i] = atl_asm_new_label(ctx->emit);
    }
    
    ctx->spill_base_offset = 0x20; /* Above saved registers */
    ctx->local_base_offset = 0x10;
    ctx->frame_size = 0x100;
    
    /* Initialize ARM register mappings */
    for (int i = 0; i < 16; i++) {
        ctx->arm_reg_spill_slot[i] = -1;
    }
    
    ctx->alloc_func = malloc;
    ctx->free_func = free;
    
    /* Single address space - translated code at same VA as ARM */
    ctx->same_va_space = true;
    ctx->arm_base_va = 0;
    ctx->x86_base_va = 0;
    
    /* SMC invalidation support */
    ctx->smc_invalidation = true;
    
    /* ARMv4 first-class support */
    ctx->armv4_compat = true;
    
    /* Run register allocation */
    if (ctx->ra) {
        atl_reg_alloc_run(ctx->ra);
    }
    
    return ctx;
}

void atl_lower_context_destroy(atl_lower_context_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->emit) atl_asm_emitter_destroy(ctx->emit);
    if (ctx->ra) atl_reg_alloc_destroy(ctx->ra);
    if (ctx->block_to_label) free(ctx->block_to_label);
    free(ctx);
}

x86_reg_t atl_lower_get_reg(atl_lower_context_t *ctx, atl_mir_value_t *val)
{
    if (!ctx || !val) return (x86_reg_t)-1;
    if (ctx->ra) {
        return atl_reg_alloc_get_reg(ctx->ra, val);
    }
    return (x86_reg_t)-1;
}

void atl_lower_spill_to_mem(atl_lower_context_t *ctx, x86_reg_t reg, atl_mir_value_t *val)
{
    if (!ctx || !ctx->emit) return;
    int slot = ctx->ra ? atl_reg_alloc_get_spill_slot(ctx->ra, val) : -1;
    if (slot < 0) slot = 0;
    int32_t offset = - (ctx->spill_base_offset + slot * 8);
    atl_asm_emit_mov_mem_reg(ctx->emit, X86_REG_RBP, offset, reg, 8);
}

void atl_lower_reload_from_mem(atl_lower_context_t *ctx, x86_reg_t reg, atl_mir_value_t *val)
{
    if (!ctx || !ctx->emit) return;
    int slot = ctx->ra ? atl_reg_alloc_get_spill_slot(ctx->ra, val) : -1;
    if (slot < 0) slot = 0;
    int32_t offset = - (ctx->spill_base_offset + slot * 8);
    atl_asm_emit_mov_reg_mem(ctx->emit, reg, X86_REG_RBP, offset, 8);
}

void atl_lower_invalidate_smc(atl_lower_context_t *ctx, void *addr, size_t size)
{
    if (!ctx) return;
    if (ctx->smc_invalidation) {
        __builtin___clear_cache((char*)addr, (char*)addr + size);
        /* Additional SMC invalidation for self-modifying code */
#ifdef __x86_64__
        /* On x86, no explicit I-cache flush needed in most cases, but we call clear_cache */
#endif
    }
}

bool atl_lower_instruction(atl_lower_context_t *ctx, atl_mir_inst_t *inst)
{
    if (!ctx || !inst || !ctx->emit) return false;
    
    atl_asm_emitter_t *emit = ctx->emit;
    atl_mir_opcode_t op = inst->opcode;
    
    switch (op) {
        case ATL_MIR_OP_ADD: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (rd != r1) {
                    atl_asm_emit_mov_reg_reg(emit, rd, r1, true);
                }
                atl_asm_emit_add_reg_reg(emit, rd, r2, true);
            }
            break;
        }
        case ATL_MIR_OP_SUB: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (rd != r1) {
                    atl_asm_emit_mov_reg_reg(emit, rd, r1, true);
                }
                atl_asm_emit_sub_reg_reg(emit, rd, r2, true);
            }
            break;
        }
        case ATL_MIR_OP_MUL: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (rd != r1) {
                    atl_asm_emit_mov_reg_reg(emit, rd, r1, true);
                }
                atl_asm_emit_imul_reg_reg(emit, rd, r2, true);
            }
            break;
        }
        case ATL_MIR_OP_AND: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (rd != r1) {
                    atl_asm_emit_mov_reg_reg(emit, rd, r1, true);
                }
                atl_asm_emit_and_reg_reg(emit, rd, r2, true);
            }
            break;
        }
        case ATL_MIR_OP_OR: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (rd != r1) {
                    atl_asm_emit_mov_reg_reg(emit, rd, r1, true);
                }
                atl_asm_emit_or_reg_reg(emit, rd, r2, true);
            }
            break;
        }
        case ATL_MIR_OP_XOR: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (rd != r1) {
                    atl_asm_emit_mov_reg_reg(emit, rd, r1, true);
                }
                atl_asm_emit_xor_reg_reg(emit, rd, r2, true);
            }
            break;
        }
        case ATL_MIR_OP_SHL: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (rd != r1) {
                    atl_asm_emit_mov_reg_reg(emit, rd, r1, true);
                }
                /* Move shift count to CL if needed */
                if (r2 != X86_REG_RCX) {
                    atl_asm_emit_mov_reg_reg(emit, X86_REG_RCX, r2, true);
                }
                atl_asm_emit_shl_reg_cl(emit, rd, true);
            }
            break;
        }
        case ATL_MIR_OP_SHR: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (rd != r1) {
                    atl_asm_emit_mov_reg_reg(emit, rd, r1, true);
                }
                if (r2 != X86_REG_RCX) {
                    atl_asm_emit_mov_reg_reg(emit, X86_REG_RCX, r2, true);
                }
                atl_asm_emit_shr_reg_cl(emit, rd, true);
            }
            break;
        }
        case ATL_MIR_OP_SAR: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (rd != r1) {
                    atl_asm_emit_mov_reg_reg(emit, rd, r1, true);
                }
                if (r2 != X86_REG_RCX) {
                    atl_asm_emit_mov_reg_reg(emit, X86_REG_RCX, r2, true);
                }
                atl_asm_emit_sar_reg_cl(emit, rd, true);
            }
            break;
        }
        case ATL_MIR_OP_JNI_CALL: {
            atl_jni_mir_lower_call(ctx, inst);
            break;
        }
        case ATL_MIR_OP_SYSCALL: {
            /* Handle syscall */
            break;
        }
        case ATL_MIR_OP_NEG: {
            if (inst->num_operands >= 1 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (rd != r1) {
                    atl_asm_emit_mov_reg_reg(emit, rd, r1, true);
                }
                /* neg r64 */
                if (x86_needs_rex(rd)) atl_asm_emit_rex(emit, true, false, false, x86_needs_rex(rd));
                else atl_asm_emit_rex(emit, true, false, false, false);
                atl_asm_emit_byte(emit, 0xF7);
                atl_asm_emit_modrm(emit, 3, 3, x86_reg_low(rd));
            }
            break;
        }
        case ATL_MIR_OP_NOT: {
            if (inst->num_operands >= 1 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (rd != r1) {
                    atl_asm_emit_mov_reg_reg(emit, rd, r1, true);
                }
                /* not r64 */
                if (x86_needs_rex(rd)) atl_asm_emit_rex(emit, true, false, false, x86_needs_rex(rd));
                else atl_asm_emit_rex(emit, true, false, false, false);
                atl_asm_emit_byte(emit, 0xF7);
                atl_asm_emit_modrm(emit, 3, 2, x86_reg_low(rd));
            }
            break;
        }
        case ATL_MIR_OP_LOAD32:
        case ATL_MIR_OP_LOAD64: {
            if (inst->num_operands >= 1 && inst->result) {
                x86_reg_t addr = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (addr == (x86_reg_t)-1) addr = X86_REG_RAX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                int size = (op == ATL_MIR_OP_LOAD64) ? 8 : 4;
                atl_asm_emit_mov_reg_mem(emit, rd, addr, 0, size);
            }
            break;
        }
        case ATL_MIR_OP_LOAD8: {
            if (inst->num_operands >= 1 && inst->result) {
                x86_reg_t addr = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (addr == (x86_reg_t)-1) addr = X86_REG_RAX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                /* movzx for zero extension */
                if (x86_needs_rex(rd) || x86_needs_rex(addr)) {
                    atl_asm_emit_rex(emit, false, x86_needs_rex(rd), false, x86_needs_rex(addr));
                }
                atl_asm_emit_byte(emit, 0x0F);
                atl_asm_emit_byte(emit, 0xB6);
                atl_asm_emit_modrm(emit, 0, x86_reg_low(rd), x86_reg_low(addr));
            }
            break;
        }
        case ATL_MIR_OP_LOAD16: {
            if (inst->num_operands >= 1 && inst->result) {
                x86_reg_t addr = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (addr == (x86_reg_t)-1) addr = X86_REG_RAX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                if (x86_needs_rex(rd) || x86_needs_rex(addr)) {
                    atl_asm_emit_rex(emit, false, x86_needs_rex(rd), false, x86_needs_rex(addr));
                }
                atl_asm_emit_byte(emit, 0x0F);
                atl_asm_emit_byte(emit, 0xB7);
                atl_asm_emit_modrm(emit, 0, x86_reg_low(rd), x86_reg_low(addr));
            }
            break;
        }
        case ATL_MIR_OP_STORE8:
        case ATL_MIR_OP_STORE16:
        case ATL_MIR_OP_STORE32:
        case ATL_MIR_OP_STORE64: {
            if (inst->num_operands >= 2) {
                x86_reg_t addr = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t val = atl_lower_get_reg(ctx, inst->operands[1]);
                if (addr == (x86_reg_t)-1) addr = X86_REG_RAX;
                if (val == (x86_reg_t)-1) val = X86_REG_RCX;
                int size = 4;
                if (op == ATL_MIR_OP_STORE8) size = 1;
                else if (op == ATL_MIR_OP_STORE16) size = 2;
                else if (op == ATL_MIR_OP_STORE64) size = 8;
                atl_asm_emit_mov_mem_reg(emit, addr, 0, val, size);
            }
            break;
        }
        case ATL_MIR_OP_BR: {
            if (inst->num_succ > 0 && inst->succ[0]) {
                int lbl = ctx->block_to_label[inst->succ[0]->id];
                atl_asm_emit_jmp(emit, lbl);
            }
            break;
        }
        case ATL_MIR_OP_BR_IF: {
            if (inst->num_operands >= 1 && inst->num_succ >= 2) {
                x86_reg_t cond = atl_lower_get_reg(ctx, inst->operands[0]);
                if (cond == (x86_reg_t)-1) cond = X86_REG_RAX;
                /* test reg, reg */
                atl_asm_emit_test_reg_reg(emit, cond, cond, true);
                /* jnz true branch */
                int lbl_true = ctx->block_to_label[inst->succ[0]->id];
                int lbl_false = ctx->block_to_label[inst->succ[1]->id];
                atl_asm_emit_jcc(emit, 5, lbl_true); /* JNZ = 5 */
                atl_asm_emit_jmp(emit, lbl_false);
            }
            break;
        }
        case ATL_MIR_OP_CMP_EQ: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                atl_asm_emit_cmp_reg_reg(emit, r1, r2, true);
                /* sete al */
                if (x86_needs_rex(rd)) atl_asm_emit_rex(emit, false, false, false, x86_needs_rex(rd));
                atl_asm_emit_byte(emit, 0x0F);
                atl_asm_emit_byte(emit, 0x94);
                atl_asm_emit_modrm(emit, 3, 0, x86_reg_low(rd));
                /* movzx r64, al */
                if (x86_needs_rex(rd)) atl_asm_emit_rex(emit, true, x86_needs_rex(rd), false, x86_needs_rex(rd));
                else atl_asm_emit_rex(emit, true, x86_needs_rex(rd), false, false);
                atl_asm_emit_byte(emit, 0x0F);
                atl_asm_emit_byte(emit, 0xB6);
                atl_asm_emit_modrm(emit, 3, x86_reg_low(rd), x86_reg_low(rd));
            }
            break;
        }
        case ATL_MIR_OP_CMP_NE: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                atl_asm_emit_cmp_reg_reg(emit, r1, r2, true);
                if (x86_needs_rex(rd)) atl_asm_emit_rex(emit, false, false, false, x86_needs_rex(rd));
                atl_asm_emit_byte(emit, 0x0F);
                atl_asm_emit_byte(emit, 0x95);
                atl_asm_emit_modrm(emit, 3, 0, x86_reg_low(rd));
                if (x86_needs_rex(rd)) atl_asm_emit_rex(emit, true, x86_needs_rex(rd), false, x86_needs_rex(rd));
                else atl_asm_emit_rex(emit, true, x86_needs_rex(rd), false, false);
                atl_asm_emit_byte(emit, 0x0F);
                atl_asm_emit_byte(emit, 0xB6);
                atl_asm_emit_modrm(emit, 3, x86_reg_low(rd), x86_reg_low(rd));
            }
            break;
        }
        case ATL_MIR_OP_CMP_LT: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                atl_asm_emit_cmp_reg_reg(emit, r1, r2, true);
                if (x86_needs_rex(rd)) atl_asm_emit_rex(emit, false, false, false, x86_needs_rex(rd));
                atl_asm_emit_byte(emit, 0x0F);
                atl_asm_emit_byte(emit, 0x9C);
                atl_asm_emit_modrm(emit, 3, 0, x86_reg_low(rd));
                if (x86_needs_rex(rd)) atl_asm_emit_rex(emit, true, x86_needs_rex(rd), false, x86_needs_rex(rd));
                else atl_asm_emit_rex(emit, true, x86_needs_rex(rd), false, false);
                atl_asm_emit_byte(emit, 0x0F);
                atl_asm_emit_byte(emit, 0xB6);
                atl_asm_emit_modrm(emit, 3, x86_reg_low(rd), x86_reg_low(rd));
            }
            break;
        }
        case ATL_MIR_OP_CMP_GT: {
            if (inst->num_operands >= 2 && inst->result) {
                x86_reg_t r1 = atl_lower_get_reg(ctx, inst->operands[0]);
                x86_reg_t r2 = atl_lower_get_reg(ctx, inst->operands[1]);
                x86_reg_t rd = atl_lower_get_reg(ctx, inst->result);
                if (r1 == (x86_reg_t)-1) r1 = X86_REG_RAX;
                if (r2 == (x86_reg_t)-1) r2 = X86_REG_RCX;
                if (rd == (x86_reg_t)-1) rd = X86_REG_RAX;
                atl_asm_emit_cmp_reg_reg(emit, r1, r2, true);
                if (x86_needs_rex(rd)) atl_asm_emit_rex(emit, false, false, false, x86_needs_rex(rd));
                atl_asm_emit_byte(emit, 0x0F);
                atl_asm_emit_byte(emit, 0x9F);
                atl_asm_emit_modrm(emit, 3, 0, x86_reg_low(rd));
                if (x86_needs_rex(rd)) atl_asm_emit_rex(emit, true, x86_needs_rex(rd), false, x86_needs_rex(rd));
                else atl_asm_emit_rex(emit, true, x86_needs_rex(rd), false, false);
                atl_asm_emit_byte(emit, 0x0F);
                atl_asm_emit_byte(emit, 0xB6);
                atl_asm_emit_modrm(emit, 3, x86_reg_low(rd), x86_reg_low(rd));
            }
            break;
        }
        case ATL_MIR_OP_RET: {
            if (inst->num_operands >= 1) {
                x86_reg_t r = atl_lower_get_reg(ctx, inst->operands[0]);
                if (r != X86_REG_RAX && r != (x86_reg_t)-1) {
                    atl_asm_emit_mov_reg_reg(emit, X86_REG_RAX, r, true);
                } else if (r == (x86_reg_t)-1) {
                    /* try to get constant or just leave */
                }
            }
            /* epilogue */
            /* mov rsp, rbp; pop rbp */
            atl_asm_emit_mov_reg_reg(emit, X86_REG_RSP, X86_REG_RBP, true);
            atl_asm_emit_pop_reg(emit, X86_REG_RBP);
            atl_asm_emit_ret(emit);
            break;
        }
        default: {
            /* For unhandled opcodes, emit nop to keep structure */
            atl_asm_emit_nop(emit);
            break;
        }
    }
    
    ctx->inst_index++;
    return true;
}

void *atl_mir_lower_function(atl_mir_function_t *func, size_t *out_size)
{
    if (!func) {
        if (out_size) *out_size = 0;
        return NULL;
    }
    
    atl_lower_context_t *ctx = atl_lower_context_create(func);
    if (!ctx) {
        if (out_size) *out_size = 0;
        return NULL;
    }
    
    /* Function prologue */
    atl_asm_emit_push_reg(ctx->emit, X86_REG_RBP);
    atl_asm_emit_mov_reg_reg(ctx->emit, X86_REG_RBP, X86_REG_RSP, true);
    /* Allocate stack space */
    if (ctx->frame_size > 0) {
        atl_asm_emit_sub_reg_imm(ctx->emit, X86_REG_RSP, (int32_t)ctx->frame_size, true);
    }
    
    /* Lower each basic block */
    atl_mir_block_t *block = func->first_block;
    while (block) {
        /* Bind label for this block */
        if ((size_t)block->id < ctx->block_count) {
            atl_asm_bind_label(ctx->emit, ctx->block_to_label[block->id]);
        }
        
        /* Lower instructions in block */
        atl_mir_inst_t *inst = block->first_inst;
        while (inst) {
            atl_lower_instruction(ctx, inst);
            inst = inst->next;
        }
        
        block = block->next;
    }
    
    /* Fixup all jumps */
    atl_asm_fixup_jumps(ctx->emit);
    
    /* Invalidate instruction cache for self-modifying code if needed */
    if (ctx->smc_invalidation) {
        atl_lower_invalidate_smc(ctx, ctx->emit->code, ctx->emit->size);
    }
    
    /* Copy code to executable memory (coexisting with bionic linker allocator) */
    void *exec_mem = mmap(NULL, ctx->emit->size, PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (exec_mem == MAP_FAILED) {
        /* Fallback */
        exec_mem = malloc(ctx->emit->size);
        if (!exec_mem) {
            atl_lower_context_destroy(ctx);
            if (out_size) *out_size = 0;
            return NULL;
        }
    }
    
    memcpy(exec_mem, ctx->emit->code, ctx->emit->size);
    
    /* Ensure executable and proper permissions */
    if (mprotect(exec_mem, ctx->emit->size, PROT_READ | PROT_EXEC) != 0) {
        /* Try to keep as is */
    }
    
    /* Invalidate SMC if needed */
    if (ctx->smc_invalidation) {
        __builtin___clear_cache((char*)exec_mem, (char*)exec_mem + ctx->emit->size);
    }
    
    size_t result_size = ctx->emit->size;
    if (out_size) *out_size = result_size;
    
    atl_lower_context_destroy(ctx);
    return exec_mem;
}

bool atl_mir_lower_module(atl_mir_module_t *module)
{
    if (!module) return false;
    
    atl_mir_function_t *func = module->first_function;
    while (func) {
        size_t code_size = 0;
        void *code = atl_mir_lower_function(func, &code_size);
        if (code) {
            func->compiled_code = code;
            func->compiled_size = (uint32_t)code_size;
            func->tier = 1; /* Basic JIT */
        }
        func = func->next;
    }
    
    return true;
}
