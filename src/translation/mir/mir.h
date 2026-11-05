#ifndef ATL_MIR_H
#define ATL_MIR_H

#include "mir_types.h"
#include "mir_opcodes.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * MIR Public API
 * ============================================================================
 * Main entry point for the MIR (Medium IR) system.
 * Provides module creation, function building, passes, and lowering.
 * ============================================================================ */

/* ---------------------------------------------------------------------------
 * Context Creation/Destruction
 * --------------------------------------------------------------------------- */

atl_mir_context_t *atl_mir_context_create(void);
void atl_mir_context_destroy(atl_mir_context_t *ctx);

/* Context options */
void atl_mir_context_set_verify_ssa(atl_mir_context_t *ctx, bool enable);
void atl_mir_context_set_debug_print(atl_mir_context_t *ctx, bool enable);
void atl_mir_context_set_opt_level(atl_mir_context_t *ctx, uint32_t level);
void atl_mir_context_set_game_opts(atl_mir_context_t *ctx, bool enable);

/* Error handling */
int atl_mir_context_last_error(atl_mir_context_t *ctx);
const char *atl_mir_context_error_message(atl_mir_context_t *ctx);

/* ---------------------------------------------------------------------------
 * Module Management
 * --------------------------------------------------------------------------- */

atl_mir_module_t *atl_mir_module_create(atl_mir_context_t *ctx);
void atl_mir_module_destroy(atl_mir_module_t *module);

/* Target configuration */
void atl_mir_module_set_target(atl_mir_module_t *module,
                                bool is_armv4, bool is_armv7,
                                bool has_neon, bool has_vfp, bool is_thumb2);

/* Function creation */
atl_mir_function_t *atl_mir_module_add_function(atl_mir_module_t *module,
                                                  const char *name,
                                                  atl_mir_type_t return_type,
                                                  uint32_t num_args,
                                                  const atl_mir_type_t *arg_types);

/* Global data */
void *atl_mir_module_add_global(atl_mir_module_t *module, size_t size, size_t alignment);
const char *atl_mir_module_intern_string(atl_mir_module_t *module, const char *str);

/* ---------------------------------------------------------------------------
 * Function Builder API
 * --------------------------------------------------------------------------- */

/* Create a builder for a function */
atl_mir_builder_t *atl_mir_builder_create(atl_mir_function_t *func);
void atl_mir_builder_destroy(atl_mir_builder_t *b);

/* Block management */
atl_mir_block_t *atl_mir_builder_create_block(atl_mir_builder_t *b, const char *name);
void atl_mir_builder_set_block(atl_mir_builder_t *b, atl_mir_block_t *block);
atl_mir_block_t *atl_mir_builder_current_block(atl_mir_builder_t *b);

/* Seal block (mark as complete, no more predecessors) */
void atl_mir_builder_seal_block(atl_mir_builder_t *b, atl_mir_block_t *block);

/* Branch targets */
void atl_mir_builder_br(atl_mir_builder_t *b, atl_mir_block_t *target);
void atl_mir_builder_br_if(atl_mir_builder_t *b, atl_mir_value_t *cond,
                            atl_mir_block_t *then_block, atl_mir_block_t *else_block);
void atl_mir_builder_switch(atl_mir_builder_t *b, atl_mir_value_t *value,
                             atl_mir_block_t *default_block,
                             uint32_t num_cases, const uint64_t *case_values,
                             atl_mir_block_t *const *case_blocks);

/* Return */
void atl_mir_builder_ret(atl_mir_builder_t *b, atl_mir_value_t *value);
void atl_mir_builder_ret_void(atl_mir_builder_t *b);

/* Unreachable */
void atl_mir_builder_unreachable(atl_mir_builder_t *b);

/* ---------------------------------------------------------------------------
 * Value Access
 * --------------------------------------------------------------------------- */

/* Function arguments */
atl_mir_value_t *atl_mir_builder_get_arg(atl_mir_builder_t *b, uint32_t index);

