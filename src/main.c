#include <stdio.h>
#include <stdlib.h>

#include "sim_mem.h"
#include "sim_main.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Expected path to bin file and offset.\n");
        return 1;
    }
    // Arguments: bin file, offset (segments pre-computed)
    const char *path = argv[1];
    const int offset = atoi(argv[2]);
    printf("%s\n", path);

    FILE* prog = fopen(path, "r");
    if (prog == NULL) {
        printf("Can't open file: %s\n", path);
        return 1;
    }

    size_t prog_size = init_mem(prog, offset);
    
    sim_state_t* state = sim_init();

    sim_run(state);

    fclose(prog);
    return 0;
}

