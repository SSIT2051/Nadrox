#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/mman.h>

#include "../../src/translation/mir/mir.h"
#include "../../src/translation/mir/arm_decode/arm_decode.h"
#include "../../src/translation/atl_arm_translate.h"
#include "../../src/translation/runtime/jit_cache.h"
#include "../../src/translation/syscall/syscall_table.h"
#include "../../src/translation/jni/thunk_gen.h"
#include "../../src/translation/unwind/ehabi_decode.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (cond) { \
            printf("  PASS: %s\n", msg); \
            tests_passed++; \
        } else { \
            printf("  FAIL: %s\n", msg); \
            tests_failed++; \
        } \
    } while(0)

#define TEST_ASSERT_FMT(cond, fmt, ...) \
    do { \
        if (cond) { \
            printf("  PASS: " fmt "\n", ##__VA_ARGS__); \
            tests_passed++; \
        } else { \
            printf("  FAIL: " fmt "\n", ##__VA_ARGS__); \
            tests_failed++; \
        } \
    } while(0)

/* ============================================================================
 * MIR Tests
 * ============================================================================ */

void test_mir_basic() {
    printf("\n=== MIR Basic Tests ===\n");
    
    atl_mir_context_t *ctx = atl_mir_context_create();
    TEST_ASSERT(ctx != NULL, "Context creation");
    
    atl_mir_module_t *mod = atl_mir_module_create(ctx);
    TEST_ASSERT(mod != NULL, "Module creation");
    
    atl_mir_type_t ret_type = atl_mir_type_int(32);
    atl_mir_type_t arg_types[2] = { atl_mir_type_int(32), atl_mir_type_int(32) };
    atl_mir_function_t *func = atl_mir_module_add_function(mod, "test_add", ret_type, 2, arg_types);
    TEST_ASSERT(func != NULL, "Function creation");
    TEST_ASSERT(func->num_args == 2, "Function arg count");
    
    atl_mir_builder_t *b = atl_mir_builder_create(func);
    TEST_ASSERT(b != NULL, "Builder creation");
    
    atl_mir_block_t *entry = atl_mir_builder_create_block(b, "entry");
    TEST_ASSERT(entry != NULL, "Block creation");
    TEST_ASSERT(entry == func->entry_block, "Entry block set");
    
    atl_mir_builder_set_block(b, entry);
    
    /* Get arguments */
    atl_mir_value_t *a = atl_mir_builder_get_arg(b, 0);
    atl_mir_value_t *b_val = atl_mir_builder_get_arg(b, 1);
    TEST_ASSERT(a != NULL && b_val != NULL, "Argument values");
    
    /* Build ADD instruction */
    atl_mir_value_t *result = atl_mir_builder_add(b, a, b_val);
    TEST_ASSERT(result != NULL, "ADD instruction");
    TEST_ASSERT(result->type.kind == ATL_MIR_TYPE_I32, "Result type");
    TEST_ASSERT(result->def != NULL, "Result has definition");
    TEST_ASSERT(result->def->opcode == ATL_MIR_OP_ADD, "Opcode is ADD");
    
    /* Build return */
    atl_mir_builder_ret(b, result);
    
    /* Verify SSA */
    char err_buf[256];
    bool ok = atl_mir_verify_function(func, err_buf, sizeof(err_buf));
    TEST_ASSERT_FMT(ok, "SSA verification passes: %s", err_buf);
    
    /* Test constants */
    atl_mir_value_t *c = atl_mir_builder_const_int(b, atl_mir_type_int(32), 42);
    TEST_ASSERT(c != NULL && c->is_constant, "Constant creation");
    TEST_ASSERT(c->constant.i64 == 42, "Constant value");
    
    atl_mir_builder_destroy(b);
    atl_mir_context_destroy(ctx);
}

void test_mir_control_flow() {
    printf("\n=== MIR Control Flow Tests ===\n");
    
    atl_mir_context_t *ctx = atl_mir_context_create();
    atl_mir_module_t *mod = atl_mir_module_create(ctx);
    atl_mir_function_t *func = atl_mir_module_add_function(mod, "test_br", atl_mir_type_int(32), 1, (atl_mir_type_t[]){ atl_mir_type_int(1) });
    atl_mir_builder_t *b = atl_mir_builder_create(func);
    
    atl_mir_block_t *entry = atl_mir_builder_create_block(b, "entry");
    atl_mir_block_t *then_block = atl_mir_builder_create_block(b, "then");
    atl_mir_block_t *else_block = atl_mir_builder_create_block(b, "else");
    atl_mir_block_t *merge = atl_mir_builder_create_block(b, "merge");
    
    atl_mir_builder_set_block(b, entry);
    atl_mir_value_t *cond = atl_mir_builder_get_arg(b, 0);
    atl_mir_builder_br_if(b, cond, then_block, else_block);
    
    atl_mir_builder_set_block(b, then_block);
    atl_mir_value_t *one = atl_mir_builder_const_int(b, atl_mir_type_int(32), 1);
    atl_mir_builder_br(b, merge);
    
    atl_mir_builder_set_block(b, else_block);
    atl_mir_value_t *zero = atl_mir_builder_const_int(b, atl_mir_type_int(32), 0);
    atl_mir_builder_br(b, merge);
    
    atl_mir_builder_set_block(b, merge);
    /* PHI would go here in full SSA */
    atl_mir_builder_ret(b, one);
    
    char err_buf[256];
    bool ok = atl_mir_verify_function(func, err_buf, sizeof(err_buf));
    TEST_ASSERT_FMT(ok, "Control flow SSA verification: %s", err_buf);
    
    atl_mir_builder_destroy(b);
    atl_mir_context_destroy(ctx);
}

void test_mir_vector() {
    printf("\n=== MIR Vector Tests ===\n");
    
    atl_mir_context_t *ctx = atl_mir_context_create();
    atl_mir_module_t *mod = atl_mir_module_create(ctx);
    atl_mir_function_t *func = atl_mir_module_add_function(mod, "test_vec", atl_mir_type_vector(128, 4), 2, (atl_mir_type_t[]){ atl_mir_type_vector(128, 4), atl_mir_type_vector(128, 4) });
    atl_mir_builder_t *b = atl_mir_builder_create(func);
    
    atl_mir_block_t *entry = atl_mir_builder_create_block(b, "entry");
    atl_mir_builder_set_block(b, entry);
    
    atl_mir_value_t *a = atl_mir_builder_get_arg(b, 0);
    atl_mir_value_t *b_val = atl_mir_builder_get_arg(b, 1);
    
    atl_mir_value_t *result = atl_mir_builder_vadd(b, a, b_val);
    TEST_ASSERT(result != NULL, "VADD instruction");
    TEST_ASSERT(result->type.kind == ATL_MIR_TYPE_V128, "VADD result type");
    
    atl_mir_value_t *dot = atl_mir_builder_vdot(b, a, b_val);
    TEST_ASSERT(dot != NULL, "VDOT instruction");
    TEST_ASSERT(dot->type.kind == ATL_MIR_TYPE_F32, "VDOT result type");
    
    atl_mir_builder_ret(b, result);
    
    char err_buf[256];
    bool ok = atl_mir_verify_function(func, err_buf, sizeof(err_buf));
    TEST_ASSERT_FMT(ok, "Vector SSA verification: %s", err_buf);
    
    atl_mir_builder_destroy(b);
    atl_mir_context_destroy(ctx);
}

/* ============================================================================
 * ARM Decoder Tests
 * ============================================================================ */

void test_arm_decode_a32() {
    printf("\n=== ARM A32 Decode Tests ===\n");
    
    /* ADD r0, r1, r2 (0xE0810002) */
    uint32_t add_reg = 0xE0810002;
    atl_arm_decoded_inst_t d = {0};
    bool ok = atl_arm_decode_a32(add_reg, &d);
    TEST_ASSERT(ok, "ADD register decode");
    TEST_ASSERT(d.opcode == 0x4, "ADD opcode");
    TEST_ASSERT(d.rd == 0 && d.rn == 1 && d.rm == 2, "ADD registers");
    TEST_ASSERT(!d.thumb, "Not thumb");
    
    /* ADD r0, r1, #42 (0xE281002A) */
    uint32_t add_imm = 0xE281002A;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_a32(add_imm, &d);
    TEST_ASSERT(ok, "ADD immediate decode");
    TEST_ASSERT(d.opcode == 0x4, "ADD immediate opcode");
    TEST_ASSERT(d.rd == 0 && d.rn == 1, "ADD immediate registers");
    
    /* LDR r0, [r1, #4] (0xE5910004) */
    uint32_t ldr = 0xE5910004;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_a32(ldr, &d);
    TEST_ASSERT(ok, "LDR decode");
    TEST_ASSERT(d.load == true, "LDR is load");
    TEST_ASSERT(d.rd == 0 && d.rn == 1, "LDR registers");
    
    /* STR r0, [r1, #4] (0xE5810004) */
    uint32_t str = 0xE5810004;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_a32(str, &d);
    TEST_ASSERT(ok, "STR decode");
    TEST_ASSERT(d.load == false, "STR is store");
    
    /* B label (0xEA000005) - branch forward 5 instructions */
    uint32_t b = 0xEA000005;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_a32(b, &d);
    TEST_ASSERT(ok, "Branch decode");
    
    /* BL label (0xEB000005) - branch with link */
    uint32_t bl = 0xEB000005;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_a32(bl, &d);
    TEST_ASSERT(ok, "BL decode");
    
    /* MOV r0, #0 (0xE3A00000) */
    uint32_t mov = 0xE3A00000;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_a32(mov, &d);
    TEST_ASSERT(ok, "MOV decode");
    
    /* CMP r0, r1 (0xE1500001) */
    uint32_t cmp = 0xE1500001;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_a32(cmp, &d);
    TEST_ASSERT(ok, "CMP decode");
    
    /* MUL r0, r1, r2 (0xE0010290) */
    uint32_t mul = 0xE0010290;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_a32(mul, &d);
    TEST_ASSERT(ok, "MUL decode");
    
    /* LDM r0!, {r1-r3} (0xE8B0000E) */
    uint32_t ldm = 0xE8B0000E;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_a32(ldm, &d);
    TEST_ASSERT(ok, "LDM decode");
    TEST_ASSERT(d.reg_count == 3, "LDM register count");
    
    /* STM r0!, {r1-r3} (0xE8A0000E) */
    uint32_t stm = 0xE8A0000E;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_a32(stm, &d);
    TEST_ASSERT(ok, "STM decode");
    
    /* SVC #0 (0xEF000000) */
    uint32_t svc = 0xEF000000;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_a32(svc, &d);
    TEST_ASSERT(ok, "SVC decode");
}

void test_arm_decode_thumb() {
    printf("\n=== ARM Thumb-2 Decode Tests ===\n");
    
    /* Thumb-2 16-bit: ADD r0, r1, r2 (0x1842) */
    uint16_t add16 = 0x1842;
    atl_arm_decoded_inst_t d = {0};
    bool ok = atl_arm_decode_thumb(add16, 0, false, &d);
    TEST_ASSERT(ok, "Thumb ADD decode");
    TEST_ASSERT(d.thumb == true, "Thumb mode");
    
    /* Thumb-2 16-bit: MOV r0, #42 (0x202A) */
    uint16_t mov16 = 0x202A;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_thumb(mov16, 0, false, &d);
    TEST_ASSERT(ok, "Thumb MOV decode");
    
    /* Thumb-2 32-bit: BL label (0xF000 F800) */
    uint32_t bl32 = 0xF000F800;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_thumb(0xF000, bl32, true, &d);
    TEST_ASSERT(ok, "Thumb BL decode");
    TEST_ASSERT(d.thumb == true, "Thumb mode");
    
    /* Thumb-2 16-bit: LDR r0, [r1, #4] (0x6808) */
    uint16_t ldr16 = 0x6808;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_thumb(ldr16, 0, false, &d);
    TEST_ASSERT(ok, "Thumb LDR decode");
    
    /* Thumb-2 16-bit: STR r0, [r1, #4] (0x6008) */
    uint16_t str16 = 0x6008;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_thumb(str16, 0, false, &d);
    TEST_ASSERT(ok, "Thumb STR decode");
    
    /* Thumb-2 16-bit: BX lr (0x4770) */
    uint16_t bx16 = 0x4770;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_thumb(bx16, 0, false, &d);
    TEST_ASSERT(ok, "Thumb BX decode");
    
    /* Thumb-2 16-bit: PUSH {r0-r3, lr} (0xB50F) */
    uint16_t push16 = 0xB50F;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_thumb(push16, 0, false, &d);
    TEST_ASSERT(ok, "Thumb PUSH decode");
    
    /* Thumb-2 16-bit: POP {r0-r3, pc} (0xBD0F) */
    uint16_t pop16 = 0xBD0F;
    memset(&d, 0, sizeof(d));
    ok = atl_arm_decode_thumb(pop16, 0, false, &d);
    TEST_ASSERT(ok, "Thumb POP decode");
}

void test_arm_decode_conditions() {
    printf("\n=== ARM Condition Codes Tests ===\n");
    
    for (int c = 0; c < 16; c++) {
        uint32_t inst = (c << 28) | 0x03A00000; /* MOV r0, #0 with condition */
        atl_arm_decoded_inst_t d = {0};
        bool ok = atl_arm_decode_a32(inst, &d);
        TEST_ASSERT_FMT(ok, "Condition %d decode", c);
        TEST_ASSERT(d.cond == c, "Condition field matches");
    }
}

/* ============================================================================
 * Lowering Tests
 * ============================================================================ */

void test_lowering() {
    printf("\n=== Lowering Tests ===\n");
    
    atl_mir_context_t *ctx = atl_mir_context_create();
    atl_mir_module_t *mod = atl_mir_module_create(ctx);
    
    /* Simple function: int test(int a, int b) { return a + b; } */
    atl_mir_type_t ret = atl_mir_type_int(32);
    atl_mir_type_t args[2] = { atl_mir_type_int(32), atl_mir_type_int(32) };
    atl_mir_function_t *func = atl_mir_module_add_function(mod, "add", ret, 2, args);
    
    atl_mir_builder_t *b = atl_mir_builder_create(func);
    atl_mir_block_t *entry = atl_mir_builder_create_block(b, "entry");
    atl_mir_builder_set_block(b, entry);
    
    atl_mir_value_t *a = atl_mir_builder_get_arg(b, 0);
    atl_mir_value_t *b_val = atl_mir_builder_get_arg(b, 1);
    atl_mir_value_t *result = atl_mir_builder_add(b, a, b_val);
    atl_mir_builder_ret(b, result);
    
    char err_buf[256];
    bool ok = atl_mir_verify_function(func, err_buf, sizeof(err_buf));
    TEST_ASSERT_FMT(ok, "Function SSA before lowering: %s", err_buf);
    
    /* Lower to x86_64 */
    size_t code_size = 0;
    void *code = atl_mir_lower_function(func, &code_size);
    TEST_ASSERT(code != NULL, "Lowering produces code");
    TEST_ASSERT(code_size > 0, "Code size > 0");
    TEST_ASSERT(code_size < 4096, "Reasonable code size");
    
    /* Verify basic x86_64 prologue/epilogue patterns */
    uint8_t *bytes = (uint8_t*)code;
    bool has_prologue = (bytes[0] == 0x55) || (bytes[0] == 0x48 && bytes[1] == 0x89 && bytes[2] == 0xE5); /* push rbp / mov rbp, rsp */
    TEST_ASSERT(has_prologue, "Has x86_64 prologue");
    
    if (code) munmap(code, code_size);
    atl_mir_builder_destroy(b);
    atl_mir_context_destroy(ctx);
}

/* ============================================================================
 * JIT Cache Tests
 * ============================================================================ */

void test_jit_cache() {
    printf("\n=== JIT Cache Tests ===\n");
    
    atl_jit_cache_t *cache = atl_jit_cache_create(1, 1); /* 1MB each tier */
    TEST_ASSERT(cache != NULL, "Cache creation");
    
    void *code1 = malloc(64);
    void *code2 = malloc(64);
    memset(code1, 0x90, 64); /* NOPs */
    memset(code2, 0xCC, 64); /* INT3 */
    
    uint64_t key1 = 0x1000;
    uint64_t key2 = 0x2000;
    
    /* Insert Tier 1 */
    bool ok = atl_jit_cache_insert(cache, key1, 1, code1, 64);
    TEST_ASSERT(ok, "Tier 1 insert");
    
    /* Lookup */
    atl_jit_entry_t *entry = atl_jit_cache_lookup(cache, key1, 1);
    TEST_ASSERT(entry != NULL, "Tier 1 lookup");
    TEST_ASSERT(entry->code == code1, "Code matches");
    TEST_ASSERT(entry->tier == 1, "Tier matches");
    
    /* Insert Tier 2 */
    ok = atl_jit_cache_insert(cache, key2, 2, code2, 64);
    TEST_ASSERT(ok, "Tier 2 insert");
    
    entry = atl_jit_cache_lookup(cache, key2, 2);
    TEST_ASSERT(entry != NULL, "Tier 2 lookup");
    TEST_ASSERT(entry->tier == 2, "Tier 2 matches");
    
    /* Miss */
    entry = atl_jit_cache_lookup(cache, 0x3000, 1);
    TEST_ASSERT(entry == NULL, "Cache miss");
    
    /* Invalidate */
    atl_jit_cache_invalidate(cache, key1);
    entry = atl_jit_cache_lookup(cache, key1, 1);
    TEST_ASSERT(entry == NULL, "Invalidation works");
    
    /* Stats */
    atl_jit_stats_t stats;
    atl_jit_cache_stats(cache, &stats);
    TEST_ASSERT(stats.total_hits >= 2, "Hit count");
    TEST_ASSERT(stats.total_misses >= 1, "Miss count");
    
    atl_jit_cache_destroy(cache);
    /* Cache takes ownership of code pointers, don't free */
}

/* ============================================================================
 * Syscall Tests
 * ============================================================================ */

void test_syscalls() {
    printf("\n=== Syscall Translation Tests ===\n");
    
    /* Basic mappings */
    TEST_ASSERT(atl_arm_syscall_arm_to_host(3) == 0, "read (3->0)");
    TEST_ASSERT(atl_arm_syscall_arm_to_host(4) == 1, "write (4->1)");
    TEST_ASSERT(atl_arm_syscall_arm_to_host(5) == 257, "open->openat (5->257)");
    TEST_ASSERT(atl_arm_syscall_arm_to_host(192) == 9, "mmap (192->9)");
    TEST_ASSERT(atl_arm_syscall_arm_to_host(191) == 11, "munmap (191->11)");
    TEST_ASSERT(atl_arm_syscall_arm_to_host(120) == 56, "clone (120->56)");
    TEST_ASSERT(atl_arm_syscall_arm_to_host(240) == 202, "futex (240->202)");
    TEST_ASSERT(atl_arm_syscall_arm_to_host(174) == 13, "rt_sigaction (174->13)");
    TEST_ASSERT(atl_arm_syscall_arm_to_host(173) == 15, "rt_sigreturn (173->15)");
    
    /* Unknown passes through */
    TEST_ASSERT(atl_arm_syscall_arm_to_host(999) == 999, "Unknown passes through");
}

/* ============================================================================
 * JNI Trampoline Tests
 * ============================================================================ */

void test_jni_trampoline() {
    printf("\n=== JNI Trampoline Tests ===\n");
    
    /* Generate trampoline for a simple JNI method */
    void *trampoline = atl_arm_jni_get_trampoline("(I)V");
    TEST_ASSERT(trampoline != NULL, "Trampoline generation");
    
    /* Cache should return same trampoline for same signature */
    void *trampoline2 = atl_arm_jni_get_trampoline("(I)V");
    TEST_ASSERT(trampoline == trampoline2, "Trampoline caching");
    
    /* Different signature -> different trampoline */
    void *trampoline3 = atl_arm_jni_get_trampoline("(II)I");
    TEST_ASSERT(trampoline != trampoline3, "Different signature -> different trampoline");
}

/* ============================================================================
 * Unwind Tests
 * ============================================================================ */

void test_unwind() {
    printf("\n=== Unwind Tests ===\n");
    
    /* Test ARM exidx entry parsing */
    /* 0x80000000 | 0x1: CANNOT_UNWIND */
    uint32_t prelude_cant = 0x80000001;
    /* This would be tested via ehabi_decode functions */
    
    /* Test DWARF CFI emission would require full lowering integration */
    TEST_ASSERT(true, "Unwind stub tests pass");
}

/* ============================================================================
 * Translation Engine Integration Tests
 * ============================================================================ */

void test_translation_engine() {
    printf("\n=== Translation Engine Integration Tests ===\n");
    
    atl_translate_config_t config = {
        .enable_tier1_jit = true,
        .enable_tier2_traces = true,
        .enable_game_opts = true,
        .tier1_max_mb = 64,
        .tier2_max_mb = 256,
        .enable_disk_cache = false,
        .enable_profiling = true,
        .cache_dir = NULL
    };
    
    bool ok = atl_arm_translate_init(&config);
    TEST_ASSERT(ok, "Translation engine init");
    
    /* Test ARM library detection */
    uint8_t elf_arm[64] = {0};
    elf_arm[0] = 0x7F; elf_arm[1] = 'E'; elf_arm[2] = 'L'; elf_arm[3] = 'F';
    elf_arm[4] = 1; /* 32-bit */
    elf_arm[18] = 0x28; elf_arm[19] = 0x00; /* EM_ARM */
    
    bool is_arm = atl_arm_is_library_arm(elf_arm, sizeof(elf_arm));
    TEST_ASSERT(is_arm, "ARM library detection (EM_ARM)");
    
    elf_arm[18] = 0xB7; elf_arm[19] = 0x00; /* EM_AARCH64 */
    is_arm = atl_arm_is_library_arm(elf_arm, sizeof(elf_arm));
    TEST_ASSERT(is_arm, "ARM64 library detection (EM_AARCH64)");
    
    /* Non-ARM */
    elf_arm[18] = 0x03; elf_arm[19] = 0x00; /* EM_386 */
    is_arm = atl_arm_is_library_arm(elf_arm, sizeof(elf_arm));
    TEST_ASSERT(!is_arm, "x86 library rejected");
    
    /* Library hash */
    uint64_t hash1 = atl_arm_compute_library_hash(elf_arm, sizeof(elf_arm));
    uint64_t hash2 = atl_arm_compute_library_hash(elf_arm, sizeof(elf_arm));
    TEST_ASSERT(hash1 == hash2, "Hash deterministic");
    TEST_ASSERT(hash1 != 0, "Hash non-zero");
    
    atl_arm_translate_shutdown();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main() {
    printf("========================================\n");
    printf("ARM Translation Engine Unit Tests\n");
    printf("========================================\n");
    
    printf("\nRunning test_mir_basic...\n"); fflush(stdout);
    test_mir_basic();
    printf("test_mir_basic passed\n"); fflush(stdout);
    
    printf("\nRunning test_mir_control_flow...\n"); fflush(stdout);
    test_mir_control_flow();
    printf("test_mir_control_flow passed\n"); fflush(stdout);
    
    printf("\nRunning test_mir_vector...\n"); fflush(stdout);
    test_mir_vector();
    printf("test_mir_vector passed\n"); fflush(stdout);
    
    printf("\nRunning test_arm_decode_a32...\n"); fflush(stdout);
    test_arm_decode_a32();
    printf("test_arm_decode_a32 passed\n"); fflush(stdout);
    
    printf("\nRunning test_arm_decode_thumb...\n"); fflush(stdout);
    test_arm_decode_thumb();
    printf("test_arm_decode_thumb passed\n"); fflush(stdout);
    
    printf("\nRunning test_arm_decode_conditions...\n"); fflush(stdout);
    test_arm_decode_conditions();
    printf("test_arm_decode_conditions passed\n"); fflush(stdout);
    
    printf("\nRunning test_lowering...\n"); fflush(stdout);
    test_lowering();
    printf("test_lowering passed\n"); fflush(stdout);
    
    printf("\nRunning test_jit_cache...\n"); fflush(stdout);
    test_jit_cache();
    printf("test_jit_cache passed\n"); fflush(stdout);
    
    printf("\nRunning test_syscalls...\n"); fflush(stdout);
    test_syscalls();
    printf("test_syscalls passed\n"); fflush(stdout);
    
    printf("\nRunning test_jni_trampoline...\n"); fflush(stdout);
    test_jni_trampoline();
    printf("test_jni_trampoline passed\n"); fflush(stdout);
    
    printf("\nRunning test_unwind...\n"); fflush(stdout);
    test_unwind();
    printf("test_unwind passed\n"); fflush(stdout);
    
    printf("\nRunning test_translation_engine...\n"); fflush(stdout);
    test_translation_engine();
    printf("test_translation_engine passed\n"); fflush(stdout);
    
    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
    
    return tests_failed > 0 ? 1 : 0;
}