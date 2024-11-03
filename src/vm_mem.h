#ifndef VM_MEM_H
#define VM_MEM_H

#include <stdio.h>
#include <stdint.h>

typedef struct {
    size_t prog_size;
    size_t prog_start;
} prog_info_t;

extern uint8_t* mem;
extern prog_info_t prog_info;


void init_mem(FILE *prog, int offset);

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

static inline void store_u16(int addr, uint16_t val) {
    mem[addr] = val & 0xFF;
    mem[addr+1] = val >> 8;
}

#endif // VM_MEM_H