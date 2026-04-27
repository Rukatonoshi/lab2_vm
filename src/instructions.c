#include "../include/instructions.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Master instruction table
instruction_info_t instructions[256];

// Combined instruction array with both standalone and group instructions
static const instruction_info_t all_instructions[] = {

    // Standalone instructions
    #define INSTR(opcode, name, arg_size, flags) \
        {opcode, #name, arg_size, flags, false},

    // Group elements
    #define GROUP_ELEMENT(opcode, group, name, arg_size, flags) \
        {opcode, #name, arg_size, flags, true},

    // Variable-length instructions
    #define VARLEN_INSTR(code, name, flags, format_name, format_count) \
        {code, #name, 0, flags, false},

    #define INSTR_GROUP(opcode, name)
    #define END_GROUP
    #define INSTR_FLAG(name, value)

    #define INSTR_FORMAT(name)
    #define FORMAT_FIELD(field_type, field_size, repeat_from)
    #define END_INSTR_FORMAT

    #include "../include/opcodes.def"

    #undef INSTR
    #undef GROUP_ELEMENT
    #undef INSTR_GROUP
    #undef END_GROUP
    #undef INSTR_FLAG
    #undef VARLEN_INSTR
    #undef INSTR_FORMAT
    #undef FORMAT_FIELD
    #undef END_INSTR_FORMAT

    {0, NULL, 0, 0, false}
};

// Initialize master instruction table
void init_instructions(void) {
    // Clear all entries to invalid state
    for (int i = 0; i < 256; i++) {
        instructions[i].opcode = (uint8_t)i;
        instructions[i].instr_name = "UNKNOWN";
        instructions[i].arg_size = 0;
        instructions[i].flags = 0;
        instructions[i].is_group = false;
    }

    // Copy all instructions from the generated array to master table
    size_t count = sizeof(all_instructions) / sizeof(all_instructions[0]);

    for (size_t i = 0; i < count - 1; i++) {  // Skip sentinel
        uint8_t opcode = all_instructions[i].opcode;

        instructions[opcode].opcode = opcode;
        instructions[opcode].instr_name = all_instructions[i].instr_name;
        instructions[opcode].arg_size = all_instructions[i].arg_size;
        instructions[opcode].flags = all_instructions[i].flags;
        instructions[opcode].is_group = all_instructions[i].is_group;
    }
}

// Init
__attribute__((constructor))
static void auto_init_instructions(void) {
    init_instructions();
}

// DEBUG FUNCTION
void debug_print_instructions(void) {
    printf("=== INSTRUCTION TABLE ===\n");

    for (int i = 0; i < 256; i++) {
        if (instructions[i].instr_name &&
            strcmp(instructions[i].instr_name, "UNKNOWN") != 0) {

            printf("0x%02X: %-20s (arg=%d, flags=0x%02X, group=%d)\n",
                   instructions[i].opcode,
                   instructions[i].instr_name,
                   instructions[i].arg_size,
                   instructions[i].flags,
                   instructions[i].is_group);
        }
    }
}
