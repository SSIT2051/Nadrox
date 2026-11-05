#include "lowering.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

const char *x86_reg_name(x86_reg_t reg)
{
    static const char *names[] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8",  "r9",  "r10","r11","r12","r13","r14","r15"
    };
    if (reg < 0 || reg >= X86_NUM_PHYS_REGS) return "invalid";
    return names[reg];
}

atl_asm_emitter_t *atl_asm_emitter_create(size_t initial_capacity)
{
    atl_asm_emitter_t *emit = calloc(1, sizeof(atl_asm_emitter_t));
    if (!emit) return NULL;
    
    if (initial_capacity == 0) initial_capacity = 4096;
    emit->code = malloc(initial_capacity);
    if (!emit->code) {
        free(emit);
        return NULL;
    }
    
    emit->capacity = initial_capacity;
    emit->size = 0;
    emit->pos = 0;
    
    /* Initialize label table */
    emit->label_table.capacity = 64;
    emit->label_table.count = 0;
    emit->label_table.labels = calloc(emit->label_table.capacity, sizeof(char*));
    emit->label_table.positions = calloc(emit->label_table.capacity, sizeof(int32_t));
    emit->label_table.fixups = calloc(emit->label_table.capacity, sizeof(int32_t*));
    emit->label_table.fixup_counts = calloc(emit->label_table.capacity, sizeof(size_t));
    
    for (size_t i = 0; i < emit->label_table.capacity; i++) {
        emit->label_table.positions[i] = -1;
        emit->label_table.fixups[i] = NULL;
        emit->label_table.fixup_counts[i] = 0;
    }
    
    /* Initialize jumps table */
    emit->jumps.capacity = 256;
    emit->jumps.count = 0;
    emit->jumps.inst_pos = calloc(emit->jumps.capacity, sizeof(size_t));
    emit->jumps.label_id = calloc(emit->jumps.capacity, sizeof(int32_t));
    emit->jumps.rel_offset = calloc(emit->jumps.capacity, sizeof(int32_t));
    
    /* Coexist with bionic linker memory allocator */
    emit->alloc_func = malloc;
    emit->free_func = free;
    
    /* SMC support */
    emit->smc_enabled = true;
    emit->invalidate_icache = NULL;
    
    return emit;
}

void atl_asm_emitter_destroy(atl_asm_emitter_t *emit)
{
    if (!emit) return;
    
    if (emit->code) free(emit->code);
    
    if (emit->label_table.labels) {
        for (size_t i = 0; i < emit->label_table.count; i++) {
            if (emit->label_table.labels[i]) free(emit->label_table.labels[i]);
        }
        free(emit->label_table.labels);
    }
    if (emit->label_table.positions) free(emit->label_table.positions);
    if (emit->label_table.fixups) {
        for (size_t i = 0; i < emit->label_table.count; i++) {
            if (emit->label_table.fixups[i]) free(emit->label_table.fixups[i]);
        }
        free(emit->label_table.fixups);
    }
    if (emit->label_table.fixup_counts) free(emit->label_table.fixup_counts);
    
    if (emit->jumps.inst_pos) free(emit->jumps.inst_pos);
    if (emit->jumps.label_id) free(emit->jumps.label_id);
    if (emit->jumps.rel_offset) free(emit->jumps.rel_offset);
    
    free(emit);
}

void atl_asm_emitter_reset(atl_asm_emitter_t *emit)
{
    if (!emit) return;
    emit->size = 0;
    emit->pos = 0;
    
    /* Reset label fixups */
    for (size_t i = 0; i < emit->label_table.count; i++) {
        emit->label_table.positions[i] = -1;
        if (emit->label_table.fixups[i]) {
            free(emit->label_table.fixups[i]);
            emit->label_table.fixups[i] = NULL;
        }
        emit->label_table.fixup_counts[i] = 0;
    }
    emit->jumps.count = 0;
}

