#ifndef vm_MAIN_H
#define vm_MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include "vm_mem.h"
#include "util.h"

typedef union {
    uint16_t x;
    struct {
        uint8_t l;
        uint8_t h;
    } __attribute__((packed)) b;
} x86_reg_t;

typedef union {
    struct {
        uint8_t c_f   : 1;
        uint8_t res0 : 1;
        uint8_t p_f  : 1;
        uint8_t res1 : 1;
        uint8_t a_f  : 1;
        uint8_t res2 : 1;
        uint8_t z_f  : 1;
        uint8_t s_f  : 1;
        uint8_t t_f  : 1;
        uint8_t i_f  : 1;
        uint8_t d_f  : 1;
        uint8_t o_f  : 1;
        uint8_t res3 : 4;
    };
    uint16_t num;
} __attribute__((packed)) x86_flags_t;

#define FLAGS_RESERVED_MASK 0xF02A
#define FLAGS_DEFAULT       0xF002

typedef struct {
    uint32_t op1;
    uint32_t op2;
    uint32_t res;
    uint8_t opc;
    uint8_t is_16;
} last_op_t;

typedef struct {
    x86_reg_t a;
    x86_reg_t b;
    x86_reg_t c;
    x86_reg_t d;
    uint16_t si;
    uint16_t di;
    uint16_t bp;
    uint16_t sp;
    uint16_t es;
    uint16_t cs;
    uint16_t ss;
    uint16_t ds;
    uint16_t ip;
    // TODO change this to a union type
    x86_flags_t flags;
    int int_src;
    // TODO remove this Shit
    int seg_override;
} x86_cpu_t; 


// TODO separate out into CPU and VM structs, 
// but dont want to add another indirection so idk
typedef struct {
    x86_cpu_t cpu;
    struct {
        bool enable_trace;
    } opts;
    struct {
        uint64_t cycles;
        int32_t bkpt;
        bool bkpt_clear;
    };
} vm_t;

typedef union {
    x86_flags_t fl;
    uint16_t num;
} flags_union;


vm_t* vm_init();

void vm_run(vm_t* state, int max_cycles);

#define LOAD_IP_BYTE(cpu) (load_u8(cpu->cs, (cpu->ip++)))
static inline uint16_t LOAD_IP_WORD(x86_cpu_t* cpu) {
    uint8_t b1 = LOAD_IP_BYTE(cpu);
    uint8_t b2 = LOAD_IP_BYTE(cpu);
    return (b2 << 8) + b1;
}

static inline void push_u8(x86_cpu_t *cpu, uint8_t val) {
    cpu->sp -= 1;
    store_u8(cpu->ss, cpu->sp, val);
}

static inline void push_u16(x86_cpu_t *cpu, uint16_t val) {
    push_u8(cpu, val >> 8);
    push_u8(cpu, val & 0xFF);
}

static inline uint8_t pop_u8(x86_cpu_t *cpu) {
    uint8_t res = load_u8(cpu->ss, cpu->sp);
    cpu->sp += 1;
    return res;
}

static inline uint16_t pop_u16(x86_cpu_t *cpu) {
    uint8_t b1 = pop_u8(cpu);
    uint8_t b2 = pop_u8(cpu);
    return (b2 << 8) + b1;
}

static inline void pop_flags(x86_cpu_t *cpu) {
    uint16_t fl = pop_u16(cpu);
    cpu->flags.num = FLAGS_DEFAULT | (fl & ~FLAGS_RESERVED_MASK);
}

#endif // vm_MAIN_H