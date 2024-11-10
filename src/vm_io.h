#ifndef VM_IO_H
#define VM_IO_H

#include <stdint.h>
#include <stdbool.h>

void io_init();

void io_write_u16(uint16_t addr, uint16_t data);

void io_tick(uint64_t cycles);

uint8_t io_int_ack();

bool io_int_poll();

#endif // VM_IO_H