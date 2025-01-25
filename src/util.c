#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "util.h"

int parse_offset_segment(char* offset_segment) {
    int base = strtol(offset_segment, &offset_segment, 16);
    char sep = *offset_segment;
    offset_segment++;
    int offset = strtol(offset_segment, &offset_segment, 16);
    if (sep != ':') {
        return -1;
    }
    int addr = SEGMENT(base, offset);
    printf("Loading at %x\n", addr);
    return addr;
}


bool sep_whitespace(char c){ 
    return isspace(c);
}

bool sep_semi(char c){ 
    return c == ';';
}


char* arg_next(arg_split_t *it) {
    if (it->consumed) return NULL;
    assert(it->buf);

    // Skip leading whitespace
    while (isspace(*(it->buf))) {
        it->buf++;
    }

    // End of string? Iterator consumed
    if (*(it->buf) == '\0') {
        it->consumed = true;
        return NULL;
    }

    char* start = it->buf;

    // Move to the next whitespace or end of string
    while (*(it->buf) && !it->sep_match(*(it->buf))) {
        it->buf++;
    }

    // Null-terminate the current token if not at the end
    if (*(it->buf)) {
        *(it->buf) = '\0';
        it->buf++;
    } else {
        it->consumed = true;
    }

    return start;
}