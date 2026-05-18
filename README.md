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

## Frequency analyzer (work #3)

Counts occurrences of 1–2 instruction sequences in Lama bytecode and prints them sorted by frequency.

To use instruction frequency analyzer, you need to make `frequency_analyzer` target:
```bash
make frequency_analyzer

./frequency_analyzer <bytecode_file>
```

### Algorithm

The analysis runs in five phases:

**Phase 1 - Reachability analysis.**  
BFS from public symbol entry points marks reachable addresses and jump targets.
Jump/call targets break sequence chains.

**Phase 2 - Count pass.**  
A dry run over reachable instructions counts the total number of sequence records without allocating anything.
This allows a single exact `malloc` in the next phase.

**Phase 3 - Collection.**  
A second pass populates a `SeqKey[]` array.
Each record stores only `(offset, len)` - a reference into the original bytecode buffer, not a copy of the bytes.
`count` is absent at this stage: every record represents exactly one occurrence and the final count is derived later from sort order.

**Phase 4 - Deduplication.**  
`qsort` by raw byte content groups identical sequences.
A linear scan collapses each run into a single `SeqResult` with `count = run length`.
Bitsets and `SeqKey[]` are freed before the results array is allocated.

**Phase 5 - Output.**  
Results are sorted by descending frequency and printed via the original bytecode buffer using the preserved `offset`.

### Memory Layout

Two separate structures are used to minimize peak memory:

```c
// Sorting phase only - no count field
typedef struct __attribute__((packed)) {
    uint32_t offset;   // 4 bytes
    uint16_t len;      // 2 bytes
} SeqKey;              // 6 bytes total

// After deduplication - unique sequences only
typedef struct __attribute__((packed)) {
    uint32_t offset;   // 4 bytes
    uint32_t count;    // 4 bytes
    uint16_t len;      // 2 bytes
} SeqResult;           // 10 bytes total
```

Reachability maps are stored as bitsets (1 bit per bytecode address) rather than byte arrays

### Peak Memory by Phase

```
Phase 1 - BFS (worklist alive):  
  reachable:    code_size / 8
  jump_targets: code_size / 8
  worklist:     code_size × 4
  -----------------------------
  Total:        4.25 × code_size

Phase 2  
no allocations

Phase 3 - SeqKey[] allocated, bitsets still alive:  
  reachable:    code_size / 8
  jump_targets: code_size / 8
  keys:         6 × 2 × code_size = 12 × code_size
  -------------------------------------------------
  Total:        12.25 × code_size  ← peak

Phase 4 - Both arrays alive briefly:  
  keys:         6 × total_seqs
  results:      10 × unique_count
  ------------------------------
  worst-case scenario:
  total_seqs:   2 * code_size / 5 (min len for instruction with operands) = 0.4 * code_size
  unique_count: 0.4 * code_size
  -----------------------------
  Total:        6.4 * code_size

Phase 5 - Output:  
  results:      10 × unique_count
```

<details>

<summary>Instruction frequency analyzer results for Sort.bc</summary>

