#ifndef I8237_H
#define I8237_H

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

typedef union {
    uint16_t x;
    struct {
        uint8_t l;
        uint8_t h;
    };
} dma_reg_t;

//typedef uint8_t* (*dma_cb_t)();

typedef struct {
    dma_reg_t addr;
    dma_reg_t cntr;
//    dma_cb_t req_fn;
//    dma_cb_t ack_fn;
    uint8_t page;
    bool auto_en;
    bool down_en;
    bool wr_en;
    bool masked;
} dma_chan_t;

typedef struct {
    dma_chan_t chans[4];
    uint8_t status;
    bool ff;
    bool en;
} i8237_state_t;

#define I8237_CMD_COND 0x04
#define I8237_SC_MASK_ON(v) ((v >> 2) & 1)
#define I8237_MODE_AUTO(v) ((v >> 4) & 1)
#define I8237_MODE_DOWN(v) ((v >> 5) & 1)
#define I8237_MODE_WR(v)   ((v >> 2) & 1)

extern i8237_state_t i8237_state;

void i8237_init();

void i8237_cr_write(uint8_t port, uint8_t val);

static inline void i8237_chan_write(uint8_t port, uint8_t val) {
    assert(port < 8);
    if (port & 1) {
        if (i8237_state.ff) {
            i8237_state.chans[port >> 1].cntr.h = val;
        } else {
            i8237_state.chans[port >> 1].cntr.l = val;
        }
    } else {
        if (i8237_state.ff) {
            i8237_state.chans[port >> 1].addr.h = val;
        } else {
            i8237_state.chans[port >> 1].addr.l = val;
        }
    }
    i8237_state.ff = !i8237_state.ff;
}

static inline uint8_t i8237_chan_read(uint8_t port) {
    uint8_t ret = 0;
    if (port & 1) {
        ret = i8237_state.ff ? i8237_state.chans[port >> 1].cntr.h
            : i8237_state.chans[port >> 1].cntr.l;
    } else {
        ret = i8237_state.ff ? i8237_state.chans[port >> 1].addr.h
            : i8237_state.chans[port >> 1].addr.l;
    }
    i8237_state.ff = !i8237_state.ff;
    return ret;
}

uint8_t i8237_cr_read(uint8_t port);

void i8237_xfer(uint8_t chan, uint8_t *buf);

#define DMA_CHAN_REG_END   0x07
#define DMA_CTRL_REG_END   0x0F
#define DMA_PAGE_CHAN0     0x87
#define DMA_PAGE_CHAN1     0x83
#define DMA_PAGE_CHAN2     0x81
#define DMA_PAGE_CHAN3     0x82

#endif // I8237_H