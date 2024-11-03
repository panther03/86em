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

uint32_t get_16b_mem_base(sim_state_t *state, uint8_t rm) {
    uint32_t base = (rm & 0b1) ? state->di : state->si;
    switch (rm & 0b110) {
        case 0b000: return base + state->b.x;
        case 0b010: return base + state->bp;
        case 0b100: return base;
        // 0b110
        default: return (rm & 0b1) ? state->b.x : LOAD_IP_WORD(state);
    }
}

uint32_t read_mod_rm(sim_state_t *state, uint8_t mod, uint8_t rm, uint8_t is_16) {
    uint32_t reg = is_16 ? read_reg_u16(state, mod) : read_reg_u8(state, mod);
    if (mod == 0b11) {
        // register only
        return reg;
    } else {
        uint32_t base = get_16b_mem_base(state, rm);
        if (mod == 0b01) {
            base += LOAD_IP_BYTE;
        } else if (mod == 0b10) {
            base += LOAD_IP_WORD(state);
        }
        return load_u16(base);
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
        uint32_t base = get_16b_mem_base(state, rm);
        if (mod == 0b01) {
            base += LOAD_IP_BYTE;
        } else if (mod == 0b10) {
            base += LOAD_IP_WORD(state);
        }
        store_u16(base, val);
    }
}

static inline uint32_t parity(uint32_t op) {
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

uint32_t x86_xor(sim_state_t *state, uint32_t op1, uint32_t op2, uint8_t is_16) {
    uint32_t op_size = (is_16 ? 16 : 8);
    uint32_t res = (op1 ^ op2) & ((1 << op_size)-1);
    state->flags.c_f = 0;
    state->flags.o_f = 0;
    state->flags.a_f = 0;
    state->flags.p_f = parity(res);
    state->flags.s_f = (res >> (op_size-1));
    state->flags.z_f = res == 0;
    return res;
}

uint8_t insn_mode(uint8_t opc) {
    if (((opc & 0x4) == 0) && ((opc & 0xF0) <= 0x30 || (opc & 0xFC) == 0x88)) {
        // arithmetic type instruction
        return 1;
    } else if ((opc & 0xC6) == 0x04) {
        // AX/AL immediate instruction 
        return 2;
    } else if ((opc & 0xF8) == 0x40) {
        // Inc
        return 3;
    }  else if ((opc & 0xF8) == 0x48) {
        // Dec
        return 4;
    } else if ((opc & 0xF8) == 0x50) {
        // Push
        return 5;
    } else if ((opc & 0xF8) == 0x58) {
        // Pop
        return 6;
    } else if ((opc & 0xF8) == 0x98) {
        // XCHG
        return 7;
    } else if ((opc & 0xF8) == 0xB8) {
        // MOV (16-bit)
        return 8;
    } else if ((opc & 0xF8) == 0xB0) {
        // MOV (8-bit)
        return 9;
    }

    return 0;
}


void sim_run(sim_state_t* state, size_t prog_size) {
    int prog_end = SEGMENT(state->cs, state->ip) + prog_size;
    while (SEGMENT(state->cs,state->ip) < prog_end) {
        uint8_t opc = LOAD_IP_BYTE;
        //if (opc == 0xF) {
        //    opc = (opc << 8) + LOAD_IP;
        //}

        dump_state(state);

        uint8_t mode = insn_mode(opc);

        switch (insn_mode(opc)) {
        case 1: {
            mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t) LOAD_IP_BYTE;
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

            state->last_op.op1 = op1;

            switch (opc & 0xFC) {
                case  0x0: op1 = x86_add(state, op1, op2, 0, s); break;
                case  0x8: op1 = x86_or(state, op1, op2, s); break;
                case 0x10: {
                    op1 = x86_add(state, op1, op2, state->flags.c_f, s);
                    break;
                }
                case 0x18: {
                    op1 = x86_sub(state, op1, op2, state->flags.c_f, s);
                    break;
                }
                case 0x20: {
                    op1 = x86_and(state, op1, op2, s);
                    break;
                }
                case 0x28: {
                    op1 = x86_sub(state, op1, op2, 0, s);
                    break;
                }
                case 0x30: {
                    op1 = x86_xor(state, op1, op2, s);
                    break;
                }
                case 0x38: {
                    x86_sub(state, op1, op2, 0, s);
                    break;
                }
                case 0x80: {
                    op1 = op2;
                    break;
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
            break;
        }
        case 2: {
            uint8_t s = opc & 0x1;
            uint32_t imm = 0;
            uint32_t op1 = 0;
            if (s) {
                imm = LOAD_IP_WORD(state);
                op1 = state->a.x;
            } else {
                imm = LOAD_IP_BYTE;
                op1 = state->a.b.l;
            }
            switch (opc & 0x38) {
                case 0x0: {
                    op1 = x86_add(state, op1, imm, 0, s);
                    break;
                }
                case 0x8: {
                    op1 = x86_or(state, op1, imm, s);
                    break;
                }
                case 0x10: {
                    op1 = x86_add(state, op1, imm, state->flags.c_f, s);
                    break;
                }
                case 0x18: {
                    op1 = x86_sub(state, op1, imm, state->flags.c_f, s);
                    break;
                }
                case 0x20: {
                    op1 = x86_and(state, op1, imm, s);
                    break;
                }
                case 0x28: {
                    op1 = x86_sub(state, op1, imm, state->flags.c_f, s);
                    break;
                }
                case 0x30: {
                    op1 = x86_xor(state, op1, imm, s);
                    break;
                }
                case 0x38: {
                    x86_sub(state, op1, imm, 0, s);
                    break;
                }
            }

            if (s) {
                state->a.x = op1;
            } else {
                state->a.b.l = op1;
            }
            break;
        }
        case 3: {
            uint8_t reg = opc & 0x7;
            uint32_t res = read_reg_u16(state, reg) + 1;
            write_reg_u16(state, reg, res);
            break;
        }
        case 4: {
            uint8_t reg = opc & 0x7;
            uint32_t res = read_reg_u16(state, reg) - 1;
            write_reg_u16(state, reg, res);
            break;
        }
        case 5: {
            uint8_t reg = opc & 0x7;
            uint32_t res = read_reg_u16(state, reg);
            state->sp -= 2;
            store_u16(SEGMENT(state->ss, state->sp), res);
            break;
        }
        case 6: {
            uint8_t reg = opc & 0x7;
            uint32_t res = load_u16(SEGMENT(state->ss, state->sp));
            write_reg_u16(state, reg, res);
            state->sp += 2;
            break;
        }
        case 7: {
            uint8_t reg = opc & 0x7;
            uint32_t op1 = read_reg_u16(state, reg);
            write_reg_u16(state, reg, state->a.x);
            state->a.x = op1;
            break;
        }
        case 8: {
            uint8_t reg = opc & 0x7;
            uint32_t imm = LOAD_IP_WORD(state);
            write_reg_u16(state, reg, imm);
            break;
        }
        case 9: {
            uint8_t reg = opc & 0x7;
            uint32_t imm = load_u8(SEGMENT(state->cs, state->ip));
            state->ip += 1;
            write_reg_u8(state, reg, imm);
            break;
        }
        default: {
            printf("panic\n");
            exit(1);
        }
        }
    }
    dump_state(state);
}