/* Constants */
atl_mir_value_t *atl_mir_builder_const_int(atl_mir_builder_t *b, atl_mir_type_t type, int64_t value);
atl_mir_value_t *atl_mir_builder_const_float(atl_mir_builder_t *b, atl_mir_type_t type, double value);
atl_mir_value_t *atl_mir_builder_const_ptr(atl_mir_builder_t *b, void *ptr);
atl_mir_value_t *atl_mir_builder_const_vector(atl_mir_builder_t *b, atl_mir_type_t type, const void *data);

/* Local variables (stack slots) */
atl_mir_value_t *atl_mir_builder_create_local(atl_mir_builder_t *b, atl_mir_type_t type, size_t size);
atl_mir_value_t *atl_mir_builder_load_local(atl_mir_builder_t *b, atl_mir_value_t *local);
void atl_mir_builder_store_local(atl_mir_builder_t *b, atl_mir_value_t *local, atl_mir_value_t *value);

/* ---------------------------------------------------------------------------
 * Arithmetic Instructions
 * --------------------------------------------------------------------------- */

atl_mir_value_t *atl_mir_builder_add(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_sub(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_mul(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_div(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_mod(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_neg(atl_mir_builder_t *b, atl_mir_value_t *a);
atl_mir_value_t *atl_mir_builder_abs(atl_mir_builder_t *b, atl_mir_value_t *a);
atl_mir_value_t *atl_mir_builder_min(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_max(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_fma(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val, atl_mir_value_t *c);

/* ---------------------------------------------------------------------------
 * Bitwise Instructions
 * --------------------------------------------------------------------------- */

atl_mir_value_t *atl_mir_builder_and(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_or(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_xor(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_not(atl_mir_builder_t *b, atl_mir_value_t *a);
atl_mir_value_t *atl_mir_builder_shl(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_shr(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_sar(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_rol(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_ror(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);

/* ---------------------------------------------------------------------------
 * Compare Instructions
 * --------------------------------------------------------------------------- */

atl_mir_value_t *atl_mir_builder_cmp_eq(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_cmp_ne(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_cmp_lt(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_cmp_le(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_cmp_gt(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_cmp_ge(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_cmp_ult(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_cmp_ule(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_cmp_ugt(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_cmp_uge(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);

/* ---------------------------------------------------------------------------
 * Memory Instructions
 * --------------------------------------------------------------------------- */

atl_mir_value_t *atl_mir_builder_load(atl_mir_builder_t *b, atl_mir_type_t type, atl_mir_value_t *addr, uint32_t align);
atl_mir_value_t *atl_mir_builder_load8(atl_mir_builder_t *b, atl_mir_value_t *addr);
atl_mir_value_t *atl_mir_builder_load16(atl_mir_builder_t *b, atl_mir_value_t *addr);
atl_mir_value_t *atl_mir_builder_load32(atl_mir_builder_t *b, atl_mir_value_t *addr);
atl_mir_value_t *atl_mir_builder_load64(atl_mir_builder_t *b, atl_mir_value_t *addr);

void atl_mir_builder_store(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value, uint32_t align);
void atl_mir_builder_store8(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value);
void atl_mir_builder_store16(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value);
void atl_mir_builder_store32(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value);
void atl_mir_builder_store64(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value);

void atl_mir_builder_prefetch(atl_mir_builder_t *b, atl_mir_value_t *addr, int rw, int locality);

/* ---------------------------------------------------------------------------
 * Vector/SIMD Instructions
 * --------------------------------------------------------------------------- */

atl_mir_value_t *atl_mir_builder_vadd(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_vsub(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_vmul(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_vdot(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_vcross(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_vnormalize(atl_mir_builder_t *b, atl_mir_value_t *a);
atl_mir_value_t *atl_mir_builder_vswizzle(atl_mir_builder_t *b, atl_mir_value_t *a, uint32_t mask);
atl_mir_value_t *atl_mir_builder_vload(atl_mir_builder_t *b, atl_mir_type_t type, atl_mir_value_t *addr, uint32_t align);
void atl_mir_builder_vstore(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *value, uint32_t align);
atl_mir_value_t *atl_mir_builder_vsplat(atl_mir_builder_t *b, atl_mir_type_t type, atl_mir_value_t *scalar);

/* ---------------------------------------------------------------------------
 * Conversion Instructions
 * --------------------------------------------------------------------------- */

atl_mir_value_t *atl_mir_builder_i2f(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from);
atl_mir_value_t *atl_mir_builder_f2i(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from);
atl_mir_value_t *atl_mir_builder_zext(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from);
atl_mir_value_t *atl_mir_builder_sext(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from);
atl_mir_value_t *atl_mir_builder_trunc(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from);
atl_mir_value_t *atl_mir_builder_bitcast(atl_mir_builder_t *b, atl_mir_type_t to_type, atl_mir_value_t *from);

/* ---------------------------------------------------------------------------
 * Stack/Frame Instructions
 * --------------------------------------------------------------------------- */

atl_mir_value_t *atl_mir_builder_alloc(atl_mir_builder_t *b, size_t size, uint32_t align);
void atl_mir_builder_free(atl_mir_builder_t *b, atl_mir_value_t *ptr);
atl_mir_value_t *atl_mir_builder_frame_addr(atl_mir_builder_t *b);
atl_mir_value_t *atl_mir_builder_stack_ptr(atl_mir_builder_t *b);

/* ---------------------------------------------------------------------------
 * Atomic Instructions
 * --------------------------------------------------------------------------- */

atl_mir_value_t *atl_mir_builder_cas(atl_mir_builder_t *b, atl_mir_type_t type,
                                      atl_mir_value_t *addr, atl_mir_value_t *expected, atl_mir_value_t *desired);
atl_mir_value_t *atl_mir_builder_xchg(atl_mir_builder_t *b, atl_mir_type_t type,
                                       atl_mir_value_t *addr, atl_mir_value_t *value);
atl_mir_value_t *atl_mir_builder_fetch_add(atl_mir_builder_t *b, atl_mir_type_t type,
                                            atl_mir_value_t *addr, atl_mir_value_t *value);
void atl_mir_builder_fence(atl_mir_builder_t *b);
atl_mir_value_t *atl_mir_builder_atomic_load(atl_mir_builder_t *b, atl_mir_type_t type, atl_mir_value_t *addr);

/* ---------------------------------------------------------------------------
 * Game-Specific Instructions
 * --------------------------------------------------------------------------- */

atl_mir_value_t *atl_mir_builder_mat4_mul(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val);
atl_mir_value_t *atl_mir_builder_mat4_vec4(atl_mir_builder_t *b, atl_mir_value_t *mat, atl_mir_value_t *vec);
atl_mir_value_t *atl_mir_builder_quat_slerp(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val, atl_mir_value_t *t);
atl_mir_value_t *atl_mir_builder_tex_sample(atl_mir_builder_t *b, atl_mir_value_t *sampler, atl_mir_value_t *coord);
atl_mir_value_t *atl_mir_builder_fog_compute(atl_mir_builder_t *b, atl_mir_value_t *dist, atl_mir_value_t *density);
atl_mir_value_t *atl_mir_builder_lerp(atl_mir_builder_t *b, atl_mir_value_t *a, atl_mir_value_t *b_val, atl_mir_value_t *t);

/* ---------------------------------------------------------------------------
 * Syscall/JNI/Intrinsic Instructions
 * --------------------------------------------------------------------------- */

atl_mir_value_t *atl_mir_builder_syscall(atl_mir_builder_t *b, uint32_t syscall_num,
                                          uint32_t num_args, atl_mir_value_t **args);
atl_mir_value_t *atl_mir_builder_jni_call(atl_mir_builder_t *b, const char *method_name,
                                           uint32_t num_args, atl_mir_value_t **args);
atl_mir_value_t *atl_mir_builder_intrinsic(atl_mir_builder_t *b, const char *name,
                                            uint32_t num_args, atl_mir_value_t **args);
void atl_mir_builder_cacheflush(atl_mir_builder_t *b, atl_mir_value_t *addr, atl_mir_value_t *len);
atl_mir_value_t *atl_mir_builder_tls_get(atl_mir_builder_t *b, uint32_t tls_index);

/* ---------------------------------------------------------------------------
 * Debug/Profiling Instructions
 * --------------------------------------------------------------------------- */

void atl_mir_builder_profile_point(atl_mir_builder_t *b, uint32_t point_id);
void atl_mir_builder_debug_loc(atl_mir_builder_t *b, const char *file, uint32_t line, uint32_t arm_pc);

/* ---------------------------------------------------------------------------
 * Generic Instruction Builder (for custom/complex instructions)
 * --------------------------------------------------------------------------- */

atl_mir_value_t *atl_mir_builder_inst(atl_mir_builder_t *b, atl_mir_opcode_t op,
                                       atl_mir_type_t result_type,
                                       uint32_t num_operands, atl_mir_value_t **operands);

/* ---------------------------------------------------------------------------
 * Pass Manager
 * --------------------------------------------------------------------------- */

typedef struct atl_mir_pass_manager atl_mir_pass_manager_t;

atl_mir_pass_manager_t *atl_mir_pass_manager_create(atl_mir_context_t *ctx);
void atl_mir_pass_manager_destroy(atl_mir_pass_manager_t *pm);

/* Add optimization passes */
void atl_mir_pass_manager_add_const_prop(atl_mir_pass_manager_t *pm);
void atl_mir_pass_manager_add_dce(atl_mir_pass_manager_t *pm);
void atl_mir_pass_manager_add_cse(atl_mir_pass_manager_t *pm);
void atl_mir_pass_manager_add_licm(atl_mir_pass_manager_t *pm);
void atl_mir_pass_manager_add_strength_reduce(atl_mir_pass_manager_t *pm);
void atl_mir_pass_manager_add_vectorize(atl_mir_pass_manager_t *pm);
void atl_mir_pass_manager_add_game_fusion(atl_mir_pass_manager_t *pm);
void atl_mir_pass_manager_add_inlining(atl_mir_pass_manager_t *pm);
void atl_mir_pass_manager_add_regalloc(atl_mir_pass_manager_t *pm);
void atl_mir_pass_manager_add_scheduling(atl_mir_pass_manager_t *pm);

/* Run all passes on a function */
bool atl_mir_pass_manager_run(atl_mir_pass_manager_t *pm, atl_mir_function_t *func);

/* Run all passes on a module */
bool atl_mir_pass_manager_run_module(atl_mir_pass_manager_t *pm, atl_mir_module_t *module);

/* ---------------------------------------------------------------------------
 * SSA Verification
 * --------------------------------------------------------------------------- */

bool atl_mir_verify_function(atl_mir_function_t *func, char *error_buf, size_t error_buf_size);
bool atl_mir_verify_module(atl_mir_module_t *module, char *error_buf, size_t error_buf_size);

/* ---------------------------------------------------------------------------
 * Debug Printing
 * --------------------------------------------------------------------------- */

void atl_mir_print_function(atl_mir_function_t *func, FILE *out);
void atl_mir_print_module(atl_mir_module_t *module, FILE *out);
void atl_mir_print_inst(atl_mir_inst_t *inst, FILE *out);

/* ---------------------------------------------------------------------------
 * Lowering to x86_64
 * --------------------------------------------------------------------------- */

/* Lower MIR function to x86_64 machine code */
void *atl_mir_lower_function(atl_mir_function_t *func, size_t *out_size);

/* Lower entire module */
bool atl_mir_lower_module(atl_mir_module_t *module);

/* ---------------------------------------------------------------------------
 * ARM Decode Integration
 * --------------------------------------------------------------------------- */

/* Decode ARM binary to MIR function */
atl_mir_function_t *atl_arm_decode_function(atl_mir_module_t *module,
                                             const uint8_t *code, size_t code_size,
                                             uint32_t start_pc, bool is_thumb);

/* Decode ARM basic block to MIR */
atl_mir_block_t *atl_arm_decode_block(atl_mir_function_t *func,
                                       const uint8_t *code, size_t code_size,
                                       uint32_t start_pc, bool is_thumb);

#ifdef __cplusplus
}
#endif

#endif /* ATL_MIR_H */