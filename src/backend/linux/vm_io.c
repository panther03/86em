#include "vm_io.h"
#include <stdlib.h>

void io_write_u16(uint16_t addr, uint16_t data) {
    switch(addr) {
        case 0xFF: { exit(data); }
        default: { break; }
    }
}