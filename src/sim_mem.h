#ifndef MEMORY_H
#define MEMORY_H

#include <stdio.h>
#include <stdint.h>

extern uint8_t* mem;

size_t init_mem(FILE *prog, int offset);

// memory is little endian
static inline uint8_t load_u8(int addr) {
    return mem[addr];
}

static inline uint16_t load_u16(int addr) {
    return (mem[addr+1] << 8) + (mem[addr]);
}

static inline uint32_t load_u32(int addr) {
    return (mem[addr+3] << 24) + (mem[addr+2] << 16) + (mem[addr+1] << 8) + (mem[addr]);
}

#endif // MEMORY_H