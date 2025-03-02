#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

typedef struct {
    char* buf;
    bool consumed;
    bool (*sep_match)(char);
} arg_split_t;

int parse_offset_segment(char* offset_segment);
char* arg_next(arg_split_t *it);

bool sep_whitespace(char c);
bool sep_semi(char c);

#define SEGMENT(base,offset) ((((base) << 4) + ((uint16_t)(offset))) & 0xFFFFF)
#define SEXT_8_16(x) ((int16_t)((int8_t)(x)))
#define SEXT_16_32(x) ((int32_t)((int16_t)(x)))

#endif // UTIL_H