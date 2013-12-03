/******************************************************************************\
    Copyright (C) 2007  stamit@stamit.gr
    This is free software and comes with ABSOLUTELY NO WARRANTY;  not even for
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  You can redistribute it
and/or modify it under the terms of version 2 of the GNU General Public License
as published by the Free Software Foundation.  See the file COPYING for details.
\******************************************************************************/
#ifndef PROCS_H_
#define PROCS_H_

#include <sys/types.h>
#include "util.h"

/* `any_processes' returns nonzero iff there is at least one process started
 * with `process_add' which is still running. */
bool any_processes(void );

/* `process_has_pid' returns nonzero if a PID was started by this module. */
bool process_has_pid(pid_t pid);

/* `process_add' spawns a process and respawns it whenever it terminates. */
int process_add(char **argv);

/* `process_remove' kills (SIGTERM) and stops respawning a processes whose
 * arguments start with "argv".  `pid' is the PID of a `stop' process to be
 * signaled when we are finally done. */
int process_remove(char **argv, int pid);

/* `dead_process' is called by you after `wait'ing for a dying process. */
void dead_process(pid_t pid, int status);

/* `process_kill_all' sends SIGKILL to all running processes and `wait's for
 * their termination.  Use with care. */
void process_kill_all(void );

#if DEBUG
void processes_debug_report(void );
#endif

#endif
