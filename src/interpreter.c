#include "interpreter.h"
//#include <stdio.h>
//#include <stdlib.h>
//#include <stdarg.h>
//#include <string.h>

static size_t RUNTIME_VSTACK_SIZE = 1024 * 1024;
static u_int32_t *stack_fp;
static u_int32_t *stack_start;
static u_int32_t current_frame_locals;

void *__start_custom_data;
void *__stop_custom_data;
interpreter_state interpreterState;

// Verbose description of error and code locations
static void runtime_error(const char *fmt, ...) {
    // Offset of current instruction
    long offset = interpreterState.ip - interpreterState.code_start - 1;
    fprintf(stderr, "Runtime error at offset %ld (0x%lx): ", offset, offset);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

// Check that we have required bytes remaining in the code section
static inline void check_code_bounds(size_t bytes_to_read) {
    if (interpreterState.ip + bytes_to_read > interpreterState.code_end) {
        runtime_error("Requested value is out of bounds:\nip=%p\nbytes=%zu\ncode_end=%p",
                      *interpreterState.ip, bytes_to_read, *interpreterState.code_end);
    }
}

// Read next byte
static inline u_int8_t get_next_byte() {
    check_code_bounds(1);
    return *interpreterState.ip++;
}

// Read next integer
static inline u_int32_t get_next_int() {
    check_code_bounds(sizeof(int));
    interpreterState.ip += sizeof(int);
    return *(u_int32_t *) (interpreterState.ip - sizeof(int));
}

static inline char *get_next_string() {
    u_int32_t offset = get_next_int(); //check inside func
    return (char*) get_string_with_ip(interpreterState.byteFile, offset, interpreterState.ip); //check in func
}

static inline void vstack_push(u_int32_t value) {
    if (stack_start == __gc_stack_top) {
        runtime_error("ERROR: Virtual stack limit exceeded.");
    }
    *(--__gc_stack_top) = value;
}

static inline u_int32_t vstack_pop() {
    if (__gc_stack_top >= stack_fp) {
        runtime_error("ERROR: Illegal pop.");
    }
    return *(__gc_stack_top++);
}

static inline void copy_on_stack(u_int32_t value, int count) {
    for (int i = 0; i < count; ++i) {
        vstack_push(value);
    }
}

static inline void reverse_on_stack(int count) {
    u_int32_t *st = __gc_stack_top;
    u_int32_t *arg = st + count - 1;
    while (st < arg) {
        u_int32_t tmp = *st;
        *st = *arg;
        *arg = tmp;
        st++;
        arg--;
    }
}

static u_int32_t *get_by_loc(u_int8_t bytecode, u_int32_t value) {
    switch (low_bits(bytecode)) {
        case L_GLOBAL:
            if (value >= interpreterState.byteFile->global_area_size) {
                runtime_error("Global index %u out of bounds (size %u)",
                              value, interpreterState.byteFile->global_area_size);
            }
            return interpreterState.globals_base + value;
        case L_LOCAL:
            if (value >= current_frame_locals) {
                runtime_error("Local index %u out of bounds (current frame has %u locals)",
                              value, current_frame_locals);
            }
            return stack_fp - value - 2;
        case L_ARGUMENT:
            u_int32_t n_args = *(stack_fp + 1);
            if (value >= n_args) {
                runtime_error("Argument index %u out of bounds (current call has %u args)",
                              value, n_args);
            }
            return stack_fp + value + 3;
        case L_CLOSURE: {
            u_int32_t n_args = *(stack_fp + 1);
            u_int32_t *argument = stack_fp + n_args + 2;
            u_int32_t *closure_val = (u_int32_t *) *argument;
            if (closure_val == NULL) {
                runtime_error("CLOSURE: null closure encountered");
            }
            // Check if it's closure
            data *d = TO_DATA((void *) closure_val);
            if (TAG(d->tag) != CLOSURE_TAG) {
                runtime_error("CLOSURE: object is not a closure");
            }
            // Closure size with entry
            u_int32_t total_words = d->tag >> 3; // n+1, n - num of captured variables
            u_int32_t n_captured = total_words - 1;
            if (value >= n_captured) {
                runtime_error("CLOSURE: index %u out of bounds (captured variables: %u)",
                              value, n_captured);
            }
            return (u_int32_t *) Belem_link((void *) closure_val, BOX(value + 1));
        }
        default:
            runtime_error("Invalid location type %d", low_bits(bytecode));
    }

    // Should not reach
    return NULL;
}

static void jump(u_int32_t ip_offset) {
    char * jmp_addr = interpreterState.code_start + ip_offset;
    if (jmp_addr < interpreterState.code_start || jmp_addr >= interpreterState.code_end) {
        runtime_error("Jump address %p points outside of code section [%p, %p)",
                (void *) jmp_addr, (void *) interpreterState.code_end, (void *) interpreterState.code_end);
    }
    interpreterState.ip = jmp_addr;
}

void exec_binop(u_int8_t bytecode) {
    u_int32_t b_val = vstack_pop();
    u_int32_t a_val = vstack_pop();
    u_int8_t op = low_bits(bytecode);

    // Check if operands type is integer
    int a_is_int = UNBOXED(a_val);
    int b_is_int = UNBOXED(b_val);

    // For EQUAL, one of the operands must be an integer. Integers are never equal to values of other types.
    if (op == EQUAL) {
        if (a_is_int && b_is_int) {
            int a = UNBOX(a_val);
            int b = UNBOX(b_val);
            vstack_push(BOX(a == b));
        } else if (a_is_int || b_is_int) {
            vstack_push(BOX(0));
        } else {
            runtime_error("BINOP EQUAL called with two non-integer arguments: %s and %s",
                          type_name(a_val), type_name(b_val));
        }

        return;
    }

    // Check if both operands are integers indeed
    if (!a_is_int || !b_is_int) {
        runtime_error("BINOP expected integers, got %s and %s", type_name(a_val), type_name(b_val));
    }

    int a = UNBOX(a_val);
    int b = UNBOX(b_val);
    int result;

    switch (op) {
        case PLUS:          result = a + b; break;
        case MINUS:         result = a - b; break;
        case MULTIPLY:      result = a * b; break;
        case DIVIDE:
            if (b == 0) runtime_error("Division by zero: a=%d, b=0", a);
            result = a / b;
            break;
        case REMAINDER:
            if (b == 0) runtime_error("Remainder by zero: a=%d, b=0", a);
            result = a % b;
            break;
        case LESS:          result = a < b; break;
        case LESS_EQUAL:    result = a <= b; break;
        case GREATER:       result = a > b; break;
        case GREATER_EQUAL: result = a >= b; break;
        case EQUAL:         result = a == b; break;
        case NOT_EQUAL:     result = a != b; break;
        case AND:           result = a && b; break;
        case OR:            result = a || b; break;
        default:
            runtime_error("Unknown binop bytecode: %d", low_bits(bytecode));
    }

    vstack_push(BOX(result));
}

void exec_ld(u_int8_t bytecode) {
    u_int32_t index = get_next_int();
    u_int32_t value = *get_by_loc(bytecode, index);
    vstack_push(value);
}

void exec_lda(u_int8_t bytecode) {
    u_int32_t index = get_next_int();
    u_int32_t value = (u_int32_t) get_by_loc(bytecode, index);
    vstack_push(value);
}

void exec_st(u_int8_t bytecode) {
    u_int32_t index = get_next_int();
    u_int32_t value = vstack_pop();
    *get_by_loc(bytecode, index) = value;
    vstack_push(value);
}

void exec_patt(u_int8_t bytecode) {
    u_int32_t *element = (u_int32_t *) vstack_pop();
    u_int32_t result = -1;
    switch (low_bits(bytecode)) {
        case PATT_STR: {
            result = Bstring_patt(element, (u_int32_t *) vstack_pop());
            break;
        }
        case PATT_TAG_STR: {
            result = Bstring_tag_patt(element);
            break;
        }
        case PATT_TAG_ARR: {
            result = Barray_tag_patt(element);
            break;
        }
        case PATT_TAG_SEXP: {
            result = Bsexp_tag_patt(element);
            break;
        }
        case PATT_BOXED: {
            result = Bboxed_patt(element);
            break;
        }
        case PATT_UNBOXED: {
            result = Bunboxed_patt(element);
            break;
        }
        case PATT_TAG_CLOSURE: {
            result = Bclosure_tag_patt(element);
            break;
        }
        default: {
            runtime_error("ERROR: Unknown pattern type.\n");
        }
    }
    vstack_push(result);
}

void exec_const() {
    u_int32_t const_v = BOX(get_next_int());
    vstack_push(const_v);
}

void exec_string() {
    char *string = get_next_string();
    vstack_push((u_int32_t) Bstring(string));
}

void exec_sexp() {
    char *sexp_name = get_next_string();
    u_int32_t sexp_tag = LtagHash(sexp_name);
    u_int32_t sexp_arity = get_next_int();
    reverse_on_stack(sexp_arity);
    u_int32_t bsexp = (u_int32_t) Bsexp_my(BOX(sexp_arity + 1), sexp_tag, (int *) __gc_stack_top);
    __gc_stack_top += sexp_arity;
    vstack_push(bsexp);
}

void exec_sta() {
    u_int32_t value = vstack_pop();
    int32_t idx_val = vstack_pop(); //signed

    // The operation is overloaded;
    // its behavior depends on the second-to-top value on the stack, which must be either
    // a reference to a variable or an integer
    if (!UNBOXED(idx_val)) {
        // Second-to-top value is a referene
        vstack_push((u_int32_t) Bsta((void *) value, idx_val, 0));
        return;
    }

    // Check if obj type is aggregative (string/array/sexp)
    u_int32_t obj = vstack_pop();
    if (!is_aggregative(obj)) {
        runtime_error("STA expected aggregative (string/array/sexp), got %s",
                      type_name(value));
    }

    // Index must be positive
    if (idx_val < 0) {
        runtime_error("STA index cannot be negative: %d", idx_val);
    }

    // Get len of obj and check if it positive
    int len = Llength((void*)obj);
    if (len < 0) {
        runtime_error("STA: cannot determine length of object type %s",
                      type_name(obj));
    }

    // Check idx bounds
    if (idx_val >= len) {
        runtime_error("STA index %d out of bounds (length %d)", idx_val, len);
    }

    u_int32_t result = (u_int32_t) Bsta((void*)value, idx_val, (void*)obj);
    vstack_push(result);
}

void exec_jmp() {
    u_int32_t ip_offset = get_next_int();
    jump(ip_offset);
}

void exec_cjmp_z() {
    u_int32_t ip_offset = get_next_int();
    int cmp_value = vstack_pop();

    if (!UNBOXED(cmp_value)) {
        runtime_error("Wrong jump condition type: expected integer, got %s", type_name(cmp_value));
    }

    if (UNBOX(cmp_value) == 0) {
        jump(ip_offset);
    }
}

void exec_cjmp_nz() {
    u_int32_t ip_offset = get_next_int();
    int cmp_value = vstack_pop();

    if (!UNBOXED(cmp_value)) {
        runtime_error("Wrong jump condition type: expected integer, got %s", type_name(cmp_value));
    }

    if (UNBOX(cmp_value) != 0) {
        jump(ip_offset);
    }
}

void exec_call_read() {
    int r = Lread();
    vstack_push(r);
}

void exec_call_write() {
    u_int32_t arg = vstack_pop();
    // Check if integer
    if (!UNBOXED(arg)) {
        runtime_error("Lwrite expected integer, got %s", type_name(arg));
    }
    int w = Lwrite((int) arg);
    vstack_push(w);
}

void exec_call_string() {
    u_int32_t s = (u_int32_t) Lstring((void *) vstack_pop());
    vstack_push(s);
}

void exec_call_length() {
    u_int32_t arg = vstack_pop();
    if (!is_aggregative(arg)) {
        runtime_error("Llength expected string, array or sexp, got %s", type_name(arg));
    }
    u_int32_t l = (u_int32_t) Llength((void *) arg);
    vstack_push(l);
}

void exec_call_array() {
    u_int32_t len = get_next_int();
    reverse_on_stack(len);
    u_int32_t result = (u_int32_t) Barray_my(BOX(len), (int *) __gc_stack_top);
    __gc_stack_top += len;
    vstack_push(result);
}

void exec_closure() {
    u_int32_t ip = get_next_int();
    u_int32_t bn = get_next_int();
    u_int32_t *values = (u_int32_t *) malloc(bn * sizeof(u_int32_t));
    if (!values) {
        runtime_error("CLOSURE: out of memory while allocating %u captured values", bn);
    }

    for (u_int32_t i = 0; i < bn; ++i) {
        u_int8_t b = (u_int8_t) get_next_byte();
        u_int32_t value = (u_int32_t) get_next_int();
        values[i] = *get_by_loc(b, value);
    }

    u_int32_t bclosure = (u_int32_t) Bclosure_my(BOX(bn), interpreterState.byteFile->code_ptr + ip, (int*) values);
    free(values);
    vstack_push(bclosure);
}

void exec_elem() {
    int32_t index = vstack_pop(); //signed
    void *obj = (void *) vstack_pop();

    if (!is_aggregative((u_int32_t) obj)) {
        runtime_error("ELEM expected aggregative (string/array/sexp), got %s",
                      type_name((u_int32_t) obj));
    }

    if (!UNBOXED(index)) {
        runtime_error("ELEM index must be integer, got %s",
                      type_name(index));
    }

    // Index must be positive
    if (index < 0) {
        runtime_error("ELEM index cannot be negative: %d", index);
    }

    // Get len of obj and check if it positive
    int len = Llength(obj);
    if (len < 0) {
        runtime_error("ELEM: cannot determine length of object type %s",
                      type_name((u_int32_t) obj));
    }

    // Check idx bounds
    if (index >= len) {
        runtime_error("ELEM index %d out of bounds (length %d)", index, len);
    }

    u_int32_t belem = (u_int32_t) Belem(obj, index);
    vstack_push(belem);
}

void exec_begin() {
    int32_t n_args = get_next_int();   // signed
    int32_t n_locals = get_next_int(); // signed

    if (n_args < 0) runtime_error("ERROR: BEGIN has negative number of arguments: %d", n_args);
    if (n_locals < 0) runtime_error("ERROR: BEGIN has negative number of locals: %d", n_locals);
    vstack_push((u_int32_t) stack_fp);
    vstack_push(current_frame_locals);
    //DEBUG
    //fprintf(stdout, "BEGIN: current_frame_locals = %u\n", current_frame_locals);
    stack_fp = __gc_stack_top + 1;

    // Current frame locals
    current_frame_locals = n_locals;

    // Init space for new locals
    copy_on_stack(BOX(0), n_locals);
}

void exec_cbegin() {
    int32_t n_args = get_next_int(); // signed
    int32_t n_locals = get_next_int(); // signed

    if (n_args < 0) runtime_error("ERROR: BEGIN has negative number of arguments: %d", n_args);
    if (n_locals < 0) runtime_error("ERROR: BEGIN has negative number of locals: %d", n_locals);

    vstack_push((u_int32_t) stack_fp);
    vstack_push(current_frame_locals);
    //DEBUG
    //fprintf(stdout, "CBEGIN current_frame_locals = %u\n", current_frame_locals);

    stack_fp = __gc_stack_top + 1;
    current_frame_locals = n_locals;
    copy_on_stack(BOX(0), n_locals);
}

void exec_end() {
    u_int32_t return_value = vstack_pop();

    u_int32_t saved_locals = *(stack_fp - 1);
    current_frame_locals = saved_locals;

    __gc_stack_top = stack_fp;

    u_int32_t prev_fp = *(__gc_stack_top++);
    stack_fp = (u_int32_t*)prev_fp;

    u_int32_t n_args = vstack_pop();
    char *addr = (char*)vstack_pop();

    __gc_stack_top += n_args;

    vstack_push(return_value);
    interpreterState.ip = addr;
}

void exec_drop() {
    vstack_pop();
}

void exec_dup() {
    copy_on_stack(vstack_pop(), 2);
}

void exec_tag() {
    char *tag_name = get_next_string();
    u_int32_t n = get_next_int();
    u_int32_t t = LtagHash(tag_name);
    void *d = (void *) vstack_pop();
    vstack_push(Btag(d, t, BOX(n)));
}

void exec_array() {
    u_int32_t len = get_next_int();
    u_int32_t array = Barray_patt((u_int32_t *) vstack_pop(), BOX(len));
    vstack_push(array);
}

void exec_fail() {
    u_int32_t a = get_next_int();
    u_int32_t b = get_next_int();
    runtime_error("ERROR: Failed executing FAIL %d %d.", a, b);
}

void exec_line() {
    get_next_int();
}

void exec_swap() {
    reverse_on_stack(2);
}

void exec_call() {
    u_int32_t call_offset = get_next_int();
    u_int32_t n_args = get_next_int();
    reverse_on_stack(n_args);
    vstack_push((u_int32_t) interpreterState.ip);
    vstack_push(n_args);
    interpreterState.ip = interpreterState.byteFile->code_ptr + call_offset;
}

void exec_callc() {
    u_int32_t n_args = get_next_int();

    // Stack should have at least n arguments + closure itself
    if (stack_fp - __gc_stack_top < n_args + 1) {
        runtime_error("CALLC: stack underflow: need %d args + closure, but only %d elements available",
                      n_args, (int)(stack_fp - __gc_stack_top));
    }

    // Closure is stored below args (n_args from top)
    u_int32_t closure_val = __gc_stack_top[n_args];
    if (!is_closure(closure_val)) {
        runtime_error("CALLC: first operand must be a closure, got %s", type_name(closure_val));
    }

    char *callee = (char *) Belem((u_int32_t *) closure_val, BOX(0));

    // Pushes the returned value onto stack
    reverse_on_stack(n_args);
    vstack_push((u_int32_t) interpreterState.ip);
    vstack_push(n_args + 1);
    interpreterState.ip = callee;
}

// Get the entry point of the program (the "main" public symbol).
static inline char* find_main_entrypoint(byte_file *bf, const char *code_end) {
    // Check public symbols
    if (bf->public_symbols_number == 0) {
        runtime_error("No public symbols in bytecode file");
    }

    for (u_int32_t i = 0; i < bf->public_symbols_number; i++) {
        const char *name = get_public_name(bf, i);
        if (strcmp(name, "main") == 0) {
            u_int32_t offset = get_public_offset(bf, i);
            char *entry = bf->code_ptr + offset;

            // Check that the entry point lies within the code bounds
            if (entry < bf->code_ptr || entry >= code_end) {
                runtime_error("'main' offset %u points outside code section "
                        "(code bounds: [%p, %p))\n",
                        offset, (void*)bf->code_ptr, (void*)code_end);
            }
            return entry;
        }
    }

    // Print first few public symbols for debug
    runtime_error("Main not found. Available symbols (%u total):", bf->public_symbols_number);
    for (u_int32_t i = 0; i < bf->public_symbols_number && i < 10; i++) {
        fprintf(stderr, "  '%s'\n", get_public_name(bf, i));
    }

    runtime_error("Required public symbol 'main' not found\n");
    return NULL; // unreachable
}

void init_interpreter(byte_file *bf) {
    stack_start = malloc(RUNTIME_VSTACK_SIZE * sizeof(u_int32_t));
    if (stack_start == NULL) {
        runtime_error("ERROR: Failed to allocate memory for virtual stack.");
    }
    // init __gc_stack_bottom and __gc_stack_top for detection of lama GC and call extern __gc__init
    __gc_stack_bottom = stack_start + RUNTIME_VSTACK_SIZE;
    __gc_stack_top = __gc_stack_bottom;

    // Add globals to stack
    __gc_stack_top -= bf->global_area_size;
    interpreterState.globals_base = __gc_stack_top;

    __gc_init();

    stack_fp = __gc_stack_top;
    vstack_push(0); // argv
    vstack_push(0); // argc
    vstack_push(2); // dummys

    interpreterState.byteFile = bf;
    interpreterState.code_start = bf->code_ptr;
    interpreterState.code_end = bf->code_ptr + bf->code_size;
    interpreterState.ip = find_main_entrypoint(bf, (const char*) interpreterState.code_end);
    // DEBUG
//    printf("\nCode_start=%p\nCode_end=%p\nCode_size=%u\nip=%p\n",
//            interpreterState.code_end,
//            interpreterState.code_start,
//            bf->code_size,
//            interpreterState.ip);
}

void interpret() {
    do {
        u_int8_t bytecode = get_next_byte();
        bytecode_type bc_type = get_bytecode_type(bytecode);
#define EXEC_WITH_LOWER_BITS(BC_NAME, EXEC_SUFFIX) \
        case BC_NAME:              \
            exec_##EXEC_SUFFIX(bytecode);  \
            break;
#define EXEC(BC_NAME, EXEC_SUFFIX) \
        case BC_NAME:              \
            exec_##EXEC_SUFFIX();  \
            break;
        switch (bc_type) {
            // Interpret bytecodes with meaningful lower bits
            EXEC_WITH_LOWER_BITS(BINOP, binop)
            EXEC_WITH_LOWER_BITS(LD, ld)
            EXEC_WITH_LOWER_BITS(LDA, lda)
            EXEC_WITH_LOWER_BITS(ST, st)
            EXEC_WITH_LOWER_BITS(PATT, patt)
            // Interpret other bytecodes
            EXEC(CONST, const)
            EXEC(XSTRING, string)
            EXEC(SEXP, sexp)
            EXEC(STA, sta)
            EXEC(JMP, jmp)
            EXEC(CJMP_Z, cjmp_z)
            EXEC(CJMP_NZ, cjmp_nz)
            EXEC(ELEM, elem)
            EXEC(BEGIN, begin)
            EXEC(CBEGIN, cbegin)
            EXEC(CALL, call)
            EXEC(CALLC, callc)
            EXEC(CALL_READ, call_read)
            EXEC(CALL_WRITE, call_write)
            EXEC(CALL_STRING, call_string)
            EXEC(CALL_LENGTH, call_length)
            EXEC(CALL_ARRAY, call_array)
            EXEC(END, end)
            EXEC(DROP, drop)
            EXEC(DUP, dup)
            EXEC(TAG, tag)
            EXEC(ARRAY, array)
            EXEC(FAIL, fail)
            EXEC(LINE, line)
            EXEC(CLOSURE, closure)
            EXEC(SWAP, swap)
            case STI:
                runtime_error("ERROR: STI bytecode is deprecated.\n");
                break;
            case RET:
                runtime_error("ERROR: RET bytecode has UB.\n");
                break;
            default:
                runtime_error("ERROR: Unknown bytecode type.\n");
        }
    } while (interpreterState.ip != 0);
#undef EXEC_WITH_LOWER_BITS
#undef EXEC
}
