#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// GENERATE FLAG DEFINITIONS FROM OPCODES.DEF
#define INSTR_FLAG(name, value) name = value,

typedef enum {
    #include "opcodes.def"
} instruction_flags_t;

#undef INSTR_FLAG

// BIT MANIPULATION UTILITIES
#define LOW_BITS_COUNT  4
#define LOW_BITS_MASK   ((1 << LOW_BITS_COUNT) - 1)
#define HIGH_BITS_MASK  (~LOW_BITS_MASK)

static inline uint8_t high_bits(const uint8_t instruction) {
    return (instruction & HIGH_BITS_MASK) >> LOW_BITS_COUNT;
}

static inline uint8_t low_bits(const uint8_t instruction) {
    return instruction & LOW_BITS_MASK;
}

// GENERATE BYTECODE ENUM FROM OPCODES.DEF
typedef enum {
    #define INSTR(opcode, name, arg_size, flags) BC_TYPE_##name = opcode,
    #define INSTR_GROUP(opcode, name) BC_TYPE_##name = opcode,

    #define GROUP_ELEMENT(...)
    #define END_GROUP
    #define INSTR_FLAG(...)
    #define VARLEN_INSTR(code, name, flags, format_name, format_count) \
        BC_TYPE_##name = code,

    #define INSTR_FORMAT(...)
    #define FORMAT_FIELD(...)
    #define END_INSTR_FORMAT

    #include "opcodes.def"

    BC_TYPE_MAX
} bytecode_type_t;

// Extract high bits from INSTR_GROUP definitions
typedef enum {
    #define INSTR(...)
    #undef INSTR_GROUP
    #define INSTR_GROUP(opcode, name) HIGH_BITS_##name = ((opcode) & HIGH_BITS_MASK) >> LOW_BITS_COUNT,
    #undef GROUP_ELEMENT
    #define GROUP_ELEMENT(...)
    #undef END_GROUP
    #define END_GROUP
    #undef INSTR_FLAG
    #define INSTR_FLAG(...)
    #undef VARLEN_INSTR
    #define VARLEN_INSTR(...)
    #undef INSTR_FORMAT
    #define INSTR_FORMAT(...)
    #undef FORMAT_FIELD
    #define FORMAT_FIELD(...)
    #undef END_INSTR_FORMAT
    #define END_INSTR_FORMAT
    #define ADDR_MODE(...)

    #include "opcodes.def"

    HIGH_BITS_MAX
} high_bits_group_t;

// Extract low bits from GROUP_ELEMENT definitions
typedef enum {
    #define GROUP_ELEMENT(opcode, group, name, arg_size, flags) \
        name = opcode,

    #define INSTR(...)
    #define INSTR_GROUP(...)
    #define END_GROUP
    #define INSTR_FLAG(...)
    #define VARLEN_INSTR(...)
    #define INSTR_FORMAT(...)
    #define FORMAT_FIELD(...)
    #define END_INSTR_FORMAT

    #include "opcodes.def"

} opcode_subtype_t;

// Extract addressing mode from ADDR_MODE definitions
typedef enum {
    #define ADDR_MODE(name, value) name = value,

    #define INSTR(...)
    #define INSTR_GROUP(...)
    #define GROUP_ELEMENT(...)
    #define END_GROUP
    #define INSTR_FLAG(...)
    #define VARLEN_INSTR(...)
    #define INSTR_FORMAT(...)
    #define FORMAT_FIELD(...)
    #define END_INSTR_FORMAT

    #include "opcodes.def"

    ADDR_MODE_MAX
} addr_mode_t;

// Instruction information
typedef struct {
    uint8_t opcode;              // Full opcode byte
    const char* instr_name;      // Instruction name
    int arg_size;                // Argument size in bytes
    uint8_t flags;               // Instruction flags
    bool is_group;               // True if group-based instruction
} instruction_info_t;

// Global lookup table (contains both standalone and group instructions)
extern instruction_info_t instructions[256];

// Lookup functions
static inline instruction_info_t* get_instruction_info(uint8_t opcode) {
    if (opcode >= 256) return NULL;
    return &instructions[opcode];
}

static inline const char* get_instruction_name(uint8_t opcode) {
    instruction_info_t* info = get_instruction_info(opcode);
    return info ? info->instr_name : "UNKNOWN";
}

static inline int get_instruction_arg_size(uint8_t opcode) {
    instruction_info_t* info = get_instruction_info(opcode);
    return info ? info->arg_size : 0;
}

static inline uint8_t get_instruction_flags(uint8_t opcode) {
    instruction_info_t* info = get_instruction_info(opcode);
    return info ? info->flags : 0;
}

static inline bool is_group_instruction(uint8_t opcode) {
    instruction_info_t* info = get_instruction_info(opcode);
    return info ? info->is_group : false;
}

// Initialization
void init_instructions(void);

// Address resolution
// Resolve address based on addressing mode and index
uint32_t *resolve_address(addr_mode_t mode, uint32_t index);

// Print instructions table
void debug_print_instructions(void);

#endif // INSTRUCTIONS_H
