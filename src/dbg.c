#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <string.h>

#include "util.h"
#include "vm.h"

/* A static variable for holding the line. */
static char *line_read = (char *)NULL;

// https://web.mit.edu/gnu/doc/html/rlman_2.html
/* Read a string, and return a pointer to it.  Returns NULL on EOF. */
char * rl_gets ()
{
  /* If the buffer has already been allocated, return the memory
     to the free pool. */
  if (line_read)
    {
      free (line_read);
      line_read = (char *)NULL;
    }

  /* Get a line from the user. */
  line_read = readline ("> ");

  /* If the line has any text in it, save it on the history. */
  if (line_read && *line_read)
    add_history (line_read);

  return (line_read);
}

void dbg_intf(vm_t* vm) {
    while (1) {
        char* line = rl_gets();
        if (line == NULL) {
            break;
        }
        arg_split_t it = { line, false };

        const char* cmd = arg_next(&it);
        // empty command?
        if (cmd == NULL || *cmd == 0) {
          continue;
        }
        if (strcmp(cmd, "run") == 0 || strcmp(cmd, "r") == 0) {
          int cyc_d = -1;
          const char* cyc = arg_next(&it);
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
              printf("Expected breakpoint argument, either segment or IP in hex");
              continue;
            }
            bkpt_d = SEGMENT(vm->cpu.cs, bkpt_d);
          }
          vm->bkpt = bkpt_d;
          vm->bkpt_clear = false;
        } else {
          printf("unknown command: %s\n", cmd);
        }
    }
}