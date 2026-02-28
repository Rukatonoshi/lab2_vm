#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "byte_file.h"
#include "bytecode_decoder.h"

// Error handling
static void fatal_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

// Instruction decoding and printing
typedef struct {
    bytecode_type opcode;      // main opcode type (CONST, BINOP, ...)
    u_int8_t subtype;           // for groups: operation or location type
    u_int32_t params[32];       // parameters (enough for CLOSURE)
    size_t param_count;        // number of parameters stored
    size_t length;             // total length in bytes
} InstrInfo;

static bool decode_instruction(const u_int8_t *code, size_t max_len, u_int32_t addr, InstrInfo *info) {
    if (max_len < 1) return false;
    u_int8_t first = code[0];
    u_int8_t h = high_bits(first);
    u_int8_t l = low_bits(first);
    size_t pos = 1;

    // Determine main opcode and subtype
    bytecode_type type;
    if (h == BINOP_HIGH_BITS) {
        type = BINOP;
        info->subtype = l;
    } else if (h == LD_HIGH_BITS) {
        type = LD;
        info->subtype = l;
    } else if (h == LDA_HIGH_BITS) {
        type = LDA;
        info->subtype = l;
    } else if (h == ST_HIGH_BITS) {
        type = ST;
        info->subtype = l;
    } else if (h == PATT_HIGH_BITS) {
        type = PATT;
        info->subtype = l;
    } else {
        type = (bytecode_type)first;
        info->subtype = 0;
        // BEGIN and CBEGIN (0x52 & 0x53) have the same structure
        if (first == BEGIN + 1) {
            type = BEGIN;
        }
    }
    info->opcode = type;
    // DEBUG
//    printf("DEBUG: addr offset=0x%02x first=0x%02x type=%d subtype=%d\n",
//       addr, first, info->opcode, info->subtype, code);

    // Helper macros to read integers and bytes
    #define READ_INT() do { \
        if (pos + 4 > max_len) return false; \
        u_int32_t val = (u_int32_t)code[pos] | ((u_int32_t)code[pos+1]<<8) | \
                       ((u_int32_t)code[pos+2]<<16) | ((u_int32_t)code[pos+3]<<24); \
        if (info->param_count >= 32) return false; \
        info->params[info->param_count++] = val; \
        pos += 4; \
    } while(0)

    #define READ_BYTE() do { \
        if (pos + 1 > max_len) return false; \
        u_int8_t b = code[pos]; \
        if (info->param_count >= 32) return false; \
        info->params[info->param_count++] = b; \
        pos += 1; \
    } while(0)

    info->param_count = 0;
    switch (type) {
        // Instructions without parameters
        case BINOP:
        case PATT:
        case DROP: case DUP: case SWAP: case ELEM:
        case END: case RET:
        case CALL_READ: case CALL_WRITE: case CALL_LENGTH: case CALL_STRING:
        case STA: case STI:
            break;
        case CALL_ARRAY:
            READ_INT();
            break;

        // One 32‑bit parameter
        case CONST:
        case XSTRING:
        case JMP:
        case CJMP_Z:
        case CJMP_NZ:
        case LINE:
            READ_INT();
            break;

        // LD, LDA, ST – one index
        case LD: case LDA: case ST:
            READ_INT();
            break;

        // Two ints
        case SEXP:
            READ_INT(); // string index
            READ_INT(); // arity
            break;
        // TODO check Begin
        case BEGIN:
        case CBEGIN:
            READ_INT(); // n_args
            READ_INT(); // n_locals
            break;
        case CALL:
            READ_INT(); // target offset
            READ_INT(); // n_args
            break;
        case TAG:
            READ_INT(); // string index
            READ_INT(); // n
            break;
        case FAIL:
            READ_INT(); // line
            READ_INT(); // column
            break;
        case ARRAY:
        case CALLC:
            READ_INT();
            break;

        // CLOSURE: ip, bn, then bn pairs (byte + int)
        case CLOSURE:
            READ_INT(); // ip
            READ_INT(); // bn
            {
                u_int32_t bn = info->params[info->param_count-1]; // last read
                for (u_int32_t i = 0; i < bn; i++) {
                    READ_BYTE(); // location type
                    READ_INT();  // index
                }
            }
            break;

        default:
            // Unknown opcode – treat as error
            return false;
    }

    info->length = pos;
    return true;
}

