#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ADDRESSING MODES (from opcodes.def)
#undef ADDR_MODE
#define ADDR_MODE(mode, name, symbol) ADDR_##name = mode,
typedef enum {
    #include "opcodes.def"
    ADDR_MODE_MAX

} addr_mode_t;
#undef ADDR_MODE

#define INSTR_FLAG_JUMP 0x01
#define INSTR_FLAG_HALT 0x02
#define INSTR_FLAG_BREAK 0x04
#define INSTR_FLAG_VARLEN 0x08

// GROUP INFO STORAGE - stores group properties (arg_size, flags)
typedef struct {
    int arg_size;
    uint8_t flags;
} group_info_t;

// Global group info table indexed by group name hash
extern group_info_t group_info_table[];

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

// Generate BYTECODE ENUM FROM OPCODES.DEF
// Used in interpreter
typedef enum {
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

    #define INSTR(opcode, name, arg_size, flags) BC_TYPE_##name = opcode,

    #define BINOP(opcode, name, symbol) BC_TYPE_BINOP_##name = opcode,
    #define LD(opcode, name) BC_TYPE_LD_##name = opcode,
    #define LDA(opcode, name) BC_TYPE_LDA_##name = opcode,
    #define ST(opcode, name) BC_TYPE_ST_##name = opcode,
    #define PATT(opcode, name) BC_TYPE_PATT_##name = opcode,
    #define GROUP_INFO(name, arg_size, flags)
    #define INSTR_FORMAT(name)
    #define FIELD_INT(name, size)
    #define FIELD_ADDR_MODE(name, mode)
    #define END_INSTR_FORMAT(name)

    #include "opcodes.def"

    #undef INSTR
    #undef BINOP
    #undef LD
    #undef LDA
    #undef ST
    #undef PATT
    #undef GROUP_INFO
    #undef INSTR_FORMAT
    #undef FIELD_INT
    #undef FIELD_ADDR_MODE
    #undef END_INSTR_FORMAT
    #undef ADDR_MODE

    BC_TYPE_MAX
} bytecode_type_t;

// HIGH_BITS constants from instruction structure
typedef enum {
    HIGH_BITS_BINOP = 0,
    HIGH_BITS_LD = 2,
    HIGH_BITS_LDA = 3,
    HIGH_BITS_ST = 4,
    HIGH_BITS_PATT = 6,
    HIGH_BITS_MAX
} high_bits_group_t;

// Generate BINOP operation constants from opcodes.def
typedef enum {
    #define BINOP(opcode, name, symbol) BINOP_##name = opcode,

    #define INSTR(opcode, name, arg_size, flags)
    #define LD(opcode, name)
    #define LDA(opcode, name)
    #define ST(opcode, name)
    #define PATT(opcode, name)
    #define GROUP_INFO(name, arg_size, flags)
    #define INSTR_FORMAT(name)
    #define FIELD_INT(name, size)
    #define FIELD_ADDR_MODE(name, mode)
    #define END_INSTR_FORMAT(name)
    #define ADDR_MODE(mode, name, symbol)

    #include "opcodes.def"

    #undef INSTR
    #undef BINOP
    #undef LD
    #undef LDA
    #undef ST
    #undef PATT
    #undef GROUP_INFO
    #undef INSTR_FORMAT
    #undef FIELD_INT
    #undef FIELD_ADDR_MODE
    #undef END_INSTR_FORMAT
    #undef ADDR_MODE

    BINOP_MAX
} binop_op_t;

// Generate PATT operation constants from opcodes.def
typedef enum {
    #define PATT(opcode, name) PATT_##name = opcode,
    #define INSTR(opcode, name, arg_size, flags)

    #define BINOP(opcode, name, symbol)
    #define LD(opcode, name)
    #define LDA(opcode, name)
    #define ST(opcode, name)
    #define GROUP_INFO(name, arg_size, flags)
    #define INSTR_FORMAT(name)
    #define FIELD_INT(name, size)
    #define FIELD_ADDR_MODE(name, mode)
    #define END_INSTR_FORMAT(name)
    #define ADDR_MODE(mode, name, symbol)

    #include "opcodes.def"

    #undef INSTR
    #undef BINOP
    #undef LD
    #undef LDA
    #undef ST
    #undef PATT
    #undef GROUP_INFO
    #undef INSTR_FORMAT
    #undef FIELD_INT
    #undef FIELD_ADDR_MODE
    #undef END_INSTR_FORMAT
    #undef ADDR_MODE

    PATT_MAX
} patt_op_t;

// Binary operation symbols mapping
extern const char* binop_symbols[];

// Addressing mode symbols mapping
extern const char* addr_mode_symbols[];

// Instruction information
typedef struct {
    uint8_t opcode;              // Full opcode byte
    const char* instr_name;      // Instruction name
    int arg_size;                // Argument size in bytes
    uint8_t flags;               // Instruction flags
    bool is_group;               // True if group-based instruction
    const char* symbol;          // Symbol for display (for BINOP)
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

// Get BINOP symbol if applicable
static inline const char* get_binop_symbol(uint8_t opcode) {
    if (high_bits(opcode) == HIGH_BITS_BINOP) {
        uint8_t low = low_bits(opcode);
        if (low >= 1 && low <= 0x0D) {
            return binop_symbols[low];
        }
    }
    return NULL;
}

// Initialization
void init_instructions(void);

// Print instructions table
void debug_print_instructions(void);

// INSTRUCTION FORMAT SYSTEM for variable-length instructions
typedef enum {
    FIELD_TYPE_INT,
    FIELD_TYPE_ADDR_MODE,
} field_type_t;

typedef struct {
    const char* name;
    field_type_t type;
    int size;           // Size in bytes
    const char* mode;   // Mode name for ADDR_MODE fields (NULL for INT fields)
} field_descriptor_t;

typedef struct {
    const char* name;
    const field_descriptor_t* fields;
    int field_count;
} instruction_format_t;

// Get instruction format by name
const instruction_format_t* get_instruction_format(const char* name);

#endif // INSTRUCTIONS_H
