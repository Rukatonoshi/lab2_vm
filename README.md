# Lama interpreter

## Iterative bytecode interpreter of the [Lama](https://github.com/PLTools/Lama.git) language

## Build
In the project root directory execute:
```bash
make
```

## Run interpreter
In the project root directory run compile version:
```bash
./lama-interpreter <path_to_bc_file>
```

Example for lama bubble-sort:
```bash
./lama-interpreter performance/Sort.bc
```

To generate lama bytecode execute:
```bash
lamac -b <path_to_lama_file>
```

Example for lama bubble-sort:
```bash
lamac -b performance/Sort.lama
```

## Performance comparison

* 2.98s - Lama recursive interpreter
* 2.14s - Iterative bytecode interpreter

Example of time measurement for iterative bytecode interpreter:
```bash
time ./lama-interpreter performance/Sort.bc
```

Example of time measurement for Lama recursive interpreter (bench.input - empty file):
```bash
time lamac -i performance/Sort.lama < performance/bench.input
```

## Tests

To run tests, you can use sh script:
```bash
./run-tests.sh
```

Tests were taken from Lama repository.
Script will automatically check if Lama repository exists and `lamac` will be used to generate Lama bytecode.
The output of `lama-interperter` and `lamac -i` will be compared.

Example of passed test:
```bash
Running regression/test802.lama...
1
2
3
4
5
6
7
8
9
10
test passed
```

Example of failed test:
```bash
Running regression/test803.lama...
1
2
3
4
5
6
Runtime error at offset 469 (0x1d5): Lwrite expected integer, got closure
test failed! expected output:
Fatal error: exception Failure("int value expected (Closure ([\"unit\"], <not supported>, <not supported>))\n")
```
