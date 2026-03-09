#ifndef FREQUENCY_ANALYZER_H
#define FREQUENCY_ANALYZER_H

#include "byte_file.h"

// Limitation for number of unique sequences
#define MAX_UNIQUE_SEQUENCES 100000

#define READ_INT() do { \
    if (!ensure_capacity(info, info->param_count + 1)) return false; \
    if (pos + 4 > max_len) return false; \
    u_int32_t val = (u_int32_t)code[pos] | ((u_int32_t)code[pos+1]<<8) | \
                   ((u_int32_t)code[pos+2]<<16) | ((u_int32_t)code[pos+3]<<24); \
    info->params[info->param_count++] = val; \
    pos += 4; \
} while(0)

#define READ_BYTE() do { \
    if (!ensure_capacity(info, info->param_count + 1)) return false; \
    if (pos + 1 > max_len) return false; \
    u_int8_t b = code[pos]; \
    info->params[info->param_count++] = b; \
    pos += 1; \
} while(0)

void analyze_frequency(byte_file *bf);

#endif
