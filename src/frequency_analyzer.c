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

static bool decode_instruction(const u_int8_t *code, size_t max_len, InstrInfo *info) {
    if (max_len < 1) return false;

    u_int8_t first = code[0];
    size_t pos = 1;

    u_int8_t flags = get_flags(first);
    int arg_size = get_arg_size(first);

    // get_arg_size returns -1 for unknown or incorrect opcodes
    if (arg_size < 0) {
        fatal_error("Failed to get information about instruction with opcode 0x%02x", first);
    }

    // Set opcode and subtype based on group membership
    info->opcode = first;
    info->param_count = 0;
    info->subtype = low_bits(first);

    // Allocate appropriate initial capacity based on instruction type
    size_t initial_capacity = 8; // Default for non-VARLEN instructions
    const instruction_format_t* format = NULL;
    if (flags & INSTR_FLAG_VARLEN) {
        format = get_instruction_format(info->opcode);
        if (format) {
            // Count non-repeating fields before count field
            int non_repeating = 0;
            for (int i = 0; i < format->field_count; i++) {
                if (strstr(format->fields[i].name, "count") != NULL) {
                    break;
                }
                non_repeating++;
            }
            // Calculate sensible initial capacity (estimate for 2-4 repeating iterations)
            int repeating_fields = format->field_count - non_repeating;
            size_t estimated = non_repeating + repeating_fields * 4;
            initial_capacity = (estimated < 8) ? 8 : (estimated < 16) ? 16 : (estimated < 32) ? 32 : 64;
        } else {
            // Fallback for formats without proper structure
            initial_capacity = 16;
        }
    }
    if (!init_instr_info(info, initial_capacity)) return false;

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
            }
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
    const char *name = get_instr_name(info->opcode);

    if (strcmp(name, "UNKNOWN") == 0) {
        fatal_error("Failed to get instruction name with opcode 0x%02x", info->opcode);
        return;
    }

    // Print instruction name
    fprintf(out, "%s", name);

    if (info->param_count == 0) return;

    uint8_t flags = get_flags(info->opcode);
    const instruction_format_t* format = NULL;

    // Print parameters
    if (flags & INSTR_FLAG_VARLEN) {
        format = get_instruction_format(info->opcode);
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
            }
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
                            mode_str = get_addr_mode_symbol(mode);
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

static void print_sequence(FILE *out, const u_int8_t *data, size_t len) {
    size_t pos = 0;
    int first = 1;
    while (pos < len) {
        InstrInfo info;
        if (!decode_instruction(data + pos, len - pos, &info)) {
            fatal_error("Failed to decode instruction at offset 0x%02x", pos);
            return;
        }
        if (!first) fprintf(out, ", ");
        print_instr(&info, out);
        pos += info.length;
        first = 0;
    }
}

// Reachability analysis
static bool split_after(uint8_t op) {
    uint8_t flags = get_flags(op);

    // Split after jumps, calls, and terminal instructions
    if (flags & INSTR_FLAG_JUMP) return true;
    if (flags & INSTR_FLAG_HALT) return true;
    if (flags & INSTR_FLAG_BREAK) return true;

    return false;
}

static void print_function_calls(byte_file *bf) {
    printf("\n--- Public Functions ---\n");

    for (u_int32_t i = 0; i < bf->public_symbols_number; i++) {
        const char* name = get_public_name(bf, i);
        u_int32_t code_offset = get_public_offset(bf, i);
        u_int32_t file_offset = bf->code_offset_base + code_offset;
        printf("Function %u: %s at addr 0x%08x (file offset 0x%08x)\n", i, name, code_offset, file_offset);
    }
}

