# pragma once

#include <stdbool.h>

#define LOW_BITS_COUNT 4
#define LOW_BITS_MASK ((1 << LOW_BITS_COUNT) - 1)
#define HIGH_BITS_MASK ~LOW_BITS_MASK

// Opcode definitions
typedef enum {
    BINOP = 0x00,       // Binary operations group identifier
    CONST = 0x10,       // `CONST k`
    XSTRING = 0x11,     // `STRING s`
    SEXP = 0x12,        // `SEXP s n`
    STI = 0x13,         // indirect store to a variable
    STA = 0x14,         // indirect store to a variable or an aggregate
    JMP = 0x15,         // `JMP l`
    END = 0x16,         // `END`
    RET = 0x17,         // `RET`
    DROP = 0x18,        // `DROP`
    DUP = 0x19,         // `DUP`
    SWAP = 0x1A,        // `SWAP`
    ELEM = 0x1B,        // `ELEM` (lookup by index)
    LD = 0x20,          //  push value of ... onto the stack
    LDA = 0x30,         //  push ref of ... onto the stack
    ST = 0x40,          //  store value in ...
    CJMP_Z = 0x50,      // `CJMPz l`
    CJMP_NZ = 0x51,     // `CJMPnz l`
    BEGIN = 0x52,       // `BEGIN a n`
    CBEGIN = 0x53,      // `CBEGIN a n`
    CLOSURE = 0x54,     // `CLOSURE 1 n V(m)`
    CALLC = 0x55,       // `CALLC n`
    CALL = 0x56,        // `CALL l n`
    TAG = 0x57,         // `TAG s n`
    ARRAY = 0x58,       // `ARRAY n`
    FAIL = 0x59,        // `FAIL ln col`
    LINE = 0x5A,        // `LINE ln`
    PATT = 0x60,         // `PATT group`
    CALL_READ = 0x70,   // `CALL Lread`
    CALL_WRITE = 0x71,  // `CALL Lwrite`
    CALL_LENGTH = 0x72, // `CALL Llength`
    CALL_STRING = 0x73, // `CALL Lstring`
    CALL_ARRAY = 0x74  // `CALL Barray`
} bytecode_type;

// BINOP codes definitions
typedef enum {
    PLUS = 0x01,          // `BINOP +`
    MINUS = 0x02,         // `BINOP -`
    MULTIPLY = 0x03,      // `BINOP *`
    DIVIDE = 0x04,        // `BINOP /`
    REMAINDER = 0x05,     // `BINOP %`
    LESS = 0x06,          // `BINOP <`
    LESS_EQUAL = 0x07,    // `BINOP <=`
    GREATER = 0x08,       // `BINOP >`
    GREATER_EQUAL = 0x09, // `BINOP >=`
    EQUAL = 0x0A,         // `BINOP ==`
    NOT_EQUAL = 0x0B,     // `BINOP !=`
    AND = 0x0C,           // `BINOP &&`
    OR = 0x0D             // `BINOP !!`
} binop_type;

// LD, LDA and ST
typedef enum {
    L_GLOBAL = 0x00,
    L_LOCAL = 0x01,
    L_ARGUMENT = 0x02,
    L_CLOSURE = 0x03
} LOC;

// PATT group
enum PATT_TYPE {
    PATT_STR,        // `PATT =str`
    PATT_TAG_STR,    // `PATT #string`
    PATT_TAG_ARR,    // `PATT #array`
    PATT_TAG_SEXP,   // `PATT #sexp`
    PATT_BOXED,      // `PATT #ref`
    PATT_UNBOXED,    // `PATT #val`
    PATT_TAG_CLOSURE // `PATT #fun`
};

// Binary masks for groups
typedef enum {
    BINOP_HIGH_BITS = 0x00,
    LD_HIGH_BITS = 0x02,
    LDA_HIGH_BITS = 0x03,
    ST_HIGH_BITS = 0x04,
    PATT_HIGH_BITS = 0x06
} bytecode_high_bits;

static inline u_int8_t high_bits(const u_int8_t instruction) {
    return (instruction & HIGH_BITS_MASK) >> LOW_BITS_COUNT;
}

static inline u_int8_t low_bits(const u_int8_t instruction) {
    return instruction & LOW_BITS_MASK;
}

static inline bytecode_type get_bytecode_type(const u_int8_t ip) {
    u_int8_t h = high_bits(ip);
    switch (h) {
        case BINOP_HIGH_BITS:
            return BINOP;
        case LD_HIGH_BITS:
            return LD;
        case LDA_HIGH_BITS:
            return LDA;
        case ST_HIGH_BITS:
            return ST;
        case PATT_HIGH_BITS:
            return PATT;
        default:
            if (ip == BEGIN + 1) {
                return BEGIN;
            }
            return (bytecode_type) ip;
    }
}
