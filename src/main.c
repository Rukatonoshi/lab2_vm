#include "string.h"
#include "assert.h"

#include "interpreter.h"
#include "byte_file.h"
#include "frequency_analyzer.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        failure("Usage: %s [analyze] <bytecode_file>\n", argv[0]);
    }

    if (strcmp(argv[1], "analyze") == 0) {
        byte_file *bf = read_file(argv[2]);
        analyze_frequency(bf);
        free(bf);
    } else {
        byte_file *bf = read_file(argv[1]);
        init_interpreter(bf);
        interpret();
        free(bf);
    }
    return 0;
}