static void print_instr(const InstrInfo *info, FILE *out) {
    switch (info->opcode) {
        case BINOP: {
            const char *op = NULL;
            switch (info->subtype) {
                case PLUS: op = "+"; break;
                case MINUS: op = "-"; break;
                case MULTIPLY: op = "*"; break;
                case DIVIDE: op = "/"; break;
                case REMAINDER: op = "%"; break;
                case LESS: op = "<"; break;
                case LESS_EQUAL: op = "<="; break;
                case GREATER: op = ">"; break;
                case GREATER_EQUAL: op = ">="; break;
                case EQUAL: op = "=="; break;
                case NOT_EQUAL: op = "!="; break;
                case AND: op = "&&"; break;
                case OR: op = "||"; break;
                default: break;
            }
            if (op)
                fprintf(out, "BINOP %s", op);
            else
                fprintf(out, "BINOP ??? (subtype=%u)", info->subtype);
            break;
        }
        case CONST:    fprintf(out, "CONST %d", (int32_t)info->params[0]); break;
        case XSTRING:  fprintf(out, "STRING %u", info->params[0]); break;
        case SEXP:     fprintf(out, "SEXP %u %u", info->params[0], info->params[1]); break;
        case STA:      fprintf(out, "STA"); break;
        case STI:      fprintf(out, "STI"); break;
        case JMP:      fprintf(out, "JMP 0x%02x", info->params[0]); break;
        case END:      fprintf(out, "END"); break;
        case RET:      fprintf(out, "RET"); break;
        case DROP:     fprintf(out, "DROP"); break;
        case DUP:      fprintf(out, "DUP"); break;
        case SWAP:     fprintf(out, "SWAP"); break;
        case ELEM:     fprintf(out, "ELEM"); break;
        case LD: {
            const char *loc = "?";
            switch (info->subtype) {
                case L_GLOBAL: loc = "Global"; break;
                case L_LOCAL:  loc = "Local"; break;
                case L_ARGUMENT: loc = "Arg"; break;
                case L_CLOSURE: loc = "Closure"; break;
            }
            fprintf(out, "LD %s %u", loc, info->params[0]);
            break;
        }
        case LDA: {
            const char *loc = "?";
            switch (info->subtype) {
                case L_GLOBAL: loc = "Global"; break;
                case L_LOCAL:  loc = "Local"; break;
                case L_ARGUMENT: loc = "Arg"; break;
                case L_CLOSURE: loc = "Closure"; break;
            }
            fprintf(out, "LDA %s %u", loc, info->params[0]);
            break;
        }
        case ST: {
            const char *loc = "?";
            switch (info->subtype) {
                case L_GLOBAL: loc = "Global"; break;
                case L_LOCAL:  loc = "Local"; break;
                case L_ARGUMENT: loc = "Arg"; break;
                case L_CLOSURE: loc = "Closure"; break;
            }
            fprintf(out, "ST %s %u", loc, info->params[0]);
            break;
        }
        case CJMP_Z:   fprintf(out, "CJMPz 0x%02x", info->params[0]); break;
        case CJMP_NZ:  fprintf(out, "CJMPnz 0x%02x", info->params[0]); break;
        case BEGIN:    fprintf(out, "BEGIN %u %u", info->params[0], info->params[1]); break;
        case CBEGIN:   fprintf(out, "CBEGIN %u %u", info->params[0], info->params[1]); break;
        case CLOSURE: {
            fprintf(out, "CLOSURE %u %u", info->params[0], info->params[1]);
            u_int32_t bn = info->params[1];
            for (u_int32_t i = 0; i < bn; i++) {
                u_int8_t b = info->params[2 + i*2];
                u_int32_t idx = info->params[2 + i*2 + 1];
                const char *loc = "?";
                switch (b) {
                    case L_GLOBAL: loc = "Global"; break;
                    case L_LOCAL:  loc = "Local"; break;
                    case L_ARGUMENT: loc = "Arg"; break;
                    case L_CLOSURE: loc = "Closure"; break;
                }
                fprintf(out, " %s %u", loc, idx);
            }
            break;
        }
        case CALLC:    fprintf(out, "CALLC %u", info->params[0]); break;
        case CALL:     fprintf(out, "CALL 0x%02x %u", info->params[0], info->params[1]); break;
        case TAG:      fprintf(out, "TAG %u %u", info->params[0], info->params[1]); break;
        case ARRAY:    fprintf(out, "ARRAY %u", info->params[0]); break;
        case FAIL:     fprintf(out, "FAIL %u %u", info->params[0], info->params[1]); break;
        case LINE:     fprintf(out, "LINE %u", info->params[0]); break;
        case PATT: {
            const char *patt = "???";
            switch (info->subtype) {
                case PATT_STR:        patt = "=str"; break;
                case PATT_TAG_STR:    patt = "#string"; break;
                case PATT_TAG_ARR:    patt = "#array"; break;
                case PATT_TAG_SEXP:   patt = "#sexp"; break;
                case PATT_BOXED:      patt = "#ref"; break;
                case PATT_UNBOXED:    patt = "#val"; break;
                case PATT_TAG_CLOSURE: patt = "#fun"; break;
            }
            fprintf(out, "PATT %s", patt);
            break;
        }
        case CALL_READ:    fprintf(out, "CALL Lread"); break;
        case CALL_WRITE:   fprintf(out, "CALL Lwrite"); break;
        case CALL_LENGTH:  fprintf(out, "CALL Llength"); break;
        case CALL_STRING:  fprintf(out, "CALL Lstring"); break;
        case CALL_ARRAY:   fprintf(out, "CALL Barray %u", info->params[0]); break;
        default:           fprintf(out, "UNKNOWN(%02x)", info->opcode); break;
    }
}

