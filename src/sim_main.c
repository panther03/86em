#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "sim_main.h"
#include "sim_mem.h"

#include "opc.h"

typedef union {
    uint8_t byte;
    struct {
        uint8_t rm: 3;
        uint8_t reg: 3;
        uint8_t mod: 2;
    } __attribute__((packed)) fields;
} mod_reg_rm_t;

sim_state_t* sim_init() {
    sim_state_t* state = (sim_state_t*)calloc(sizeof(sim_state_t), 0);
    state->cs = 0xFFFF;
    state->flags.res0 = 1;
    state->flags.res1 = 1;
    state->flags.res2 = 1;
    state->flags.res3 = 0xF;
    return state;
}

void dump_state(sim_state_t* state) {
    printf("AX %d\n", state->a.x);
    printf("BX %d\n", state->b.x);
    printf("CX %d\n", state->c.x);
    printf("DX %d\n", state->d.x);
}

// TODO: optimize these into writes/reads of offsets of the struct
// if the fields are ordered the right way then you can simply extract the bits and use that as an offset
void write_reg_u16(sim_state_t *state, uint8_t reg, uint16_t val) {
    assert(reg < 0b1000);
    switch(reg) {
        case 0b000: state->a.x = val; break;
        case 0b001: state->c.x = val; break;
        case 0b010: state->d.x = val; break;
        case 0b011: state->b.x = val; break;
        case 0b100: state->sp = val; break;
        case 0b101: state->bp = val; break;
        case 0b110: state->si = val; break;
        case 0b111: state->di = val; break;
    }
}

void write_reg_u8(sim_state_t *state, uint8_t reg, uint8_t val) {
    assert(reg < 0b1000);
    switch(reg) {
        case 0b000: state->a.b.l = val; break;
        case 0b001: state->c.b.l = val; break;
        case 0b010: state->d.b.l = val; break;
        case 0b011: state->b.b.l = val; break;
        case 0b100: state->a.b.h = val; break;
        case 0b101: state->c.b.h = val; break;
        case 0b110: state->d.b.h = val; break;
        case 0b111: state->b.b.h = val; break;
    }
}

uint16_t read_reg_u16(sim_state_t *state, uint8_t reg) {
    assert(reg < 0b1000);
    switch(reg) {
        case 0b000: return state->a.x;
        case 0b001: return state->c.x;
        case 0b010: return state->d.x;
        case 0b011: return state->b.x;
        case 0b100: return state->sp;
        case 0b101: return state->bp;
        case 0b110: return state->si;
        case 0b111: return state->di;
        default: return 0;
    }
}

uint8_t read_reg_u8(sim_state_t *state, uint8_t reg) {
    assert(reg < 0b1000);
    switch(reg) {
        case 0b000: return state->a.b.l;
        case 0b001: return state->c.b.l;
        case 0b010: return state->d.b.l;
        case 0b011: return state->b.b.l;
        case 0b100: return state->a.b.h;
        case 0b101: return state->c.b.h;
        case 0b110: return state->d.b.h;
        case 0b111: return state->b.b.h;
        default: return 0;
    }
}

uint32_t read_mod_rm(sim_state_t *state, uint8_t mod, uint8_t rm, uint8_t is_16) {
    uint32_t reg = is_16 ? read_reg_u16(state, mod) : read_reg_u8(state, mod);
    if (mod == 0b11) {
        // register only
        return reg;
    } else {
        printf("not handled \n"); exit(1);
    }
}

void write_mod_rm(sim_state_t *state, uint8_t mod, uint8_t rm, uint32_t val, uint8_t is_16) {
    if (mod == 0b11) {
        // register only
        if (is_16) {
            write_reg_u16(state, rm, (uint16_t)val);
        } else {
            write_reg_u8(state, rm, (uint8_t)val);
        }
    } else {
        uint32_t reg = is_16 ? read_reg_u16(state, rm) : read_reg_u8(state, rm);
        printf("not handled \n"); exit(1);
    }
}

inline uint32_t parity(uint32_t op) {
    op ^= op >> 16;
    op ^= op >> 8;
    op ^= op >> 4;
    op ^= op >> 2;
    op ^= op >> 1;
    return (~op) & 1;
}

uint32_t x86_add(sim_state_t *state, uint32_t op1, uint32_t op2, uint32_t carry, uint8_t is_16) {
    uint32_t res = op1 + op2 + carry;
    uint32_t op_size = is_16 ? 16 : 8;
    uint32_t top_bit = 1 << (op_size-1);
    uint32_t mask = (1 << (op_size)) - 1;
    state->flags.c_f = (res >> op_size);
    state->flags.s_f = (res >> (op_size-1));
    state->flags.o_f = ((~(op1 ^ op2) & (op1 ^ res)) & top_bit) ? 1 : 0;
    state->flags.a_f = ((op1 & 0xF) + (op2 & 0xF) + carry) & 0x10 ? 1 : 0;
    res = res & mask;
    state->flags.p_f = parity(res);
    state->flags.z_f = res == 0;
    return res;
}

