#include "vm_io.h"

#include "cga.h"
#include "kbd.h"
#include "i8253.h"
#include "i8259.h"
#include "i8237.h"

#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL_mutex.h>

kbd_state_t kbd_state;

struct {
    bool sense_sw_en;
    uint8_t sense_sw;
} global_io;

void io_init() {
    global_io.sense_sw_en = true;
    global_io.sense_sw = 0b00011100;
    //              1 drive ^ | | |
    //          40x25 display ^ | |
    //        max dedotated wam ^ |
    //                   reserved ^
    i8259_init();
    i8237_init();
    i8253_init(&i8259_state.irqs[0]);
    kbd_init(&i8259_state.irqs[2]);
}

uint8_t io_int_ack() {
    return i8259_ack();
}

bool io_int_poll() {
    return i8259_int();
}

void io_access_u16(uint16_t addr) {
    static bool gfx_initd = false;
    if (CGA_REG_START <= addr && addr <= CGA_REG_END && !gfx_initd) {
        cga_start();
        gfx_initd = true;
    }
}

void io_write_u16(uint16_t addr, uint16_t data) {    
    printf("write %04x", addr);
    io_access_u16(addr);
    if (addr <= DMA_CHAN_REG_END) {
        i8237_chan_write(addr, data);
        return;
    } else if (addr <= DMA_CTRL_REG_END) {
        i8237_cr_write(addr, data);
        return;
    }
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
        case DMA_PAGE_CHAN0: i8237_state.chans[0].page = data; break;
        case DMA_PAGE_CHAN1: i8237_state.chans[1].page = data; break;
        case DMA_PAGE_CHAN2: i8237_state.chans[2].page = data; break;
        case DMA_PAGE_CHAN3: i8237_state.chans[3].page = data; break;
        case PPI_REG_PORT_B: {
            if (data & 0x80) {
                global_io.sense_sw_en = true;
            } else {
                // enable keyboard 
                global_io.sense_sw_en = false;
            }
            // stupid hack to detect KB reset
            if (data == 0xCC) {
                kbd_reset();
            }
            break;
        }
        case 0xFF: { exit(data); }
        default: { printf("unrecognized wr port %04x\n", addr); }
    }
}

uint16_t io_read_u16(uint16_t addr) {
    io_access_u16(addr);
    if (addr <= DMA_CHAN_REG_END) {
        return i8237_chan_read(addr);
    } else if (addr <= DMA_CTRL_REG_END) {
        return i8237_cr_read(addr);
    }
    switch(addr) {
        case PIT_REG_CTRL: return i8253_cr_read();
        case PIT_REG_TIMER0: return i8253_timer_read(0);
        case PIT_REG_TIMER1: return i8253_timer_read(1);
        case PIT_REG_TIMER2: return i8253_timer_read(2);
        case PIC_REG_COMMAND: return i8259_read_command();
        case PIC_REG_DATA: return i8259_read_data();
        case DMA_PAGE_CHAN0: return i8237_state.chans[0].page;
        case DMA_PAGE_CHAN1: return i8237_state.chans[1].page;
        case DMA_PAGE_CHAN2: return i8237_state.chans[2].page;
        case DMA_PAGE_CHAN3: return i8237_state.chans[3].page;
        case PPI_REG_PORT_A: {
            return global_io.sense_sw_en ? global_io.sense_sw : kbd_read();
        }
        case PPI_REG_PORT_B: {
            return global_io.sense_sw_en ? 0x80 : 0x00;
        }
        case PPI_REG_PORT_C: {
            // TODO
            return 0;
        }
        default: {
            printf("unrecognized rd port %04x\n", addr);
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

    kbd_tick(global_io.sense_sw_en);
    
    i8259_tick();
}