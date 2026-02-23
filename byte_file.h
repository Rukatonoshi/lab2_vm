#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runtime/runtime.h"

// The unpacked representation of bytecode file
typedef struct {
    char *string_ptr;                // A pointer to the beginning of the string table
    u_int32_t *public_ptr;           // A pointer to the beginning of publics table
    char *code_ptr;                  // A pointer to the bytecode itself
    u_int32_t string_table_size;     // The size of the string table (byte)
    u_int32_t global_area_size;      // The size of global area (word)
    u_int32_t public_symbols_number; // The number of public symbols
    u_int32_t code_size;             // The size of the bytecode (byte)
    char buffer[0];
} byte_file;

// Reads a bynary bytecode file by name and runs some checks
byte_file *read_file(char *file_name) {
    // 2GB file size limitation for fopen
    FILE *f = fopen(file_name, "rb");

    // Check if file could be opened
    if (f == NULL) {
        failure("failed to open file: %s\n", strerror(errno));
    }

    // Check for non negative offset
    if (fseek(f, 0, SEEK_END) < 0) {
        failure("%s\n", strerror(errno));
    }

    long file_size = ftell(f);
    if (file_size == -1) {
        failure("ftell failed: %s\n", strerror(errno));
    }

    // Additional check for file size
    if (file_size > INT_MAX - (long) sizeof(int) * 4) {
        failure("File is too big!\nSize: %ld bytes\nMax: %ld\n",
                file_size, INT_MAX - (long) sizeof(int) * 4);
    }

    // Rewind and read header (first three 32-bit values)
    rewind(f);
    u_int32_t header[3];
    if (fread(header, sizeof(u_int32_t), 3, f) != 3) {
        failure("Failed to read header: %s\n", strerror(errno));
    }

    u_int32_t string_table_size = header[0];
    u_int32_t global_area_size = header[1];
    u_int32_t public_symbols_number = header[2];

    // Sanity checks for header values
    if (string_table_size > 100 * 1024 * 1024 ||  // 100 MB
        global_area_size  > 10 * 1024 * 1024 ||   // 10 million words
        public_symbols_number > 1000000)          // 1 million symbols
    {
        failure("Header values too large: string_table=%u, global_area=%u, publics=%u\n",
                string_table_size, global_area_size, public_symbols_number);
    }

    // Checks for header fields values
    if (string_table_size < 0 ||
        public_symbols_number < 0 ||
        global_area_size < 0)
    {
        failure("Negative values in header:\nString table = %ld\nPublic syms = %ld\nGlobal area = %ld\n",
                string_table_size, public_symbols_number, global_area_size);
    }

    // Compute sizes
    size_t public_table_size = (size_t) public_symbols_number * 2 * sizeof(u_int32_t);
    size_t data_size = public_table_size + string_table_size;
    if (file_size < (long) (3 * sizeof(u_int32_t) + data_size)) {
        failure("File truncated: expected at least %zu bytes, got %ld\n",
                3 * sizeof(u_int32_t) + data_size, file_size);
    }
    u_int32_t code_size = file_size - (3 * sizeof(u_int32_t) + data_size);

    // Allocate memory for byte_file structure plus the data buffer
    byte_file *bf = (byte_file *)malloc(sizeof(byte_file) + data_size + code_size);

    if (bf == NULL) {
        failure("Unable to allocate memory for byte_file\n");
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
        failure("Failed to read data: %s\n", strerror(errno));
    }
    fclose(f);

    // Check that pointers stay within allocated buffer
    char *buffer_end = (char*) bf + sizeof(byte_file) + data_size + code_size;
    if (bf->public_ptr > (u_int32_t*)buffer_end ||
        bf->string_ptr > buffer_end ||
        bf->code_ptr > buffer_end)
    {
        free(bf);
        failure("Internal error: pointers exceed buffer bounds\n");
    }

    // Store code_size in structure for future bounds checks
    bf->code_size = code_size;
    // DEBUG
//    printf("DEBUG:\nfile_size=%ld\ncode_size=%ld\ndata_size=%ld\n", file_size, code_size, data_size);

    return bf;
}

// Get string from the string table by index with bounds checking
static inline const char* get_string(const byte_file *f, u_int32_t pos) {
    if (pos >= f->string_table_size) {
        failure("String index out of bounds: pos=%u, string_table_size=%u\n",
                pos, f->string_table_size);
    }
    return f->string_ptr + pos;
}

// Get string from the string table by index with additional IP context
static inline const char* get_string_with_ip(const byte_file *f, u_int32_t pos, const char *ip) {
    if (pos >= f->string_table_size) {
        if (ip && f->code_ptr) {
            long offset = ip - f->code_ptr;  // ip points to the current instruction
            failure("String index out of bounds at offset %ld (0x%lx): "
                    "pos=%u, string_table_size=%u\n",
                    offset, offset, pos, f->string_table_size);
        } else {
            // Fallback if IP is not available (should not happen when called from interpreter)
            failure("String index out of bounds: pos=%u, string_table_size=%u\n",
                    pos, f->string_table_size);
        }
    }
    return f->string_ptr + pos;
}

// Get the name of a public symbol by index with bounds check
static inline const char* get_public_name(const byte_file *f, u_int32_t idx) {
    if (idx >= f->public_symbols_number) {
        failure("Public symbol index out of bounds: %u (public_symbols_number: %u)\n",
                idx, f->public_symbols_number);
    }
    return get_string(f, f->public_ptr[idx * 2]);
}

// Get the offset of a public symbol by index with bounds check
static inline u_int32_t get_public_offset(const byte_file *f, u_int32_t idx) {
    if (idx >= f->public_symbols_number) {
        failure("Public symbol index out of bounds: %u (public_symbols_number: %u)\n",
                idx, f->public_symbols_number);
    }
    return f->public_ptr[idx * 2 + 1];
}