int atl_asm_new_label(atl_asm_emitter_t *emit)
{
    if (!emit) return -1;
    
    if (emit->label_table.count >= emit->label_table.capacity) {
        size_t new_cap = emit->label_table.capacity * 2;
        emit->label_table.labels = realloc(emit->label_table.labels, new_cap * sizeof(char*));
        emit->label_table.positions = realloc(emit->label_table.positions, new_cap * sizeof(int32_t));
        emit->label_table.fixups = realloc(emit->label_table.fixups, new_cap * sizeof(int32_t*));
        emit->label_table.fixup_counts = realloc(emit->label_table.fixup_counts, new_cap * sizeof(size_t));
        
        for (size_t i = emit->label_table.capacity; i < new_cap; i++) {
            emit->label_table.labels[i] = NULL;
            emit->label_table.positions[i] = -1;
            emit->label_table.fixups[i] = NULL;
            emit->label_table.fixup_counts[i] = 0;
        }
        emit->label_table.capacity = new_cap;
    }
    
    int id = (int)emit->label_table.count++;
    emit->label_table.positions[id] = -1;
    emit->label_table.fixups[id] = NULL;
    emit->label_table.fixup_counts[id] = 0;
    return id;
}

void atl_asm_bind_label(atl_asm_emitter_t *emit, int label_id)
{
    if (!emit || label_id < 0 || (size_t)label_id >= emit->label_table.count) return;
    emit->label_table.positions[label_id] = (int32_t)emit->size;
}

void atl_asm_fixup_jumps(atl_asm_emitter_t *emit)
{
    if (!emit) return;
    
    for (size_t j = 0; j < emit->jumps.count; j++) {
        int32_t label_id = emit->jumps.label_id[j];
        if (label_id < 0 || (size_t)label_id >= emit->label_table.count) continue;
        if (emit->label_table.positions[label_id] == -1) continue;
        
        size_t inst_pos = emit->jumps.inst_pos[j];
        int32_t target = emit->label_table.positions[label_id];
        /* For short/near jumps, calculate relative offset */
        /* Assuming near jump (rel32) - offset is target - (inst_pos + 5) for 5-byte jmp */
        int32_t rel = target - (int32_t)(inst_pos + 5);
        
        /* Patch the 4-byte relative offset (last 4 bytes of 5-byte instruction) */
        if (inst_pos + 5 <= emit->size) {
            memcpy(emit->code + inst_pos + 1, &rel, 4);
        }
    }
}

void atl_asm_emit_byte(atl_asm_emitter_t *emit, uint8_t b)
{
    if (!emit) return;
    if (emit->size + 1 > emit->capacity) {
        size_t new_cap = emit->capacity * 2;
        emit->code = realloc(emit->code, new_cap);
        emit->capacity = new_cap;
    }
    emit->code[emit->size++] = b;
}

void atl_asm_emit_dword(atl_asm_emitter_t *emit, uint32_t d)
{
    if (!emit) return;
    for (int i = 0; i < 4; i++) {
        atl_asm_emit_byte(emit, (d >> (i * 8)) & 0xFF);
    }
}

void atl_asm_emit_qword(atl_asm_emitter_t *emit, uint64_t q)
{
    if (!emit) return;
    for (int i = 0; i < 8; i++) {
        atl_asm_emit_byte(emit, (q >> (i * 8)) & 0xFF);
    }
}

void atl_asm_emit_rex(atl_asm_emitter_t *emit, bool w, bool r, bool x, bool b)
{
    uint8_t rex = 0x40;
    if (w) rex |= 0x48;
    if (r) rex |= 0x44;
    if (x) rex |= 0x42;
    if (b) rex |= 0x41;
    if (w || r || x || b) {
        atl_asm_emit_byte(emit, rex);
    }
}

