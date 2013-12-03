/******************************************************************************\
    Copyright (C) 2007  stamit@stamit.gr
    This is free software and comes with ABSOLUTELY NO WARRANTY;  not even for
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  You can redistribute it
and/or modify it under the terms of version 2 of the GNU General Public License
as published by the Free Software Foundation.  See the file COPYING for details.
\******************************************************************************/
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/reboot.h>

#include "util.h"


/*
 * [handler]  Signal handler.
 */
static volatile int status = -1;
void handler(int sig)
{
	switch (sig) {
	case SIGUSR1: status = 0; break;
	case SIGUSR2: status = 1; break;
	}
}


/******************************************************************************
 *
 * The steps are:
 *
 * 1. find out which name we were called by
 * 2. block signals and open `init.in'
 * 3. eat some arguments, if we are `initset'
 * 4. send opcode, PID and arguments as necessary
 * 5. close `init.in' and unblock signals
 * 6. possibly wait for a signal
 */
int main(int argc, char **argv)
{
	bool toplevel;
	char op;
	pid_t pid = 0;
	pid_t ppid = getppid();

	/* are we called by an initscript? */
	{ char *p = getenv("MYINIT");
	toplevel = !p || !p[0] || !(p[0]=='s' && !p[1]); }

	/* who am I? */
	myname = strrchr(*argv, '/');
	if (!myname) myname = *argv;
	       else ++myname;

	/* START: start process or services */
	if ( ! strcmp(myname, MY "start") ) {
		if (argc >= 2 && argv[1][0] == '/')
			op = '*';
		else if (toplevel)
			op = '(';
		else
			op = '+';

	/* STOP: stop process or services */
	} else if ( ! strcmp(myname, MY "stop" ) ) {
		if (argc >= 2 && argv[1][0] == '/')
			op = '/';
		else if (toplevel)
			op = ')';
		else
			op = '-';

	/* REBOOT, HALT, POWEROFF: bring the system down */
	} else if ( ! strcmp(myname, MY "reboot") ||
	            ! strcmp(myname, MY "halt") ||
	            ! strcmp(myname, MY "poweroff") ) {
		if (argc <= 1) {
			op = myname[strlen(MY)];
		} else if ( ! strcmp(argv[0], "-f") ) {
			sync();
			sleep(1);
			switch (myname[strlen(MY)]) {
			case 'r': reboot(RB_AUTOBOOT); return 0;
			case 'h': reboot(RB_HALT_SYSTEM); return 0;
			case 'p': reboot(RB_POWER_OFF); return 0;
			}
		} else {
			fprintf(stderr, "Usage: %s [-f]\n", myname);
			return 1;
		}

	/* initset: set environment and other options */
	} else if ( ! strcmp(myname, MY "initset") ) {
		op = '=';
		if (argc == 1) {
			return 0;
		} else if (argc == 2 && ( !strcmp(argv[1],"-h") ||
		                          !strcmp(argv[1],"--help") )) {
			puts("Adjusts settings of running `" MY "init' and performs miscellaneous functions.\n");
#if debug
			printf("%s -d                   (for debugging)\n", myname);
#endif
			printf("%s -x                   (kill stray processes)\n", myname);
			printf("%s -c command [arg...]  (run command on ctrl-alt-del)\n", myname);
			printf("%s -k command [arg...]  (run command on alt-up)\n", myname);
			printf("%s -h command [arg...]  (run command when all services are down)\n", myname);
			printf("%s [--] env=var...      (set environment variables)\n", myname);
			return 0;
		}
	} else {
		fprintf(stderr, "This program can be called as "
				"`" MY "start', "
				"`" MY "stop', "
				"`" MY "initset', "
				"`" MY "reboot', "
		                "`" MY "halt' or "
				"`" MY "poweroff'.\n");
		return 1;
	}

	/*********************************************************************/

	/* open and lock init_out and init_in */
	{ int fd=-1;
	BLOCK_SIGS

	{ char *init_in = make_input_fifo_filename();
	fd = open_and_lock(init_in, O_WRONLY);
	free(init_in); }
	if (fd == -1) { status = 1; goto trouble; }

	/* pass any initset environment variables and see what to do
	 * with the rest of the arguments */
	{ bool nonopts = false;
	for (SHIFT ; op == '=' ; SHIFT) {
		bool nonopt = false;
		if (!*argv) {
			op = 0;
		} else if (nonopts) {
			nonopt = true;
		} else if ( ! strcmp(*argv, "-c") ||
		            ! strcmp(*argv, "--on-ctrlaltdel") ) {
			op = 'I';
		} else if ( ! strcmp(*argv, "-k") ||
		            ! strcmp(*argv, "--on-keyboard") ) {
			op = 'W';
		} else if ( ! strcmp(*argv, "-h") ||
		            ! strcmp(*argv, "--on-halt") ) {
			op = '0';
		} else if ( ! strcmp(*argv, "-x") ) {
			op = '!';
#if DEBUG
		} else if ( ! strcmp(*argv, "-D") ) {
			op = '?';
#endif
		} else if (argv[0][1] == '-' && !argv[0][2]) {
			nonopts = true;
		} else {
			nonopt = true;
		}
		
		if (nonopt) {
			if ( write(fd, &op, 1) < 0 )
			{ COMPLAIN("cannot write"); status = 1; goto trouble; }
			if ( write_string(fd, *argv) < 0 )
			{ COMPLAIN("cannot write"); status = 1; goto trouble; }
		}
	} }

	/* pass opcode */
	if ( op && write(fd, &op, 1) != 1 )
	{ COMPLAIN("cannot write"); status = 1; goto trouble; }

	/* pass arguments */
	switch (op) {

	    case '*': case '/':  /* start /... & stop /... */
	    case '+': case '-':  /* start & stop */
	    case '(': case ')':  /* top-level start & stop */
		if ( ! getenv("NOSIGNAL") )
			pid = getpid();
		if ( write(fd, &pid, sizeof(pid)) != sizeof(pid) )
		{ COMPLAIN("cannot write"); status = 1; goto trouble; }
		if ( write(fd, &ppid, sizeof(ppid)) != sizeof(ppid) )
		{ COMPLAIN("cannot write"); status = 1; goto trouble; }
		if ( write_argv(fd, argc, argv) < 0 )
		{ COMPLAIN("cannot write"); status = 1; goto trouble; }
		break;

	    case '!':  /* process sweep */
		if ( ! getenv("NOSIGNAL") )
			pid = getpid();
		if ( write(fd, &pid, sizeof(pid)) != sizeof(pid) )
		{ COMPLAIN("cannot write"); status = 1; goto trouble; }
		break;

	    case 'r': case 'h': case 'p':  /* reboot, halt, poweroff */
		break;

	    case 'I': case 'W': case '0':
		if ( write_argv(fd, argc, argv) < 0 )
		{ COMPLAIN("cannot write"); status = 1; goto trouble; }
		break;
	}

trouble:
	/* pass all signals to `handler' */
	if (pid) {
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sigaddset(&sa.sa_mask, SIGUSR1);
		sigaddset(&sa.sa_mask, SIGUSR2);
		sa.sa_flags = 0; /*SA_RESTART;*/
		sa.sa_handler = handler;
		sigaction(SIGUSR1, &sa, NULL);
		sigaction(SIGUSR2, &sa, NULL);
	}

	/* unlock and close `init.in' */
	if (fd != -1) unlock_and_close(fd);
	UNBLOCK_SIGS }

	/* wait until we get signal */
	if (pid) while (status == -1) pause();

	return status && (op != '-') && (op != ')');
}
