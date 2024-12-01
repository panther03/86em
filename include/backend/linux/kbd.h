#ifndef KBD_H
#define KBD_H

#include <SDL2/SDL_mutex.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint8_t scancode;
    bool new_scancode;
    bool *irq;
} kbd_state_t;

extern kbd_state_t kbd_state;

static inline void kbd_init(bool *irq) {
    memset(&kbd_state, 0, sizeof(kbd_state_t));
    kbd_state.irq = irq;
    kbd_state.new_scancode = false;
    kbd_state.scancode = 0xAA;
}

static inline void kbd_reset() {
    kbd_state.scancode = 0xAA;
    kbd_state.new_scancode = true;
}

static inline void kbd_set_scancode(uint8_t scancode) {
    if (kbd_state.scancode != 0xAA) {
        kbd_state.scancode = scancode;
        kbd_state.new_scancode = true;
    }
}

static inline uint8_t kbd_read() {
    uint8_t val = kbd_state.scancode;
    kbd_state.scancode = 0;
    return val;
}

static inline void kbd_tick(bool suppress) {
    if (suppress) {
        *(kbd_state.irq) = false;
    } else {
        *(kbd_state.irq) = kbd_state.new_scancode;
        // a scancode could get eaten if kbd_set_scancode is ran right here
        kbd_state.new_scancode = false;
    }
}

#endif // KBD_H