void atl_asm_emit_modrm(atl_asm_emitter_t *emit, uint8_t mod, uint8_t reg, uint8_t rm)
{
    atl_asm_emit_byte(emit, ((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7));
}

void atl_asm_emit_sib(atl_asm_emitter_t *emit, uint8_t scale, uint8_t index, uint8_t base)
{
    atl_asm_emit_byte(emit, ((scale & 3) << 6) | ((index & 7) << 3) | (base & 7));
}

/* Helper to encode register with REX if needed */
static bool x86_x86_needs_rex(x86_reg_t reg)
{
    return reg >= X86_REG_R8;
}

static uint8_t x86_x86_reg_low(x86_reg_t reg)
{
    if (reg >= X86_REG_R8) return reg - X86_REG_R8;
    return reg & 7;
}

void atl_asm_emit_mov_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64)
{
    if (!emit) return;
    bool d64 = dst >= X86_REG_R8 || (is64 && dst >= 4);
    bool s64 = src >= X86_REG_R8 || is64;
    /* Simple case */
    if (is64 || dst >= X86_REG_R8 || src >= X86_REG_R8) {
        atl_asm_emit_rex(emit, is64, x86_x86_needs_rex(src), false, x86_x86_needs_rex(dst));
    }
    atl_asm_emit_byte(emit, 0x89);
    atl_asm_emit_modrm(emit, 3, x86_x86_reg_low(src), x86_x86_reg_low(dst));
}

void atl_asm_emit_mov_reg_imm64(atl_asm_emitter_t *emit, x86_reg_t dst, uint64_t imm)
{
    if (!emit) return;
    if (x86_x86_needs_rex(dst)) {
        atl_asm_emit_rex(emit, true, false, false, true);
    } else {
        atl_asm_emit_rex(emit, true, false, false, false);
    }
    /* mov r64, imm64 */
    atl_asm_emit_byte(emit, 0xB8 + x86_x86_reg_low(dst));
    atl_asm_emit_qword(emit, imm);
}

void atl_asm_emit_mov_reg_mem(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t base, int32_t disp, int size)
{
    if (!emit) return;
    bool is64 = (size == 8);
    if (is64 || x86_x86_needs_rex(dst) || x86_x86_needs_rex(base)) {
        atl_asm_emit_rex(emit, is64, x86_x86_needs_rex(dst), false, x86_x86_needs_rex(base));
    }
    if (size == 1) {
        atl_asm_emit_byte(emit, 0x8A);
    } else if (size == 2) {
        atl_asm_emit_byte(emit, 0x66);
        atl_asm_emit_byte(emit, 0x8B);
    } else {
        atl_asm_emit_byte(emit, 0x8B);
    }
    if (disp == 0 && base != X86_REG_RBP && base != X86_REG_R13) {
        atl_asm_emit_modrm(emit, 0, x86_x86_reg_low(dst), x86_x86_reg_low(base));
    } else if (disp >= -128 && disp <= 127) {
        atl_asm_emit_modrm(emit, 1, x86_x86_reg_low(dst), x86_x86_reg_low(base));
        atl_asm_emit_byte(emit, disp & 0xFF);
    } else {
        atl_asm_emit_modrm(emit, 2, x86_x86_reg_low(dst), x86_x86_reg_low(base));
        atl_asm_emit_dword(emit, (uint32_t)disp);
    }
}

void atl_asm_emit_mov_mem_reg(atl_asm_emitter_t *emit, x86_reg_t base, int32_t disp, x86_reg_t src, int size)
{
    if (!emit) return;
    bool is64 = (size == 8);
    if (is64 || x86_x86_needs_rex(src) || x86_x86_needs_rex(base)) {
        atl_asm_emit_rex(emit, is64, false, false, x86_x86_needs_rex(base));
        if (x86_x86_needs_rex(src)) atl_asm_emit_rex(emit, is64, true, false, x86_x86_needs_rex(base)); /* simplified */
    }
    if (size == 1) {
        atl_asm_emit_byte(emit, 0x88);
    } else if (size == 2) {
        atl_asm_emit_byte(emit, 0x66);
        atl_asm_emit_byte(emit, 0x89);
    } else {
        atl_asm_emit_byte(emit, 0x89);
    }
    if (disp == 0 && base != X86_REG_RBP && base != X86_REG_R13) {
        atl_asm_emit_modrm(emit, 0, x86_x86_reg_low(src), x86_x86_reg_low(base));
    } else if (disp >= -128 && disp <= 127) {
        atl_asm_emit_modrm(emit, 1, x86_x86_reg_low(src), x86_x86_reg_low(base));
        atl_asm_emit_byte(emit, disp & 0xFF);
    } else {
        atl_asm_emit_modrm(emit, 2, x86_x86_reg_low(src), x86_x86_reg_low(base));
        atl_asm_emit_dword(emit, (uint32_t)disp);
    }
}

