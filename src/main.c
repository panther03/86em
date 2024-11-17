#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "vm_mem.h"
#include "vm.h"
#include "dbg.h"
#include "util.h"

int main(int argc, char **argv) {
    int dbg = 0;
    int trace = 0;
    opterr = 0;
    int c;

    while ((c = getopt(argc, argv, "dt")) != -1 ) {
        switch (c) {
            case 'd': dbg = 1; break;
            case 't': trace = 1; break;
            default: 
                abort();
        }
    }

    if (argc - optind < 2) {
        printf("Expected path to bin file and offset.\n");
        return 1;
    }
    // Arguments: bin file, offset (segments pre-computed)
    const char *path = argv[optind];
    const int offset = parse_offset_segment(argv[optind + 1]);
    if (offset < 0) {
        printf("Expected segment of the form abcd:1234 (not 0)\n");
        exit(1);
    }
    printf("%s\n", path);

    FILE* prog = fopen(path, "r");
    if (prog == NULL) {
        printf("Can't open file: %s\n", path);
        return 1;
    }

    init_mem(prog, offset);
    fclose(prog);
    
    vm_t* vm = vm_init();

    vm->opts.enable_trace = trace;

    if (dbg) {
        vm->opts.enable_trace = 1;
        dbg_intf(vm);
    } else {
        vm_run(vm, -1);
    }

    return 0;
}

