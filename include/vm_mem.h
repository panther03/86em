#ifndef VM_MEM_H
#define VM_MEM_H

#include <stdio.h>
#include <stdint.h>
#include "util.h"

typedef struct {
    size_t prog_size;
    size_t prog_start;
} prog_info_t;

extern uint8_t* mem;
extern prog_info_t prog_info;


void init_mem_blank();
void load_mem(FILE *prog, int offset);

// memory is little endian
static inline uint8_t load_u8(uint16_t seg, uint16_t offset) {
    return mem[SEGMENT(seg, offset)];
}

static inline uint8_t load_u8_direct(uint32_t addr) {
    return mem[addr & 0xFFFFF];
}

// todo: this is not actually how it works on x86..
static inline uint16_t load_u16(uint16_t seg, uint16_t offset) {
    return (mem[SEGMENT(seg, offset+1)] << 8) + (mem[SEGMENT(seg, offset)]);
}

static inline uint32_t load_u32(uint16_t seg, uint16_t offset) {
    return (mem[SEGMENT(seg, offset+3)] << 24)
         + (mem[SEGMENT(seg, offset+2)] << 16)
         + (mem[SEGMENT(seg, offset+1)] << 8)
         + (mem[SEGMENT(seg, offset+0)]);
}

static inline void store_u16(uint16_t seg, uint16_t offset, uint16_t val) {
    mem[SEGMENT(seg, offset)] = val & 0xFF;
    mem[SEGMENT(seg, offset+1)] = val >> 8;
}

static inline void store_u8(uint16_t seg, uint16_t offset, uint8_t val) {
    mem[SEGMENT(seg, offset)] = val;
}

static inline void store_u8_direct(uint32_t addr, uint8_t val) {
    mem[addr & 0xFFFFF] = val;
}

#endif // VM_MEM_H