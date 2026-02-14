#include "byte_file.h"
#include "string.h"
#include "assert.h"
#include "interpreter.h"
#include "analyzer/analyzer.h"

int main(int argc, char *argv[]) {
    assert(argc == 3);
    byte_file *bf = read_file(argv[2]);
    if (strcmp(argv[1], "interpret") == 0) {
        init_interpreter(bf);
        interpret();
    } else if (strcmp(argv[1], "analyze") == 0) {
        analyze_bytecode_frequency(stdout, bf);
    }
    return 0;
}
