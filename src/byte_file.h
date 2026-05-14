#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <limits.h>

static inline void my_failure(const char* fmt, ...) {
    char buffer[512];
    int len = snprintf(buffer, sizeof(buffer), "Error: ");
    if (len < 0 || len >= (int)sizeof(buffer)) {
        fprintf(stderr, "Error: message too long\n");
        exit(EXIT_FAILURE);
    }

    // Append formatted message
    int n = vsnprintf(buffer + len, sizeof(buffer) - len, fmt, (va_list)0);
    if (n < 0 || n >= (int)(sizeof(buffer) - len)) {
        // Truncated
        buffer[sizeof(buffer) - 1] = '\0';
    }

    // Add newline if missing
    if (strlen(buffer) == 0 || buffer[strlen(buffer) - 1] != '\n') {
        strcat(buffer, "\n");
    }

    // Output to stderr
    fprintf(stderr, "%s", buffer);
    exit(EXIT_FAILURE);
}


// The unpacked representation of bytecode file
typedef struct {
    char *string_ptr;                // A pointer to the beginning of the string table
    u_int32_t *public_ptr;           // A pointer to the beginning of publics table
    char *code_ptr;                  // A pointer to the bytecode itself
    int32_t string_table_size;       // The size of the string table (byte)
    int32_t global_area_size;        // The size of global area (word)
    int32_t public_symbols_number;   // The number of public symbols
    u_int32_t code_size;             // The size of the bytecode (byte)
    u_int32_t code_offset_base;       // The file offset of code beginning
    char buffer[0];
} byte_file;

// Reads a bynary bytecode file by name and runs some checks
static inline byte_file *read_file(const char *file_name) {
    // 2GB file size limitation for fopen
    FILE *f = fopen(file_name, "rb");

    // Check if file could be opened
    if (f == NULL) {
        my_failure("failed to open file: %s\n", strerror(errno));
    }

    // Check for non negative offset
    if (fseek(f, 0, SEEK_END) < 0) {
        my_failure("%s\n", strerror(errno));
    }

    long file_size = ftell(f);
    if (file_size == -1) {
        my_failure("ftell failed: %s\n", strerror(errno));
    }

    // Additional check for file size
    if (file_size > INT_MAX - (long) sizeof(int) * 4) {
        my_failure("File is too big!\nSize: %ld bytes\nMax: %ld\n",
                file_size, INT_MAX - (long) sizeof(int) * 4);
    }

    // Rewind and read header (first three 32-bit values)
    rewind(f);
    int32_t header[3];
    if (fread(header, sizeof(int32_t), 3, f) != 3) {
        my_failure("Failed to read header: %s\n", strerror(errno));
    }

    int32_t string_table_size = header[0];
    int32_t global_area_size = header[1];
    int32_t public_symbols_number = header[2];

    // Checks for header fields values
    if (string_table_size < 0 ||
        public_symbols_number < 0 ||
        global_area_size < 0)
    {
        my_failure("Negative values in header:\nString table = %ld\nPublic syms = %ld\nGlobal area = %ld\n",
                string_table_size, public_symbols_number, global_area_size);
    }

    // Compute sizes
    size_t public_table_size = (size_t) public_symbols_number * 2 * sizeof(u_int32_t);
    size_t data_size = public_table_size + string_table_size;
    if (file_size < (long) (3 * sizeof(u_int32_t) + data_size)) {
        my_failure("File truncated: expected at least %zu bytes, got %ld\n",
                3 * sizeof(u_int32_t) + data_size, file_size);
    }
    u_int32_t code_size = file_size - (3 * sizeof(u_int32_t) + data_size);

    // Allocate memory for byte_file structure plus the data buffer
    byte_file *bf = (byte_file *)malloc(sizeof(byte_file) + data_size + code_size);

    if (bf == NULL) {
        my_failure("Unable to allocate memory for byte_file\n");
    }

    // Fill header fields
    bf->string_table_size = string_table_size;
    bf->global_area_size = global_area_size;
    bf->public_symbols_number = public_symbols_number;

    // Set pointers within the buffer
    char *buffer = bf->buffer;
    bf->public_ptr = (u_int32_t*) buffer;
    bf->string_ptr = buffer + public_table_size;
    bf->code_ptr = bf->string_ptr + string_table_size;

    // Read the remaining data (publics + strings + code)
    if (fread(buffer, 1, data_size + code_size, f) != data_size + code_size) {
        free(bf);
        my_failure("Failed to read data: %s\n", strerror(errno));
    }
    fclose(f);

    // Check that pointers stay within allocated buffer
    char *buffer_end = (char*) bf + sizeof(byte_file) + data_size + code_size;
    if (bf->public_ptr > (u_int32_t*)buffer_end ||
        bf->string_ptr > buffer_end ||
        bf->code_ptr > buffer_end)
    {
        free(bf);
        my_failure("Internal error: pointers exceed buffer bounds\n");
    }

    if ((char *) bf->public_ptr + public_table_size > buffer_end) {
        free(bf);
        my_failure("Public symbols table exceeds file bounds (public_symbols_number=%u)", public_symbols_number);
    }
    if (bf->string_ptr + bf->string_table_size > buffer_end) {
        free(bf);
        my_failure("String table exceeds file bounds (string_table_size=%u)", string_table_size);
    }
    if (bf->code_ptr + code_size > buffer_end) {
        free(bf);
        my_failure("Code block exceeds file bounds (code_size=%u)", code_size);
    }

    // Store code_size in structure for future bounds checks
    bf->code_size = code_size;

    bf->code_offset_base = 12 + bf->public_symbols_number * 2 * sizeof(u_int32_t) + bf->string_table_size;
    // DEBUG
//    printf("DEBUG:\nfile_size=%ld\ncode_size=%ld\ndata_size=%ld\n", file_size, code_size, data_size);

    return bf;
}

// Get string from the string table by index with bounds checking
static inline const char* get_string(const byte_file *f, u_int32_t pos) {
    if (pos >= f->string_table_size) {
        my_failure("String index out of bounds: pos=%u, string_table_size=%u\n",
                pos, f->string_table_size);
    }
    return f->string_ptr + pos;
}

// Get string from the string table by index with additional IP context
static inline const char* get_string_with_ip(const byte_file *f, u_int32_t pos, const char *ip) {
    if (pos >= f->string_table_size) {
        if (ip && f->code_ptr) {
            long offset = ip - f->code_ptr;  // ip points to the current instruction
            my_failure("String index out of bounds at offset %ld (0x%lx): "
                    "pos=%u, string_table_size=%u\n",
                    offset, offset, pos, f->string_table_size);
        } else {
            // Fallback if IP is not available (should not happen when called from interpreter)
            my_failure("String index out of bounds: pos=%u, string_table_size=%u\n",
                    pos, f->string_table_size);
        }
    }
    return f->string_ptr + pos;
}

// Get the name of a public symbol by index with bounds check
static inline const char* get_public_name(const byte_file *f, u_int32_t idx) {
    if (idx >= f->public_symbols_number) {
        my_failure("Public symbol index out of bounds: %u (public_symbols_number: %u)\n",
                idx, f->public_symbols_number);
    }
    return get_string(f, f->public_ptr[idx * 2]);
}

// Get the offset of a public symbol by index with bounds check
static inline u_int32_t get_public_offset(const byte_file *f, u_int32_t idx) {
    if (idx >= f->public_symbols_number) {
        my_failure("Public symbol index out of bounds: %u (public_symbols_number: %u)\n",
                idx, f->public_symbols_number);
    }
    return f->public_ptr[idx * 2 + 1];
}