static void print_reachability_stats(byte_file *bf, u_int8_t *reachable, const uint8_t *jump_targets) {
    u_int32_t reachable_count = 0;
    printf("\n--- Jump targets ---\n");
    for (u_int32_t i = 0; i < bf->code_size; i++) {
        if (BIT_GET(reachable, i)) {
            reachable_count++;
            if (BIT_GET(jump_targets, i))
                printf("    addr: 0x%04x, file_offset: 0x%04x - entry/jump target\n", i, bf->code_offset_base + i);
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
static void analyze_reachability(byte_file *bf, uint8_t *reachable, uint8_t *jump_targets) {
    uint32_t *worklist = malloc(bf->code_size * sizeof(uint32_t));
    if (!worklist) fatal_error("Out of memory!");
    uint32_t wl_size = 0;

#if DEBUG_ANALYSIS
    printf("DEBUG: Found %u public symbols\n", bf->public_symbols_number);
#endif

    // Public symbols
    for (uint32_t i = 0; i < bf->public_symbols_number; i++) {
        uint32_t addr = get_public_offset(bf,i);
#if DEBUG_ANALYSIS
        printf("DEBUG: Public symbol %s at offset 0x%08x (0x%08x)\n",
                    get_public_name(bf, i),
                    addr,
                    bf->code_offset_base + addr);
#endif
        if (addr >= bf->code_size)
            fatal_error("Public symbol offset 0x%08x (0x%08x) out of bounds", addr, bf->code_offset_base + addr);
        if (!BIT_GET(reachable, addr)) {
            BIT_SET(reachable, addr);
            BIT_SET(jump_targets, addr);
            worklist[wl_size++] = addr;
        }
    }

    while (wl_size > 0) {
        uint32_t addr = worklist[--wl_size];

        InstrInfo info;
        if (!decode_instruction((uint8_t*) bf->code_ptr + addr, bf->code_size - addr, &info))
            fatal_error("Failed to decode instruction at 0x%x", addr);

        uint8_t flags = get_flags(info.opcode);

#if DEBUG_ANALYSIS
        const char *instr_name = get_instr_name(info.opcode);
        printf("DEBUG: Visiting addr=0x%08x (0x%08x), bytes_len=%zu, name=%s\n",
               addr, bf->code_offset_base + addr, info.length, instr_name);
#endif

        // If jump, add to worklist
        if (flags & INSTR_FLAG_JUMP) {
            uint32_t target = info.params[0];
            if (target >= bf->code_size)
                fatal_error("Jump target %u out of bounds at 0x%x", target, addr);
            BIT_SET(jump_targets, target);
            if (!BIT_GET(reachable, target)) {
                BIT_SET(reachable, target);
                worklist[wl_size++] = target;
            }
        }

        // Next instruction is reachable, if next instr not terminal
        if (!(flags & INSTR_FLAG_HALT)) {
            uint32_t next = addr + info.length;
            if (next < bf->code_size && !BIT_GET(reachable, next)) {
                BIT_SET(reachable, next);
                worklist[wl_size++] = next;
            }
        }
        free_instr_info(&info);
    }

    free(worklist);
}

// For sequence sorting by byte comparison
// NOTE: count field is intentionally absent to minimize memory usage
//  *   keys:    6 * 2 * code_size = 12 * code_size   (sorting phase)
//  *   results: 10 * unique_count << 10 * total_seqs   (after dedup)
typedef struct __attribute__((packed)) {
    uint32_t offset;
    uint16_t len;
} SeqKey;

// For sequence sorting by frequency
// NOTE: forms via deduplication of SeqKeys
typedef struct __attribute__((packed)) {
    uint32_t offset;
    uint32_t count;
    uint16_t len;
} SeqResult;

static const uint8_t *seq_compare_base = NULL;

// Byte comparison
static int compare_keys(const void *a, const void *b) {
    const SeqKey *sa = (const SeqKey*)a;
    const SeqKey *sb = (const SeqKey*)b;
    size_t min_len = sa->len < sb->len ? sa->len : sb->len;
    int cmp = memcmp(seq_compare_base + sa->offset,
                     seq_compare_base + sb->offset, min_len);
    if (cmp != 0) return cmp;
    return (sa->len < sb->len) ? -1 : (sa->len > sb->len) ? 1 : 0;
}

// Frequency counter
static int compare_results_by_freq(const void *a, const void *b) {
    const SeqResult *ua = (const SeqResult*)a;
    const SeqResult *ub = (const SeqResult*)b;
    if (ua->count != ub->count)
        return (ua->count < ub->count) ? 1 : -1;
    size_t min_len = ua->len < ub->len ? ua->len : ub->len;
    int cmp = memcmp(seq_compare_base + ua->offset,
                     seq_compare_base + ub->offset, min_len);
    if (cmp != 0) return cmp;
    return (ua->len < ub->len) ? -1 : (ua->len > ub->len) ? 1 : 0;
}

typedef void (*seq_callback)(void *ctx, uint32_t offset, uint16_t len);

// Iterate over reachable bytecode and emit sequence records via callback
// For each instruction we emit:
//   * A single-instruction record (offset, len).
//   * A two-instruction record (prev_offset, prev_len + len), with split flag checks
static void process_sequences(byte_file *bf, const uint8_t *reachable, const uint8_t *jump_targets,
                              seq_callback callback, void *ctx) {
    uint32_t addr = 0, prev_addr = 0;
    size_t prev_len = 0;
    bool has_prev = false;

    while (addr < bf->code_size) {
        if (!BIT_GET(reachable, addr)) { addr++; has_prev = false; continue; }
        if (BIT_GET(jump_targets, addr)) has_prev = false;

        InstrInfo info;
        if (!decode_instruction((uint8_t*)bf->code_ptr + addr, bf->code_size - addr, &info))
            fatal_error("Failed to decode instruction at 0x%x", addr);

        callback(ctx, addr, (uint16_t)info.length);

        if (has_prev) {
            size_t pair_len = prev_len + info.length;
            callback(ctx, prev_addr, (uint16_t)pair_len);
        }

        if (split_after(info.opcode)) {
            has_prev = false;
        } else {
            prev_addr = addr;
            prev_len = info.length;
            has_prev = true;
        }

        addr += info.length;
        free_instr_info(&info);
    }
}

// Counts how many sequence records will be emitted
// No allocations here
static void count_callback(void *ctx, uint32_t offset, uint16_t len) {
    (void)offset; (void)len;
    (*(size_t*)ctx)++;
}

// Writes one SeqKey per emitted sequence into the buffer
static void add_key_callback(void *ctx, uint32_t offset, uint16_t len) {
    SeqKey **ptr = (SeqKey**)ctx;
    (*ptr)->offset = offset;
    (*ptr)->len    = len;
    (*ptr)++;
}

void analyze_frequency(byte_file *bf) {
    // Both are freed as soon as the second process_sequences pass completes
    uint8_t *reachable    = calloc(BITSET_SIZE(bf->code_size), 1);
    uint8_t *jump_targets = calloc(BITSET_SIZE(bf->code_size), 1);
    if (!reachable || !jump_targets) fatal_error("Out of memory");

    analyze_reachability(bf, reachable, jump_targets);
    print_reachability_stats(bf, reachable, jump_targets);
    print_function_calls(bf);

    // Count total sequences without allocating structures
    size_t total_seqs = 0;
    process_sequences(bf, reachable, jump_targets, count_callback, &total_seqs);

    // Populate SeqKey structure, where each record represent a single occurence
    SeqKey *keys = malloc(total_seqs * sizeof(SeqKey));
    if (!keys) fatal_error("Out of memory");

    // Pointer to keys structure, so we can iterate through the structure, saving the pointer to beginning in *keys
    SeqKey *write_ptr = keys;
    process_sequences(bf, reachable, jump_targets, add_key_callback, &write_ptr);

    // Free reachable and jump_targets
    free(reachable);    reachable    = NULL;
    free(jump_targets); jump_targets = NULL;

    // Sort by raw byte content so that identical sequences become adjacent
    seq_compare_base = bf->code_ptr;
    qsort(keys, total_seqs, sizeof(SeqKey), compare_keys);

    // Linear scan over sorted array of SeqKeys to count number of unique sequences
    size_t unique_count = 0;
    for (size_t i = 0; i < total_seqs; ) {
        size_t j = i;
        while (j < total_seqs && compare_keys(&keys[i], &keys[j]) == 0) j++;
        unique_count++;
        i = j;
    }

    // Memory peak
    //      *   keys:    6 * total_seqs
    //      *   results: 10 * unique_count
    SeqResult *results = malloc(unique_count * sizeof(SeqResult));
    if (!results) fatal_error("Out of memory");

    // [ADD][ADD][ADD][LD 1][LD 1][LD 2][RET][RET][RET][RET]
    //  i         j
    //  └───────┘ count=3
    size_t r = 0;
    for (size_t i = 0; i < total_seqs; ) {
        size_t j = i;
        while (j < total_seqs && compare_keys(&keys[i], &keys[j]) == 0) j++;
        results[r].offset = keys[i].offset;
        results[r].len    = keys[i].len;
        results[r].count  = (uint32_t)(j - i);
        r++;
        i = j;
    }

    // SeqKeys successfully consumed to became SeqResults
    free(keys); keys = NULL;

    // Sequences with equal count are ordered by their raw bytes for a stable, deterministic output
    qsort(results, unique_count, sizeof(SeqResult), compare_results_by_freq);

    for (size_t i = 0; i < unique_count; i++) {
        printf("\n%u : ", results[i].count);
        print_sequence(stdout,
                       (uint8_t*)bf->code_ptr + results[i].offset,
                       results[i].len);
    }

    free(results);
}
