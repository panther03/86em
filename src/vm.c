#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "vm.h"
#include "vm_io.h"
#include "vm_mem.h"

#include "opc.h"

#include "cfg.h"

typedef struct {
    union {
        uint8_t rm_byte;
        struct {
            uint8_t rm : 3;
            uint8_t reg : 3;
            uint8_t mod : 2;
        } __attribute__((packed));
    };
    uint16_t disp;
} mod_reg_rm_t;

vm_t *vm_init() {
    vm_t *vm = (vm_t *)calloc(1, sizeof(vm_t));
    vm->cpu.cs = 0xFFFF;
    vm->cpu.flags.res0 = 1;
    vm->cpu.flags.res1 = 0;
    vm->cpu.flags.res2 = 0;
    vm->cpu.flags.res3 = 0xF;
    // No interrupt source by default
    vm->cpu.int_src = -1;
    vm->cpu.seg_override = -1;

    vm->cycles = 0;
    vm->bkpt = -1;
    vm->bkpt_clear = true;

    io_init();
    return vm;
}

#ifdef CFG_DIFF_TRACE
#define HIGHLIGHT(a, b)                                                        \
    if (a != b) {                                                              \
        printf("\e[0;31m");                                                    \
    }
#define HIGHLIGHT_END printf("\e[0m");
#else
#define HIGHLIGHT(a, b)
#define HIGHLIGHT_END
#endif

void dump_flags(uint16_t flags, uint16_t old_flags) {
    const char flags_symbols[] = {'C', ' ', 'P', ' ', 'A', ' ', 'Z',
                                  'S', 'T', 'I', 'D', 'O', '\0'};
    printf("%s\n", flags_symbols);
    for (int i = 0; i < 12; i++, flags >>= 1, old_flags >>= 1) {
        int lsb_n = flags & 1;
#ifdef CFG_DIFF_TRACE
        int lsb_o = old_flags & 1;
        if (lsb_n != lsb_o) {
            printf("\e[0;31m");
        }
        putc(lsb_n ? 'X' : '-', stdout);
        if (lsb_n != lsb_o) {
            printf("\e[0m");
        }
#else
        putc(lsb_n ? 'X' : ' ', stdout);
#endif
    }
    putc('\n', stdout);
}

void dump_cpu(x86_cpu_t *cpu, x86_cpu_t *old_cpu) {
    printf("========================================\n");
    printf("\e[0;36mIP %04x\e[0m\t", cpu->ip);
    HIGHLIGHT_END
    HIGHLIGHT(cpu->a.x, old_cpu->a.x)
    printf("AX %04x\t", cpu->a.x);
    HIGHLIGHT_END
    HIGHLIGHT(cpu->b.x, old_cpu->b.x)
    printf("BX %04x\t", cpu->b.x);
    HIGHLIGHT_END
    HIGHLIGHT(cpu->c.x, old_cpu->c.x)
    printf("CX %04x\t", cpu->c.x);
    HIGHLIGHT_END
    HIGHLIGHT(cpu->d.x, old_cpu->d.x)
    printf("DX %04x\n", cpu->d.x);
    HIGHLIGHT_END

    HIGHLIGHT(cpu->cs, old_cpu->cs)
    printf("CS %04x\t", cpu->cs);
    HIGHLIGHT_END
    HIGHLIGHT(cpu->ds, old_cpu->ds)
    printf("DS %04x\t", cpu->ds);
    HIGHLIGHT_END
    HIGHLIGHT(cpu->ss, old_cpu->ss)
    printf("SS %04x\t", cpu->ss);
    HIGHLIGHT_END
    HIGHLIGHT(cpu->es, old_cpu->es)
    printf("ES %04x\n", cpu->es);
    HIGHLIGHT_END

    HIGHLIGHT(cpu->si, old_cpu->si)
    printf("SI %04x\t", cpu->si);
    HIGHLIGHT_END
    HIGHLIGHT(cpu->di, old_cpu->di)
    printf("DI %04x\t", cpu->di);
    HIGHLIGHT_END
    HIGHLIGHT(cpu->bp, old_cpu->bp)
    printf("BP %04x\t", cpu->bp);
    HIGHLIGHT_END
    HIGHLIGHT(cpu->sp, old_cpu->sp)
    printf("SP %04x\t", cpu->sp);
    HIGHLIGHT_END

#ifdef CFG_DIFF_TRACE
    HIGHLIGHT(cpu->flags.num, old_cpu->flags.num)
    printf("FL %04x\n", cpu->flags.num);
    HIGHLIGHT_END
    dump_flags(cpu->flags.num, old_cpu->flags.num);
#else
    printf("FL %04x\n", cpu->flags.num);
    dump_flags(cpu->flags.num, cpu->flags.num);
#endif
    printf("========================================\n");
}

// TODO: optimize these into writes/reads of offsets of the struct
// if the fields are ordered the right way then you can vmply extract the bits
// and use that as an offset
void write_reg_u16(x86_cpu_t *cpu, uint8_t reg, uint16_t val) {
    assert(reg < 0b1000);
    switch (reg) {
    case 0b000:
        cpu->a.x = val;
        break;
    case 0b001:
        cpu->c.x = val;
        break;
    case 0b010:
        cpu->d.x = val;
        break;
    case 0b011:
        cpu->b.x = val;
        break;
    case 0b100:
        cpu->sp = val;
        break;
    case 0b101:
        cpu->bp = val;
        break;
    case 0b110:
        cpu->si = val;
        break;
    case 0b111:
        cpu->di = val;
        break;
    }
}

void write_reg_u8(x86_cpu_t *cpu, uint8_t reg, uint8_t val) {
    assert(reg < 0b1000);
    switch (reg) {
    case 0b000:
        cpu->a.b.l = val;
        break;
    case 0b001:
        cpu->c.b.l = val;
        break;
    case 0b010:
        cpu->d.b.l = val;
        break;
    case 0b011:
        cpu->b.b.l = val;
        break;
    case 0b100:
        cpu->a.b.h = val;
        break;
    case 0b101:
        cpu->c.b.h = val;
        break;
    case 0b110:
        cpu->d.b.h = val;
        break;
    case 0b111:
        cpu->b.b.h = val;
        break;
    }
}

uint16_t read_reg_u16(x86_cpu_t *cpu, uint8_t reg) {
    assert(reg < 0b1000);
    switch (reg) {
    case 0b000:
        return cpu->a.x;
    case 0b001:
        return cpu->c.x;
    case 0b010:
        return cpu->d.x;
    case 0b011:
        return cpu->b.x;
    case 0b100:
        return cpu->sp;
    case 0b101:
        return cpu->bp;
    case 0b110:
        return cpu->si;
    case 0b111:
        return cpu->di;
    default:
        return 0;
    }
}

