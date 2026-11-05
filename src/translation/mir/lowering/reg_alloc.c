#include "lowering.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

atl_reg_alloc_t *atl_reg_alloc_create(atl_mir_function_t *func)
{
    atl_reg_alloc_t *ra = calloc(1, sizeof(atl_reg_alloc_t));
    if (!ra) return NULL;
    
    ra->func = func;
    ra->intervals = NULL;
    ra->num_intervals = 0;
    ra->next_spill_slot = 0;
    
    /* Initialize physical register state */
    for (int i = 0; i < X86_NUM_PHYS_REGS; i++) {
        ra->phys_reg_used[i] = false;
        ra->phys_reg_interval[i] = NULL;
    }
    
    /* Reserve RSP, RBP */
    ra->phys_reg_used[X86_REG_RSP] = true;
    ra->phys_reg_used[X86_REG_RBP] = true;
    
    ra->alloc_func = malloc;
    ra->free_func = free;
    
    return ra;
}

void atl_reg_alloc_destroy(atl_reg_alloc_t *ra)
{
    if (!ra) return;
    
    atl_live_interval_t *cur = ra->intervals;
    while (cur) {
        atl_live_interval_t *next = cur->next;
        if (cur->value) {
            /* Don't free value, it's owned by MIR */
        }
        free(cur);
        cur = next;
    }
    
    free(ra);
}

static atl_live_interval_t *create_interval(atl_reg_alloc_t *ra, atl_mir_value_t *value)
{
    atl_live_interval_t *interval = calloc(1, sizeof(atl_live_interval_t));
    if (!interval) return NULL;
    
    interval->value = value;
    interval->start = 0;
    interval->end = 1000000; /* Very large initially */
    interval->reg = (x86_reg_t)-1;
    interval->spill_slot = -1;
    interval->fixed = false;
    interval->next = ra->intervals;
    ra->intervals = interval;
    ra->num_intervals++;
    
    return interval;
}

bool atl_reg_alloc_run(atl_reg_alloc_t *ra)
{
    if (!ra || !ra->func) return false;
    
    /* Build live intervals - simplified linear scan */
    /* For each value in function, create interval */
    atl_mir_value_t *val = ra->func->first_value;
    while (val) {
        if (!val->is_constant && val->def) {
            /* Create interval for non-constant defined values */
            create_interval(ra, val);
        }
        val = val->next;
    }
    
    /* Also handle function arguments */
    for (uint32_t i = 0; i < ra->func->num_args && i < ATL_MIR_MAX_ARGS; i++) {
        if (ra->func->args[i]) {
            atl_live_interval_t *iv = create_interval(ra, ra->func->args[i]);
            /* Arguments might be passed in registers (System V AMD64 ABI) */
            if (i == 0) iv->reg = X86_REG_RDI;
            else if (i == 1) iv->reg = X86_REG_RSI;
            else if (i == 2) iv->reg = X86_REG_RDX;
            else if (i == 3) iv->reg = X86_REG_RCX;
            else if (i == 4) iv->reg = X86_REG_R8;
            else if (i == 5) iv->reg = X86_REG_R9;
            iv->fixed = true;
        }
    }
    
    /* Linear scan allocation */
    /* Try to allocate caller-saved registers first for temporaries */
    int alloc_order[] = {X86_REG_RAX, X86_REG_RCX, X86_REG_RDX, X86_REG_RSI, X86_REG_RDI,
                         X86_REG_R8, X86_REG_R9, X86_REG_R10, X86_REG_R11,
                         X86_REG_RBX, X86_REG_R12, X86_REG_R13, X86_REG_R14, X86_REG_R15};
    
    for (int oi = 0; oi < 14; oi++) {
        int reg_idx = alloc_order[oi];
        if (reg_idx == X86_REG_RSP || reg_idx == X86_REG_RBP) continue;
        if (ra->phys_reg_used[reg_idx]) continue;
        
        /* Find an unassigned interval that can use this register */
        atl_live_interval_t *cur = ra->intervals;
        while (cur) {
            if (cur->reg == (x86_reg_t)-1 && !cur->fixed) {
                /* Check if this register is suitable */
                if (x86_reg_caller_saved[reg_idx]) {
                    cur->reg = (x86_reg_t)reg_idx;
                    ra->phys_reg_used[reg_idx] = true;
                    ra->phys_reg_interval[reg_idx] = cur;
                    break;
                } else {
                    /* Callee-saved, also usable */
                    cur->reg = (x86_reg_t)reg_idx;
                    ra->phys_reg_used[reg_idx] = true;
                    ra->phys_reg_interval[reg_idx] = cur;
                    break;
                }
            }
            cur = cur->next;
        }
    }
    
    /* Spill remaining unassigned intervals */
    atl_live_interval_t *cur = ra->intervals;
    while (cur) {
        if (cur->reg == (x86_reg_t)-1 && !cur->fixed) {
            cur->reg = (x86_reg_t)-1; /* spilled */
            cur->spill_slot = ra->next_spill_slot++;
        }
        cur = cur->next;
    }
    
    return true;
}

x86_reg_t atl_reg_alloc_get_reg(atl_reg_alloc_t *ra, atl_mir_value_t *value)
{
    if (!ra || !value) return (x86_reg_t)-1;
    
    atl_live_interval_t *cur = ra->intervals;
    while (cur) {
        if (cur->value == value) {
            return cur->reg;
        }
        cur = cur->next;
    }
    return (x86_reg_t)-1;
}

int atl_reg_alloc_get_spill_slot(atl_reg_alloc_t *ra, atl_mir_value_t *value)
{
    if (!ra || !value) return -1;
    
    atl_live_interval_t *cur = ra->intervals;
    while (cur) {
        if (cur->value == value) {
            return cur->spill_slot;
        }
        cur = cur->next;
    }
    return -1;
}

bool atl_reg_alloc_is_spilled(atl_reg_alloc_t *ra, atl_mir_value_t *value)
{
    if (!ra || !value) return true;
    x86_reg_t reg = atl_reg_alloc_get_reg(ra, value);
    return reg == (x86_reg_t)-1;
}
