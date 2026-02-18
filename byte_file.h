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
    u_int32_t *global_ptr;           // A pointer to the global area
    u_int32_t string_table_size;     // The size of the string table (byte)
    u_int32_t global_area_size;      // The size of global area (word)
    u_int32_t public_symbols_number; // The number of public symbols
    char buffer[0];
} byte_file;

// Reads a bynary bytecode file by name and runs some checks
byte_file *read_file(char *file_name) {
    // 2GB file size limitation for fopen
    FILE *f = fopen(file_name, "rb");
    byte_file *bf;

    // Check if file could be opened
    if (f == NULL) {
        failure("failed to open file: %s\n", strerror(errno));
    }

    // Check for non negative offset
    if (fseek(f, 0, SEEK_END) < 0) {
        failure("%s\n", strerror(errno));
    }

    long file_size = ftell(f);

    // Additional check for file size
    if (file_size > INT_MAX - (long)sizeof(int) * 4) {
        failure("File is too big!\nSize: %ld bytes\nMax: %ld\n",
                file_size, INT_MAX - (long)sizeof(int) * 4);
    }

    size_t buffer_size = sizeof(int) * 4 + file_size;
    bf = (byte_file *) malloc(buffer_size);

    if (bf == NULL) {
        failure("Severity ERROR: unable to allocate memory for byte_file.\n");
    }

    rewind(f);

    if (file_size != fread(&bf->string_table_size, 1, file_size, f)) {
        free(bf);
        failure("fread failed: %s\n", strerror(errno));
    }

    fclose(f);

    // Checks for header fields values
    if (bf->string_table_size < 0 ||
        bf->public_symbols_number < 0 ||
        bf->global_area_size < 0)
    {
        free(bf);
        failure("Negative values in header:\nString table = %ld\nPublic syms = %ld\nGlobal area = %ld\n",
                bf->string_table_size, bf->public_symbols_number, bf->global_area_size);
    }

    char *buffer_end = (char *) bf + buffer_size;

    // Public symbols table
    size_t public_table_size = bf->public_symbols_number * 2 * sizeof(int);
    if (bf->buffer + public_table_size > buffer_end) {
        free(bf);
        failure("Publics symbols table exceeds file bounds\n");
    }
    bf->public_ptr = (u_int32_t *) bf->buffer;

    // Strings table
    bf->string_ptr = &bf->buffer[public_table_size];
    if(bf->string_ptr + bf->string_table_size > buffer_end) {
        free(bf);
        failure("String table exceeds file bounds\n");
    }

    // Bytecode block
    bf->code_ptr = (char *) &bf->string_ptr[bf->string_table_size];
    if (bf->code_ptr >= buffer_end || bf->code_ptr < (char *) bf) {
        free(bf);
        failure("Bytecode block exceeds file bounds\n");
    }

    // Global area
    bf->global_ptr = (u_int32_t *) malloc(bf->global_area_size * sizeof(int));
    if (!bf->global_ptr && bf->global_area_size > 0) {
        free(bf);
        failure("Error: failed to allocate memory for global area\n");
    }

    return bf;
}

//CHECK WIP TODO
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
