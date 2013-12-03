/******************************************************************************\
    Copyright (C) 2007  stamit@stamit.gr
    This is free software and comes with ABSOLUTELY NO WARRANTY;  not even for
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  You can redistribute it
and/or modify it under the terms of version 2 of the GNU General Public License
as published by the Free Software Foundation.  See the file COPYING for details.
\******************************************************************************/
#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

typedef char bool;
#define true 1
#define false 0

#define new(t) ( (t *)malloc(sizeof(t)) )
#define newa(t,n) ( (t *)malloc(sizeof(t) * (n)) )

#define SHIFT (--argc, ++argv)

#define LIST_INS(item, list) do { \
	item->next = list; \
	item->prevp = &list; \
	if (list) list->prevp = &item->next; \
	list = item; \
} while(0)
#define LIST_DEL(item, list) do { \
	*item->prevp = item->next; \
	if (item->next) item->next->prevp = item->prevp; \
} while(0)

#define LIST_INS1(item, list) do { \
	item->next1 = list; \
	item->prevp1 = &list; \
	if (list) list->prevp1 = &item->next1; \
	list = item; \
} while(0)
#define LIST_DEL1(item, list) do { \
	*item->prevp1 = item->next1; \
	if (item->next1) item->next1->prevp1 = item->prevp1; \
} while(0)

#define NOINTR(Call) \
	while((Call) == -1) { if(errno != EINTR) break; } \

#define BLOCK_SIGS \
	sigset_t ss; \
	sigfillset(&ss); \
	NOINTR(sigprocmask(SIG_BLOCK, &ss, &ss)) \

#define UNBLOCK_SIGS \
	NOINTR(sigprocmask(SIG_SETMASK, &ss, NULL)) \

#define RESET_SIG_ALL \
	{ struct sigaction sa; \
	sigemptyset(&sa.sa_mask); \
	sa.sa_flags = 0; \
	sa.sa_handler = SIG_DFL; \
	{ int i; for(i = 1 ; i < NSIG ; ++i) NOINTR(sigaction(i,&sa,NULL)) } } \
	{ sigset_t ss; \
	sigemptyset(&ss); \
	sigprocmask(SIG_SETMASK, &ss, NULL); \
	/*NOINTR(sigprocmask(SIG_BLOCK, &ss, &ss));*/ } \

#define IMCONFUSED { \
	fprintf(stderr, "Confused by errors. Waiting for a signal...\n"); \
	pause(); \
	fprintf(stderr, "Got signal. Continuing.\n"); \
}

#define COMPLAIN(a) \
	fprintf(stderr, "%s: %s: %s\n", myname, a, strerror(errno))
#define COMPLAIN1(a,b) \
	fprintf(stderr, "%s: "a": %s\n", myname, b, strerror(errno))

#define SHOUT(a) \
	fprintf(stderr, "%s: %s\n", myname, a)
#define SHOUT1(a,b) \
	fprintf(stderr, "%s: "a"\n", myname, b)

#if DEBUG
# define DBG(str) fprintf(stderr, "%s: %s\n", myname, (str));
# define DBG1(str, a) fprintf(stderr, ("%s: "str"\n"), myname, (a));
# define DBG2(str, a, b) fprintf(stderr, ("%s: "str"\n"), myname, (a), (b));
# define DBG3(str, a, b, c) fprintf(stderr, ("%s: "str"\n"), myname, (a), (b), (c));
#else
# define DBG(str)
# define DBG1(str, a)
# define DBG2(str, a, b)
# define DBG3(str, a, b, c)
#endif

/* `myname' holds the name that the program gives in error messages. */
extern char *myname;

/* `read_string0' reads characters, replaces NUL characters with `replace_nul'
 * and stops if it reaches end-of-file or a character becomes NUL.  Returns
 * pointer to NUL-terminated string which must be freed with `free'. */
char *read_string0(int fd, char nul, char replace_nul);

/* `read_string' reads a null-terminated string of unrestricted length from a
 * file descriptor (uses realloc). */
char *read_string(int fd);
int write_string(int fd, char *str);

/* `read_argv' reads an int (argc) from fd followed by argc consecutive
 * NUL-terminated strings. */
int read_argv(int fd, int *pargc, char ***pargv);
void free_argv(char **argv);
int write_argv(int fd, int argc, char **argv);
void spew_argv(char *str, char **argv);

/* `concatenate' concatenates two null-terminated strings, optionally adding a
 * character `c' between them (if c is not NUL). */
char *concatenate(char *a, char c, char *b);


/* `process_spawn' `fork's and `exec's a program.  `uid' and `gid' say which
 * user ID and group ID to assume (-1 to avoid this).  `delay' says how many
 * seconds to delay `exec'ing (thus stalling the process).  Returns -1 on
 * failure.  On success, sets *ppid (if ppid!=0) and returns the PID. */
int process_spawn(pid_t *ppid, char *prog, char *argv[], char letter,
                  uid_t uid, gid_t gid, unsigned int delay);

/* `open_and_lock' opens and locks a file or FIFO or complains to stderr. */
int open_and_lock(const char *filename, int flags);

/* `unlock_and_close' undoes what `open_and_lock' did. */
void unlock_and_close(int fd);

/* `get_init_home' finds which initscript directory we are supposed to use (do
 * not free). */
char *get_init_home(void );

/* `make_input_fifo_filename' finds which `init.in' FIFO to use. */
char *make_input_fifo_filename(void );

#endif
