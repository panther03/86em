#ifndef DBG_H
#define DBG_H

#include "vm.h"

void dbg_repl(vm_t* state);
void dbg_run_cmds(vm_t* state, char *arg_command);

#endif // DBG_H