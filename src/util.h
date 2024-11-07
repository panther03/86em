#ifndef UTIL_H
#define UTIL_H

int parse_offset_segment(char* offset_segment);

#define SEGMENT(base,offset) ((base << 4) + offset)
#define SEXT_8_16(x) ((int16_t)((int8_t)(x)))

#endif // UTIL_H