#ifndef INSTRUCTIONS_H
#define INSTRUCTIONS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ADDRESSING MODES (Global, local, argument, constant)
typedef enum {
    #define ADDR_MODE(mode, name, symbol) ADDR_##name = mode,

    #include "opcodes.def"

    ADDR_MODE_MAX
} addr_mode_t;

// group properties (arg_size, flags)
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
    #define INSTR(opcode, name, arg_size, flags) BC_TYPE_##name = opcode,

    #include "opcodes.def"

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

    #include "opcodes.def"

    BINOP_MAX
} binop_op_t;

// Generate PATT operation constants from opcodes.def
typedef enum {
    #define PATT(opcode, name) PATT_##name = opcode,

    #include "opcodes.def"

    PATT_MAX
} patt_op_t;

static inline int get_arg_size(uint8_t opcode) {
    switch (opcode) {
#define INSTR(op, name, arg_size, flags) case op: return (arg_size);
#include "opcodes.def"
        default: return -1;
    }
}

static inline uint8_t get_flags(uint8_t opcode) {
    switch (opcode) {
#define INSTR(op, name, arg_size, flags) case op: return (uint8_t)(flags);
#include "opcodes.def"
        default: return 0;
    }
}

static inline const char *get_instr_name(uint8_t opcode) {
    switch (opcode) {
#define INSTR(op, name, arg_size, flags) case op: return #name;
#include "opcodes.def"
        default: return "UNKNOWN";
    }
}


static inline const char *get_binop_symbol(uint8_t opcode) {
    switch (opcode) {
#define BINOP(op, name, symbol) case op: return #symbol;
#include "opcodes.def"
        default: return NULL;
    }
}

static inline const char *get_addr_mode_symbol(addr_mode_t mode) {
    switch (mode) {
#define ADDR_MODE(m, name, symbol) case m: return #symbol;
#include "opcodes.def"
        default: return "?";
    }
}

// Print instructions table
void debug_print_instructions(void);

// variable-length instructions
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
    uint8_t opcode;
    const field_descriptor_t* fields;
    int field_count;
} instruction_format_t;

// Get instruction format by opcode
const instruction_format_t* get_instruction_format(uint8_t opcode);

#endif // INSTRUCTIONS_H
