/******************************************************************************\
    Copyright (C) 2007  stamit@stamit.gr
    This is free software and comes with ABSOLUTELY NO WARRANTY;  not even for
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  You can redistribute it
and/or modify it under the terms of version 2 of the GNU General Public License
as published by the Free Software Foundation.  See the file COPYING for details.
\******************************************************************************/
#ifndef SERVS_H__HEADER__
#define SERVS_H__HEADER__

#include <sys/types.h>
#include "util.h"

/* `any_services' returns true if at least one service is at least partly up. */
bool any_services(void );

/* `service_has_pid' returns true if a PID belongs to an initscript. */
bool service_has_pid(pid_t pid);

/* `service_start' references a service.  If it has not been referenced so far,
 * its initscript is launched.  With `toplevel', you indicate whether this
 * reference is one of the references whose removal is necessary and sufficient
 * for stopping all services.  On error, complains to stderr and returns
 * negative. */
int service_start(const char *name, bool toplevel);

/* `service_stop' dereferences a service.  */
void service_stop(const char *name, bool toplevel);

/* `service_stop_all' removes all top-level references.  This has to be
 * sufficient for stopping all services. */
void service_stop_all(void );

/* `dead_initscript' is called by you after `wait'ing for a dying process which
 * possibly belongs to an initscript. */
void dead_initscript(pid_t pid, int status);

/* `service_request' will attempt to start (or stop, if nrequired<0) all
 * services named in `names' and will send the SIGUSR1 signal to `pid' ,
 * indicating success, or SIGUSR2, indicating failure.  `tl' is whether to add
 * top-level references. */
void service_request(pid_t pid, pid_t ppid, char **names, int nrequired, bool tl);

#if DEBUG
void services_debug_report(void );
#endif

#endif