void atl_asm_emit_add_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst) || x86_x86_needs_rex(src)) {
        atl_asm_emit_rex(emit, is64, x86_x86_needs_rex(src), false, x86_x86_needs_rex(dst));
    }
    atl_asm_emit_byte(emit, 0x01);
    atl_asm_emit_modrm(emit, 3, x86_x86_reg_low(src), x86_x86_reg_low(dst));
}

void atl_asm_emit_sub_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst) || x86_x86_needs_rex(src)) {
        atl_asm_emit_rex(emit, is64, x86_x86_needs_rex(src), false, x86_x86_needs_rex(dst));
    }
    atl_asm_emit_byte(emit, 0x29);
    atl_asm_emit_modrm(emit, 3, x86_x86_reg_low(src), x86_x86_reg_low(dst));
}

void atl_asm_emit_sub_reg_imm(atl_asm_emitter_t *emit, x86_reg_t dst, int32_t imm, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst)) {
        atl_asm_emit_rex(emit, is64, false, false, x86_x86_needs_rex(dst));
    }
    if (imm >= -128 && imm <= 127) {
        atl_asm_emit_byte(emit, 0x83);
        atl_asm_emit_modrm(emit, 3, 5, x86_x86_reg_low(dst)); /* /5 for SUB */
        atl_asm_emit_byte(emit, imm & 0xFF);
    } else {
        atl_asm_emit_byte(emit, 0x81);
        atl_asm_emit_modrm(emit, 3, 5, x86_x86_reg_low(dst));
        atl_asm_emit_dword(emit, (uint32_t)imm);
    }
}

void atl_asm_emit_and_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst) || x86_x86_needs_rex(src)) {
        atl_asm_emit_rex(emit, is64, x86_x86_needs_rex(src), false, x86_x86_needs_rex(dst));
    }
    atl_asm_emit_byte(emit, 0x21);
    atl_asm_emit_modrm(emit, 3, x86_x86_reg_low(src), x86_x86_reg_low(dst));
}

void atl_asm_emit_or_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst) || x86_x86_needs_rex(src)) {
        atl_asm_emit_rex(emit, is64, x86_x86_needs_rex(src), false, x86_x86_needs_rex(dst));
    }
    atl_asm_emit_byte(emit, 0x09);
    atl_asm_emit_modrm(emit, 3, x86_x86_reg_low(src), x86_x86_reg_low(dst));
}

void atl_asm_emit_xor_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst) || x86_x86_needs_rex(src)) {
        atl_asm_emit_rex(emit, is64, x86_x86_needs_rex(src), false, x86_x86_needs_rex(dst));
    }
    atl_asm_emit_byte(emit, 0x31);
    atl_asm_emit_modrm(emit, 3, x86_x86_reg_low(src), x86_x86_reg_low(dst));
}

void atl_asm_emit_shl_reg_cl(atl_asm_emitter_t *emit, x86_reg_t dst, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst)) {
        atl_asm_emit_rex(emit, is64, false, false, x86_x86_needs_rex(dst));
    }
    atl_asm_emit_byte(emit, 0xD3);
    atl_asm_emit_modrm(emit, 3, 4, x86_x86_reg_low(dst));
}

void atl_asm_emit_shr_reg_cl(atl_asm_emitter_t *emit, x86_reg_t dst, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst)) {
        atl_asm_emit_rex(emit, is64, false, false, x86_x86_needs_rex(dst));
    }
    atl_asm_emit_byte(emit, 0xD3);
    atl_asm_emit_modrm(emit, 3, 5, x86_x86_reg_low(dst));
}