uint8_t read_reg_u8(x86_cpu_t *cpu, uint8_t reg) {
    assert(reg < 0b1000);
    switch (reg) {
    case 0b000:
        return cpu->a.b.l;
    case 0b001:
        return cpu->c.b.l;
    case 0b010:
        return cpu->d.b.l;
    case 0b011:
        return cpu->b.b.l;
    case 0b100:
        return cpu->a.b.h;
    case 0b101:
        return cpu->c.b.h;
    case 0b110:
        return cpu->d.b.h;
    case 0b111:
        return cpu->b.b.h;
    default:
        return 0;
    }
}

uint16_t read_seg(x86_cpu_t *cpu, uint8_t sr) {
    switch (sr) {
    case 0b00:
        return cpu->es;
    case 0b01:
        return cpu->cs;
    case 0b10:
        return cpu->ss;
    case 0b11:
        return cpu->ds;
    default: {
        printf("the FUCK : %d\n", sr);
        return 0;
    }
    }
}

void write_seg(x86_cpu_t *cpu, uint8_t sr, uint16_t val) {
    switch (sr) {
    case 0b00:
        cpu->es = val;
        break;
    case 0b01:
        cpu->cs = val;
        break;
    case 0b10:
        cpu->ss = val;
        break;
    case 0b11:
        cpu->ds = val;
        break;
    }
}

uint32_t get_16b_mem_base(x86_cpu_t *cpu, mod_reg_rm_t mod_reg_rm) {
    uint32_t base = (mod_reg_rm.rm & 0b1) ? cpu->di : cpu->si;
    switch (mod_reg_rm.rm & 0b110) {
    case 0b000:
        return base + cpu->b.x;
    case 0b010:
        if (cpu->seg_override < 0)
            cpu->seg_override = -2;
        return base + cpu->bp;
    case 0b100:
        return base;
    // 0b110
    default:
        if (mod_reg_rm.rm & 0b1) {
            return cpu->b.x;
        } else {
            if (mod_reg_rm.mod) {
                if (cpu->seg_override < 0)
                    cpu->seg_override = -2;
                return cpu->bp;
            } else {
                return 0;
            }
        }
    }
}

static inline uint16_t x86_get_data_segment(x86_cpu_t *cpu) {
    // Segment override is an offset
    // -1 on the default (DS), 0 - 3 otherwise
    // -1 wraps back around to 3 if we take mod 4 (& 0b11)
    uint16_t segment = *(&cpu->es + ((4 + cpu->seg_override) & 0b11));
    return segment;
}

static inline mod_reg_rm_t read_mod_reg_rm(x86_cpu_t *cpu) {
    mod_reg_rm_t mod_reg_rm;
    mod_reg_rm.rm_byte = LOAD_IP_BYTE(cpu);
    //printf("mod %d (%d)\n", mod_reg_rm.mod, mod_reg_rm.rm_byte);
    if (mod_reg_rm.mod == 0b01) {
        mod_reg_rm.disp = SEXT_8_16(LOAD_IP_BYTE(cpu));
    } else if (mod_reg_rm.mod == 0b10) {
        mod_reg_rm.disp = LOAD_IP_WORD(cpu);
    } else if (mod_reg_rm.mod == 0b00 && mod_reg_rm.rm == 0b110) {
        mod_reg_rm.disp = LOAD_IP_WORD(cpu);
    } else {
        mod_reg_rm.disp = 0;
    }
    return mod_reg_rm;
}

static inline uint32_t mod_rm_effective_addr(x86_cpu_t *cpu,
                                             mod_reg_rm_t mod_reg_rm) {
    uint32_t base = mod_reg_rm.disp + get_16b_mem_base(cpu, mod_reg_rm);
    base = base & 0xFFFF;
    assert(-2 <= cpu->seg_override && cpu->seg_override <= 3);
    uint32_t addr = SEGMENT(x86_get_data_segment(cpu), base);
    //printf("mod rm addr: %08x\n", addr);
    return addr;
}

uint32_t read_mod_rm(x86_cpu_t *cpu, mod_reg_rm_t mod_reg_rm,
                     uint8_t is_16) {
    uint32_t reg = is_16 ? read_reg_u16(cpu, mod_reg_rm.rm)
                         : read_reg_u8(cpu, mod_reg_rm.rm);
    if (mod_reg_rm.mod == 0b11) {
        // register only
        return reg;
    } else {
        uint32_t addr = mod_rm_effective_addr(cpu, mod_reg_rm);
        return is_16 ? load_u16(addr) : load_u8(addr);
    }
}

void write_mod_rm(x86_cpu_t *cpu, mod_reg_rm_t mod_reg_rm, uint32_t val,
                  uint8_t is_16) {
    if (mod_reg_rm.mod == 0b11) {
        // register only
        if (is_16) {
            write_reg_u16(cpu, mod_reg_rm.rm, (uint16_t)val);
        } else {
            write_reg_u8(cpu, mod_reg_rm.rm, (uint8_t)val);
        }
    } else {
        uint32_t addr = mod_rm_effective_addr(cpu, mod_reg_rm);
        if (is_16) {
            store_u16(addr, val);
        } else {
            store_u8(addr, val);
        }        
    }
}

static inline uint8_t parity_byte(uint8_t op) {
    op ^= op >> 4;
    op ^= op >> 2;
    op ^= op >> 1;
    return (~op) & 1;
}

uint32_t x86_add(x86_cpu_t *cpu, uint32_t op1, uint32_t op2, uint32_t carry,
                 uint8_t is_16) {
    printf("%08x %08x %08x\n", op1, op2, carry);
    uint32_t res = op1 + op2 + carry;
    uint32_t op_size = is_16 ? 16 : 8;
    uint32_t top_bit = 1 << (op_size - 1);
    uint32_t mask = (1 << (op_size)) - 1;
    cpu->flags.c_f = (res >> op_size);
    cpu->flags.s_f = (res >> (op_size - 1));
    cpu->flags.o_f = ((~(op1 ^ op2) & (op1 ^ res)) & top_bit) ? 1 : 0;
    cpu->flags.a_f = ((op1 & 0xF) + (op2 & 0xF) + carry) & 0x10 ? 1 : 0;
    res = res & mask;
    cpu->flags.p_f = parity_byte(res);
    cpu->flags.z_f = res == 0;
    return res;
}

