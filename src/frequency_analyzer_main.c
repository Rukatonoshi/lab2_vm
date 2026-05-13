// frequency_analyzer_main.c
// Main file for frequency analyzer tool

#include "byte_file.h"
#include "frequency_analyzer.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <bytecode_file>\n", argv[0]);
        return 1;
    }

    byte_file *bf = read_file(argv[1]);
    if (!bf) {
        fprintf(stderr, "Failed to read bytecode file: %s\n", argv[1]);
        return 1;
    }

    printf("Analyzing: %s\n", argv[1]);
    printf("Code size: %u bytes\n", bf->code_size);
    printf("Global area size: %u words\n", bf->global_area_size);
    printf("Public symbols: %u\n\n", bf->public_symbols_number);

    analyze_frequency(bf);

    free(bf);
    return 0;
}