void atl_asm_emit_sar_reg_cl(atl_asm_emitter_t *emit, x86_reg_t dst, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst)) {
        atl_asm_emit_rex(emit, is64, false, false, x86_x86_needs_rex(dst));
    }
    atl_asm_emit_byte(emit, 0xD3);
    atl_asm_emit_modrm(emit, 3, 7, x86_x86_reg_low(dst));
}

void atl_asm_emit_imul_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst) || x86_x86_needs_rex(src)) {
        atl_asm_emit_rex(emit, is64, x86_x86_needs_rex(dst), false, x86_x86_needs_rex(src));
    }
    atl_asm_emit_byte(emit, 0x0F);
    atl_asm_emit_byte(emit, 0xAF);
    atl_asm_emit_modrm(emit, 3, x86_x86_reg_low(dst), x86_x86_reg_low(src));
}

void atl_asm_emit_div_reg(atl_asm_emitter_t *emit, x86_reg_t src, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(src)) {
        atl_asm_emit_rex(emit, is64, false, false, x86_x86_needs_rex(src));
    }
    atl_asm_emit_byte(emit, 0xF7);
    atl_asm_emit_modrm(emit, 3, 6, x86_x86_reg_low(src));
}

void atl_asm_emit_idiv_reg(atl_asm_emitter_t *emit, x86_reg_t src, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(src)) {
        atl_asm_emit_rex(emit, is64, false, false, x86_x86_needs_rex(src));
    }
    atl_asm_emit_byte(emit, 0xF7);
    atl_asm_emit_modrm(emit, 3, 7, x86_x86_reg_low(src));
}

void atl_asm_emit_cmp_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst) || x86_x86_needs_rex(src)) {
        atl_asm_emit_rex(emit, is64, x86_x86_needs_rex(src), false, x86_x86_needs_rex(dst));
    }
    atl_asm_emit_byte(emit, 0x39);
    atl_asm_emit_modrm(emit, 3, x86_x86_reg_low(src), x86_x86_reg_low(dst));
}

void atl_asm_emit_test_reg_reg(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t src, bool is64)
{
    if (!emit) return;
    if (is64 || x86_x86_needs_rex(dst) || x86_x86_needs_rex(src)) {
        atl_asm_emit_rex(emit, is64, x86_x86_needs_rex(src), false, x86_x86_needs_rex(dst));
    }
    atl_asm_emit_byte(emit, 0x85);
    atl_asm_emit_modrm(emit, 3, x86_x86_reg_low(src), x86_x86_reg_low(dst));
}

void atl_asm_emit_jmp(atl_asm_emitter_t *emit, int label_id)
{
    if (!emit) return;
    size_t inst_pos = emit->size;
    atl_asm_emit_byte(emit, 0xE9); /* JMP rel32 */
    atl_asm_emit_dword(emit, 0);   /* placeholder */
    
    if (emit->jumps.count >= emit->jumps.capacity) {
        size_t new_cap = emit->jumps.capacity * 2;
        emit->jumps.inst_pos = realloc(emit->jumps.inst_pos, new_cap * sizeof(size_t));
        emit->jumps.label_id = realloc(emit->jumps.label_id, new_cap * sizeof(int32_t));
        emit->jumps.rel_offset = realloc(emit->jumps.rel_offset, new_cap * sizeof(int32_t));
        emit->jumps.capacity = new_cap;
    }
    emit->jumps.inst_pos[emit->jumps.count] = inst_pos;
    emit->jumps.label_id[emit->jumps.count] = label_id;
    emit->jumps.rel_offset[emit->jumps.count] = 0;
    emit->jumps.count++;
}