uint32_t x86_sub(x86_cpu_t *cpu, uint32_t op1, uint32_t op2, uint32_t carry,
                 uint8_t is_16) {
    uint32_t op_size = is_16 ? 16 : 8;
    uint32_t mask = (1 << (op_size)) - 1;
    cpu->flags.c_f = (op1 & mask) < ((op2 & mask) + carry);
    cpu->flags.a_f = (op1 & 0xF) < ((op2 & 0xF) + carry);
    uint32_t op2_start = op2;
    op2 = ((op2 ^ mask) + 1) & mask;
    uint32_t res = op1 + op2 - carry;
    uint32_t top_bit = 1 << (op_size - 1);
    cpu->flags.s_f = (res >> (op_size - 1));
    cpu->flags.o_f =  (((op1 ^ op2_start) & ~(op2_start ^ res)) & top_bit) ? 1 : 0;
    res = res & mask;
    cpu->flags.p_f = parity_byte(res);
    cpu->flags.z_f = res == 0;
    return res;
}

uint32_t x86_or(x86_cpu_t *cpu, uint32_t op1, uint32_t op2, uint8_t is_16) {
    uint32_t op_size = (is_16 ? 16 : 8);
    uint32_t mask = ((1 << op_size) - 1);
    uint32_t res = (op1 | op2) & mask;
    cpu->flags.c_f = 0;
    cpu->flags.o_f = 0;
    cpu->flags.a_f = 0;
    cpu->flags.p_f = parity_byte(res);
    cpu->flags.s_f = (res >> (op_size - 1));
    cpu->flags.z_f = res == 0;
    return res;
}

uint32_t x86_and(x86_cpu_t *cpu, uint32_t op1, uint32_t op2, uint8_t is_16) {
    uint32_t op_size = (is_16 ? 16 : 8);
    uint32_t mask = ((1 << op_size) - 1);
    uint32_t res = (op1 & op2) & mask;
    cpu->flags.c_f = 0;
    cpu->flags.o_f = 0;
    cpu->flags.a_f = 0;
    cpu->flags.p_f = parity_byte(res);
    cpu->flags.s_f = (res >> (op_size - 1));
    cpu->flags.z_f = res == 0;
    return res;
}

uint32_t x86_xor(x86_cpu_t *cpu, uint32_t op1, uint32_t op2, uint8_t is_16) {
    uint32_t op_size = (is_16 ? 16 : 8);
    uint32_t mask = ((1 << op_size) - 1);
    uint32_t res = (op1 ^ op2) & mask;
    cpu->flags.c_f = 0;
    cpu->flags.o_f = 0;
    cpu->flags.a_f = 0;
    cpu->flags.p_f = parity_byte(res);
    cpu->flags.s_f = (res >> (op_size - 1));
    cpu->flags.z_f = res == 0;
    return res;
}

// https://c9x.me/x86/html/file_module_x86_id_273.html
uint32_t x86_rotate(x86_cpu_t *cpu, uint8_t op, uint32_t x, uint32_t shamt,
                    uint8_t is_16) {
    uint8_t op_size = is_16 ? 16 : 8;
    uint8_t rc_temp_shamt = shamt % (is_16 ? 17 : 9);
    uint8_t ro_temp_shamt = shamt % (is_16 ? 16 : 8);
    switch (op) {
    case 0b010: {
        while (rc_temp_shamt != 0) {
            uint8_t temp_cf = (x >> (op_size - 1));
            x = (x << 1) + cpu->flags.c_f;
            cpu->flags.c_f = temp_cf;
            rc_temp_shamt -= 1;
        }
        if (shamt == 1) {
            cpu->flags.o_f = (x >> (op_size - 1)) ^ cpu->flags.c_f;
        }
        break;
    }
    case 0b011: {
        if (shamt == 1) {
            cpu->flags.o_f = (x >> (op_size - 1)) ^ cpu->flags.c_f;
        }
        while (rc_temp_shamt != 0) {
            uint8_t temp_cf = x & 1;
            // todo op_size correct here?
            x = (x >> 1) + (cpu->flags.c_f << op_size);
            cpu->flags.c_f = temp_cf;
            rc_temp_shamt -= 1;
        }
        break;
    }
    case 0b000: {
        while (ro_temp_shamt != 0) {
            x = (x << 1) + cpu->flags.c_f;
            ro_temp_shamt -= 1;
        }
        cpu->flags.c_f = x & 1;
        if (shamt == 1) {
            cpu->flags.o_f = (x >> (op_size - 1)) ^ cpu->flags.c_f;
        }
        break;
    }
    case 0b001: {
        while (ro_temp_shamt != 0) {
            // todo op_size correct here?
            x = (x >> 1) + (cpu->flags.c_f << op_size);
            ro_temp_shamt -= 1;
        }
        cpu->flags.c_f = x >> (op_size - 1);
        if (shamt == 1) {
            cpu->flags.o_f = (x >> (op_size - 1)) ^ (x >> (op_size - 2));
        }
        break;
    }
    }
    cpu->flags.p_f = parity_byte(x);
    cpu->flags.s_f = (x >> (op_size - 1));
    cpu->flags.z_f = x == 0;
    return x;
}

// https://c9x.me/x86/html/file_module_x86_id_285.html
uint32_t x86_shift(x86_cpu_t *cpu, uint8_t op, uint32_t x, uint32_t shamt,
                   uint8_t is_16) {
    uint8_t op_size = is_16 ? 16 : 8;
    uint32_t carry_shamt = (op_size - shamt > 0) ? op_size - shamt : 0;

    printf("x: %d", x);
    switch (op) {
    // SHL
    case 0b100: {
        cpu->flags.c_f = (x >> carry_shamt);
        x <<= shamt;
        if (shamt == 1) {
            cpu->flags.o_f = (x >> (op_size - 1)) ^ cpu->flags.c_f;
        }
        break;
    }
    // SHR
    case 0b101: {
        cpu->flags.c_f = (x >> (shamt - 1)) & 0x1;
        if (shamt == 1) {
            cpu->flags.o_f = (x >> (op_size - 1));
        }
        x >>= shamt;
        break;
    }
    // SAR
    case 0b111: {
        cpu->flags.c_f = (x >> (shamt - 1)) & 0x1;
        // convert to signed
        x = (((int32_t)x) << (32 - op_size)) >> (32 - op_size);
        x >>= shamt;
        if (shamt == 1) {
            cpu->flags.o_f = 0;
        }
        break;
    }
    }
    cpu->flags.p_f = parity_byte(x);
    cpu->flags.s_f = (x >> (op_size - 1));
    cpu->flags.z_f = x == 0;
    return x;
}

