#include "../include/instructions.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#undef INSTR_FORMAT
#undef FIELD_INT
#undef FIELD_ADDR_MODE
#undef END_INSTR_FORMAT

#define INSTR_FORMAT(name) \
    static field_descriptor_t name##_fields[] = {

#define FIELD_INT(name, size) \
    { name, FIELD_TYPE_INT, size, NULL },

#define FIELD_ADDR_MODE(name, mode) \
    { name, FIELD_TYPE_ADDR_MODE, 1, #mode },   // #mode → "ADDR_MODE"

#define END_INSTR_FORMAT(name) \
    }; \
    static const instruction_format_t format_##name = { \
        #name, \
        name##_fields, \
        sizeof(name##_fields) / sizeof(name##_fields[0]) \
    };

// Включаем opcodes.def, чтобы создать format_CLOSURE (и другие)
#include "../include/opcodes.def"

// Сразу убираем временные макросы, чтобы не мешали дальнейшим включениям
#undef INSTR_FORMAT
#undef FIELD_INT
#undef FIELD_ADDR_MODE
#undef END_INSTR_FORMAT

// Таблица указателей на все сгенерированные форматы
// (при добавлении нового формата не забудьте добавить его сюда)
static const instruction_format_t* format_table[] = {
    &format_CLOSURE,
    NULL
};

// Функция поиска формата по имени
const instruction_format_t* get_instruction_format(const char* name) {
    if (!name) return NULL;
    for (int i = 0; format_table[i]; ++i) {
        if (strcmp(format_table[i]->name, name) == 0)
            return format_table[i];
    }
    return NULL;
}

#undef BINOP
#define BINOP(opcode, name, symbol) #symbol,
const char* binop_symbols[] = {
    #include "opcodes.def"
};
#undef BINOP

#undef ADDR_MODE
#define ADDR_MODE(mode, name, symbol) #symbol,
const char* addr_mode_symbols[] = {
    #include "opcodes.def"
    #undef GROUP_INFO
    #undef BINOP
    #undef LD
    #undef LDA
    #undef ST
    #undef PATT
    #undef ADDR_MODE
    #undef INSTR
    #undef INSTR_FORMAT
    #undef FIELD_INT
    #undef FIELD_ADDR_MODE
    #undef END_INSTR_FORMAT
};
#undef ADDR_MODE

#undef GROUP_INFO
#define GROUP_INFO(name,arg_size, flags) \
    [HIGH_BITS_##name] = {arg_size, flags},
group_info_t group_info_table[HIGH_BITS_MAX] = {
    #include "opcodes.def"
        #undef GROUP_INFO
    #undef BINOP
    #undef LD
    #undef LDA
    #undef ST
    #undef PATT
    #undef ADDR_MODE
    #undef INSTR
    #undef INSTR_FORMAT
    #undef FIELD_INT
    #undef FIELD_ADDR_MODE
    #undef END_INSTR_FORMAT
};
#undef GROUP_INFO

// MASTER INSTRUCTION TABLE
instruction_info_t instructions[256];

// Combined instruction array with both standalone and group instructions
static const instruction_info_t all_instructions[] = {

    // Group-based instructions - they get properties from group_info_table
    #define GROUP_INFO(name, arg_size, flags)

    #define BINOP(opcode, name, symbol) \
        {opcode, "BINOP_" #name, 0, 0, true, #symbol},

    #define LD(opcode, name) \
        {opcode, "LD_" #name, 0, 0, true, NULL},

    #define LDA(opcode, name) \
        {opcode, "LDA_" #name, 0, 0, true, NULL},

    #define ST(opcode, name) \
        {opcode, "ST_" #name, 0, 0, true, NULL},

    #define PATT(opcode, name) \
        {opcode, "PATT_" #name, 0, 0, true, NULL},

    // Standalone instructions
    #define INSTR(opcode, name, arg_size, flags) \
        {opcode, #name, arg_size, flags, false, NULL},

    #define INSTR_FORMAT(name)
    #define FIELD_INT(name, size)
    #define FIELD_ADDR_MODE(name, mode)
    #define END_INSTR_FORMAT(name)

    #include "../include/opcodes.def"

    #undef GROUP_INFO
    #undef BINOP
    #undef LD
    #undef LDA
    #undef ST
    #undef PATT
    #undef ADDR_MODE
    #undef INSTR
    #undef INSTR_FORMAT
    #undef FIELD_INT
    #undef FIELD_ADDR_MODE
    #undef END_INSTR_FORMAT

    {0, NULL, 0, 0, false, NULL}  // Sentinel
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
        instructions[i].symbol = NULL;
    }

    // Copy all instructions from the generated array to master table
    size_t count = sizeof(all_instructions) / sizeof(all_instructions[0]);

    for (size_t i = 0; i < count - 1; i++) {  // Skip sentinel
        uint8_t opcode = all_instructions[i].opcode;

        instructions[opcode].opcode = opcode;
        instructions[opcode].instr_name = all_instructions[i].instr_name;
        instructions[opcode].flags = all_instructions[i].flags;
        instructions[opcode].is_group = all_instructions[i].is_group;
        instructions[opcode].symbol = all_instructions[i].symbol;

        // For group instructions, get arg_size from group_info_table
        if (all_instructions[i].is_group) {
            uint8_t h = high_bits(opcode);
            if (h < HIGH_BITS_MAX) {
                instructions[opcode].arg_size = group_info_table[h].arg_size;
                instructions[opcode].flags = group_info_table[h].flags;
            }
        } else {
            // Standalone instructions use their own arg_size
            instructions[opcode].arg_size = all_instructions[i].arg_size;
        }
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

            printf("0x%02X: %-20s (arg=%d, flags=0x%02X, group=%d",
                   instructions[i].opcode,
                   instructions[i].instr_name,
                   instructions[i].arg_size,
                   instructions[i].flags,
                   instructions[i].is_group);

            if (instructions[i].symbol) {
                printf(", symbol='%s'", instructions[i].symbol);
            }

            printf(")\n");
        }
    }

    printf("\n=== GROUP INFO TABLE ===\n");
    const char* group_names[] = {"BINOP", "LD", "LDA", "ST", "PATT"};
    for (int i = 0; i < HIGH_BITS_MAX; i++) {
        if (group_info_table[i].arg_size > 0 || group_info_table[i].flags > 0) {
            const char* name = (i < 5) ? group_names[i] : "UNKNOWN";
            printf("%s: arg_size=%d, flags=0x%02X\n",
                   name,
                   group_info_table[i].arg_size,
                   group_info_table[i].flags);
        }
    }
}
