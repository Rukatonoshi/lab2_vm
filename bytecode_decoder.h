# pragma once

#include <stdbool.h>

#define LOW_BITS_COUNT 4
#define LOW_BITS_MASK ((1 << LOW_BITS_COUNT) - 1)
#define HIGH_BITS_MASK ~LOW_BITS_MASK

typedef enum {
    BINOP = 0x00,
    CONST = 0x10,
    XSTRING = 0x11,
    SEXP = 0x12,
    STI = 0x13,
    STA = 0x14,
    JMP = 0x15,
    END = 0x16,
    RET = 0x17,
    DROP = 0x18,
    DUP = 0x19,
    SWAP = 0x1A,
    ELEM = 0x1B,
    LD = 0x20,
    LDA = 0x30,
    ST = 0x40,
    CJMP_Z = 0x50,
    CJMP_NZ = 0x51,
    CALL_READ = 0x70,
    CALL_WRITE = 0x71,
    CALL_LENGTH = 0x72,
    CALL_STRING = 0x73,
    CALL_ARRAY = 0x74,
    BEGIN = 0x52,
    CLOSURE = 0x54,
    CALLC = 0x55,
    CALL = 0x56,
    TAG = 0x57,
    ARRAY = 0x58,
    FAIL = 0x59,
    LINE = 0x5A,
    PATT = 0x60
} bytecode_type;

typedef enum {
    PLUS = 0x01,
    MINUS = 0x02,
    MULTIPLY = 0x03,
    DIVIDE = 0x04,
    REMAINDER = 0x05,
    LESS = 0x06,
    LESS_EQUAL = 0x07,
    GREATER = 0x08,
    GREATER_EQUAL = 0x09,
    EQUAL = 0x0A,
    NOT_EQUAL = 0x0B,
    AND = 0x0C,
    OR = 0x0D
} binop_type;

typedef enum {
    GLOBAL = 0x00,
    LOCAL = 0x01,
    ARGUMENT = 0x02,
    CLOJURE = 0x03
} LOC;

enum PATT_TYPE {
    PATT_STR,
    PATT_TAG_STR,
    PATT_TAG_ARR,
    PATT_TAG_SEXP,
    PATT_BOXED,
    PATT_UNBOXED,
    PATT_TAG_CLOSURE
};

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