uint32_t x86_arith(x86_cpu_t *cpu, uint8_t fnc, uint32_t op1, uint32_t op2,
                   uint8_t s) {
    switch (fnc) {
    case 0x0:
        return x86_add(cpu, op1, op2, 0, s);
    case 0x1:
        return x86_or(cpu, op1, op2, s);
    case 0x2:
        return x86_add(cpu, op1, op2, cpu->flags.c_f, s);
    case 0x3:
        return x86_sub(cpu, op1, op2, cpu->flags.c_f, s);
    case 0x4:
        return x86_and(cpu, op1, op2, s);
    case 0x5:
        return x86_sub(cpu, op1, op2, 0, s);
    case 0x6:
        return x86_xor(cpu, op1, op2, s);
    case 0x7:
        x86_sub(cpu, op1, op2, 0, s);
        return op1;
    default: {
        printf("unrecognized arithmetic function %d", fnc);
        exit(-1);
    }
    }
}

static inline void x86_string_insn(x86_cpu_t *cpu, uint8_t opc) {
    uint8_t sz = opc & 0x1;
    int src_addr = SEGMENT(x86_get_data_segment(cpu), cpu->si);
    int dest_addr = SEGMENT(cpu->es, cpu->di);
    int si_ofs = 0;
    int di_ofs = 0;
    switch (opc) {
    // MOVS (8-bit)
    case 0xA4: {
        store_u8(dest_addr, load_u8(src_addr));
        si_ofs = 1;
        di_ofs = 1;
        break;
    }
    // MOVS (16-bit)
    case 0xA5: {
        store_u16(dest_addr, load_u16(src_addr));
        si_ofs = 2;
        di_ofs = 2;
        break;
    }
    // CMPS (8-bit)
    case 0xA6: {
        x86_sub(cpu, load_u8(src_addr), load_u8(dest_addr), 0, sz);
        si_ofs = 1;
        di_ofs = 1;
        break;
    }
    // CMPS (16-bit)
    case 0xA7: {
        x86_sub(cpu, load_u16(src_addr), load_u16(dest_addr), 0, sz);
        si_ofs = 2;
        di_ofs = 2;
        break;
    }
    // STOS (8-bit)
    case 0xAA: {
        store_u8(dest_addr, cpu->a.b.l);
        si_ofs = 1;
        di_ofs = 1;
        break;
    }
    // STOS (16-bit)
    case 0xAB: {
        store_u16(dest_addr, cpu->a.x);
        si_ofs = 2;
        di_ofs = 2;
        break;
    }
    // LODS (8-bit)
    case 0xAC: {
        cpu->a.b.l = load_u8(src_addr);
        si_ofs = 1;
        break;
    }
    // LODS (16-bit)
    case 0xAD: {
        cpu->a.x = load_u16(src_addr);
        si_ofs = 2;
        break;
    }
    // SCAS (8-bit)
    case 0xAE: {
        x86_sub(cpu, cpu->a.b.l, load_u8(dest_addr), 0, sz);
        si_ofs = 1;
        di_ofs = 1;
        break;
    }
    // SCAS (16-bit)
    case 0xAF: {
        x86_sub(cpu, cpu->a.x, load_u16(dest_addr), 0, sz);
        si_ofs = 2;
        di_ofs = 2;
        break;
    }
    }
    //printf("%d %d\n", si_ofs, di_ofs);
    if (cpu->flags.d_f) {
        cpu->si -= si_ofs;
        cpu->di -= di_ofs;
    } else {
        cpu->si += si_ofs;
        cpu->di += di_ofs;
    }
}

static inline void x86_group1(x86_cpu_t *cpu, uint8_t is_16) {
    mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
    uint32_t op1 = read_mod_rm(cpu, mod_reg_rm, is_16);

    switch (mod_reg_rm.reg) {
    // TEST
    case 0b001:
    case 0b000: {
        uint32_t imm = is_16 ? LOAD_IP_WORD(cpu) : LOAD_IP_BYTE(cpu);
        x86_and(cpu, op1, imm, is_16);
        break;
    }
    // NOT
    case 0b010: {
        write_mod_rm(cpu, mod_reg_rm, ~op1, is_16);
        break;
    }
    // NEG
    case 0b011: {
        // TODO is the carry flag logic different here or does this work
        op1 = x86_sub(cpu, 0, op1, 0, is_16);
        write_mod_rm(cpu, mod_reg_rm, op1, is_16);
        break;
    }
    // MUL
    case 0b100: {
        if (is_16) {
            uint32_t prod = cpu->a.x * op1;
            cpu->a.x = prod;
            cpu->d.x = prod >> 16;
            cpu->flags.o_f = cpu->flags.c_f = (cpu->d.x != 0);
            cpu->flags.s_f = (cpu->a.x >> 15);
        } else {
            cpu->a.x = cpu->a.b.l * op1;
            cpu->flags.o_f = cpu->flags.c_f = (cpu->a.b.h != 0);
            cpu->flags.s_f = (cpu->a.x >> 8);
        }
        cpu->flags.p_f = parity_byte(cpu->a.x);
        cpu->flags.z_f = cpu->a.x == 0;
        break;
    }
    // IMUL
    case 0b101: {
        if (is_16) {
            int32_t ax_s = SEXT_16_32(cpu->a.x);
            int32_t prod = ax_s * SEXT_16_32(op1);
            cpu->a.x = prod;
            cpu->d.x = prod >> 16;
            ax_s = SEXT_16_32(cpu->a.x);
            cpu->flags.o_f = cpu->flags.c_f = (ax_s != prod);
            cpu->flags.s_f = (cpu->a.x >> 15);
        } else {
            cpu->a.x = SEXT_8_16(cpu->a.b.l) * SEXT_8_16(op1);
            int16_t al_sext =  SEXT_8_16(cpu->a.b.l);
            int16_t ax = cpu->a.x;
            cpu->flags.o_f = cpu->flags.c_f = (al_sext != ax);
            cpu->flags.s_f = (cpu->a.x >> 8);
        }
        cpu->flags.p_f = parity_byte(cpu->a.x);
        cpu->flags.z_f = cpu->a.x == 0;
        break;
    }
    // DIV/IDIV
    case 0b110:
    case 0b111: {
        if (is_16) {
            // TODO divide exceptions
            uint32_t divisor =
                (mod_reg_rm.reg & 1) ? (SEXT_16_32(op1)) : (int32_t)op1;
            if (divisor == 0) {
                printf("DivideByZero\n");
                break;
            }
            uint32_t dividend = (cpu->d.x << 16) + cpu->a.x;
            uint32_t remainder = dividend % divisor;
            dividend /= divisor;
            cpu->a.x = dividend;
            cpu->d.x = remainder;
            break;
        } else {
            uint16_t divisor = 
                (mod_reg_rm.reg & 1) ? (SEXT_8_16(op1)) : (int16_t) op1;
            if (divisor == 0) goto DIVEXC;

            uint16_t dividend = cpu->a.x;            
            int16_t remainder;
            int16_t quotient;
            if (mod_reg_rm.reg & 1) {
                remainder = (int16_t)dividend % (int16_t)divisor;
                quotient = (int16_t)dividend / (int16_t)divisor;
                int16_t res_sign = (dividend ^ divisor) & 0x8000;
                
                if ((res_sign && quotient > 0x7F)
                    || (!res_sign && ((quotient < 0x80) || (quotient > 0xFF))))
                    goto DIVEXC;
            } else {
                remainder = dividend % divisor;
                quotient = dividend / divisor;
                if (quotient > 0xFF) goto DIVEXC;
            }
            cpu->a.b.l = quotient;
            cpu->a.b.h = remainder;
        }
        break;
DIVEXC:
        // even though these flags are UB, the testcases store them to RAM
        // because of the exception so we can't mask them
        // 8086 seems to clear the undefined flags
        cpu->flags.num = cpu->flags.num & 0b1111011100101010;
        cpu->flags.p_f = parity_byte(cpu->a.x);
        cpu->flags.z_f = is_16 ? (cpu->a.x == 0) : (cpu->a.b.l == 0);
        cpu->int_src = 0;
        break;

    }
    }
    
}

