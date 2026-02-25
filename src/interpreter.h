#pragma once

#include "bytecode_decoder.h"
#include "byte_file.h"
#include <stdbool.h>

extern int Lread();
extern int Lwrite(int);
extern int Llength(void *);
extern void *Lstring(void *p);
extern void *Bstring(void *);
extern void *Belem(void *p, int i);
extern void *Bsta(void *v, int i, void *x);
extern void *Barray_my(int bn, int *data_);
extern void *Bsexp_my(int bn, int tag, int *data_);
extern int LtagHash(char *s);
extern int Btag(void *d, int t, int n);
extern int Barray_patt(void *d, int n);
extern void *Bclosure_my(int bn, void *entry, int *values);
extern void *Belem_link(void *p, int i);
extern int Bstring_patt(void *x, void *y);
extern int Bstring_tag_patt(void *x);
extern int Barray_tag_patt(void *x);
extern int Bsexp_tag_patt(void *x);
extern int Bunboxed_patt(void *x);
extern int Bboxed_patt(void *x);
extern int Bclosure_tag_patt(void *x);

// Data struct from runtime.c
typedef struct {
  int tag;
  char contents[0];
} data;

// redefined from runtime.c for performance using the preprocessor
# define UNBOXED(x)  (((int) (x)) &  0x0001)
# define UNBOX(x)    (((int) (x)) >> 1)
# define BOX(x)      ((((int) (x)) << 1) | 0x0001)

# define TO_DATA(x) ((data*)((char*)(x)-sizeof(int)))
# define STRING_TAG  0x00000001
# define ARRAY_TAG   0x00000003
# define SEXP_TAG    0x00000005
# define CLOSURE_TAG 0x00000007
# define TAG(x)  (x & 0x00000007)

// Check, if given value matches wanted tag
static inline bool check_tag(u_int32_t val, u_int32_t wanted_tag) {
    if (UNBOXED(val)) return false;

    data *d = TO_DATA((void *)val);
    return TAG(d->tag) == wanted_tag;
}

static inline bool is_string(u_int32_t val) {
    return check_tag(val, STRING_TAG);
}

static inline bool is_array(u_int32_t val) {
    return check_tag(val, ARRAY_TAG);
}

static inline bool is_sexp(u_int32_t val) {
    return check_tag(val, SEXP_TAG);
}

static inline bool is_closure(u_int32_t val) {
    return check_tag(val, CLOSURE_TAG);
}

static inline bool is_aggregative(u_int32_t val) {
    return is_string(val) || is_array(val) || is_sexp(val);
}

// Returns type of stringified(?) value
static const char* type_name(u_int32_t val) {
    if (UNBOXED(val)) return "integer";
    // Detect obj tag
    data *d = TO_DATA((void *) val);
    int tag = TAG(d->tag);
    switch (tag) {
        case STRING_TAG:  return "string";
        case ARRAY_TAG:   return "array";
        case SEXP_TAG:    return "sexp";
        case CLOSURE_TAG: return "closure";
        default:          return "unknown boxed";
    }
}

extern u_int32_t *__gc_stack_top, *__gc_stack_bottom;
extern void *__start_custom_data, *__stop_custom_data;
extern void __gc_init(void);

typedef struct {
    byte_file  *byteFile;
    char       *ip;
    char       *code_start;
    const char *code_end;
    u_int32_t *globals_base;
} interpreter_state;
extern interpreter_state interpreterState;

void init_interpreter(byte_file *bf);
void interpret();
