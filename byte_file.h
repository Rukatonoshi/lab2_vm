#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "runtime/runtime.h"

typedef struct {
    char *string_ptr;
    u_int32_t *public_ptr;
    char *code_ptr;
    u_int32_t *global_ptr;
    u_int32_t string_table_size;
    u_int32_t global_area_size;
    u_int32_t public_symbols_number;
    char buffer[0];
} byte_file;

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
    bf = (byte_file *) malloc(sizeof(int) * 4 + file_size);

    if (bf == 0) {
        failure("Severity ERROR: unable to allocate memory for byte_file.\n");
    }

    rewind(f);

    if (file_size != fread(&bf->string_table_size, 1, file_size, f)) {
        failure("%s\n", strerror(errno));
    }

    fclose(f);

    bf->string_ptr = &bf->buffer[bf->public_symbols_number * 2 * sizeof(int)];
    bf->public_ptr = (u_int32_t *) bf->buffer;
    bf->code_ptr = (char *) &bf->string_ptr[bf->string_table_size];
    bf->global_ptr = (u_int32_t *) malloc(bf->global_area_size * sizeof(int));
    return bf;
}