// TODO: make this into a table
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
    } else if ((opc & 0xF8) == 0x48) {
        // Dec
        return 4;
    } else if ((opc & 0xF8) == 0x50) {
        // Push
        return 5;
    } else if ((opc & 0xF8) == 0x58) {
        // Pop
        return 6;
    } else if ((opc & 0xF8) == 0x90) {
        // XCHG
        return 7;
    } else if ((opc & 0xF8) == 0xB8) {
        // MOV (16-bit)
        return 8;
    } else if ((opc & 0xF8) == 0xB0) {
        // MOV (8-bit)
        return 9;
    } else if ((opc & 0xF0) == 0x70 || opc == 0xE3) {
        // Branch
        return 10;
    } else if ((opc & 0xFC) == 0x80) {
        // Immediate-type
        return 11;
    } else if ((opc & 0xFC) == 0xD0) {
        // Shift
        return 12;
    } else if ((opc & 0xF4) == 0xA4 || ((opc & 0xFE) == 0xAA)) {
        // String instruction
        return 13;
    } else if ((opc & 0xFC) == 0xF0 || (opc & 0xE7) == 0x26) {
        // Prefix
        return 14;
    }

    return 0;
}

static inline void x86_handle_interrupts(x86_cpu_t *cpu) {
    int int_src = cpu->int_src;

    if (int_src >= 0)
        goto INTERRUPT_FOUND;

    // NMI is not handled, nothing critical uses NMI in IBM PC

    if (cpu->flags.i_f && io_int_poll()) {
        int_src = io_int_ack();
        goto INTERRUPT_FOUND;
    }

    if (cpu->flags.t_f) {
        int_src = 1;
        goto INTERRUPT_FOUND;
    }

    // No interrupts
    return;

INTERRUPT_FOUND:;
    uint8_t temp_tf;

    // TODO?
    cpu->int_src = -1;

    do {
        // PUSH FLAGS
        push_u16(cpu, cpu->flags.num);
        // LET TEMP = TF
        temp_tf = cpu->flags.t_f;
        // CLEAR IF & TF
        cpu->flags.t_f = 0;
        cpu->flags.i_f = 0;
        // PUSH CS & IP
        push_u16(cpu, cpu->cs);
        push_u16(cpu, cpu->ip);
        // CALL INTERRUPT SERVICE ROUTINE
        assert(int_src < 256);
        cpu->ip = load_u16(int_src * 4);
        cpu->cs = load_u16(int_src * 4 + 2);
    } while (temp_tf); // add NMI check here to enable NMI functionality
}

