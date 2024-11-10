#include "vm_io.h"

#include "cga.h"
#include "i8253.h"
#include "i8259.h"

#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL_mutex.h>

void io_init() {
    i8259_init();
    i8253_init(&i8259_state.irqs[0]);
}

uint8_t io_int_ack() {
    return i8259_ack();
}

void io_access_u16(uint16_t addr) {
    static bool gfx_initd = false;
    if (CGA_REG_START <= addr && addr <= CGA_REG_END && !gfx_initd) {
        cga_start();
        gfx_initd = true;
    }
}

void io_write_u16(uint16_t addr, uint16_t data) {    
    io_access_u16(addr);
    switch(addr) {
        case CGA_REG_MODE: {
            SDL_LockMutex(cga_state.lock);
            cga_state.mode = data & 0xFF;
            SDL_UnlockMutex(cga_state.lock);
            break;
        }
        case PIT_REG_CTRL: i8253_cr_write(data); break;
        case PIT_REG_TIMER0: i8253_timer_write(0, data); break;
        case PIT_REG_TIMER1: i8253_timer_write(1, data); break;
        case PIT_REG_TIMER2: i8253_timer_write(2, data); break;
        case PIC_REG_COMMAND: i8259_write_command(data); break;
        case PIC_REG_DATA: i8259_write_data(data); break;
        case 0xFF: { exit(data); }
        default: { break; }
    }
}

uint16_t io_read_u16(uint16_t addr) {
    io_access_u16(addr);
    switch(addr) {
        case PIT_REG_CTRL: return i8253_cr_read();
        case PIT_REG_TIMER0: return i8253_timer_read(0);
        case PIT_REG_TIMER1: return i8253_timer_read(1);
        case PIT_REG_TIMER2: return i8253_timer_read(2);
        case PIC_REG_COMMAND: return i8259_read_command();
        case PIC_REG_DATA: return i8259_read_data();
        default: {
            printf("Unregistered I/O access %x\n", addr);
            exit(EXIT_FAILURE);
        };
    }
}

void io_tick(uint64_t cycles) {
    // Timer ticks every other instruction
    // Laziest attempt at "cycle accuracy" (just what is required to pass the PC BIOS check)
    if (cycles % 2 == 1) {
        i8253_tick();
    }
    i8259_tick();
}