static void print_sequence(FILE *out, const u_int8_t *data, size_t len) {
    size_t pos = 0;
    int first = 1;
    while (pos < len) {
        InstrInfo info;
        if (!decode_instruction(data + pos, len - pos, pos,&info)) {
            fprintf(out, "<invalid>");
            return;
        }
        if (!first) fprintf(out, ", ");
        print_instr(&info, out);
        pos += info.length;
        first = 0;
    }
}

// Hash table for counting sequences (using uthash)
#include "uthash.h"

typedef struct {
    char *bytes;            // copy of the byte sequence
    size_t len;
    u_int32_t count;
    UT_hash_handle hh;
} CountEntry;

static CountEntry *counts = NULL;

static void increment_count(const u_int8_t *data, size_t len) {
    CountEntry *entry;
    HASH_FIND(hh, counts, data, len, entry);
    if (!entry) {
        entry = (CountEntry*)malloc(sizeof(CountEntry));
        entry->bytes = (char*)malloc(len);
        memcpy(entry->bytes, data, len);
        entry->len = len;
        entry->count = 0;
        HASH_ADD_KEYPTR(hh, counts, entry->bytes, len, entry);
    }
    entry->count++;
}

// Reachability analysis
static bool is_control_transfer(bytecode_type op) {
    return op == JMP || op == CJMP_Z || op == CJMP_NZ || op == CALL;
}

static bool is_terminal(bytecode_type op) {
    return op == JMP || op == END || op == RET || op == FAIL;
}

static bool split_after(bytecode_type op) {
    return op == JMP || op == CALL || op == CALLC || op == RET || op == END || op == FAIL;
}

// Comparison function for sorting entries (file scope, static)
static int compare_entries(const void *a, const void *b) {
    const CountEntry *ea = *(const CountEntry**)a;
    const CountEntry *eb = *(const CountEntry**)b;
    if (ea->count != eb->count) {
        return (ea->count < eb->count) ? 1 : -1; // higher count first
    }
    size_t min_len = ea->len < eb->len ? ea->len : eb->len;
    int cmp = memcmp(ea->bytes, eb->bytes, min_len);
    if (cmp != 0) return cmp;
    if (ea->len < eb->len) return -1;
    if (ea->len > eb->len) return 1;
    return 0;
}

