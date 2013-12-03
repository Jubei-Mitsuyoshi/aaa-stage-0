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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/ioctl.h>  /* for keyboard request (to receive SIGWINCH) */
#include <sys/kd.h>     /* for keyboard request (to receive SIGWINCH) */

#include "util.h"
#include "procs.h"
#include "servs.h"


static char **on_nothing = 0; /* command to run when there is nothing left running */
static char on_nothing_i = 's'; /* what to do when everything stops */
static char **on_keyboard = 0; /* how to respond to SIGWINCH */
static char **on_ctrlaltdel = 0; /* how to respond to Ctrl+Alt+Delete */

/* `lone_proc' holds the process ID of the process doing any of the special jobs
 * above.  This is so that only one such process will be running. */
static pid_t lone_proc = 0;

/* `process_sweep' kills the stray children of others. */
static void process_sweep(void )
{
#define INTLEN 10
	/* "/proc" '/' "12345" '/' "stat" NUL */
	/* "/proc" '/' "12345" '/' "cmdline" NUL */
	char fn[5+1+INTLEN+1+7+1] = "/proc";
	int mypid = getpid();
	DIR *d;
	struct dirent *de;

	d = opendir(fn);
	if (!d) return;

	fn[5] = '/';

	while ( (de = readdir(d)) != NULL ) {
		int i, ii;
		int pid;
		char *cmdline;

		pid = atoi(de->d_name);
		if (!pid) continue;

		/* after "/proc/", append PID "/stat" */
		strncpy(fn+5+1, de->d_name, INTLEN);
		fn[5+1+INTLEN] = '\0';
		ii = strlen(fn);
		strcpy(fn+ii, "/stat");

		{ long unsigned int vsize;
		FILE *f = fopen(fn, "r");
		if (!f) continue;
		fscanf(f, "%*d %*s %*c %d "
		          "%*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u "
		          "%*d %*d %*d %*d %*d %*d %*u %lu", &i,&vsize);
		/* this will ignore Linux's internal daemons */
		if (!vsize) { fclose(f); continue; }
		fclose(f); }

		/* is it a child of init? */
		if (i != mypid) continue;

		/* is init responsible? */
		if (pid == lone_proc) continue;
		if (process_has_pid(pid)) continue;
		if (service_has_pid(pid)) continue;

		strcpy(fn+ii, "/cmdline");

		{ int fd = open(fn, O_RDONLY);
		if (fd >= 0) {
			cmdline = read_string0(fd, '\0', ' ');
			close(fd);
		} else {
			cmdline = 0;
		} }

		if (cmdline) {
			fprintf(stderr, "%s: SIGKILLing stray process %d: %s\n",
			                myname, pid, cmdline);
			free(cmdline);
		} else
			fprintf(stderr, "%s: SIGKILLing stray process %d\n",
			                myname, pid);

		/* DIE! DIE! DIE! */
		kill(pid, SIGKILL);
	}
	closedir(d);
}

/* `process_command' handles process-related commands. */
static int process_command(char op, int fd)
{
	int pid, ppid;
	char **argv;
	int status = 0;
	size_t i;

	i = read(fd, &pid, sizeof(pid));
	if (i == 0) return 1;
	if (i < 0) COMPLAIN("process_command");

	i = read(fd, &ppid, sizeof(ppid));
	if (i == 0) return 1;
	if (i < 0) COMPLAIN("process_command");

	if ( read_argv(fd, 0, &argv) < 0 )
	{ if (pid) kill(pid, SIGUSR2); return -1; }

	switch (op) {
	case '*': status = process_add(argv); break;
	case '/': status = process_remove(argv, pid); break;
	}

	if (pid) {
		if (status > 0)
			kill(pid, SIGUSR1);
		else if (status < 0)
			kill(pid, SIGUSR2);
	}

	return 0;
}

/* `service_command' handles service-related commands. */
static int service_command(char op, int fd)
{
	size_t bytes;
	pid_t pid, ppid;
	int argc;
	char **argv;
	int i;
	bool toplevel;

	bytes = read(fd, &pid, sizeof(pid));
	if (bytes == -1) COMPLAIN("service_command");
	if (bytes != sizeof(pid)) return -1;

	bytes = read(fd, &ppid, sizeof(ppid));
	if (bytes == -1) COMPLAIN("service_command");
	if (bytes != sizeof(ppid)) return -1;

	read_argv(fd, &argc, &argv);
	if ( argc < 0 ) COMPLAIN("service_command");

	switch (op) {
	case '+': i = +1; toplevel = false; break;
	case '(': i = +1; toplevel = true; break;
	case '-': i = -1; toplevel = false; break;
	case ')': i = -1; toplevel = true; break;
	default: return -1;
	}

	service_request(pid,ppid, argv,i*argc, toplevel);

	return 0;
}

