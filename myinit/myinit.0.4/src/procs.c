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

#include "procs.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"


typedef struct process process;

struct process {
	process *next, **prevp;

	char **argv;
	pid_t pid;

	time_t first_time;
	int spawns_or_client;
};


process *processes = 0;


bool process_has_pid(pid_t pid)
{
	process *p;
	for ( p = processes ; p && (p->pid >= 0 ? p->pid : -p->pid) != pid ; p = p->next ) ;
	return p!=0;
}

int process_add(char **argv)
{
	process *proc;
	pid_t pid = process_spawn(0, 0, argv, 0, -1, -1, 0);
	if (pid < 0) return -1;

	proc = new(process);
	proc->argv = argv;
	proc->pid = pid;
	proc->first_time = time(NULL);
	proc->spawns_or_client = 1;
	LIST_INS(proc,processes);

	/*DBG2("PROCESS_ADD %d %s", pid, argv[0]);*/
	return 1;
}

static process *process_find(char **argv)
{
	process *proc;
	for (proc = processes ; proc ; proc = proc->next) {
		char **parg, **parg2;
		bool match = true;
		for (parg=proc->argv,parg2=argv ; match&&*parg2 ;++parg,++parg2)
			if (!*parg || strcmp(*parg, *parg2)) 
				match = false;
		if (match) break;
	}
	return proc;
}

int process_remove(char **argv, int pid)
{
	process *proc = process_find(argv);
	if (!proc) {
		fprintf(stderr, "%s: cannot find such a process:", myname);
		for (; *argv ; ++argv) {
			fputc(' ', stderr);
			fputs(*argv, stderr);
		}
		fputs(" [...]\n", stderr);
		return -1;
	}

	/*DBG3("PROCESS_REMOVE %d %s %d",proc->pid,argv[0],pid);*/
	proc->pid = -proc->pid;
	proc->spawns_or_client = pid;

	kill(-proc->pid, SIGTERM);
	return 0;  /* not dead yet */
}

static void process_delete(process *proc)
{
	LIST_DEL(proc,processes);
	free_argv(proc->argv);
	free(proc);
}

void dead_process(pid_t pid, int status)
{
	process *proc;
	time_t delay = 0;

	/*
	 * See if the process needs to be respawned.
	 */
	for (proc = processes ; proc && (proc->pid >= 0 ? proc->pid != pid :
	                        -proc->pid != pid ) ; proc = proc->next) ;
	if (!proc) return;

	/*
	 * Have we been waiting for process to die for the last time?
	 */
	if (proc->pid < 0) {
		/*DBG3("DEAD %d %s - SIGNAL %d", pid, proc->argv[0], proc->spawns_or_client);*/
		if (proc->spawns_or_client)
			kill(proc->spawns_or_client, SIGUSR1);
		process_delete(proc);
		return;
	}

	/*
	 * Check if process respawns too fast.
	 */
	if (proc->spawns_or_client >= 8) {
		time_t t = time(NULL);
		if ( t < (proc->first_time + 2*60) ) {
			delay = 5*60;

			proc->first_time = t + delay;
			proc->spawns_or_client = 0;

			SHOUT1("`%s' respawns too fast; "
			       "stalling it for 5 minutes",
			       proc->argv[0]);
		}
	}

	/*
	 * Respawn process.
	 */
	/*DBG2("RESPAWN %u %s", pid, proc->argv[0])*/
	proc->pid = process_spawn(0, 0, proc->argv, 0, -1, -1, delay);
	if (proc->pid < 0) return;

	++proc->spawns_or_client;
}

void process_kill_all()
{
	process *proc, *next;
	for (proc = processes ; proc ; proc = next) {
		next = proc->next;
		if (proc->pid < 0) proc->pid = -proc->pid;

		fprintf(stderr, "%s: killing leftover process: ", myname);
		spew_argv(0, proc->argv);
		fputc('\n', stderr);

		kill(proc->pid, SIGKILL);
		waitpid(proc->pid, 0, 0);
		process_delete(proc);
	}
}

#if DEBUG
void processes_debug_report(void )
{
	process *proc;

	fputs("\nPROCESSES:\n", stderr);

	for (proc = processes ; proc ; proc = proc->next) {
		char **p;
		for (p = proc->argv ; *p ; ++p) {
			if (p != proc->argv)
				fputc(' ', stderr);
			fputs(*p, stderr);
		}
		fputc('\n', stderr);
	}
}
#endif
