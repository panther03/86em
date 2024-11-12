#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>

#include "util.h"
#include "vm.h"
#include "vm_mem.h"
#include "vm_io.h"

#include "opc.h"

#include "cfg.h"

typedef union
{
    uint8_t byte;
    struct
    {
        uint8_t rm : 3;
        uint8_t reg : 3;
        uint8_t mod : 2;
    } __attribute__((packed)) fields;
} mod_reg_rm_t;

typedef union
{
    x86_flags_t fl;
    uint16_t num;
} flags_union;

vm_state_t *vm_init()
{
    vm_state_t *state = (vm_state_t *)calloc(1, sizeof(vm_state_t));
    state->cs = 0xFFFF;
    state->flags.res0 = 1;
    state->flags.res1 = 0;
    state->flags.res2 = 0;
    state->flags.res3 = 0xF;
    state->cycles = 0;
    state->bkpt = -1;
    state->bkpt_clear = true;
    // No interrupt source by default
    state->int_src = -1;
    state->seg_override = -1;

    io_init();
    return state;
}

#ifdef CFG_DIFF_TRACE
#define HIGHLIGHT(a, b)     \
    if (a != b)             \
    {                       \
        printf("\e[0;31m"); \
    }
#define HIGHLIGHT_END printf("\e[0m");
#else
#define HIGHLIGHT(a, b)
#define HIGHLIGHT_END
#endif

void dump_flags(uint16_t flags, uint16_t old_flags)
{
    const char flags_symbols[] = {'C', ' ', 'P', ' ', 'A', ' ', 'Z', 'S', 'T', 'I', 'D', 'O', '\0'};
    printf("%s\n", flags_symbols);
    for (int i = 0; i < 12; i++, flags >>= 1, old_flags >>= 1)
    {
        int lsb_n = flags & 1;
#ifdef CFG_DIFF_TRACE
        int lsb_o = old_flags & 1;
        if (lsb_n != lsb_o)
        {
            printf("\e[0;31m");
        }
        putc(lsb_n ? 'X' : '-', stdout);
        if (lsb_n != lsb_o)
        {
            printf("\e[0m");
        }
#else
        putc(lsb_n ? 'X' : ' ', stdout);
#endif
    }
    putc('\n', stdout);
}

void dump_state(vm_state_t *state, vm_state_t *old_state)
{
    printf("========================================\n");
    printf("\e[0;36mIP %04x\e[0m\t", state->ip);
    HIGHLIGHT_END
    HIGHLIGHT(state->a.x, old_state->a.x)
    printf("AX %04x\t", state->a.x);
    HIGHLIGHT_END
    HIGHLIGHT(state->b.x, old_state->b.x)
    printf("BX %04x\t", state->b.x);
    HIGHLIGHT_END
    HIGHLIGHT(state->c.x, old_state->c.x)
    printf("CX %04x\t", state->c.x);
    HIGHLIGHT_END
    HIGHLIGHT(state->d.x, old_state->d.x)
    printf("DX %04x\n", state->d.x);
    HIGHLIGHT_END
    
    HIGHLIGHT(state->cs, old_state->cs)
    printf("CS %04x\t", state->cs);
    HIGHLIGHT_END
    HIGHLIGHT(state->ds, old_state->ds)
    printf("DS %04x\t", state->ds);
    HIGHLIGHT_END
    HIGHLIGHT(state->ss, old_state->ss)
    printf("SS %04x\t", state->ss);
    HIGHLIGHT_END
    HIGHLIGHT(state->es, old_state->es)
    printf("ES %04x\n", state->es);
    HIGHLIGHT_END

    
    HIGHLIGHT(state->si, old_state->si)
    printf("SI %04x\t", state->si);
    HIGHLIGHT_END
    HIGHLIGHT(state->di, old_state->di)
    printf("DI %04x\t", state->di);
    HIGHLIGHT_END
    HIGHLIGHT(state->bp, old_state->bp)
    printf("BP %04x\t", state->bp);
    HIGHLIGHT_END
    HIGHLIGHT(state->sp, old_state->sp)
    printf("SP %04x\n", state->sp);
    HIGHLIGHT_END
    
    flags_union flags_u;
    flags_u.fl = state->flags;
#ifdef CFG_DIFF_TRACE
    flags_union old_flags_u;
    old_flags_u.fl = old_state->flags;
    dump_flags(flags_u.num, old_flags_u.num);
#else
    dump_flags(flags_u.num, flags_u.num);
#endif
    printf("========================================\n");
}

// TODO: optimize these into writes/reads of offsets of the struct
// if the fields are ordered the right way then you can vmply extract the bits and use that as an offset
void write_reg_u16(vm_state_t *state, uint8_t reg, uint16_t val)
{
    assert(reg < 0b1000);
    switch (reg)
    {
    case 0b000:
        state->a.x = val;
        break;
    case 0b001:
        state->c.x = val;
        break;
    case 0b010:
        state->d.x = val;
        break;
    case 0b011:
        state->b.x = val;
        break;
    case 0b100:
        state->sp = val;
        break;
    case 0b101:
        state->bp = val;
        break;
    case 0b110:
        state->si = val;
        break;
    case 0b111:
        state->di = val;
        break;
    }
}

void write_reg_u8(vm_state_t *state, uint8_t reg, uint8_t val)
{
    assert(reg < 0b1000);
    switch (reg)
    {
    case 0b000:
        state->a.b.l = val;
        break;
    case 0b001:
        state->c.b.l = val;
        break;
    case 0b010:
        state->d.b.l = val;
        break;
    case 0b011:
        state->b.b.l = val;
        break;
    case 0b100:
        state->a.b.h = val;
        break;
    case 0b101:
        state->c.b.h = val;
        break;
    case 0b110:
        state->d.b.h = val;
        break;
    case 0b111:
        state->b.b.h = val;
        break;
    }
}