void vm_run(vm_t *vm, int max_cycles) {
    x86_cpu_t *cpu = &vm->cpu;
    int prog_end = prog_info.prog_start + prog_info.prog_size;
    int cyc_start = vm->cycles;
#ifdef CFG_DIFF_TRACE
    x86_cpu_t  old_cpu = *cpu;
#endif
    int addr;
    //((addr = SEGMENT(cpu->cs, cpu->ip)) < prog_end)
    while ((max_cycles < 0 ||
            ((vm->cycles - cyc_start) < ((uint64_t)max_cycles)))) {
        addr = SEGMENT(cpu->cs, cpu->ip);
        if (addr == vm->bkpt && !vm->bkpt_clear) {
            printf("Breakpoint hit at %08x\n", addr);
            vm->bkpt_clear = true;
            return;
        }
        if ((vm->bkpt > 0) && vm->bkpt_clear) {
            vm->bkpt_clear = false;
        }
        uint8_t opc = LOAD_IP_BYTE(cpu);
        uint8_t mode;
        uint8_t pfx = 0;
        cpu->seg_override = -1;
        vm->cycles++;

        // Get all prefixes
        while ((mode = insn_mode(opc)) == 14) {
            if ((opc & 0xFC) == 0xF0) {
                // ignore LOCK prefix
                pfx = (opc & 2) ? opc : 0;
            } else {
                // Must be a segment override
                cpu->seg_override = (opc >> 3) & 0b11;
            }
            opc = LOAD_IP_BYTE(cpu);
        }

        if (vm->opts.enable_trace) {
            printf("Op: %02x; Pfx: %02x; SegOvr: %d; Mode: %d\n", opc, pfx, cpu->seg_override, mode);
        }

        switch (insn_mode(opc)) {
        case 1: {
            
            mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
            uint8_t s = opc & 0x1;
            uint8_t d = opc & 0x2;

            uint32_t op1 = s ? read_reg_u16(cpu, mod_reg_rm.reg)
                             : read_reg_u8(cpu, mod_reg_rm.reg);
            uint32_t op2 = read_mod_rm(cpu, mod_reg_rm, s);

            if (!d) {
                // swap
                uint32_t temp;
                temp = op1;
                op1 = op2;
                op2 = temp;
            }

            if ((opc & 0xFC) == 0x88) {
                op1 = op2;
            } else {
                op1 = x86_arith(cpu, (opc >> 3) & 0x7, op1, op2, s);
            }

            if (!d) {
                write_mod_rm(cpu, mod_reg_rm, op1, s);
            } else {
                if (s) {
                    write_reg_u16(cpu, mod_reg_rm.reg, op1);
                } else {
                    write_reg_u8(cpu, mod_reg_rm.reg, op1);
                }
            }
            break;
        }
        case 2: {
            uint8_t s = opc & 0x1;
            uint32_t imm = 0;
            uint32_t op1 = 0;
            if (s) {
                imm = LOAD_IP_WORD(cpu);
                op1 = cpu->a.x;
            } else {
                imm = LOAD_IP_BYTE(cpu);
                op1 = cpu->a.b.l;
            }
            op1 = x86_arith(cpu, (opc >> 3) & 0x7, op1, imm, s);

            if (s) {
                cpu->a.x = op1;
            } else {
                cpu->a.b.l = op1;
            }
            break;
        }
        case 3: {
            uint8_t reg = opc & 0x7;
            uint8_t carry_save = cpu->flags.c_f;
            uint32_t res = x86_add(cpu, read_reg_u16(cpu, reg), 1, 0, 1);
            cpu->flags.c_f = carry_save;
            write_reg_u16(cpu, reg, res);
            break;
        }
        case 4: {
            uint8_t reg = opc & 0x7;
            uint8_t carry_save = cpu->flags.c_f;
            uint32_t res = x86_sub(cpu, read_reg_u16(cpu, reg), 1, 0, 1);
            cpu->flags.c_f = carry_save;
            write_reg_u16(cpu, reg, res);
            break;
        }
        case 5: {
            uint8_t reg = opc & 0x7;
            uint32_t res = read_reg_u16(cpu, reg);
            push_u16(cpu, res);
            break;
        }
        case 6: {
            uint8_t reg = opc & 0x7;
            write_reg_u16(cpu, reg, pop_u16(cpu));
            break;
        }
        case 7: {
            uint8_t reg = opc & 0x7;
            uint32_t op1 = read_reg_u16(cpu, reg);
            write_reg_u16(cpu, reg, cpu->a.x);
            cpu->a.x = op1;
            break;
        }
        case 8: {
            uint8_t reg = opc & 0x7;
            uint32_t imm = LOAD_IP_WORD(cpu);
            write_reg_u16(cpu, reg, imm);
            break;
        }
        case 9: {
            uint8_t reg = opc & 0x7;
            uint32_t imm = LOAD_IP_BYTE(cpu);
            write_reg_u8(cpu, reg, imm);
            break;
        }
        case 10: {
            uint32_t imm = SEXT_8_16(LOAD_IP_BYTE(cpu));
            uint32_t cond = 0;
            switch (opc & 0b1110) {
            // J0
            case 0x0:
                cond = (cpu->flags.o_f);
                break;
            // JB
            case 0x2:
                cond = (cpu->flags.c_f);
                break;
            // JE
            case 0x4:
                cond = (cpu->flags.z_f);
                break;
            // JBE
            case 0x6:
                cond = (cpu->flags.c_f) | (cpu->flags.z_f);
                break;
            // JS
            case 0x8:
                cond = (cpu->flags.s_f);
                break;
            // JP
            case 0xA:
                cond = (cpu->flags.p_f);
                break;
            // JL
            case 0xC:
                cond = (cpu->flags.s_f) ^ (cpu->flags.o_f);
                break;
            // JLE
            case 0xE:
                cond = (cpu->flags.z_f) |
                       ((cpu->flags.s_f) ^ (cpu->flags.o_f));
                break;
            }
            cond ^= (opc & 0x1);
            if (opc == 0xE3) {
                cond = (cpu->c.x == 0);
            }
            if (cond) {
                cpu->ip = cpu->ip + imm;
            }
            break;
        }
        case 11: {
            uint8_t s = opc & 0x1;
            uint8_t x = (opc >> 1) & 0x1;
            mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
            uint32_t imm =
                s ? (x ? (SEXT_8_16(LOAD_IP_BYTE(cpu))) : LOAD_IP_WORD(cpu))
                  : (LOAD_IP_BYTE(cpu));
            uint32_t op1 = read_mod_rm(cpu, mod_reg_rm, s);
            op1 = x86_arith(cpu, mod_reg_rm.reg, op1, imm, s);
            write_mod_rm(cpu, mod_reg_rm, op1, s);
            break;
        }
        case 12: {
            uint8_t s = opc & 0x1;
            uint32_t shamt = (opc & 0x2) ? cpu->c.b.l : 1;
            mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
            uint32_t op1 = read_mod_rm(cpu, mod_reg_rm, s);
            if ((mod_reg_rm.reg & 0b100) == 0b100) {
                op1 = x86_shift(cpu, mod_reg_rm.reg, op1, shamt, s);
            } else {
                op1 = x86_rotate(cpu, mod_reg_rm.reg, op1, shamt, s);
            }
            write_mod_rm(cpu, mod_reg_rm, op1, s);
            break;
        }
        case 13: {
            // String instruction
            bool compare_enable = ((opc & 0x06) == 0x6);
            uint16_t counter = pfx ? cpu->c.x : 1;
            while (counter != 0) {
                x86_string_insn(cpu, opc);
                counter--;
                if (compare_enable && ((pfx & 0x1) ^ cpu->flags.z_f))
                    break;
            }
            if (pfx) {
                cpu->c.x = counter;
            }
            break;
        }
        default: {
            switch (opc) {
            case 0x06: {
                push_u16(cpu, cpu->es);
                break;
            }
            case 0x07: {
                cpu->es = pop_u16(cpu);
                break;
            }
            case 0x16: {
                push_u16(cpu, cpu->ss);
                break;
            }
            case 0x17: {
                cpu->ss = pop_u16(cpu);
                break;
            }
            case 0x0E: {
                push_u16(cpu, cpu->cs);
                break;
            }
            case 0x1F: {
                cpu->ds = pop_u16(cpu);
                break;
            }
            case 0x84: {
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                uint32_t op1 = read_reg_u8(cpu, mod_reg_rm.reg);
                uint32_t op2 = read_mod_rm(cpu, mod_reg_rm, 0);
                x86_and(cpu, op1, op2, 0);
                break;
            }
            case 0x85: {
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                uint32_t op1 = read_reg_u16(cpu, mod_reg_rm.reg);
                uint32_t op2 = read_mod_rm(cpu, mod_reg_rm, 1);
                x86_and(cpu, op1, op2, 1);
                break;
            }
            case 0x86: {
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                uint32_t op1 = read_reg_u8(cpu, mod_reg_rm.reg);
                uint32_t op2 = read_mod_rm(cpu, mod_reg_rm, 0);
                write_mod_rm(cpu, mod_reg_rm, op1, 0);
                write_reg_u8(cpu, mod_reg_rm.reg, op2);
                break;
            }
            case 0x87: {
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                uint32_t op1 = read_reg_u16(cpu, mod_reg_rm.reg);
                uint32_t op2 = read_mod_rm(cpu, mod_reg_rm, 1);
                write_mod_rm(cpu, mod_reg_rm, op1, 1);
                write_reg_u16(cpu, mod_reg_rm.reg, op2);
                break;
            }
            case 0x8C: {
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                uint16_t val = read_seg(cpu, mod_reg_rm.reg & 0b11);
                write_mod_rm(cpu, mod_reg_rm, val, 1);
                break;
            }
            case 0x8D: {
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                // TODO: i guess this should be the case?
                // no point in doing effective address of a register
                assert(mod_reg_rm.mod != 0b11);
                uint32_t base = mod_reg_rm.disp + get_16b_mem_base(cpu, mod_reg_rm);
                write_reg_u16(cpu, mod_reg_rm.reg, base & 0xFFFF);
                break;
            }
            case 0x8E: {
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                uint32_t op = read_mod_rm(cpu, mod_reg_rm, 1);
                write_seg(cpu, mod_reg_rm.reg & 0b11, op);
                break;
            }
            case 0x8F: {
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                uint16_t tos = pop_u16(cpu);
                write_mod_rm(cpu, mod_reg_rm, tos, 1);
                break;
            }
            // CBW
            case 0x98: {
                cpu->a.x = SEXT_8_16(cpu->a.b.l);
                break;
            }
            // CWD
            case 0x99: {
                cpu->d.x = (cpu->a.x & 0x80) ? 0xFF : 0;
                break;
            }
            // CALL (FAR, ABSOLUTE)
            case 0x9A: {
                uint32_t ip = LOAD_IP_WORD(cpu);
                uint32_t cs = LOAD_IP_WORD(cpu);
                push_u16(cpu, cpu->cs);
                push_u16(cpu, cpu->ip);
                cpu->ip = ip;
                cpu->cs = cs;
                break;
            }
            case 0x9C: {
                push_u16(cpu, cpu->flags.num);
                break;
            }
            case 0x9D: {
                pop_flags(cpu);
                break;
            }
            case 0x9E: {
                x86_flags_t ah_flags;
                ah_flags.num = cpu->a.b.h;
                cpu->flags.s_f = ah_flags.s_f;
                cpu->flags.z_f = ah_flags.z_f;
                cpu->flags.a_f = ah_flags.a_f;
                cpu->flags.p_f = ah_flags.p_f;
                cpu->flags.c_f = ah_flags.c_f;
                break;
            }
            case 0x9F: {
                cpu->a.b.h = (cpu->flags.num) & 0xFF;
                break;
            }
            case 0xA0: {
                uint32_t offset = LOAD_IP_WORD(cpu);
                uint32_t addr = SEGMENT(x86_get_data_segment(cpu), offset);
                cpu->a.b.l = load_u8(addr);
                break;
            }
            case 0xA1: {
                uint32_t offset = LOAD_IP_WORD(cpu);
                uint32_t addr = SEGMENT(x86_get_data_segment(cpu), offset);
                cpu->a.x = load_u16(addr);
                break;
            }
            case 0xA2: {
                uint32_t offset = LOAD_IP_WORD(cpu);
                uint32_t addr = SEGMENT(x86_get_data_segment(cpu), offset);
                store_u8(addr, cpu->a.b.l);
                break;
            }
            case 0xA3: {
                uint32_t offset = LOAD_IP_WORD(cpu);
                uint32_t addr = SEGMENT(x86_get_data_segment(cpu), offset);
                store_u16(addr, cpu->a.x);
                break;
            }
            case 0xA8: {
                uint32_t imm = LOAD_IP_BYTE(cpu);
                x86_and(cpu, cpu->a.b.l, imm, 0);
                break;
            }
            case 0xA9: {
                uint32_t imm = LOAD_IP_WORD(cpu);
                x86_and(cpu, cpu->a.x, imm, 1);
                break;
            }
            case 0xC2: {
                uint32_t imm = LOAD_IP_WORD(cpu);
                cpu->ip = pop_u16(cpu);
                cpu->sp += imm;
                break;
            }
            case 0xC3: {
                cpu->ip = pop_u16(cpu);
                break;
            }
            // LES
            case 0xC4:
            // LDS
            case 0xC5: {
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                uint32_t op1 = mod_rm_effective_addr(cpu, mod_reg_rm);
                write_reg_u16(cpu, mod_reg_rm.reg, load_u16(op1));
                if (opc & 1) {
                    cpu->ds = load_u16(op1+2);
                } else {
                    cpu->es = load_u16(op1+2);
                }
                break;
            }
            case 0xC6:
            case 0xC7: {
                uint8_t s = opc & 1;
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                uint32_t imm = s ? LOAD_IP_WORD(cpu) : LOAD_IP_BYTE(cpu);
                write_mod_rm(cpu, mod_reg_rm, imm, s);
                break;
            }
            case 0xCA: {
                uint32_t imm = LOAD_IP_WORD(cpu);
                cpu->ip = pop_u16(cpu);
                cpu->cs = pop_u16(cpu);
                cpu->sp += imm;
                break;
            }
            case 0xCB: {
                cpu->ip = pop_u16(cpu);
                cpu->cs = pop_u16(cpu);
                break;
            }
            case 0xCC: {
                assert(cpu->int_src < 0);
                cpu->int_src = 3;
                break;
            }
            case 0xCD: {
                assert(cpu->int_src < 0);
                uint32_t imm = LOAD_IP_BYTE(cpu);
                cpu->int_src = imm;
                break;
            }
            case 0xCE: {
                assert(cpu->int_src < 0);
                if (cpu->flags.o_f) {
                    cpu->int_src = 4;
                }
                break;
            }
            case 0xCF: {
                cpu->ip = pop_u16(cpu);
                cpu->cs = pop_u16(cpu);
                pop_flags(cpu);
                break;
            }
            // XLATB
            case 0xD7: {
                    uint32_t addr = SEGMENT(x86_get_data_segment(cpu), ((cpu->b.x + cpu->a.b.l) & 0xFFFF));
                cpu->a.b.l =
                    load_u8(addr);
                break;
            }
            // LOOPNZ
            case 0xE0: {
                uint8_t ofs = LOAD_IP_BYTE(cpu);
                cpu->c.x--;
                if ((cpu->flags.z_f == 0) && cpu->c.x) {
                    cpu->ip += SEXT_8_16(ofs);
                }
                break;
            }
            // LOOPZ
            case 0xE1: {
                uint8_t ofs = LOAD_IP_BYTE(cpu);
                cpu->c.x--;
                if (cpu->flags.z_f && cpu->c.x) {
                    cpu->ip += SEXT_8_16(ofs);
                }
                break;
            }
            // LOOP
            case 0xE2: {
                uint8_t ofs = LOAD_IP_BYTE(cpu);
                cpu->c.x--;
                if (cpu->c.x) {
                    cpu->ip += SEXT_8_16(ofs);
                }
                break;
            }
            case 0xE4: {
                uint32_t imm = LOAD_IP_BYTE(cpu);
                cpu->a.b.l = io_read_u16(imm);
                break;
            }
            case 0xE5: {
                uint32_t imm = LOAD_IP_BYTE(cpu);
                cpu->a.x = io_read_u16(imm);
                break;
            }
            case 0xE6: {
                uint32_t imm = LOAD_IP_BYTE(cpu);
                io_write_u16(imm, cpu->a.b.l);
                break;
            }
            case 0xE7: {
                uint32_t imm = LOAD_IP_BYTE(cpu);
                io_write_u16(imm, cpu->a.x);
                break;
            }
            case 0xEC: {
                cpu->a.b.l = io_read_u16(cpu->d.x);
                break;
            }
            case 0xED: {
                cpu->a.x = io_read_u16(cpu->d.x);
                break;
            }
            case 0xEE: {
                io_write_u16(cpu->d.x, cpu->a.b.l);
                break;
            }
            case 0xEF: {
                io_write_u16(cpu->d.x, cpu->a.x);
                break;
            }
            case 0xE8: {
                // near call
                uint32_t ip_inc = LOAD_IP_WORD(cpu);
                // TODO: exceptions
                push_u16(cpu, cpu->ip);
                cpu->ip = cpu->ip + ip_inc;
                break;
            }
            // TODO exceptions for near and far jump
            case 0xE9: {
                // near jump
                uint32_t ip_inc = LOAD_IP_WORD(cpu);
                cpu->ip = cpu->ip + ip_inc;
                break;
            }
            case 0xEA: {
                // far jump
                uint32_t ip = LOAD_IP_WORD(cpu);
                uint32_t cs = LOAD_IP_WORD(cpu);
                cpu->ip = ip;
                cpu->cs = cs;
                break;
            }
            case 0xEB: {
                // short jump
                uint32_t ip_inc = SEXT_8_16(LOAD_IP_BYTE(cpu));
                cpu->ip = cpu->ip + ip_inc;
                break;
            }
            case 0xF4: {
                // Halt
                printf("CPU halt\n");
                return;
            }
            case 0xF5: {
                cpu->flags.c_f = ~cpu->flags.c_f;
                break;
            }
            case 0xF6:
            case 0xF7: {
                x86_group1(cpu, opc & 1);
                break;
            }
            case 0xF8: {
                // clc
                cpu->flags.c_f = 0;
                break;
            }
            case 0xF9: {
                // stc
                cpu->flags.c_f = 1;
                break;
            }
            case 0xFA: {
                // cli
                cpu->flags.i_f = 0;
                break;
            }
            case 0xFB: {
                // sti
                cpu->flags.i_f = 1;
                break;
            }
            case 0xFC: {
                // cld
                cpu->flags.d_f = 0;
                break;
            }
            case 0xFD: {
                // std
                cpu->flags.d_f = 1;
                break;
            }
            case 0xFE: {
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                uint32_t op1 = read_mod_rm(cpu, mod_reg_rm, 0);
                uint8_t carry_save = cpu->flags.c_f;
                if (mod_reg_rm.reg) {
                    op1 = x86_sub(cpu, op1, 1, 0, 0);
                } else {
                    op1 = x86_add(cpu, op1, 1, 0, 0);
                }
                
                cpu->flags.c_f = carry_save;
                write_mod_rm(cpu, mod_reg_rm, op1, 0);
                break;
            }
            case 0xFF: {
                mod_reg_rm_t mod_reg_rm = read_mod_reg_rm(cpu);
                if (mod_reg_rm.reg == 0b110) cpu->sp -= 2;
                uint32_t op1 = read_mod_rm(cpu, mod_reg_rm, 1);
                uint32_t addr = mod_rm_effective_addr(cpu, mod_reg_rm);
                uint8_t carry_save = cpu->flags.c_f;

                switch (mod_reg_rm.reg) {
                // TODO: why does the manual say mem16 specifically but not
                // reg16/mem16? INC
                case 0b000: {
                    uint32_t res = x86_add(cpu, op1, 1, 0, 1);
                    cpu->flags.c_f = carry_save;
                    write_mod_rm(cpu, mod_reg_rm, res, 1);
                    break;
                }
                // DEC
                case 0b001: {
                    uint32_t res = x86_sub(cpu, op1, 1, 0, 1);
                    cpu->flags.c_f = carry_save;
                    write_mod_rm(cpu, mod_reg_rm, res, 1);
                    break;
                }
                // Near call absolute
                case 0b010: {
                    push_u16(cpu, cpu->ip);
                    cpu->ip = op1;
                    break;
                }
                // Far call absolute
                case 0b011: {
                    push_u16(cpu, cpu->cs);
                    push_u16(cpu, cpu->ip);
                    cpu->ip = load_u16(addr);
                    cpu->cs = load_u16(addr + 2);
                    break;
                }
                // Near jump absolute
                case 0b100: {
                    cpu->ip = op1;
                    break;
                }
                // Far jump absolute
                // TODO idk if it works the same as the far call
                case 0b101: {
                    cpu->ip = load_u16(addr);
                    cpu->cs = load_u16(addr + 2);
                    break;
                }
                // Push
                case 0b110: {
                    store_u8(SEGMENT(cpu->ss, cpu->sp+1), op1 >> 8);
                    store_u8(SEGMENT(cpu->ss, cpu->sp), op1);
                    break;
                }
                default: {
                    printf("unrecognized group 2 function: %d\n",
                           mod_reg_rm.reg);
                    exit(EXIT_FAILURE);
                }
                }
                break;
            }
            default: {
                printf("unrecognized opcode: %02x\n", opc);
                exit(1);
            }
            }
        }
        }

        io_tick(vm->cycles);
        x86_handle_interrupts(cpu);

        if (vm->opts.enable_trace) {
            #ifdef CFG_DIFF_TRACE
                    dump_cpu(cpu, &old_cpu);
                    memcpy(&old_cpu, cpu, sizeof(x86_cpu_t ));
            #else
                    dump_cpu(cpu, NULL);
            #endif
        }
    }
}