/* `command_read' reads and executes a single command from `fd' and returns
 * 1 if there was NO input (probably because the other side closed the FIFO)
 * 0 on success or if there were minor problems,
 * -1 if there was a problem and we should reopen init.in,
 * -2 if there was a serious problem and we should give up. */
static int command_read(int fd)
{
	int status = 0;
	char op;

	/* read opcode */
	switch ( read(fd, &op, 1) ) {
	case 1: break;
	case 0: return 1;
	default:
		COMPLAIN("command_read");
		if (errno == EINTR)
			return 1;
		return -1;
	}

	/*DBG1("READ %c", op)*/

	{ BLOCK_SIGS

	switch (op) {

	    case '*': case '/':  /* start/stop process */
		status = process_command(op, fd);
		break;

	    case '+': case '-':  /* start/stop services (initscript) */
	    case '(': case ')':  /* start/stop services (administrator) */
		status = service_command(op, fd);
		break;

	    case '=':  /* VARIABLE=value */
		{ char *a = read_string(fd), *b;
		if (!a) { status = -1; break; }

		/*DBG1("SET %s", a);*/

		b = strchr(a, '=');
		if (b) {
			*b = '\0';
			setenv(a, ++b, 1);
		} else
			unsetenv(a);
		free(a); }
		break;

	    case '0':  /* --on-halt */
	    case 'I':  /* --on-ctrlaltdel */
	    case 'W':  /* --on-keyboard */
		/* DBG1("OP %c", op); */

		{ char **argv;
		if ( read_argv(fd, 0, &argv) < 0 ) { status = -1; break; }
		switch (op) {
		    case '0':
			if (on_nothing) free_argv(on_nothing);
			on_nothing = argv;
			break;
		    case 'I':
			if (on_ctrlaltdel) free_argv(on_ctrlaltdel);
			on_ctrlaltdel = argv;
			break;
		    case 'W':
			if (on_keyboard) free_argv(on_keyboard);
			on_keyboard = argv;
			break;
		} }
		break;

	    case 'h':
	    case 'p':
	    case 'r':
		/* DBG1("OP %c", op); */
		on_nothing_i = op;
		service_stop_all();
		break;

	    case '!':  /* process sweep */
		/* DBG1("OP %c", op); */
		{ int cpid = 0;
		int i = read(fd, &cpid, sizeof(cpid));
		if (i != sizeof(cpid)) {
			if (i < 0) COMPLAIN("command_read");
			break;
		}
		process_sweep();
		if (cpid) kill(cpid, SIGUSR1); }
		break;
#if DEBUG
	    case '?':
		spew_argv("on_nothing = ", on_nothing);
		fprintf(stderr, "on_nothing_i = '%c'\n", on_nothing_i);
		spew_argv("on_keyboard = ", on_keyboard);
		spew_argv("on_ctrlaltdel = ", on_ctrlaltdel);
		fprintf(stderr, "lone_proc = %d\n", lone_proc);

		processes_debug_report();
		services_debug_report();
		break;
#endif
	}

	UNBLOCK_SIGS }

	return status;
}

static int initctl_create(char *init_in)
{
	SHOUT1("creating %s", init_in);
	while ( mkfifo(init_in, 0600) < 0 ) switch (errno) {
	case EEXIST:
		COMPLAIN1("creating %s", init_in);
		if ( unlink(init_in) == -1 )
			return -2;
		continue;
	default:
		COMPLAIN1("cannot create %s", init_in);
		return -1;
	}
	return 0;
}

/* `initctl_open' opens the `init.in' FIFO.  Attempts to create it if missing. */
static int initctl_open(char *init_in)
{
	int fd;

	while ( ( fd = open(init_in, O_RDWR) ) < 0 ) {
		COMPLAIN("initctl_open");
		switch (errno) {
		case EINTR:
			break;
		case ENOENT:
			if (initctl_create(init_in) == -1)
				return -1;
			break;
		default:
			return -1;
		}
	}

	fcntl(fd, F_SETFD, FD_CLOEXEC);
	return fd;
}
static int initctl_close(int fd)
{
	while ( close(fd) ) switch (errno) {
	case EINTR:
		continue;
	default:
		COMPLAIN("initctl_close");
		return -1;
	}

	return 0;
}

static bool has_data_pending(int fd)
{
	fd_set fds;
	struct timeval tv;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	return select(fd+1, &fds, 0, 0, &tv) == 1;
}

/* `command_expect' will wait until input is available on the FIFO and return
 * 1 if and when there is input available,
 * 0 if we were interrupted or we waited too long,
 * -1 if we should reopen the fifo or
 * -2 if an error has occured from which we can't recover.  */
