#ifndef VM_IO_H
#define VM_IO_H

#include <stdint.h>
#include <stdbool.h>

#define PPI_REG_PORT_A 0x60
#define PPI_REG_PORT_B 0x61
#define PPI_REG_PORT_C 0x62

void io_init();

void io_write_u16(uint16_t addr, uint16_t data);
uint16_t io_read_u16(uint16_t addr, uint16_t pc);

void io_tick(uint64_t cycles);

uint8_t io_int_ack();

bool io_int_poll();

#endif // VM_IO_H