uint16_t read_reg_u16(vm_state_t *state, uint8_t reg)
{
    assert(reg < 0b1000);
    switch (reg)
    {
    case 0b000:
        return state->a.x;
    case 0b001:
        return state->c.x;
    case 0b010:
        return state->d.x;
    case 0b011:
        return state->b.x;
    case 0b100:
        return state->sp;
    case 0b101:
        return state->bp;
    case 0b110:
        return state->si;
    case 0b111:
        return state->di;
    default:
        return 0;
    }
}

uint8_t read_reg_u8(vm_state_t *state, uint8_t reg)
{
    assert(reg < 0b1000);
    switch (reg)
    {
    case 0b000:
        return state->a.b.l;
    case 0b001:
        return state->c.b.l;
    case 0b010:
        return state->d.b.l;
    case 0b011:
        return state->b.b.l;
    case 0b100:
        return state->a.b.h;
    case 0b101:
        return state->c.b.h;
    case 0b110:
        return state->d.b.h;
    case 0b111:
        return state->b.b.h;
    default:
        return 0;
    }
}

uint16_t read_seg(vm_state_t *state, uint8_t sr)
{
    switch (sr)
    {
    case 0b00:
        return state->es;
    case 0b01:
        return state->cs;
    case 0b10:
        return state->ss;
    case 0b11:
        return state->ds;
    default:
        return 0;
    }
}

void write_seg(vm_state_t *state, uint8_t sr, uint16_t val)
{
    switch (sr)
    {
    case 0b00:
        state->es = val;
        break;
    case 0b01:
        state->cs = val;
        break;
    case 0b10:
        state->ss = val;
        break;
    case 0b11:
        state->ds = val;
        break;
    }
}

uint32_t get_16b_mem_base(vm_state_t *state, uint8_t rm)
{
    uint32_t base = (rm & 0b1) ? state->di : state->si;
    switch (rm & 0b110)
    {
    case 0b000:
        return base + state->b.x;
    case 0b010:
        return base + state->bp;
    case 0b100:
        return base;
    // 0b110
    default:
        return (rm & 0b1) ? state->b.x : LOAD_IP_WORD(state);
    }
}

static inline uint16_t x86_get_data_segment(vm_state_t *state) {
    // Segment override is an offset
    // -1 on the default (DS), 0 - 3 otherwise
    // -1 wraps back around to 3 if we take mod 4 (& 0b11)
    uint16_t segment = *(&state->es + ((4 + state->seg_override) & 0b11));
    state->seg_override_clear = 0;
    return segment;
}

static inline uint32_t mod_rm_effective_addr(vm_state_t *state, uint8_t mod, uint8_t rm) {
    uint32_t base = get_16b_mem_base(state, rm);
    if (mod == 0b01)
    {
        base += LOAD_IP_BYTE;
    }
    else if (mod == 0b10)
    {
        base += LOAD_IP_WORD(state);
    }
    printf("ovr: %d\n", state->seg_override);
    assert(-1 <= state->seg_override && state->seg_override <= 3);
    return SEGMENT(x86_get_data_segment(state), base);
}

// TODO these should just take the whole mod_reg_rm byte, splitting it up like this doesn't make sense
uint32_t read_mod_rm(vm_state_t *state, uint8_t mod, uint8_t rm, uint8_t is_16)
{
    uint32_t reg = is_16 ? read_reg_u16(state, rm) : read_reg_u8(state, rm);
    if (mod == 0b11)
    {
        // register only
        return reg;
    }
    else
    {
        return load_u16(mod_rm_effective_addr(state, mod, rm));
    }
}



void write_mod_rm(vm_state_t *state, uint8_t mod, uint8_t rm, uint32_t val, uint8_t is_16)
{
    if (mod == 0b11)
    {
        // register only
        if (is_16)
        {
            write_reg_u16(state, rm, (uint16_t)val);
        }
        else
        {
            write_reg_u8(state, rm, (uint8_t)val);
        }
    }
    else
    {
        store_u16(mod_rm_effective_addr(state, mod, rm), val);
    }
}

static inline uint32_t parity(uint32_t op)
{
    op ^= op >> 16;
    op ^= op >> 8;
    op ^= op >> 4;
    op ^= op >> 2;
    op ^= op >> 1;
    return (~op) & 1;
}

uint32_t x86_add(vm_state_t *state, uint32_t op1, uint32_t op2, uint32_t carry, uint8_t is_16)
{
    uint32_t res = op1 + op2 + carry;
    uint32_t op_size = is_16 ? 16 : 8;
    uint32_t top_bit = 1 << (op_size - 1);
    uint32_t mask = (1 << (op_size)) - 1;
    state->flags.c_f = (res >> op_size);
    state->flags.s_f = (res >> (op_size - 1));
    state->flags.o_f = ((~(op1 ^ op2) & (op1 ^ res)) & top_bit) ? 1 : 0;
    state->flags.a_f = ((op1 & 0xF) + (op2 & 0xF) + carry) & 0x10 ? 1 : 0;
    res = res & mask;
    state->flags.p_f = parity(res);
    state->flags.z_f = res == 0;
    return res;
}

