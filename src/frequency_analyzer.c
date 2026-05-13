#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "byte_file.h"
#include "../include/instructions.h"
#include "frequency_analyzer.h"

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
    uint8_t opcode;
    uint8_t subtype;
    u_int32_t *params;
    size_t param_count;
    size_t param_capacity;
    size_t length;
} InstrInfo;

static bool init_instr_info(InstrInfo *info, size_t initial_capacity) {
    info->params = calloc(initial_capacity, sizeof(u_int32_t));
    if (!info->params) return false;
    info->param_count = 0;
    info->param_capacity = initial_capacity;
    info->length = 0;
    return true;
}

static void free_instr_info(InstrInfo *info) {
    if (info && info->params) {
        free(info->params);
        info->params = NULL;
        info->param_count = 0;
        info->param_capacity = 0;
        info->length = 0;
    }
}

static bool ensure_capacity(InstrInfo *info, size_t needed) {
    if (info->param_capacity >= needed) return true;
    // Calculate geometrically growing capacity
    size_t new_capacity = (needed > info->param_capacity * 2) ? needed : info->param_capacity * 2;
    u_int32_t *new_params = realloc(info->params, new_capacity * sizeof(u_int32_t));
    if (!new_params) return false;
    info->params = new_params;
    info->param_capacity = new_capacity;
    return true;
}

static bool decode_instruction(const u_int8_t *code, size_t max_len, u_int32_t addr, InstrInfo *info) {
    if (max_len < 1) return false;

    u_int8_t first = code[0];
    u_int8_t h = high_bits(first);
    u_int8_t l = low_bits(first);
    size_t pos = 1;

    // Get instruction info from X-macro table
    instruction_info_t *instr = &instructions[first];
    if (!instr) {
        fatal_error("Failed to get information about instruction with opcode 0x%02x", first);
    }

    // Set opcode and subtype based on group membership
    info->opcode = first;
    info->param_count = 0;

    if (instr->is_group) {
        // Extract subtype from low bits for group instructions
        info->subtype = l;
    } else {
        // Standalone instruction, no meaningful subtype
        info->subtype = 0;
    }

    // Allocate appropriate initial capacity based on instruction type
    size_t initial_capacity = 8; // Default for non-VARLEN instructions
    if (instr->flags & INSTR_FLAG_VARLEN) {
        const instruction_format_t* fmt = get_instruction_format(info->opcode);
        if (fmt) {
            // Count non-repeating fields before count field
            int non_repeating = 0;
            for (int i = 0; i < fmt->field_count; i++) {
                if (strstr(fmt->fields[i].name, "count") != NULL) {
                    break;
                }
                non_repeating++;
            }
            // Calculate sensible initial capacity (estimate for 2-4 repeating iterations)
            int repeating_fields = fmt->field_count - non_repeating;
            size_t estimated = non_repeating + repeating_fields * 4;
            initial_capacity = (estimated < 8) ? 8 : (estimated < 16) ? 16 : (estimated < 32) ? 32 : 64;
        } else {
            // Fallback for formats without proper structure
            initial_capacity = 16;
        }
    }
    if (!init_instr_info(info, initial_capacity)) return false;

    // Universal parameter reading using format table
    const instruction_format_t* format = NULL;
    if (instr->flags & INSTR_FLAG_VARLEN) {
        format = get_instruction_format(info->opcode);
        if (!format) {
            return false;
        }
    }

    int arg_size = instr->arg_size;

    // Handle VARLEN instructions using format table
    if (format) {
        u_int32_t repeat_count = 0;
        int repeat_start_field = -1;

        // First pass: process non-repeating fields and find count field
        for (int i = 0; i < format->field_count; i++) {
            const field_descriptor_t* field = &format->fields[i];

            if (field->type == FIELD_TYPE_INT) {
                // Determine if we should process this field
                bool is_count_field = (strstr(field->name, "count") != NULL);
                bool will_process = is_count_field || repeat_start_field == -1;

                if (will_process) {
                    if (pos + field->size > max_len) return false;

                    u_int32_t val = 0;
                    for (int j = 0; j < field->size; j++) {
                        val |= (u_int32_t)code[pos + j] << (j * 8);
                    }

                    if (is_count_field) {
                        repeat_count = val;
                        repeat_start_field = i + 1;
                    }

                    if (!ensure_capacity(info, info->param_count + 1)) return false;
                    info->params[info->param_count++] = val;
                    pos += field->size;
                }
                // Skip repeating INT fields (they're processed in second pass)
            }
            // Skip repeating ADDR_MODE fields in first pass
        }

        // Second pass: process repeating fields only if count > 0
        if (repeat_count > 0 && repeat_start_field != -1) {
            int num_repeating_fields = format->field_count - repeat_start_field;

            for (u_int32_t r = 0; r < repeat_count; r++) {
                for (int bf = 0; bf < num_repeating_fields; bf++) {
                    const field_descriptor_t* repeating_field = &format->fields[repeat_start_field + bf];

                    if (repeating_field->type == FIELD_TYPE_INT) {
                        if (pos + repeating_field->size > max_len) return false;

                        u_int32_t val = 0;
                        for (int j = 0; j < repeating_field->size; j++) {
                            val |= (u_int32_t)code[pos + j] << (j * 8);
                        }
                        if (!ensure_capacity(info, info->param_count + 1)) return false;
                        info->params[info->param_count++] = val;
                        pos += repeating_field->size;
                    } else if (repeating_field->type == FIELD_TYPE_ADDR_MODE) {
                        if (pos + repeating_field->size > max_len) return false;
                        u_int8_t mode = code[pos];
                        if (!ensure_capacity(info, info->param_count + 1)) return false;
                        info->params[info->param_count++] = mode;
                        pos += repeating_field->size;
                    }
                }
            }
        }
    } else if (arg_size > 0) {
        // Fixed-size arguments: read arg_size bytes
        // Store as u_int32_t chunks (4 bytes each)
        size_t remaining = arg_size;
        while (remaining > 0) {
            if (pos + remaining > max_len) return false;

            if (remaining >= sizeof(u_int32_t)) {
                // Read 4-byte chunk
                u_int32_t val = (u_int32_t)code[pos] |
                              ((u_int32_t)code[pos+1] << 8) |
                              ((u_int32_t)code[pos+2] << 16) |
                              ((u_int32_t)code[pos+3] << 24);
                if (!ensure_capacity(info, info->param_count + 1)) return false;
                info->params[info->param_count++] = val;
                pos += sizeof(u_int32_t);
                remaining -= sizeof(u_int32_t);
            } else if (remaining == 1) {
                // Read 1-byte value
                u_int8_t val = code[pos];
                if (!ensure_capacity(info, info->param_count + 1)) return false;
                info->params[info->param_count++] = val;
                pos += remaining;
                remaining = 0;
            } else {
                // Other byte sizes: pack into 4-byte chunks
                u_int32_t val = 0;
                for (int i = 0; i < remaining; i++) {
                    val |= (u_int32_t)code[pos + i] << (i * 8);
                }
                if (!ensure_capacity(info, info->param_count + 1)) return false;
                info->params[info->param_count++] = val;
                pos += remaining;
                remaining = 0;
            }
        }
    }

    info->length = pos;
    return true;
}

