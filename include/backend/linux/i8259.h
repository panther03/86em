#ifndef I8259_H
#define I8259_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    uint8_t icw[4];
    int icw_ind;
    bool irqs[8];
    bool irqs_last[8];
    uint8_t isr;
    uint8_t imr;
    uint8_t irr;
} i8259_state_t;

#define I8259_ICW1_INIT     0x10
#define I8259_ICW1_ICW4     0x01
#define I8259_ICW1_SINGL    0x02     
#define I8259_ICW2_OFS_MASK 0xF8
#define I8259_EOI           0x20

#define PIC_REG_COMMAND     0x20 
#define PIC_REG_DATA        0x21


#define CALCULATE_ISR_MASK(isr) ((uint8_t)((0xFF)-(1<<(__builtin_ctz(isr)))+1))

extern i8259_state_t i8259_state;

void i8259_init();

void i8259_write_command(uint8_t val);

void i8259_write_data(uint8_t val);

uint8_t i8259_read_command();

uint8_t i8259_read_data();

void i8259_tick();

// TODO unsure if this is correct but it seems like it might
static inline bool i8259_int() {
    return (((i8259_state.irr & ~i8259_state.imr) & CALCULATE_ISR_MASK(i8259_state.isr)) != 0);
}

uint8_t i8259_ack();

#endif // I8259_H