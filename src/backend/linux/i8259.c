#include "i8259.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

i8259_state_t i8259_state;

void i8259_init() {
    memset(&i8259_state, 0, sizeof(i8259_state_t));
    // -1 = uninitialized, 0 = fully initialized, n>0 = during initialization
    i8259_state.icw_ind = -1; 
}

void i8259_write_command(uint8_t val) {
    if ((val & I8259_ICW1_INIT) && (i8259_state.icw_ind <= 0)) {
        // Start initialization sequence
        i8259_state.icw[0] = val;
        // Why Are You Not Using ICW4
        assert(i8259_state.icw[0] & I8259_ICW1_ICW4);
        // Why Are You Trying To Cascade PICs
        assert(i8259_state.icw[0] & I8259_ICW1_SINGL);
        i8259_state.icw_ind = 1;
    } else if (val == I8259_EOI) {
        // End of Interrupt
        // Clear highest priority ISR bit
        // Necessarily will be this way because high priority ISRs (interrupt service routines)
        // won't be interrupted by lower priority ones
        for (int i = 0; i < 8; i++) {
            if (i8259_state.isr & (1 << i)) {
                i8259_state.isr &= ~(1 << i);
            }
        }
    }
}

void i8259_write_data(uint8_t val) {
    if (i8259_state.icw_ind == 0) {
        // write mask 
        i8259_state.imr = val;
    } else if (i8259_state.icw_ind > 0) {
        i8259_state.icw[i8259_state.icw_ind] = val;
        if (i8259_state.icw_ind == 1) {
            i8259_state.icw_ind = 3;
        } else {
            i8259_state.icw_ind = 0;
        }
    }
    printf("IMR=%02x\n", i8259_state.imr);
    for (int i = 0; i < 4; i++) {
        printf("ICW[%d]=%02x\n", i, i8259_state.icw[i]);
    }
}

uint8_t i8259_read_command() {
    printf("unimplemented\n");
    exit(1);
}

uint8_t i8259_read_data() {
    return i8259_state.imr;
}

void i8259_tick() {
    // Not initialized
    if (i8259_state.icw_ind != 0) return;
    int irq_sel = -1;
    for (int i = 0; i < 8; i++) {
        // Positive edge
        if (i8259_state.irqs[i] && !i8259_state.irqs_last[i] && irq_sel < 0 ) {
            irq_sel = i;
        }
        // IRQ went down? make sure IRR bit is clear
        if (!i8259_state.irqs[i]) {
            i8259_state.irr &= ~(1 << i);        
        }
        i8259_state.irqs_last[i] = i8259_state.irqs[i];
    }
    if (irq_sel < 0) {
        return;
    }
    i8259_state.irr |= (1 << irq_sel);
}

uint8_t i8259_ack() {
    // Interrupt line should still be high when we process this, since we handle things atomically
    assert(i8259_int());
    uint8_t mask = CALCULATE_ISR_MASK(i8259_state.isr) & ~i8259_state.imr;
    
    int i;
    for (i = 0; i < 8; i++) {
        if ((1 << i) & i8259_state.irr & mask) {
            i8259_state.isr |= 1 << i;
            i8259_state.irr &= ~(1 << i);
            break;
        }
    }
    uint8_t vector_ofs = (i8259_state.icw[1] & I8259_ICW2_OFS_MASK);
    return vector_ofs + i;
}