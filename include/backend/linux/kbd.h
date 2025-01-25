#ifndef KBD_H
#define KBD_H

#include <SDL2/SDL_mutex.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define RESET_WAIT 50

typedef struct {
    uint8_t scancode_stack[8];
    uint8_t scancode_ptr;
    SDL_mutex *scancode_mutex;
    uint8_t state_sr;
    uint8_t reset_timer;
    bool *irq;
} kbd_state_t;

extern kbd_state_t kbd_state;

static inline void kbd_init(bool *irq) {
    memset(&kbd_state, 0, sizeof(kbd_state_t));
    kbd_state.irq = irq;
    kbd_state.reset_timer = 0;
    kbd_state.scancode_ptr = 0;
    kbd_state.scancode_stack[0] = 0xAA;
    kbd_state.scancode_mutex = SDL_CreateMutex();
}

static inline void kbd_update_state(uint8_t next) {
    kbd_state.state_sr = (kbd_state.state_sr << 2) + next;
    // stupid hack to detect reset based on port B commands from BIOS
    if ((kbd_state.state_sr & 0b111111) == 0b001101) {
        // stupid hack to not trip IRQ too early, otherwise it runs ISR
        // before mov ah, 0 is ran in BIOS
        kbd_state.reset_timer = RESET_WAIT;
    }
}

static inline void kbd_push_scancode(uint8_t scancode) {
    SDL_LockMutex(kbd_state.scancode_mutex);
    if (kbd_state.scancode_ptr < 8) {
        kbd_state.scancode_stack[kbd_state.scancode_ptr++] = scancode;
    }
    SDL_UnlockMutex(kbd_state.scancode_mutex);
}

static inline uint8_t kbd_read() {
    uint8_t val = kbd_state.scancode_stack[0];
    SDL_LockMutex(kbd_state.scancode_mutex);
    if (kbd_state.scancode_ptr > 0) {
        val = kbd_state.scancode_stack[--kbd_state.scancode_ptr];
    }
    SDL_UnlockMutex(kbd_state.scancode_mutex);
    return val;
}

static inline void kbd_tick() {
    if (kbd_state.reset_timer > 0 && kbd_state.reset_timer-- == 1) {
        kbd_state.scancode_stack[0] = 0xAA;
        kbd_state.scancode_ptr = 1;
    }

    if (kbd_state.state_sr & 2) {
        *(kbd_state.irq) = false;
    } else if (kbd_state.scancode_ptr > 0) {
        *(kbd_state.irq) = true;
    }
}

#endif // KBD_H