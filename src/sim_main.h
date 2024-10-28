#ifndef SIM_MAIN_H
#define SIM_MAIN_H

#include <stdint.h>

typedef union {
    uint16_t x;
    struct {
        uint8_t h;
        uint8_t l;
    } __attribute__((packed)) b;
} x86_reg_t;

typedef struct {
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
} __attribute__((packed)) x86_flags_t;

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
    uint16_t cs;
    uint16_t fs;
    uint16_t ds;
    uint16_t gs;
    uint16_t es;
    uint16_t ss;
    uint16_t ip;
    x86_flags_t flags;
    last_op_t last_op;
} sim_state_t;

sim_state_t* sim_init();

void sim_run(sim_state_t* state);

#endif // SIM_MAIN_H