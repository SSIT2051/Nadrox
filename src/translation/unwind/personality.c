/*
 * ARM EHABI Personality Routine for x86_64
 * Custom personality routine for handling ARM exceptions on x86_64
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unwind.h>

#include "personality.h"
#include "ehabi_decode.h"

/* Itanium C++ ABI unwind return codes */
#define _URC_NO_REASON                0
#define _URC_FOREIGN_EXCEPTION_CAUGHT 1
#define _URC_FATAL_PHASE2_ERROR       2
#define _URC_FATAL_PHASE1_ERROR       3
#define _URC_NORMAL_STOP              4
#define _URC_END_OF_STACK             5
#define _URC_HANDLER_FOUND            6
#define _URC_INSTALL_CONTEXT          7
#define _URC_CONTINUE_UNWIND          8

/* Exception class */
#define EXCEPTION_CLASS_ARM           0x41524D43 /* 'ARMC' */
#define EXCEPTION_CLASS_CXX           0x434C4E47ULL << 32 /* GNU GCC C++ */

/* Actions */
#define _UA_SEARCH_PHASE              1
#define _UA_CLEANUP_PHASE             2
#define _UA_HANDLER_FRAME             4
#define _UA_FORCE_UNWIND              8
#define _UA_END_OF_STACK              16

/* Global exidx table */
static struct ehabi_entry *g_exidx_entries = NULL;
static size_t g_exidx_count = 0;
static uint32_t g_arm_text_base = 0;
static void (*g_cleanup_handler)(void *) = NULL;

/* Register exidx table */
void atl_arm_register_exidx(uint32_t *exidx_base, size_t exidx_count, 
                             uint32_t text_base)
{
    if (g_exidx_entries) {
        free(g_exidx_entries);
    }
    
    g_exidx_count = exidx_count / 2; /* Each entry is 8 bytes */
    g_exidx_entries = malloc(g_exidx_count * sizeof(struct ehabi_entry));
    g_exidx_entries = g_exidx_entries;
    g_arm_text_base = text_base;
    
    /* Parse exidx entries */
    if (g_exidx_entries) {
        ehabi_parse_exidx(exidx_base, g_exidx_count, g_exidx_entries);
    }
}

/* Find ARM function for given PC */
static struct ehabi_entry *find_arm_function(uint64_t pc)
{
    if (!g_exidx_entries || g_exidx_count == 0)
        return NULL;
    
    /* PC might be in translated x86_64 code, need to map back to ARM */
    /* For now, do direct lookup assuming pc is ARM address */
    return ehabi_lookup_entry(g_exidx_entries, g_exidx_count, (uint32_t)pc);
}

/* Execute ARM cleanup code */
static int execute_arm_cleanup(struct _Unwind_Context *context, 
                                struct ehabi_entry *entry)
{
    /* In a full implementation, we would:
     * 1. Set up the ARM register context
     * 2. Jump to the cleanup code (possibly translated)
     * 3. Handle the cleanup
     */
    
    if (g_cleanup_handler) {
        g_cleanup_handler((void *)context);
        return 1;
    }
    
    return 0;
}

/* Custom ARM personality routine */
_Unwind_Reason_Code atl_arm_personality(int version,
                                        _Unwind_Action actions,
                                        uint64_t exceptionClass,
                                        struct _Unwind_Exception *exceptionObject,
                                        struct _Unwind_Context *context)
{
    /* Check version */
    if (version != 1) {
        return _URC_FATAL_PHASE1_ERROR;
    }
    
    /* Get current PC */
    uint64_t pc = _Unwind_GetIP(context);
    
    /* Find corresponding ARM function */
    struct ehabi_entry *entry = find_arm_function(pc);
    
    if (!entry) {
        /* Not an ARM function or no unwind info */
        return _URC_CONTINUE_UNWIND;
    }
    
    if (entry->cannot_unwind) {
        return _URC_CONTINUE_UNWIND;
    }
    
    /* Phase 1: Search phase */
    if (actions & _UA_SEARCH_PHASE) {
        /* Check if this is a C++ exception */
        if (exceptionClass == ((uint64_t)EXCEPTION_CLASS_CXX << 32) ||
            exceptionClass == 0x434C4E47554E5800ULL) { /* GNU C++ */
            /* Look for catch handlers */
            /* For now, continue unwinding */
            return _URC_CONTINUE_UNWIND;
        }
        
        /* Foreign exception - check if we can handle */
        if (exceptionClass == EXCEPTION_CLASS_ARM) {
            /* Native ARM exception */
            /* Search for handlers */
            return _URC_CONTINUE_UNWIND;
        }
        
        /* Foreign exception from different language/runtime */
        return _URC_CONTINUE_UNWIND;
    }
    
    /* Phase 2: Cleanup/Handler phase */
    if (actions & _UA_CLEANUP_PHASE) {
        /* Execute cleanup code */
        if (execute_arm_cleanup(context, entry)) {
            /* Cleanup executed, continue unwinding */
            return _URC_CONTINUE_UNWIND;
        }
        
        /* Check if this frame has a handler */
        if (actions & _UA_HANDLER_FRAME) {
            /* Install context for handler */
            return _URC_INSTALL_CONTEXT;
        }
        
        return _URC_CONTINUE_UNWIND;
    }
    
    return _URC_CONTINUE_UNWIND;
}

/* Register personality routine with libunwind */
void atl_arm_register_personality(void)
{
    /* Register with __register_frame if needed */
    /* The personality is specified in .eh_frame CIE */
}

/* Set cleanup handler */
void atl_arm_set_cleanup_handler(void (*handler)(void *))
{
    g_cleanup_handler = handler;
}