// Main analysis function
void analyze_frequency(byte_file *bf) {
    // Array of reachable flags (1 byte per code byte)
    u_int8_t *reachable = (u_int8_t*)calloc(bf->code_size, 1);
    u_int8_t *jump_target = (u_int8_t*)calloc(bf->code_size, 1);
    if (!reachable || !jump_target) fatal_error("Out of memory");

    // Queue for addresses to process
    u_int32_t *queue = (u_int32_t*)malloc(bf->code_size * sizeof(u_int32_t));
    if (!queue) fatal_error("Out of memory");
    u_int32_t qhead = 0, qtail = 0;

    // Enqueue all public symbols
    for (u_int32_t i = 0; i < bf->public_symbols_number; i++) {
        u_int32_t addr = get_public_offset(bf, i);
        if (addr >= bf->code_size) {
            fatal_error("Public symbol offset %u out of code bounds", addr);
        }
        if (!reachable[addr]) {
            reachable[addr] = 1;
            queue[qtail++] = addr;
        }
    }

    // Forward reachability
    while (qhead < qtail) {
        u_int32_t addr = queue[qhead++];
        InstrInfo info;
        if (!decode_instruction((u_int8_t*)bf->code_ptr + addr, bf->code_size - addr, addr, &info)) {
            fatal_error("Failed to decode instruction at offset 0x%02x", addr);
        }

        // If it's a jump, add target
        if (is_control_transfer(info.opcode)) {
            u_int32_t target = info.params[0]; // first param is target offset
            if (target >= bf->code_size) {
                fatal_error("Jump target %u out of code bounds at offset %u", target, addr);
            }
            jump_target[target] = 1;
            if (!reachable[target]) {
                reachable[target] = 1;
                queue[qtail++] = target;
            }
        }

        // If not terminal, add next instruction
        if (!is_terminal(info.opcode)) {
            u_int32_t next = addr + info.length;
            if (next < bf->code_size && !reachable[next]) {
                reachable[next] = 1;
                queue[qtail++] = next;
            }
        }
    }

    free(queue);

    // Walk through code, building basic blocks and counting sequences
    u_int32_t i = 0;
    while (i < bf->code_size) {
        // Skip unreachable areas
        if (!reachable[i]) {
            i++;
            continue;
        }

        // Start of a block
        u_int32_t block_start = i;
        const u_int8_t *prev_start = NULL;
        size_t prev_len = 0;

        while (i < bf->code_size && reachable[i]) {
            InstrInfo cur;
            if (!decode_instruction((u_int8_t*)bf->code_ptr + i, bf->code_size - i, i, &cur)) {
                fatal_error("Failed to decode instruction at offset %u", i);
            }

            // Count single instruction
            increment_count((u_int8_t*)bf->code_ptr + i, cur.length);

            // Count pair with previous if exists
            if (prev_start) {
                size_t pair_len = prev_len + cur.length;
                const u_int8_t *pair_start = prev_start;
                increment_count(pair_start, pair_len);
            }

            // Decide whether to split after this instruction
            int split = 0;
            if (split_after(cur.opcode)) {
                split = 1;
            }
            u_int32_t next_addr = i + cur.length;
            if (next_addr < bf->code_size && jump_target[next_addr]) {
                split = 1;
            }

            // Update previous
            prev_start = (u_int8_t*)bf->code_ptr + i;
            prev_len = cur.length;
            i = next_addr;

            if (split) break;
        }
        // Block ends, continue loop
    }

    free(reachable);
    free(jump_target);

    // Collect all entries from hash table
    CountEntry *entry, *tmp;
    size_t n_entries = HASH_COUNT(counts);
    CountEntry **array = (CountEntry**)malloc(n_entries * sizeof(CountEntry*));
    if (!array) fatal_error("Out of memory");
    size_t idx = 0;
    HASH_ITER(hh, counts, entry, tmp) {
        array[idx++] = entry;
    }

    qsort(array, n_entries, sizeof(CountEntry*), compare_entries);

    // Print results
    for (size_t j = 0; j < n_entries; j++) {
        entry = array[j];
        printf("\n%u : ", entry->count);
        print_sequence(stdout, (u_int8_t*)entry->bytes, entry->len);
    }

    // Cleanup
    HASH_ITER(hh, counts, entry, tmp) {
        HASH_DEL(counts, entry);
        free(entry->bytes);
        free(entry);
    }
    free(array);
}