static void print_instr(const InstrInfo *info, FILE *out) {
    instruction_info_t *instr = &instructions[info->opcode];
    if (!instr) {
        fatal_error("Failed to get information about instruction with opcode 0x%02x", info->opcode);
        return;
    }

    const char *name = instr->instr_name;
    if (strcmp(name, "UNKNOWN") == 0) {
        fatal_error("Failed to get instruction name with opcode 0x%02x", info->opcode);
        return;
    }

    // Print instruction name
    fprintf(out, "%s", name);

    // Print parameters universally
    if (info->param_count > 0) {
        const instruction_format_t* format = NULL;
        if (instr->flags & INSTR_FLAG_VARLEN) {
            format = get_instruction_format(instr->opcode);
        }

        // Format-based printing for VARLEN instructions
        if (format) {
            u_int32_t repeat_count = 0;
            int repeat_start_field = -1;
            int param_index = 0;
            uint8_t mode;
            const char* mode_str;

            // First pass: process non-repeating fields and extract count
            for (int i = 0; i < format->field_count && param_index < info->param_count; i++) {
                const field_descriptor_t* field = &format->fields[i];

                if (field->type == FIELD_TYPE_INT) {
                    // Check if this is the count field
                    if (strstr(field->name, "count") != NULL) {
                        repeat_count = info->params[param_index];
                        repeat_start_field = i + 1;
                        fprintf(out, " %u", info->params[param_index++]);
                    } else if (repeat_start_field == -1) {
                        // Before count field: process normally
                        fprintf(out, " %u", info->params[param_index++]);
                    }
                    // After count field: skip (processed in second pass)
                }
                // Skip ADDR_MODE fields in first pass
            }

            // Second pass: process repeating fields only if count > 0
            if (repeat_count > 0 && repeat_start_field != -1) {
                int num_repeating_fields = format->field_count - repeat_start_field;

                for (u_int32_t r = 0; r < repeat_count && param_index < info->param_count; r++) {
                    for (int bf = 0; bf < num_repeating_fields && param_index < info->param_count; bf++) {
                        const field_descriptor_t* repeating_field = &format->fields[repeat_start_field + bf];

                        if (repeating_field->type == FIELD_TYPE_INT) {
                            fprintf(out, " %u", info->params[param_index++]);
                        } else if (repeating_field->type == FIELD_TYPE_ADDR_MODE) {
                            mode = (uint8_t)info->params[param_index++];
                            mode_str = "U";
                            if (mode < ADDR_MODE_MAX) {
                                mode_str = addr_mode_symbols[mode];
                            }
                            // Print mode(index) format for varspec fields
                            if (param_index < info->param_count) {
                                uint32_t index = info->params[param_index++];
                                fprintf(out, " %s(%u)", mode_str, index);
                            }
                        }
                    }
                }
            }
        } else {
            // Standard parameter printing for non-VARLEN instructions
            fprintf(out, " ");
            for (size_t i = 0; i < info->param_count; i++) {
                if (i > 0) fprintf(out, " ");
                fprintf(out, "%u", info->params[i]);
            }
        }
    }
}

