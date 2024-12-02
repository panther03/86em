#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "main.h"
#include "util.h"
#include "vm.h"

/* A static variable for holding the line. */
static char *line_read = (char *)NULL;

// https://web.mit.edu/gnu/doc/html/rlman_2.html
/* Read a string, and return a pointer to it.  Returns NULL on EOF. */
char *rl_gets() {
    /* If the buffer has already been allocated, return the memory
       to the free pool. */
    if (line_read) {
        free(line_read);
        line_read = (char *)NULL;
    }

    /* Get a line from the user. */
    line_read = readline("> ");

    /* If the line has any text in it, save it on the history. */
    if (line_read && *line_read)
        add_history(line_read);

    return (line_read);
}

void dbg_cmd(vm_t *vm, char *line) {
    arg_split_t it = {line, false, .sep_match = sep_whitespace};

    const char *cmd = arg_next(&it);
    // empty command?
    if (cmd == NULL || *cmd == 0) {
        return;
    }
    if (strcmp(cmd, "run") == 0 || strcmp(cmd, "r") == 0) {
        int cyc_d = -1;
        const char *cyc = arg_next(&it);
        if (cyc != NULL) {
            cyc_d = atoi(cyc);
        }
        vm_run(vm, cyc_d);
    } else if (strcmp(cmd, "step") == 0 || strcmp(cmd, "s") == 0) {
        vm_run(vm, 1);
    } else if (strcmp(cmd, "bkpt") == 0 || strcmp(cmd, "b") == 0) {
        char *bkpt = arg_next(&it);

        char *bkpt_tmp = bkpt;
        int32_t bkpt_d = parse_offset_segment(bkpt_tmp);
        if (bkpt_d < 0) {
            bkpt_d = strtol(bkpt, NULL, 16);
            if (bkpt_d == 0) {
                printf("Expected breakpoint argument, either segment or IP in "
                       "hex");
                return;
            }
            bkpt_d = SEGMENT(vm->cpu.cs, bkpt_d);
        }
        vm->bkpt = bkpt_d;
        vm->bkpt_clear = false;
    } else if (strcmp(cmd, "trace") == 0 || strcmp(cmd, "t") == 0) {
        vm->opts.enable_trace = !vm->opts.enable_trace;
        printf("Tracing %s\n", vm->opts.enable_trace ? "on" : "off");
    } else {
        printf("unknown command: %s\n", cmd);
    }
}

void dbg_repl(vm_t *vm) {
    while (!stop_flag) {
        char *line = rl_gets();
        if (line == NULL) {
            break;
        }
        dbg_cmd(vm, line);
    }
}

void dbg_run_cmds(vm_t *vm, char *arg_command) {
    arg_split_t it = {arg_command, false, sep_semi};
    char *cmd;
    while ((cmd = arg_next(&it)) != NULL && !stop_flag) {
        dbg_cmd(vm, cmd);
    }
}