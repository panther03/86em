#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

// 1MB address space
#define MEM_SIZE 1024*1024*1024

uint8_t *mem;

size_t init_mem(FILE *prog, int offset) {
    mem = (uint8_t*)malloc(MEM_SIZE);
    assert(offset < MEM_SIZE);
    uint8_t* imem = mem + offset;

    size_t prog_size = 0;
    while (fread(imem, 1, 1, prog) && imem < (mem+MEM_SIZE)) {
        printf("%02x ", *imem);
        prog_size++;
        imem++;
    }
    return prog_size;
}