uint32_t x86_sub(vm_state_t *state, uint32_t op1, uint32_t op2, uint32_t carry, uint8_t is_16)
{
    uint32_t op_size = is_16 ? 16 : 8;
    uint32_t mask = (1 << (op_size)) - 1;
    op2 = ((op2 ^ mask) + 1) & mask;
    uint32_t res = op1 + op2 - carry;
    uint32_t top_bit = 1 << (op_size - 1);
    state->flags.c_f = (res >> op_size);
    state->flags.s_f = (res >> (op_size - 1));
    state->flags.o_f = ((~(op1 ^ op2) & (op1 ^ res)) & top_bit) ? 1 : 0;
    state->flags.a_f = ((op1 & 0xF) + (op2 & 0xF) - carry) & 0x10 ? 1 : 0;
    res = res & mask;
    state->flags.p_f = parity(res);
    state->flags.z_f = res == 0;
    return res;
}

uint32_t x86_or(vm_state_t *state, uint32_t op1, uint32_t op2, uint8_t is_16)
{
    uint32_t op_size = (is_16 ? 16 : 8);
    uint32_t res = (op1 | op2) & ((1 << op_size) - 1);
    state->flags.c_f = 0;
    state->flags.o_f = 0;
    state->flags.a_f = 0;
    state->flags.p_f = parity(res);
    state->flags.s_f = (res >> (op_size - 1));
    state->flags.z_f = res == 0;
    return res;
}

uint32_t x86_and(vm_state_t *state, uint32_t op1, uint32_t op2, uint8_t is_16)
{
    uint32_t op_size = (is_16 ? 16 : 8);
    uint32_t res = (op1 & op2) & ((1 << op_size) - 1);
    state->flags.c_f = 0;
    state->flags.o_f = 0;
    state->flags.a_f = 0;
    state->flags.p_f = parity(res);
    state->flags.s_f = (res >> (op_size - 1));
    state->flags.z_f = res == 0;
    return res;
}

uint32_t x86_xor(vm_state_t *state, uint32_t op1, uint32_t op2, uint8_t is_16)
{
    uint32_t op_size = (is_16 ? 16 : 8);
    uint32_t res = (op1 ^ op2) & ((1 << op_size) - 1);
    state->flags.c_f = 0;
    state->flags.o_f = 0;
    state->flags.a_f = 0;
    state->flags.p_f = parity(res);
    state->flags.s_f = (res >> (op_size - 1));
    state->flags.z_f = res == 0;
    return res;
}

// https://c9x.me/x86/html/file_module_x86_id_273.html
uint32_t x86_rotate(vm_state_t *state, uint8_t op, uint32_t x, uint32_t shamt, uint8_t is_16)
{
    uint8_t op_size = is_16 ? 16 : 8;
    uint8_t rc_temp_shamt = shamt % (is_16 ? 17 : 9);
    uint8_t ro_temp_shamt = shamt % (is_16 ? 16 : 8);
    switch (op)
    {
    case 0b010:
    {
        while (rc_temp_shamt != 0)
        {
            uint8_t temp_cf = (x >> (op_size - 1));
            x = (x << 1) + state->flags.c_f;
            state->flags.c_f = temp_cf;
            rc_temp_shamt -= 1;
        }
        if (shamt == 1)
        {
            state->flags.o_f = (x >> (op_size - 1)) ^ state->flags.c_f;
        }
        break;
    }
    case 0b011:
    {
        if (shamt == 1)
        {
            state->flags.o_f = (x >> (op_size - 1)) ^ state->flags.c_f;
        }
        while (rc_temp_shamt != 0)
        {
            uint8_t temp_cf = x & 1;
            // todo op_size correct here?
            x = (x >> 1) + (state->flags.c_f << op_size);
            state->flags.c_f = temp_cf;
            rc_temp_shamt -= 1;
        }
        break;
    }
    case 0b000:
    {
        while (ro_temp_shamt != 0)
        {
            x = (x << 1) + state->flags.c_f;
            ro_temp_shamt -= 1;
        }
        state->flags.c_f = x & 1;
        if (shamt == 1)
        {
            state->flags.o_f = (x >> (op_size - 1)) ^ state->flags.c_f;
        }
        break;
    }
    case 0b001:
    {
        while (ro_temp_shamt != 0)
        {
            // todo op_size correct here?
            x = (x >> 1) + (state->flags.c_f << op_size);
            ro_temp_shamt -= 1;
        }
        state->flags.c_f = x >> (op_size - 1);
        if (shamt == 1)
        {
            state->flags.o_f = (x >> (op_size - 1)) ^ (x >> (op_size - 2));
        }
        break;
    }
    }
    state->flags.p_f = parity(x);
    state->flags.s_f = (x >> (op_size - 1));
    state->flags.z_f = x == 0;
    return x;
}

// https://c9x.me/x86/html/file_module_x86_id_285.html
uint32_t x86_shift(vm_state_t *state, uint8_t op, uint32_t x, uint32_t shamt, uint8_t is_16)
{
    uint8_t op_size = is_16 ? 16 : 8;
    uint32_t carry_shamt = (op_size - shamt > 0) ? op_size - shamt : 0;

    printf("x: %d", x);
    switch (op)
    {
    // SHL
    case 0b100:
    {
        state->flags.c_f = (x >> carry_shamt);
        x <<= shamt;
        if (shamt == 1)
        {
            state->flags.o_f = (x >> (op_size - 1)) ^ state->flags.c_f;
        }
        break;
    }
    // SHR
    case 0b101:
    {
        state->flags.c_f = (x >> (shamt - 1)) & 0x1;
        if (shamt == 1)
        {
            state->flags.o_f = (x >> (op_size - 1));
        }
        x >>= shamt;
        break;
    }
    // SAR
    case 0b111:
    {
        state->flags.c_f = (x >> (shamt - 1)) & 0x1;
        // convert to signed
        x = (((int32_t)x) << (32 - op_size)) >> (32 - op_size);
        x >>= shamt;
        if (shamt == 1)
        {
            state->flags.o_f = 0;
        }
        break;
    }
    }
    state->flags.p_f = parity(x);
    state->flags.s_f = (x >> (op_size - 1));
    state->flags.z_f = x == 0;
    return x;
}