```bash
Analyzing: performance/Sort.bc
Code size: 764 bytes
Global area size: 0 words
Public symbols: 1


--- Jump targets ---
    addr: 0x0000, file_offset: 0x001e - entry/jump target
    addr: 0x002b, file_offset: 0x0049 - entry/jump target
    addr: 0x006a, file_offset: 0x0088 - entry/jump target
    addr: 0x0074, file_offset: 0x0092 - entry/jump target
    addr: 0x0075, file_offset: 0x0093 - entry/jump target
    addr: 0x0097, file_offset: 0x00b5 - entry/jump target
    addr: 0x00bf, file_offset: 0x00dd - entry/jump target
    addr: 0x00c5, file_offset: 0x00e3 - entry/jump target
    addr: 0x0106, file_offset: 0x0124 - entry/jump target
    addr: 0x0112, file_offset: 0x0130 - entry/jump target
    addr: 0x0118, file_offset: 0x0136 - entry/jump target
    addr: 0x0150, file_offset: 0x016e - entry/jump target
    addr: 0x015e, file_offset: 0x017c - entry/jump target
    addr: 0x015f, file_offset: 0x017d - entry/jump target
    addr: 0x0182, file_offset: 0x01a0 - entry/jump target
    addr: 0x0188, file_offset: 0x01a6 - entry/jump target
    addr: 0x01ac, file_offset: 0x01ca - entry/jump target
    addr: 0x0258, file_offset: 0x0276 - entry/jump target
    addr: 0x027d, file_offset: 0x029b - entry/jump target
    addr: 0x02cb, file_offset: 0x02e9 - entry/jump target
    addr: 0x02de, file_offset: 0x02fc - entry/jump target
    addr: 0x02fa, file_offset: 0x0318 - entry/jump target

--- Reachability Stats ---
Total code size: 764 bytes
Reachable instructions: 204 bytes
Reachable code: 26.70%

--- Public Functions ---
Function 0: main at addr 0x00000000 (file offset 0x0000001e)

31 : DROP
28 : DUP
21 : ELEM
16 : CONST 1
13 : CONST 1, ELEM
11 : CONST 0
11 : DROP, DUP
11 : DUP, CONST 1
10 : DROP, DROP
8 : CONST 0, ELEM
7 : DUP, CONST 0
7 : ELEM, DROP
7 : LD_ARGUMENT 0
5 : END
4 : SEXP 0 2
4 : DUP, DUP
3 : JMP 762
3 : DUP, ARRAY 2
3 : ELEM, ST_LOCAL 0
3 : LD_LOCAL 0
3 : LD_LOCAL 3
3 : ST_LOCAL 0
3 : ST_LOCAL 0, DROP
3 : CALL 351 1
3 : ARRAY 2
3 : CALL_ARRAY 2
3 : CALL_ARRAY 2, JMP 762
2 : BINOP_EQ
2 : SEXP 0 2, CALL_ARRAY 2
2 : JMP 350
2 : JMP 116
2 : DUP, TAG 0 2
2 : ELEM, CONST 0
2 : ELEM, CONST 1
2 : LD_LOCAL 1
2 : BEGIN 1 0
2 : CALL 43 1
2 : CALL 151 1
2 : TAG 0 2
1 : BINOP_SUB
1 : BINOP_SUB, CALL 43 1
1 : BINOP_GT
1 : BINOP_GT, CJMP_ZERO 600
1 : BINOP_EQ, CJMP_ZERO 274
1 : BINOP_EQ, CJMP_ZERO 191
1 : CONST 0, BINOP_EQ
1 : CONST 0, JMP 116
1 : CONST 0, LINE 9
1 : CONST 1, BINOP_SUB
1 : CONST 1, BINOP_EQ
1 : CONST 1, LINE 6
1 : CONST 1000
1 : CONST 1000, CALL 43 1
1 : SEXP 0 2, JMP 116
1 : SEXP 0 2, CALL 351 1
1 : JMP 262
1 : JMP 336
1 : JMP 386
1 : JMP 715
1 : JMP 734
1 : DROP, CONST 0
1 : DROP, JMP 262
1 : DROP, JMP 336
1 : DROP, JMP 386
1 : DROP, JMP 715
1 : DROP, JMP 734
1 : DROP, LD_LOCAL 5
1 : DROP, LINE 5
1 : DROP, LINE 15
1 : DROP, LINE 16
1 : DUP, DROP
1 : ELEM, SEXP 0 2
1 : ELEM, DUP
1 : ELEM, ST_LOCAL 1
1 : ELEM, ST_LOCAL 2
1 : ELEM, ST_LOCAL 3
1 : ELEM, ST_LOCAL 4
1 : ELEM, ST_LOCAL 5
1 : LD_LOCAL 0, SEXP 0 2
1 : LD_LOCAL 0, JMP 350
1 : LD_LOCAL 0, CALL 151 1
1 : LD_LOCAL 1, BINOP_GT
1 : LD_LOCAL 1, LD_LOCAL 3
1 : LD_LOCAL 2
1 : LD_LOCAL 2, CALL 351 1
1 : LD_LOCAL 3, LD_LOCAL 0
1 : LD_LOCAL 3, LD_LOCAL 1
1 : LD_LOCAL 3, LD_LOCAL 4
1 : LD_LOCAL 4
1 : LD_LOCAL 4, SEXP 0 2
1 : LD_LOCAL 5
1 : LD_LOCAL 5, LD_LOCAL 3
1 : LD_ARGUMENT 0, CONST 1
1 : LD_ARGUMENT 0, DUP
1 : LD_ARGUMENT 0, LD_ARGUMENT 0
1 : LD_ARGUMENT 0, CJMP_ZERO 106
1 : LD_ARGUMENT 0, CALL 351 1
1 : LD_ARGUMENT 0, CALL 151 1
1 : LD_ARGUMENT 0, CALL_ARRAY 2
1 : ST_LOCAL 1
1 : ST_LOCAL 1, DROP
1 : ST_LOCAL 2
1 : ST_LOCAL 2, DROP
1 : ST_LOCAL 3
1 : ST_LOCAL 3, DROP
1 : ST_LOCAL 4
1 : ST_LOCAL 4, DROP
1 : ST_LOCAL 5
1 : ST_LOCAL 5, DROP
1 : CJMP_ZERO 274
1 : CJMP_ZERO 600
1 : CJMP_ZERO 106
1 : CJMP_ZERO 191
1 : CJMP_NOT_ZERO 280
1 : CJMP_NOT_ZERO 637
1 : CJMP_NOT_ZERO 392
1 : CJMP_NOT_ZERO 428
1 : CJMP_NOT_ZERO 197
1 : BEGIN 1 0, LINE 18
1 : BEGIN 1 0, LINE 24
1 : BEGIN 1 1
1 : BEGIN 1 1, LINE 14
1 : BEGIN 1 6
1 : BEGIN 1 6, LINE 3
1 : BEGIN 2 0
1 : BEGIN 2 0, LINE 25
1 : CALL 117 1
1 : TAG 0 2, CJMP_NOT_ZERO 392
1 : TAG 0 2, CJMP_NOT_ZERO 428
1 : ARRAY 2, CJMP_NOT_ZERO 280
1 : ARRAY 2, CJMP_NOT_ZERO 637
1 : ARRAY 2, CJMP_NOT_ZERO 197
1 : FAIL 7 17
1 : FAIL 14 9
1 : LINE 3
1 : LINE 3, LD_ARGUMENT 0
1 : LINE 5
1 : LINE 5, LD_LOCAL 3
1 : LINE 6
1 : LINE 6, LD_LOCAL 1
1 : LINE 7
1 : LINE 7, LD_LOCAL 2
1 : LINE 9
1 : LINE 9, LD_ARGUMENT 0
1 : LINE 14
1 : LINE 14, LD_ARGUMENT 0
1 : LINE 15
1 : LINE 15, LD_LOCAL 0
1 : LINE 16
1 : LINE 16, LD_LOCAL 0
1 : LINE 18
1 : LINE 18, LINE 20
1 : LINE 20
1 : LINE 20, LD_ARGUMENT 0
1 : LINE 24
1 : LINE 24, LD_ARGUMENT 0
1 : LINE 25
1 : LINE 25, LINE 27
1 : LINE 27
1 : LINE 27, CONST 1000
```

</details>
