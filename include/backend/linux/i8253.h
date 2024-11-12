#ifndef I8253_H
#define I8253_H

#include <stdbool.h>
#include <stdint.h>

typedef union {
    uint16_t x;
    struct {
        uint8_t l;
        uint8_t h;
    } __attribute__((packed)) b;
} ctr_reg_t;

typedef struct {
    uint8_t status;
    ctr_reg_t ctrs[3];
    uint16_t ctr_limits[3];
    uint16_t ctrs_latch[3];
    int access_ctrs[3];
    bool* out[3];
    bool init;
} i8253_state_t;

#define PIT_REG_TIMER0 0x40
#define PIT_REG_TIMER1 0x41
#define PIT_REG_TIMER2 0x42
#define PIT_REG_CTRL   0x43

extern i8253_state_t i8253_state;

void i8253_init(bool *timer_irq);

void i8253_cr_write(uint8_t val);

static inline uint8_t i8253_cr_read() {
    return i8253_state.status | (i8253_state.out[0] ? 0b10000000 : 0);
}

uint8_t i8253_timer_read(uint8_t ofs);
void i8253_timer_write(uint8_t ofs, uint8_t val);

void i8253_tick();



#endif // I8253_H