static int command_expect(int fd)
{
	fd_set fds;
	struct timeval tv;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	/*DBG("WAITING")*/
	switch ( select(fd+1, &fds, 0, 0, &tv) ) {
	case 1:
		return 1;
	case 0:
		{ struct stat st;
		fstat(fd, &st);
		if (st.st_nlink == 0) {
		    SHOUT("command_expect found that the input FIFO was deleted");
		    return -1;
		} }
		return 0;
	default:
		if (errno == EINTR)
			return 0;
		COMPLAIN("command_expect");
		return -2;
	}
}

/* `sig_handler' handles all signals. */
static void sig_handler(int sig)
{
	switch (sig) {

	case SIGCHLD: {
		int status;
		pid_t pid;

		while ( (pid = waitpid(-1, &status, WNOHANG)) > 0 ) {
			if (pid < 0) {
				if (errno == ECHILD) break;
				COMPLAIN("sig_handler");
				break;
			}

			dead_process(pid, status);
			dead_initscript(pid, status);

			if (pid == lone_proc) {
				/*DBG1("DONE %d", pid);*/
				lone_proc = 0;
			}
		}
		} break;

	case SIGWINCH:
		if (getpid() != 1) {
			SHOUT("did not expect SIGWINCH");
			break;
		}
		if (on_keyboard && !lone_proc)
			process_spawn(&lone_proc, 0, on_keyboard, 'w',-1,-1,0);
		break;

	case SIGINT:
		if (getpid() == 1) {
			if ( ! on_ctrlaltdel )
				SHOUT("ctrl+alt+del is not enabled");
			else if (!lone_proc)
				process_spawn(&lone_proc, 0, on_ctrlaltdel,'i',-1,-1,0);
		} else {
			service_stop_all();
		}
		break;

	case SIGTSTP:
		if (getpid() != 1) {
			SHOUT("waiting for you...");
			raise(SIGSTOP);
		}
		break;

	case SIGCONT:
		SHOUT("thank you...");
		break;

	default:
		DBG1("SIGNAL %d", sig);
	}
}

/* `main' is where everything happens... */
int main(int argc, char **argv)
{
	pid_t pid = getpid();
	int status = 0, fd = -1;
	char *init_in = 0;

	myname = argv[0];
	umask(0077);
	/*setsid();*/

	/* trap signals, Ctrl+Alt+Del and `keyboard request' */
	{ struct sigaction sa; int sig;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP;
	sa.sa_handler = sig_handler;
	for (sig = 1 ; sig < NSIG ; ++sig) {
		if ( sig != SIGKILL && sig != SIGSTOP )
			sigaction(sig, &sa, NULL);
	} }

	if (pid == 1) {
		reboot(RB_DISABLE_CAD);
		ioctl(0, KDSIGACCEPT, SIGWINCH);
	}

	/* open `init.in' now so it is available to init.boot */
	init_in = make_input_fifo_filename();
	fd = initctl_open(init_in);
	if (fd == -1) status = -2;

	/* launch /etc/init.boot */
	if (process_spawn(&lone_proc, INIT_BOOT, argv, 0,-1,-1,0) == -1)
		status = -2;

	/* read commands from `init.in' */
	while (status > -2) {
		if (!has_data_pending(fd) && !any_services() && !lone_proc) {
			if (on_nothing_i == 's') {  /* 's' = starting up */
				BLOCK_SIGS
				on_nothing_i = 'r';
				service_request(0,0, argv+1,1, true);
				UNBLOCK_SIGS
				continue;
			}
			if ( !on_nothing )
				break;
			if ( process_spawn(&lone_proc,0,on_nothing,'h',-1,-1,0) == -1 )
				break;
		}

		status = command_expect(fd);

		if (status == -1) {
			SHOUT1("attempting to reopen %s", init_in);
			initctl_close(fd);
			fd = initctl_open(init_in);
			if (fd == -1) {
				COMPLAIN1("FAILED TO REOPEN %s", init_in);
				status = -2;
			}
		}

		if (status == 1)
			status = command_read(fd);
	}

	SHOUT("nothing to do");

	if (fd != -1)
		initctl_close(fd);
	free(init_in);

	{ struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP;
	sa.sa_handler = SIG_DFL;
	sigaction(SIGCHLD, &sa, NULL); }

	process_kill_all();

	if (pid == 1) {
		reboot(RB_ENABLE_CAD);

		if (on_nothing_i) {
			sync();
			sleep(1);
			switch (on_nothing_i) {
			case 'h': reboot(RB_HALT_SYSTEM); break;
			case 'p': reboot(RB_POWER_OFF); break;
			case 'r': reboot(RB_AUTOBOOT); break;
			}
		}
	}

	return status ? EXIT_FAILURE : EXIT_SUCCESS;
}