void atl_asm_emit_jcc(atl_asm_emitter_t *emit, int cc, int label_id)
{
    if (!emit) return;
    size_t inst_pos = emit->size;
    /* Use 0F 8x for near conditional jump */
    atl_asm_emit_byte(emit, 0x0F);
    atl_asm_emit_byte(emit, 0x80 + (cc & 0xF));
    atl_asm_emit_dword(emit, 0);
    
    if (emit->jumps.count >= emit->jumps.capacity) {
        size_t new_cap = emit->jumps.capacity * 2;
        emit->jumps.inst_pos = realloc(emit->jumps.inst_pos, new_cap * sizeof(size_t));
        emit->jumps.label_id = realloc(emit->jumps.label_id, new_cap * sizeof(int32_t));
        emit->jumps.rel_offset = realloc(emit->jumps.rel_offset, new_cap * sizeof(int32_t));
        emit->jumps.capacity = new_cap;
    }
    emit->jumps.inst_pos[emit->jumps.count] = inst_pos;
    emit->jumps.label_id[emit->jumps.count] = label_id;
    emit->jumps.rel_offset[emit->jumps.count] = 0;
    emit->jumps.count++;
}

void atl_asm_emit_call_reg(atl_asm_emitter_t *emit, x86_reg_t reg)
{
    if (!emit) return;
    if (x86_x86_needs_rex(reg)) {
        atl_asm_emit_rex(emit, false, false, false, true);
    }
    atl_asm_emit_byte(emit, 0xFF);
    atl_asm_emit_modrm(emit, 3, 2, x86_x86_reg_low(reg));
}

void atl_asm_emit_call_imm(atl_asm_emitter_t *emit, int32_t rel32)
{
    if (!emit) return;
    atl_asm_emit_byte(emit, 0xE8);
    atl_asm_emit_dword(emit, (uint32_t)rel32);
}

void atl_asm_emit_ret(atl_asm_emitter_t *emit)
{
    if (!emit) return;
    atl_asm_emit_byte(emit, 0xC3);
}

void atl_asm_emit_push_reg(atl_asm_emitter_t *emit, x86_reg_t reg)
{
    if (!emit) return;
    if (x86_x86_needs_rex(reg)) {
        atl_asm_emit_rex(emit, false, false, false, true);
    }
    atl_asm_emit_byte(emit, 0x50 + x86_x86_reg_low(reg));
}

void atl_asm_emit_pop_reg(atl_asm_emitter_t *emit, x86_reg_t reg)
{
    if (!emit) return;
    if (x86_x86_needs_rex(reg)) {
        atl_asm_emit_rex(emit, false, false, false, true);
    }
    atl_asm_emit_byte(emit, 0x58 + x86_x86_reg_low(reg));
}

void atl_asm_emit_lea(atl_asm_emitter_t *emit, x86_reg_t dst, x86_reg_t base, int32_t disp)
{
    if (!emit) return;
    if (x86_x86_needs_rex(dst) || x86_x86_needs_rex(base)) {
        atl_asm_emit_rex(emit, true, x86_x86_needs_rex(dst), false, x86_x86_needs_rex(base));
    }
    atl_asm_emit_byte(emit, 0x8D);
    if (disp == 0 && base != X86_REG_RBP && base != X86_REG_R13) {
        atl_asm_emit_modrm(emit, 0, x86_x86_reg_low(dst), x86_x86_reg_low(base));
    } else if (disp >= -128 && disp <= 127) {
        atl_asm_emit_modrm(emit, 1, x86_x86_reg_low(dst), x86_x86_reg_low(base));
        atl_asm_emit_byte(emit, disp & 0xFF);
    } else {
        atl_asm_emit_modrm(emit, 2, x86_x86_reg_low(dst), x86_x86_reg_low(base));
        atl_asm_emit_dword(emit, (uint32_t)disp);
    }
}

void atl_asm_emit_nop(atl_asm_emitter_t *emit)
{
    if (!emit) return;
    atl_asm_emit_byte(emit, 0x90);
}

void atl_asm_invalidate_code(atl_asm_emitter_t *emit, void *addr, size_t size)
{
    if (!emit) return;
    if (emit->smc_enabled) {
        /* Flush instruction cache for self-modifying code */
        __builtin___clear_cache((char*)addr, (char*)addr + size);
        if (emit->invalidate_icache) {
            emit->invalidate_icache(addr, size);
        }
        /* Also use mprotect trick if needed */
    }
}
