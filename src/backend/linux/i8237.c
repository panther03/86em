#include "i8237.h"
#include "vm_mem.h"

#include <stdio.h>
#include <string.h>

i8237_state_t i8237_state;

void i8237_init() {
    memset(&i8237_state, 0, sizeof(i8237_state_t));
    for (int i = 0; i < 4; i++) {
        i8237_state.chans[i].masked = true;
    }
}

uint8_t i8237_cr_read(uint8_t port) {
    uint8_t ret = 0;
    switch (port) {
        // lazy mf
        default: {
            printf("Unrecognized I8237 read register %d\n", port);
        }
    }
    return ret;
}


void i8237_cr_write(uint8_t port, uint8_t val) {
    printf("write %04x\n", port);
    uint8_t chan_sel = val & 0b11;
    switch (port) {
        case 0x08: {
            i8237_state.en = (val & I8237_CMD_COND) ? true : false;
            break;
        }
        case 0x0A: {
            i8237_state.chans[chan_sel].masked = I8237_SC_MASK_ON(val);
            break;
        }
        case 0x0B: {
            i8237_state.chans[chan_sel].down_en = I8237_MODE_DOWN(val);
            i8237_state.chans[chan_sel].auto_en = I8237_MODE_AUTO(val);
            i8237_state.chans[chan_sel].wr_en   = I8237_MODE_WR(val);
            // lazy about mode selection for the moment since it doesnt
            // seem to matter if we're not simulating the bus
            break;
        }
        case 0x0C: {
            i8237_state.ff = false;
            break;
        }
        case 0x0D: {
            i8237_state.ff = false;
            i8237_state.status = 0;
            for (int i = 0; i < 4; i++) {
                i8237_state.chans[i].masked = true;
            }
            break;
        }
        case 0x0E: {
            for (int i = 0; i < 4; i++) {
                i8237_state.chans[i].masked = false;
            }
            break;
        }
        case 0x0F: {
            for (int i = 0; i < 4; i++) {
                i8237_state.chans[i].masked = val & 1;
                val >>= 1;
            }
            break;
        }
        default: {
            printf("Unrecognized I8237 write register %d\n", port);
        }
    }
}


void i8237_xfer(uint8_t chan, uint8_t *buf) {
    if (!i8237_state.en || chan >= 4) return;
    dma_chan_t *chan_obj = &i8237_state.chans[chan];
    if (chan_obj->masked 
        || (chan_obj->cntr.x == 0)) return;

    uint32_t addr_base = chan_obj->page << 16;
    uint16_t addr_cnt = chan_obj->addr.x;
    int16_t direction = chan_obj->down_en ? -1 : 1;

    for (uint16_t ofs = 0; ofs < chan_obj->cntr.x; ofs += 1) {
        // keep 64KB wrapping functionality
        addr_cnt = direction * ofs + chan_obj->addr.x;
        if (chan_obj->wr_en) {
            uint8_t val = buf[ofs];
            store_u8(addr_base + addr_cnt, val);
        } else {
            uint8_t val = load_u8(addr_base + addr_cnt);
            buf[ofs] = val;
        }
    }
    
    if (!chan_obj->auto_en) {
        chan_obj->cntr.x = 0;
        chan_obj->addr.x = addr_cnt;
    }
}