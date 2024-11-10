#include "i8253.h"

#include <string.h>
#include <assert.h>

i8253_state_t i8253_state;

void i8253_init(bool *timer_irq) {
    memset(&i8253_state, 0, sizeof(i8253_state_t));
    i8253_state.out[0] = timer_irq;

    i8253_state.status = 0b01000000;

    if (timer_irq) {
        *timer_irq = false;
    }
}

void i8253_cr_write(uint8_t val) {
    i8253_state.init = true;
    // only care about counter 0
    if ((val & 0b11000000) == 0) {
        if ((val & 0b00110000) == 0) {
            // Latch mode
            i8253_state.ctrs_latch[0] = i8253_state.ctrs[0].x;
            i8253_state.status = (i8253_state.status & 0b11001111) | (val & 0b00110000);
        } else {
            i8253_state.status = (i8253_state.status & 0b11000000) | (val & 0b00111111);
        }
        i8253_state.access_ctrs[0] = 0;
    }
}

void i8253_timer_write(uint8_t ofs, uint8_t val) {
    if (ofs == 0) {
        switch (i8253_state.status & 0b00110000) {
            // TODO assuming you can't write in latch mode 
            // TODO stopping the counting while writing the first byte as described in datasheet
            case 0b00000000: break;
            case 0b00010000: i8253_state.ctrs[0].b.l = val; break;
            case 0b00100000: i8253_state.ctrs[0].b.h = val; break;
            case 0b00110000: {
                if (i8253_state.access_ctrs[0] & 1) {
                    i8253_state.ctrs[0].b.h = val;
                } else {
                    i8253_state.ctrs[0].b.l = val;
                }
                break;
            }
        }
        i8253_state.ctr_limits[0] = i8253_state.ctrs[0].x;
        i8253_state.access_ctrs[0]++;
    }
}

uint8_t i8253_timer_read(uint8_t ofs) {
    // Timer 0
    uint8_t ret;
    if (ofs == 0) {
        switch (i8253_state.status & 0b00110000) {
            case 0b00000000: ret = (i8253_state.access_ctrs[0] & 1) ? (i8253_state.ctrs_latch[0] >> 8) : (i8253_state.ctrs_latch[0]); break;
            case 0b00010000: ret = (i8253_state.ctrs[0].b.l); break;
            case 0b00100000: ret = (i8253_state.ctrs[0].b.h); break;
            case 0b00110000: ret = (i8253_state.access_ctrs[0] & 1) ? (i8253_state.ctrs[0].b.h) : (i8253_state.ctrs[0].b.l); break;
        }
    } else {
        // Timer 1 and 2: lol
        // dumb hack to pass POST
        ret = i8253_state.access_ctrs[1] > 0 ? 0 : 0xFF;
    }
    i8253_state.access_ctrs[ofs]++;
    return ret;
}

void i8253_tick() {
    if (!i8253_state.init) { return; }
    assert(i8253_state.out[0]);
    switch (i8253_state.status & 0b1110) {
        // Mode 0
        case 0b0000: {
            if (--i8253_state.ctrs[0].x == 0) {
                *i8253_state.out[0] = true;
            }
            break;
        }
        // Mode 3
        case 0b0111:
        case 0b0011: {
            if (--i8253_state.ctrs[0].x == 0xFFFF) {
                i8253_state.ctrs[0].x = i8253_state.ctr_limits[0] - 1;
            }
            *i8253_state.out[0] = (i8253_state.ctrs[0].x >= (i8253_state.ctr_limits[0]>>1));
            break;
        }
    }
}
