#ifndef DWARF_EMIT_H
#define DWARF_EMIT_H

#include <stdint.h>
#include <stddef.h>

#include "ehabi_decode.h"

/* CIE (Common Information Entry) */
struct dwarf_cie {
    uint32_t length;
    uint32_t cie_id; /* 0 in .eh_frame */
    uint8_t version;
    char augmentation[32];
    uint8_t code_alignment_factor;
    int8_t data_alignment_factor;
    uint8_t return_address_register;
    uint64_t personality_routine;
    uint8_t *instructions;
    size_t instructions_len;
};

/* FDE (Frame Description Entry) */
struct dwarf_fde {
    uint32_t length;
    uint32_t cie_pointer;
    uint64_t initial_location;
    uint64_t address_range;
    uint64_t lsda;
    struct dwarf_cie *cie;
    uint8_t *instructions;
    size_t instructions_len;
};

/* Function declarations */
void dwarf_emit_cie(struct dwarf_cie *cie);
void dwarf_emit_fde(struct dwarf_fde *fde, uint32_t func_start, uint32_t func_size);
int dwarf_convert_ehabi_to_cfi(const struct ehabi_unwind_ops *ehabi_ops,
                                uint8_t *cfi_buf, size_t cfi_buf_size);
size_t dwarf_emit_eh_frame(struct ehabi_entry *entries, size_t entry_count,
                            uint8_t *out_buf, size_t out_buf_size,
                            uint64_t text_base);

#endif /* DWARF_EMIT_H */