uint32_t x86_sub(sim_state_t *state, uint32_t op1, uint32_t op2, uint32_t carry, uint8_t is_16) {
    uint32_t res = op1 - op2 - carry;
    uint32_t op_size = is_16 ? 16 : 8;
    uint32_t top_bit = 1 << (op_size-1);
    uint32_t mask = (1 << (op_size)) - 1;
    state->flags.c_f = (res >> op_size);
    state->flags.s_f = (res >> (op_size-1));
    state->flags.o_f = ((~(op1 ^ op2) & (op1 ^ res)) & top_bit) ? 1 : 0;
    state->flags.a_f = ((op1 & 0xF) + (op2 & 0xF) + carry) & 0x10 ? 1 : 0;
    res = res & mask;
    state->flags.p_f = parity(res);
    state->flags.z_f = res == 0;
    return res;
}

uint32_t x86_or(sim_state_t *state, uint32_t op1, uint32_t op2, uint8_t is_16) {
    uint32_t op_size = (is_16 ? 16 : 8);
    uint32_t res = (op1 | op2) & ((1 << op_size)-1);
    state->flags.c_f = 0;
    state->flags.o_f = 0;
    state->flags.a_f = 0;
    state->flags.p_f = parity(res);
    state->flags.s_f = (res >> (op_size-1));
    state->flags.z_f = res == 0;
    return res;
}

uint32_t x86_and(sim_state_t *state, uint32_t op1, uint32_t op2, uint8_t is_16) {
    uint32_t op_size = (is_16 ? 16 : 8);
    uint32_t res = (op1 & op2) & ((1 << op_size)-1);
    state->flags.c_f = 0;
    state->flags.o_f = 0;
    state->flags.a_f = 0;
    state->flags.p_f = parity(res);
    state->flags.s_f = (res >> (op_size-1));
    state->flags.z_f = res == 0;
    return res;
}


void update_flags(sim_state_t *state) {
    // stub
    return;
}

uint8_t insn_mode(uint8_t opc) {
    // arithmetic type instruction
    if (((opc & 0x4) == 0) && ((opc & 0xF0) <= 0x30 || (opc & 0xFC) == 0x88)) {
        return 1;
    }
    return 0;
}

#define SEGMENT(base,offset) ((base << 4) + offset)
#define LOAD_IP (load_u8(SEGMENT(state->cs, (state->ip++))))

void sim_run(sim_state_t* state) {
    while (1) {
        uint8_t opc = LOAD_IP;
        //if (opc == 0xF) {
        //    opc = (opc << 8) + LOAD_IP;
        //}

        dump_state(state);

        printf("opc: %d\n", opc);
        uint8_t mode = insn_mode(opc);

        if (insn_mode(opc)) {
            mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t) LOAD_IP;
            uint8_t s = opc & 0x1;
            uint8_t d = opc & 0x2;

            uint32_t op1 = s ? read_reg_u16(state, mod_reg_rm.fields.reg) : read_reg_u8(state, mod_reg_rm.fields.reg);
            uint32_t op2 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, s);


            if (!d) {
                // swap
                uint32_t temp;            
                temp = op1;
                op1 = op2;
                op2 = temp;
            }
            printf("%d %d %d \n", mod_reg_rm.fields.mod, mod_reg_rm.fields.reg,mod_reg_rm.fields.rm);

            state->last_op.op1 = op1;

            switch (opc & 0xFC) {
                case  0x0: op1 = x86_add(state, op1, op2, 0, s); break;
                case  0x8: op1 = x86_or(state, op1, op2, s); break;
                case 0x10: {
                    op1 = x86_add(state, op1, op2, state->flags.c_f, s);
                    break;
                }
                case 0x18: {
                    update_flags(state);
                    op1 -= op2 - state->flags.c_f;
                    break;
                }
                case 0x20: {
                    op1 &= op2;
                    break;
                }
                case 0x28: {
                    op1 -= op2;
                    break;
                }
                case 0x30: {
                    op1 ^= op2;
                    break;
                }
                case 0x38: {
                    // TODO
                    op1 -= op2;
                    break;
                }
                case 0x80: {
                    op1 = op2;
                }
                default: {
                    printf("unrecognized arithmetic opcode %d", opc);
                    return;
                }
            }

            state->last_op.is_16 = s;
            state->last_op.res = op1;
            state->last_op.op2 = op2;
            state->last_op.opc = opc;

            if (!d) {
                write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, op1, s);
            } else {
                if (s) {
                    write_reg_u16(state, mod_reg_rm.fields.reg, op1);
                } else {
                    write_reg_u8(state, mod_reg_rm.fields.reg, op1);
                }
            }
        } else {
            switch (opc) {
                case 0x5: {
                    uint16_t imm = load_u16(SEGMENT(state->cs, state->ip));
                    state->ip += 2;
                    state->last_op.op1 = state->a.x;
                    state->a.x += imm;
                    state->last_op.res = state->a.x;
                    state->last_op.op2 = imm;
                    state->last_op.opc = opc;
                    break;
                }
                default: {
                    printf("panic\n");
                    exit(1);
                }
            }
        }
    }
}