static void print_sequence(FILE *out, const u_int8_t *data, size_t len) {
    size_t pos = 0;
    int first = 1;
    while (pos < len) {
        InstrInfo info;
        if (!decode_instruction(data + pos, len - pos, pos, &info)) {
            fatal_error("Failed to decode instruction at offset 0x%02x", pos);
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
    const u_int8_t *bytes;     // pointer to original bytecode
    size_t len;
    u_int32_t count;
    UT_hash_handle hh;
} CountEntry;

static CountEntry *counts = NULL;

static void increment_count(const u_int8_t *data, size_t len) {
    CountEntry *entry;
    HASH_FIND(hh, counts, data, len, entry);
    if (!entry) {
        if (HASH_COUNT(counts) >= MAX_UNIQUE_SEQUENCES) return;
        entry = malloc(sizeof(CountEntry));
        if (!entry) return;
        entry->bytes = data;
        entry->len = len;
        entry->count = 0;
        HASH_ADD_KEYPTR(hh, counts, entry->bytes, len, entry);
    }
    entry->count++;
}

// Reachability analysis - universal using instruction flags
static bool is_control_transfer(uint8_t op) {
    instruction_info_t *instr = &instructions[op];
    return instr && (instr->flags & INSTR_FLAG_JUMP);
}

static bool is_terminal(uint8_t op) {
    instruction_info_t *instr = &instructions[op];
    return instr && (instr->flags & INSTR_FLAG_HALT);
}

static bool split_after(uint8_t op) {
    instruction_info_t *instr = &instructions[op];
    if (!instr) return false;

    // Split after jumps, calls, and terminal instructions
    if (instr->flags & INSTR_FLAG_JUMP) return true;
    if (instr->flags & INSTR_FLAG_HALT) return true;
    if (instr->flags & INSTR_FLAG_BREAK) return true;

    return false;
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

static void print_function_calls(byte_file *bf) {
    printf("\n--- Public Functions ---\n");

    // Calculate file offset base
    uint32_t public_table_size = bf->public_symbols_number * 2 * sizeof(u_int32_t);
    uint32_t file_offset_base = 12 + public_table_size + (uint32_t)bf->string_table_size;

    for (u_int32_t i = 0; i < bf->public_symbols_number; i++) {
        const char* name = get_public_name(bf, i);
        u_int32_t code_offset = get_public_offset(bf, i);
        u_int32_t file_offset = file_offset_base + code_offset;
        printf("Function %u: %s at file offset 0x%08x (code offset 0x%08x)\n", i, name, file_offset, code_offset);
    }
}

static void print_reachability_stats(byte_file *bf, u_int8_t *reachable) {
    u_int32_t reachable_count = 0;
    for (u_int32_t i = 0; i < bf->code_size; i++) {
        if (reachable[i]) {
            reachable_count++;
        }
    }
    printf("\n--- Reachability Stats ---\n");
    printf("Total code size: %u bytes\n", bf->code_size);
    printf("Reachable instructions: %u bytes\n", reachable_count);
    if (bf->code_size > 0) {
        printf("Reachable code: %.2f%%\n", (double)reachable_count / bf->code_size * 100);
    }
}

// Main analysis function
void analyze_frequency(byte_file *bf) {
    // Array of reachable flags and split points (1 byte per code byte)
    u_int8_t *reachable = (u_int8_t*)calloc(bf->code_size, 1);
    u_int8_t *seq_split = (u_int8_t*)calloc(bf->code_size, 1);
    if (!reachable || !seq_split) fatal_error("Out of memory");

    // Mark all public symbols as entry points (split points)
#if DEBUG_ANALYSIS
    printf("DEBUG: Found %u public symbols\n", bf->public_symbols_number);
#endif

    for (u_int32_t i = 0; i < bf->public_symbols_number; i++) {
        u_int32_t addr = get_public_offset(bf, i);
#if DEBUG_ANALYSIS
        printf("DEBUG: Public symbol %s at offset 0x%08x\n", get_public_name(bf, i), addr);
#endif
        if (addr >= bf->code_size) {
            fatal_error("Public symbol offset %u out of code bounds", addr);
        }
        if (!reachable[addr]) {
            reachable[addr] = 1;
            seq_split[addr] = 1; // Public symbols are sequence start points
        }
    }

    // Forward pass: Mark reachable code and sequence split points
    uint32_t addr = 0;
    while (addr < bf->code_size) {
        if (!reachable[addr]) {
            addr++; // Skip unreachable bytes
            continue;
        }

        InstrInfo info;
        if (!decode_instruction((u_int8_t*)bf->code_ptr + addr, bf->code_size - addr, addr, &info)) {
            fatal_error("Failed to decode instruction 0x%02X at offset 0x%02x", info.opcode, addr);
        }

#if DEBUG_ANALYSIS
        instruction_info_t *instr = &instructions[info.opcode];
        printf("DEBUG: Visiting addr=0x%08x, bytes_len=%zu, name=%s\n",
               addr, info.length, instr->instr_name);
#endif

        // Mark jump targets as split points
        if (is_control_transfer(info.opcode)) {
            u_int32_t target = info.params[0];
            if (target >= bf->code_size) {
                fatal_error("Jump target %u out of code bounds at offset %u", target, addr);
            }
            if (!reachable[target]) {
                reachable[target] = 1; // New entry point
                seq_split[target] = 1;  // Mark as sequence start
            } else {
                seq_split[target] = 1;  // Mark as sequence start (even if already reachable)
            }
        }

        // Mark next instruction if not terminal
        if (!is_terminal(info.opcode)) {
            uint32_t next = addr + info.length;
            if (next < bf->code_size) {
                reachable[next] = 1;
            }
        }

        // Mark split points after calls and split_after instructions
        if (split_after(info.opcode)) {
            uint32_t next = addr + info.length;
            if (next < bf->code_size) {
                seq_split[next] = 1;
            }
        }

        addr += info.length;
    }

#if DEBUG_ANALYSIS
    if (DEBUG_ANALYSIS) {
        uint32_t reachable_bytes = 0, split_count = 0;
        for (uint32_t i = 0; i < bf->code_size; i++) {
            if (reachable[i]) reachable_bytes++;
            if (seq_split[i]) split_count++;
        }
        printf("DEBUG: Reachable bytes: %u, Split points: %u\n", reachable_bytes, split_count);
    }
#endif

    // Walk through code, building sequences using split points
    u_int32_t i = 0;
    while (i < bf->code_size) {
        // Skip unreachable areas
        if (!reachable[i]) {
            i++;
            continue;
        }

        // Start of a sequence
        const u_int8_t *prev_start = NULL;
        size_t prev_len = 0;

        while (i < bf->code_size && reachable[i]) {
            InstrInfo cur;
            if (!decode_instruction((u_int8_t*)bf->code_ptr + i, bf->code_size - i, i, &cur)) {
                fatal_error("Failed to decode instruction at offset %u", i);
            }

#if DEBUG_ANALYSIS
            instruction_info_t *instr = &instructions[cur.opcode];
            printf("DEBUG: Sequence: %s (len=%zu) at 0x%08x\n", instr->instr_name, cur.length, i);
#endif

            // Count single instruction
            increment_count((u_int8_t*)bf->code_ptr + i, cur.length);

            // Count pair with previous if exists
            if (prev_start) {
                size_t pair_len = prev_len + cur.length;
                const u_int8_t *pair_start = prev_start;
                increment_count(pair_start, pair_len);
            }

            // Update previous
            prev_start = (u_int8_t*)bf->code_ptr + i;
            prev_len = cur.length;

            uint32_t next_addr = i + cur.length;
            bool should_split = (next_addr < bf->code_size && seq_split[next_addr]);

            // Reset at split points
            if (should_split) {
                prev_start = NULL;
                prev_len = 0;
            }

            i = next_addr;
        }
    }

    // Print statistics output (before freeing reachability data)
    print_function_calls(bf);
    print_reachability_stats(bf, reachable);

    free(reachable);
    free(seq_split);

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
        free(entry);
    }
    free(array);
}
