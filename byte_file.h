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
    FILE *f = fopen(file_name, "rb");
    byte_file *bf;

    if (f == 0) {
        failure("%s\n", strerror(errno));
    }

    if (fseek(f, 0, SEEK_END) == -1) {
        failure("%s\n", strerror(errno));
    }

    long file_size = ftell(f);

    if (file_size > INT_MAX - (long)sizeof(int) * 4) {
        failure("File is too big!\nSize: %ld bytes\nMax: %ld\n",
                file_size, INT_MAX - (long)sizeof(int) * 4);
    }

    size_t buffer_size = sizeof(int) * 4 + file_size;
    bf = (byte_file *) malloc(buffer_size);

    if (bf == 0) {
        failure("Severity ERROR: unable to allocate memory for byte_file.\n");
    }

    rewind(f);

    if (file_size != fread(&bf->string_table_size, 1, file_size, f)) {
        free(bf);
        failure("%s\n", strerror(errno));
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

    // Publics table
    // TODO types of vars
    size_t public_table_size = bf->public_symbols_number * 2 * sizeof(int);
    if (bf->buffer + public_table_size > (char *) bf + buffer_size) {
        free(bf);
        failure("Publics symbols table exceeds file bounds\n");
    }
    bf->public_ptr = (u_int32_t *) bf->buffer;

    // String table
    bf->string_ptr = &bf->buffer[public_table_size];
    //TODO bf + buffer_size
    if(bf->string_ptr + bf->string_table_size > (char *) bf + buffer_size) {
        free(bf);
        failure("String table exceeds file bounds\n");
    }

    // Bytecode block
    bf->code_ptr = (char *) &bf->string_ptr[bf->string_table_size];
    if (bf->code_ptr > (char *) bf + buffer_size) {
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
