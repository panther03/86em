#include "vm_mem.h"
#include "backend/linux/cga.h"
#include <unistd.h>

int main() {
    init_mem_blank();
    cga_start();
    int c = 0;
    int d = 0;
    while(1) {
        for (int i = 0; i < 25; i++) {
            for (int j = 0; j < 40; j++) {
                int ind = (40 * i + j) << 1;
                store_u8_direct(CGA_MEM_ADDR + ind, c);
                store_u8_direct(CGA_MEM_ADDR + ind+1, c+d);
            }
        }
        c = c + 1;
        d = d + 1;
        usleep(100000);
    }
}