uint32_t x86_arith(vm_state_t *state, uint8_t fnc, uint32_t op1, uint32_t op2, uint8_t s)
{
    switch (fnc)
    {
    case 0x0:
        return x86_add(state, op1, op2, 0, s);
    case 0x1:
        return x86_or(state, op1, op2, s);
    case 0x2:
        return x86_add(state, op1, op2, state->flags.c_f, s);
    case 0x3:
        return x86_sub(state, op1, op2, state->flags.c_f, s);
    case 0x4:
        return x86_and(state, op1, op2, s);
    case 0x5:
        return x86_sub(state, op1, op2, 0, s);
    case 0x6:
        return x86_xor(state, op1, op2, s);
    case 0x7:
        x86_sub(state, op1, op2, 0, s);
        return op1;
    default:
    {
        printf("unrecognized arithmetic function %d", fnc);
        exit(-1);
    }
    }
}

static inline void x86_string_insn(vm_state_t *state, uint8_t opc) {
    uint8_t sz = opc & 0x1;
    int src_addr = SEGMENT(x86_get_data_segment(state), state->si);
    int dest_addr = SEGMENT(state->es, state->di);
    int si_ofs = 0;
    int di_ofs = 0;
    switch(opc) {
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
            x86_sub(state, load_u8(src_addr), load_u8(dest_addr), 0, sz);
            si_ofs = 1;
            di_ofs = 1;
            break;
        }
        // CMPS (16-bit)
        case 0xA7: {
            x86_sub(state, load_u16(src_addr), load_u16(dest_addr), 0, sz);
            si_ofs = 2;
            di_ofs = 2;
            break;
        }
        // STOS (8-bit) 
        case 0xAA: {
            store_u8(dest_addr, state->a.b.l);
            si_ofs = 1;
            di_ofs = 1;
            break;
        }
        // STOS (16-bit)
        case 0xAB: {
            store_u8(dest_addr, state->a.x);
            si_ofs = 2;
            di_ofs = 2;
            break;
        }
        // LODS (8-bit)
        case 0xAC: {
            state->a.b.l = load_u8(src_addr);
            si_ofs = 1;
            break;
        }
        // LODS (16-bit)
        case 0xAD: {
            state->a.x = load_u16(src_addr);
            si_ofs = 2;
            break;
        }
        // SCAS (8-bit) 
        case 0xAE: {
            x86_sub(state, state->a.b.l, load_u8(dest_addr), 0, sz);
            si_ofs = 1;
            di_ofs = 1;
            break;
        }
        // SCAS (16-bit) 
        case 0xAF: {
            x86_sub(state, state->a.x, load_u8(dest_addr), 0, sz);
            si_ofs = 2;
            di_ofs = 2;
            break;
        }
    }
    if (state->flags.d_f) {
        state->si -= si_ofs;
        state->di -= di_ofs;
    } else {
        state->si += si_ofs;
        state->di += di_ofs;
    }
}

static inline void x86_group1(vm_state_t *state, uint8_t is_16) {
    mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
    uint32_t op1 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, is_16);

    switch (mod_reg_rm.fields.reg) {
        // TEST
        case 0b000: {
            uint32_t imm = is_16 ? LOAD_IP_WORD(state) : LOAD_IP_BYTE;
            x86_and(state, op1, imm, is_16);
            break;
        }
        // NOT
        case 0b010: {
            write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, ~op1, is_16);
            break;
        }
        // NEG
        case 0b011: {
            // TODO is the carry flag logic different here or does this work
            op1 = x86_sub(state, 0, op1, 0, is_16);
            write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, op1, is_16);
            break;
        }
        // MUL
        case 0b100: {
            if (is_16) {
                uint32_t prod = state->a.x * op1;
                state->a.x = prod;
                state->d.x = prod >> 16;
                state->flags.o_f = state->flags.c_f = (state->a.x != 0);
            } else {
                state->a.x = state->a.b.l * op1;
                state->flags.o_f = state->flags.c_f = (state->a.b.h != 0);
            }
            break;
        }
        // IMUL
        case 0b101: {
            if (is_16) {
                int32_t ax_s = SEXT_16_32(state->a.x); 
                int32_t prod = ax_s * SEXT_16_32(op1);
                state->a.x = prod;
                state->d.x = prod >> 16;
                state->flags.o_f = state->flags.c_f = (state->a.x != prod);
            } else {
                state->a.x = SEXT_8_16(state->a.b.l) * SEXT_8_16(op1);
                state->flags.o_f = state->flags.c_f = (state->a.b.l == state->a.x);
            }
            break;
        }
        // DIV/IDIV
        case 0b110:
        case 0b111: {
            // TODO divide exceptions
            uint32_t divisor = (mod_reg_rm.fields.reg & 1) ? (SEXT_16_32(op1)) : (int32_t) op1;
            uint32_t dividend = (state->d.x << 16) + state->a.x;
            uint32_t remainder = dividend % divisor;
            dividend /= divisor;
            state->a.x = dividend;
            state->d.x = remainder;
            break;
        }
    }
}

