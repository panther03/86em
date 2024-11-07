#include "vm_io.h"
#include "cga.h"
#include <stdlib.h>
#include <stdbool.h>

#include <SDL2/SDL_mutex.h>

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
        case 0xFF: { exit(data); }
        default: { break; }
    }
}