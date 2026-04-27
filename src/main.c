#include "string.h"
#include "assert.h"

#include "interpreter.h"
#include "byte_file.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        failure("Usage: %s <bytecode_file>\n", argv[0]);
    }

    byte_file *bf = read_file(argv[1]);
    init_interpreter(bf);
    interpret();
    free(bf);

    return 0;
}
