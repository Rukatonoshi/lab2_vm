#include "../include/instructions.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Closure format
#define INSTR_FORMAT(opcode) \
    static field_descriptor_t fields_##opcode[] = {

#define FIELD_INT(name, size) \
    { name, FIELD_TYPE_INT, size, NULL },

#define FIELD_ADDR_MODE(name, mode) \
    { name, FIELD_TYPE_ADDR_MODE, 1, #mode },   // #mode → "ADDR_MODE"

#define END_INSTR_FORMAT(opcode) \
    }; \
    static const instruction_format_t format_##opcode = { \
        opcode, \
        fields_##opcode, \
        sizeof(fields_##opcode) / sizeof(fields_##opcode[0]) \
    };

#include "../include/opcodes.def"

const instruction_format_t *get_instruction_format(uint8_t opcode) {
    switch (opcode) {
#define INSTR_FORMAT(op)     case (op): return &format_##op;
#define END_INSTR_FORMAT(op)
#define FIELD_INT(n, s)
#define FIELD_ADDR_MODE(n, m)
#include "../include/opcodes.def"

        default: return NULL;
    }
}

// DEBUG FUNCTION
void debug_print_instructions(void) {
    printf("=== INSTRUCTIONS ===\n");

    for (int op = 0; op < 256; op++) {
        const char *name = get_instr_name((uint8_t)op);
        if (name[0] == 'U') continue;   // "UNKNOWN"

        int     arg_size = get_arg_size((uint8_t)op);
        uint8_t flags    = get_flags((uint8_t)op);

        printf("0x%02X: %-24s (arg=%d, flags=0x%02X", op, name, arg_size, flags);

        // Для BINOP — печатаем символ операции
        if (high_bits((uint8_t)op) == HIGH_BITS_BINOP) {
            const char *sym = get_binop_symbol((uint8_t)op);
            if (sym) printf(", op='%s'", sym);
        }

        // Для переменно-длинных — печатаем поля формата
        const instruction_format_t *fmt = get_instruction_format((uint8_t)op);
        if (fmt) {
            printf(", fields=[");
            for (int f = 0; f < fmt->field_count; f++) {
                const field_descriptor_t *fd = &fmt->fields[f];
                if (f) printf(", ");
                if (fd->type == FIELD_TYPE_INT)
                    printf("%s:i%d", fd->name, fd->size * 8);
                else
                    printf("%s:mode(%s)", fd->name, fd->mode ? fd->mode : "?");
            }
            printf("]");
        }

        printf(")\n");
    }
}