// TODO: make this into a table
uint8_t insn_mode(uint8_t opc)
{
    if (((opc & 0x4) == 0) && ((opc & 0xF0) <= 0x30 || (opc & 0xFC) == 0x88))
    {
        // arithmetic type instruction
        return 1;
    }
    else if ((opc & 0xC6) == 0x04)
    {
        // AX/AL immediate instruction
        return 2;
    }
    else if ((opc & 0xF8) == 0x40)
    {
        // Inc
        return 3;
    }
    else if ((opc & 0xF8) == 0x48)
    {
        // Dec
        return 4;
    }
    else if ((opc & 0xF8) == 0x50)
    {
        // Push
        return 5;
    }
    else if ((opc & 0xF8) == 0x58)
    {
        // Pop
        return 6;
    }
    else if ((opc & 0xF8) == 0x90)
    {
        // XCHG
        return 7;
    }
    else if ((opc & 0xF8) == 0xB8)
    {
        // MOV (16-bit)
        return 8;
    }
    else if ((opc & 0xF8) == 0xB0)
    {
        // MOV (8-bit)
        return 9;
    }
    else if ((opc & 0xF0) == 0x70 || opc == 0xE3)
    {
        // Branch
        return 10;
    }
    else if ((opc & 0xFC) == 0x80)
    {
        // Immediate-type
        return 11;
    }
    else if ((opc & 0xFC) == 0xD0)
    {
        // Shift
        return 12;
    }
    else if ((opc & 0xF4) == 0xA4 || ((opc & 0xFE) == 0xAA))
    {
        // String instruction
        return 13;
    } else if ((opc & 0xFC) == 0xF0 || (opc & 0xE7) == 0x26) {
        // Prefix
        return 14;
    }

    return 0;
}

static inline void x86_handle_interrupts(vm_state_t *state) {
    int int_src = state->int_src;

    if (int_src >= 0)  goto INTERRUPT_FOUND;

    // NMI is not handled, nothing critical uses NMI in IBM PC

    if (state->flags.i_f && io_int_poll()) {
        int_src = io_int_ack();
        goto INTERRUPT_FOUND;
    }

    if (state->flags.t_f) {
        int_src = 1;
        goto INTERRUPT_FOUND;
    }

    // No interrupts
    return;

INTERRUPT_FOUND:;
    flags_union flags;
    uint8_t temp_tf;

    // TODO?
    state->int_src = -1;

    do {
        // PUSH FLAGS
        flags.fl = state->flags;
        push_u16(state, flags.num);
        // LET TEMP = TF
        temp_tf = flags.fl.t_f;
        // CLEAR IF & TF
        state->flags.t_f = 0;
        state->flags.i_f = 0;
        // PUSH CS & IP
        push_u16(state, state->cs);
        push_u16(state, state->ip);
        // CALL INTERRUPT SERVICE ROUTINE
        assert(int_src < 256);
        state->ip = load_u16(int_src * 4);
        state->cs = load_u16(int_src * 4 + 2);
    } while (temp_tf); // add NMI check here to enable NMI functionality
}

