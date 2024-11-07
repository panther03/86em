#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

int parse_offset_segment(char* offset_segment) {
    int base = strtol(offset_segment, &offset_segment, 16);
    char sep = *offset_segment;
    offset_segment++;
    int offset = strtol(offset_segment, &offset_segment, 16);
    if (((base | offset) == 0) || sep != ':') {
        return -1;
    }
    int addr = SEGMENT(base, offset);
    printf("Loading at %x\n", addr);
    return addr;
}