void vm_run(vm_state_t *state, int max_cycles)
{
    int prog_end = prog_info.prog_start + prog_info.prog_size;
    int cyc_start = state->cycles;
#ifdef CFG_DIFF_TRACE
    vm_state_t old_state = *state;
#endif
    int addr;
    while (((addr = SEGMENT(state->cs, state->ip)) < prog_end) && (max_cycles < 0 || ((state->cycles - cyc_start) < ((uint64_t)max_cycles))))
    {
        if (addr == state->bkpt && !state->bkpt_clear)
        {
            printf("Breakpoint hit at %08x\n", addr);
            state->bkpt_clear = true;
            return;
        }
        if ((state->bkpt > 0) && state->bkpt_clear)
        {
            state->bkpt_clear = false;
        }
        uint8_t opc = LOAD_IP_BYTE;
        uint8_t mode;
        uint8_t pfx = 0;
        state->cycles++;

        // Get all prefixes
        while ((mode = insn_mode(opc)) == 14) {
            if ((opc & 0xFC) == 0xF0) {
                // ignore LOCK prefix
                pfx = (opc & 2) ? opc : 0;
            } else {
                // Must be a segment override
                state->seg_override = (opc >> 3) & 0b11;
            }
            opc = LOAD_IP_BYTE;
        }

        printf("Op: %02x; Pfx: %02x; Mode: %d\n", opc, pfx, mode);

        switch (insn_mode(opc))
        {
        case 1:
        {
            mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
            uint8_t s = opc & 0x1;
            uint8_t d = opc & 0x2;

            uint32_t op1 = s ? read_reg_u16(state, mod_reg_rm.fields.reg) : read_reg_u8(state, mod_reg_rm.fields.reg);
            uint32_t op2 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, s);

            if (!d)
            {
                // swap
                uint32_t temp;
                temp = op1;
                op1 = op2;
                op2 = temp;
            }

            if ((opc & 0xFC) == 0x88)
            {
                op1 = op2;
            }
            else
            {
                op1 = x86_arith(state, (opc >> 3) & 0x7, op1, op2, s);
            }

            if (!d)
            {
                write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, op1, s);
            }
            else
            {
                if (s)
                {
                    write_reg_u16(state, mod_reg_rm.fields.reg, op1);
                }
                else
                {
                    write_reg_u8(state, mod_reg_rm.fields.reg, op1);
                }
            }
            break;
        }
        case 2:
        {
            uint8_t s = opc & 0x1;
            uint32_t imm = 0;
            uint32_t op1 = 0;
            if (s)
            {
                imm = LOAD_IP_WORD(state);
                op1 = state->a.x;
            }
            else
            {
                imm = LOAD_IP_BYTE;
                op1 = state->a.b.l;
            }
            op1 = x86_arith(state, (opc >> 3) & 0x7, op1, imm, s);

            if (s)
            {
                state->a.x = op1;
            }
            else
            {
                state->a.b.l = op1;
            }
            break;
        }
        case 3:
        {
            uint8_t reg = opc & 0x7;
            uint32_t res = read_reg_u16(state, reg) + 1;
            write_reg_u16(state, reg, res);
            break;
        }
        case 4:
        {
            uint8_t reg = opc & 0x7;
            uint32_t res = read_reg_u16(state, reg) - 1;
            write_reg_u16(state, reg, res);
            break;
        }
        case 5:
        {
            uint8_t reg = opc & 0x7;
            uint32_t res = read_reg_u16(state, reg);
            push_u16(state, res);
            break;
        }
        case 6:
        {
            uint8_t reg = opc & 0x7;
            write_reg_u16(state, reg, pop_u16(state));
            break;
        }
        case 7:
        {
            uint8_t reg = opc & 0x7;
            uint32_t op1 = read_reg_u16(state, reg);
            write_reg_u16(state, reg, state->a.x);
            state->a.x = op1;
            break;
        }
        case 8:
        {
            uint8_t reg = opc & 0x7;
            uint32_t imm = LOAD_IP_WORD(state);
            write_reg_u16(state, reg, imm);
            break;
        }
        case 9:
        {
            uint8_t reg = opc & 0x7;
            uint32_t imm = LOAD_IP_BYTE;
            write_reg_u8(state, reg, imm);
            break;
        }
        case 10:
        {
            uint32_t imm = SEXT_8_16(LOAD_IP_BYTE);
            uint32_t cond = 0;
            switch (opc & 0b1110)
            {
            // J0
            case 0x0:
                cond = (state->flags.o_f);
                break;
            // JB
            case 0x2:
                cond = (state->flags.c_f);
                break;
            // JE
            case 0x4:
                cond = (state->flags.z_f);
                break;
            // JBE
            case 0x6:
                cond = (state->flags.c_f) | (state->flags.z_f);
                break;
            // JS
            case 0x8:
                cond = (state->flags.s_f);
                break;
            // JP
            case 0xA:
                cond = (state->flags.p_f);
                break;
            // JL
            case 0xC:
                cond = (state->flags.s_f) ^ (state->flags.o_f);
                break;
            // JLE
            case 0xE:
                cond = (state->flags.z_f) | ((state->flags.s_f) ^ (state->flags.o_f));
                break;
            }
            cond ^= (opc & 0x1);
            if (opc == 0xE3)
            {
                cond = (state->c.x == 0);
            }
            if (cond)
            {
                state->ip = state->ip + imm;
            }
            break;
        }
        case 11:
        {
            uint8_t s = opc & 0x1;
            uint8_t x = (opc >> 1) & 0x1;
            mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
            uint32_t imm = s ? (x ? (SEXT_8_16(LOAD_IP_BYTE)) : LOAD_IP_WORD(state)) : (LOAD_IP_BYTE);
            uint32_t op1 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, s);
            op1 = x86_arith(state, mod_reg_rm.fields.reg, op1, imm, s);
            write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, op1, s);
            break;
        }
        case 12:
        {
            uint8_t s = opc & 0x1;
            uint32_t shamt = (opc & 0x2) ? state->c.b.l : 1;
            mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
            uint32_t op1 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, s);
            if ((mod_reg_rm.fields.reg & 0b100) == 0b100)
            {
                op1 = x86_shift(state, mod_reg_rm.fields.reg, op1, shamt, s);
            }
            else
            {
                op1 = x86_rotate(state, mod_reg_rm.fields.reg, op1, shamt, s);
            }
            write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, op1, s);
            break;
        }
        case 13: {
            // String instruction
            bool compare_enable = ((opc & 0x06) == 0x6);
            uint16_t counter = pfx ? 1 : state->c.x;
            while (counter != 0) {
                x86_string_insn(state, opc);
                counter--;
                if (compare_enable && ((pfx & 0x1) ^ state->flags.z_f)) break;
            }
            if (pfx) {
                state->c.x = counter;
            }
            break;
        }
        default:
        {
            switch (opc)
            {
            case 0x06: { push_u16(state, state->es); break; }
            case 0x07: { state->es = pop_u16(state); break; }
            case 0x16: { push_u16(state, state->ss); break; }
            case 0x17: { state->ss = pop_u16(state); break; }
            case 0x0E: { push_u16(state, state->cs); break; }
            case 0x1F: { state->ds = pop_u16(state); break; }
            case 0x84: 
            {
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                uint32_t op1 = read_reg_u8(state, mod_reg_rm.fields.reg);
                uint32_t op2 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, 0);
                x86_and(state, op1, op2, 0);
                break;
            }
            case 0x85: 
            {
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                uint32_t op1 = read_reg_u16(state, mod_reg_rm.fields.reg);
                uint32_t op2 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, 1);
                x86_and(state, op1, op2, 1);
                break;
            }
            case 0x86: 
            {
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                uint32_t op1 = read_reg_u8(state, mod_reg_rm.fields.reg);
                uint32_t op2 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, 0);
                write_reg_u8(state, mod_reg_rm.fields.reg, op2);
                write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, op1, 0);
                break;
            }
            case 0x87:
            {
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                uint32_t op1 = read_reg_u16(state, mod_reg_rm.fields.reg);
                uint32_t op2 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, 1);
                write_reg_u16(state, mod_reg_rm.fields.reg, op2);
                write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, op1, 1);
                break;
            }
            case 0x8C:
            {
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                uint16_t val = read_seg(state, mod_reg_rm.fields.reg);
                write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, val, 1);
                break;
            }
            case 0x8D:
            {
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                // TODO: i guess this should be the case? 
                // no point in doing effective address of a register
                assert(mod_reg_rm.fields.mod != 0b11);
                uint32_t base = mod_rm_effective_addr(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm);
                write_reg_u16(state, mod_reg_rm.fields.reg, base);
                break;
            }
            case 0x8E:
            {
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                uint32_t op = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, 1);
                write_seg(state, mod_reg_rm.fields.reg, op);
                break;
            }
            case 0x8F:
            {
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                uint16_t tos = pop_u16(state);
                write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, tos, 1);
                break;
            }
            // CBW
            case 0x98:
            {
                state->a.x = SEXT_8_16(state->a.b.l);
                break;
            }
            // CWD
            case 0x99:
            {
                state->d.x = (state->a.x & 0x80) ? 0xFF : 0;
                break;
            }
            // CALL (FAR, ABSOLUTE)
            case 0x9A:
            {
                uint32_t ip = LOAD_IP_WORD(state);
                uint32_t cs = LOAD_IP_WORD(state);
                push_u16(state, state->cs);
                push_u16(state, state->ip);
                state->ip = ip;
                state->cs = cs;
                break;
            }
            case 0x9C:
            {
                flags_union flags;
                flags.fl = state->flags;
                push_u16(state, flags.num);
                break;
            }
            case 0x9D:
            {
                flags_union flags;
                flags.num = pop_u16(state);
                state->flags = flags.fl;
                break;
            }
            case 0x9E:
            {
                flags_union ah_flags;
                ah_flags.num = state->a.b.h;
                state->flags.s_f = ah_flags.fl.s_f;
                state->flags.z_f = ah_flags.fl.z_f;
                state->flags.a_f = ah_flags.fl.a_f;
                state->flags.p_f = ah_flags.fl.p_f;
                state->flags.c_f = ah_flags.fl.c_f;
                break;
            }
            case 0x9F:
            {
                flags_union ah_flags;
                ah_flags.fl = state->flags;
                state->a.b.h = (ah_flags.num) & 0xFF;
                break;
            }
            case 0xA0:
            {
                uint32_t offset = LOAD_IP_BYTE;
                uint32_t addr = SEGMENT(x86_get_data_segment(state), offset);
                state->a.b.l = load_u8(addr);
                break;
            }
            case 0xA1:
            {
                uint32_t offset = LOAD_IP_WORD(state);
                uint32_t addr = SEGMENT(x86_get_data_segment(state), offset);
                state->a.x = load_u16(addr);
                break;
            }
            case 0xA2:
            {
                uint32_t offset = LOAD_IP_BYTE;
                uint32_t addr = SEGMENT(x86_get_data_segment(state), offset);
                store_u8(addr, state->a.b.l);
                break;
            }
            case 0xA3:
            {
                uint32_t offset = LOAD_IP_WORD(state);
                uint32_t addr = SEGMENT(x86_get_data_segment(state), offset);
                store_u16(addr, state->a.x);
                break;
            }
            case 0xA8: 
            {
                uint32_t imm = LOAD_IP_BYTE;
                x86_and(state, state->a.b.l, imm, 0);
                break;
            }
            case 0xA9: 
            {
                uint32_t imm = LOAD_IP_WORD(state);
                x86_and(state, state->a.b.l, imm, 1);
                break;
            }
            case 0xC2:
            {
                uint32_t imm = LOAD_IP_WORD(state);
                state->ip = pop_u16(state);
                state->sp += imm;
                break;
            }
            case 0xC3:
            {
                state->ip = pop_u16(state);
                break;
            }
            // LES
            case 0xC4: 
            // LDS
            case 0xC5:
            {
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                uint32_t op1 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, 1);
                write_reg_u16(state, mod_reg_rm.fields.reg, load_u16(op1));
                if (opc & 1) {
                    state->ds = load_u16(op1+2);
                } else {
                    state->es = load_u16(op1+2);
                }
                break;
            }
            case 0xC6: 
            case 0xC7:
            {
                uint8_t s = opc & 1;
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                uint32_t imm = s ? LOAD_IP_WORD(state) : LOAD_IP_BYTE;
                write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.reg, imm,s);
                break;
            }
            case 0xCA:
            {
                state->ip = pop_u16(state);
                state->cs = pop_u16(state);
                break;
            }
            case 0xCB:
            {
                uint32_t imm = LOAD_IP_WORD(state);
                state->ip = pop_u16(state);
                state->cs = pop_u16(state);
                state->sp += imm;
                break;
            }
            case 0xCC:
            {
                assert(state->int_src < 0);
                state->int_src = 3;
                break;
            }
            case 0xCD:
            {
                assert(state->int_src < 0);
                uint32_t imm = LOAD_IP_BYTE;
                state->int_src = imm;
                break;
            }
            case 0xCE:
            {
                assert(state->int_src < 0);
                if (state->flags.o_f) {
                    state->int_src = 4;
                }
                break;
            }
            case 0xCF:
            {
                state->ip = pop_u16(state);
                state->cs = pop_u16(state);
                flags_union flags;
                flags.num = pop_u16(state);
                state->flags = flags.fl;
                break;
            }
            // XLATB
            case 0xD7:
            {
                state->a.b.l = load_u8(SEGMENT(x86_get_data_segment(state), state->b.x) + state->a.b.l);
                break;
            }
            // LOOPNZ
            case 0xE0: {
                uint8_t ofs = LOAD_IP_BYTE;
                state->c.x--;
                if ((state->flags.z_f == 0) && state->c.x) {
                    state->ip += SEXT_8_16(ofs);
                }
                break;
            }
            // LOOPZ
            case 0xE1: {
                uint8_t ofs = LOAD_IP_BYTE;
                state->c.x--;
                if (state->flags.z_f && state->c.x) {
                    state->ip += SEXT_8_16(ofs);
                }
                break;
            }
            // LOOP
            case 0xE2: {
                uint8_t ofs = LOAD_IP_BYTE;
                state->c.x--;
                if (state->c.x) {
                    state->ip += SEXT_8_16(ofs);
                }
                break;
            }
            case 0xE4:
            {
                uint32_t imm = LOAD_IP_BYTE;
                state->a.b.l = io_read_u16(imm);
                break;
            }
            case 0xE5:
            {
                uint32_t imm = LOAD_IP_BYTE;
                state->a.x = io_read_u16(imm);
                break;
            }
            case 0xE6:
            {
                uint32_t imm = LOAD_IP_BYTE;
                io_write_u16(imm, state->a.b.l);
                break;
            }
            case 0xE7:
            {
                uint32_t imm = LOAD_IP_BYTE;
                io_write_u16(imm, state->a.x);
                break;
            }
            case 0xEC:
            {
                state->a.b.l = io_read_u16(state->d.x);
                break;
            }
            case 0xED:
            {
                state->a.x = io_read_u16(state->d.x);
                break;
            }
            case 0xEE:
            {
                io_write_u16(state->d.x, state->a.b.l);
                break;
            }
            case 0xEF:
            {
                io_write_u16(state->d.x, state->a.x);
                break;
            }
            case 0xE8:
            {
                // near call
                uint32_t ip_inc = LOAD_IP_WORD(state);
                // TODO: exceptions
                push_u16(state, state->ip);
                state->ip = state->ip + ip_inc;
                break;
            }
            // TODO exceptions for near and far jump
            case 0xE9:
            {
                // near jump
                uint32_t ip_inc = LOAD_IP_WORD(state);
                state->ip = state->ip + ip_inc;
                break;
            }
            case 0xEA:
            {
                // far jump
                uint32_t ip = LOAD_IP_WORD(state);
                uint32_t cs = LOAD_IP_WORD(state);
                state->ip = ip;
                state->cs = cs;
                break;
            }
            case 0xEB:
            {
                // short jump
                uint32_t ip_inc = SEXT_8_16(LOAD_IP_BYTE);
                state->ip = state->ip + ip_inc;
                break;
            }
            case 0xF4:
            {
                // Halt
                printf("CPU halt\n");
                return;
            }
            case 0xF5:
            {
                state->flags.c_f = ~state->flags.c_f;
                break;
            }
            case 0xF6:
            case 0xF7: {
                x86_group1(state, opc & 1);
                break;
            }
            case 0xF8:
            {
                // clc
                state->flags.c_f = 0;
                break;
            }
            case 0xF9:
            {
                // stc
                state->flags.c_f = 1;
                break;
            }
            case 0xFA:
            {
                // cli
                state->flags.i_f = 0;
                break;
            }
            case 0xFB:
            {
                // sti
                state->flags.i_f = 1;
                break;
            }
            case 0xFC:
            {
                // cld
                state->flags.d_f = 0;
                break;
            }
            case 0xFD:
            {
                // std
                state->flags.d_f = 1;
                break;
            }
            case 0xFE:
            {
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                uint32_t op1 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, 0);
                op1 = op1 + mod_reg_rm.fields.reg ? -1 : 1;
                write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, op1, 0);
                break;
            }
            case 0xFF:
            {
                mod_reg_rm_t mod_reg_rm = (mod_reg_rm_t)LOAD_IP_BYTE;
                uint32_t op1 = read_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.rm, 1);                

                switch (mod_reg_rm.fields.reg) {
                    // TODO: why does the manual say mem16 specifically but not reg16/mem16?
                    // INC
                    case 0b000: {
                        write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.reg, op1 + 1, 1);
                        break;
                    }
                    // DEC
                    case 0b001: {
                        write_mod_rm(state, mod_reg_rm.fields.mod, mod_reg_rm.fields.reg, op1 - 1, 1);
                        break;
                    }
                    // Near call absolute
                    case 0b010: {
                        push_u16(state, state->ip);
                        state->ip = op1;
                        break;
                    }
                    // Far call absolute
                    case 0b011: {
                        push_u16(state, state->cs);
                        push_u16(state, state->ip);
                        state->ip = load_u16(op1);
                        state->cs = load_u16(op1+2);
                        break;
                    }
                    // Near jump absolute
                    case 0b100: {
                        state->ip = op1;
                        break;
                    }
                    // Far jump absolute 
                    // TODO idk if it works the same as the far call
                    case 0b101: {
                        state->ip = load_u16(op1);
                        state->cs = load_u16(op1+2);
                        break;
                    }
                    // Push
                    case 0b110: {
                        push_u16(state, op1);
                        break;
                    }
                    default: {
                        printf("unrecognized group 2 function: %d\n", mod_reg_rm.fields.reg);
                        exit(EXIT_FAILURE);
                    }
                }
                break;
            }
            default:
            {
                printf("unrecognized opcode: %02x\n", opc);
                exit(1);
            }
            }
        }
        }

        io_tick(state->cycles);

#ifdef CFG_DIFF_TRACE
        dump_state(state, &old_state);
        memcpy(&old_state, state, sizeof(vm_state_t));
#else
        dump_state(state, NULL);
#endif
    }
}

