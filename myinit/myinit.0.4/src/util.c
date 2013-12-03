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

#include "util.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/file.h>


char *myname = PROGRAM;


char *read_string0(int fd, char nul, char replace_nul)
{
	size_t ret,
	       alloc = 32,
	       n = 0;
	char *buf = newa(char, alloc);
	char c;

	for (;;) {
		ret = read(fd, (void*)&c, 1);
		if (ret < 0) return 0;

		if (!ret) break;

		if (c == nul) c = replace_nul;
		if (!c) goto the_end;

		if (n+1 == alloc) {
			char *new_buf = (char*)realloc(buf, alloc*2);
			if (!new_buf) { free(buf); return 0; }
			buf = new_buf;
			alloc *= 2;
		}

		buf[n++] = c;
	}

	if (!n && !replace_nul) {
		free(buf);
		return 0;
	}

the_end:
	buf[n] = '\0';
	return buf;
}

char *read_string(int fd)
{
	return read_string0(fd, '\0', '\0');
}

int write_string(int fd, char *str)
{
	int bytes = strlen(str) + 1;
	return write(fd, str, bytes);
}

int read_argv(int fd, int *pargc, char ***pargv)
{
	int argc;
	char **argv;
	int i;

	int bytes = read(fd, &argc, sizeof(argc));
	if (bytes != sizeof(argc)) return -1;

	argv = newa(char *, argc+1);
	if (!argv) return -1;

	for (i = 0 ; i < argc ; ++i) {
		argv[i] = read_string(fd);
		if (!argv[i]) { while (i--) free(argv[i]); return -1; }
	}
	argv[i] = 0;

	if (pargc) *pargc = argc;
	*pargv = argv;

	return argc;
}
void free_argv(char **argv)
{
	char **p;
	for (p = argv; *p ; ++p)
		free(*p);
	free(argv);
}

int write_argv(int fd, int argc, char **argv)
{
	int i;
	if ( write(fd, &argc, sizeof(int)) < 0 )
		return -1;
	for (i = 0 ; i < argc && *argv; ++i, ++argv)
		if ( write_string(fd, *argv) < 0 )
			return -1;
	return 0;
}

void spew_argv(char *str, char **argv)
{
	char **p;
	if (str) fputs(str, stderr);
	if (!argv)
		fputs("(null)", stderr);
	else for (p = argv ; *p ; ++p) {
		if (p != argv) fputc(' ', stderr);
		fprintf(stderr, *p);
	}
	if (str) fputc('\n', stderr);
}

char *concatenate(char *a, char c, char *b)
{
	char *result = newa(char, strlen(a)+strlen(b)+2);
	if (!result) return 0;
	if (c) sprintf(result, "%s%c%s", a, c, b);
	  else sprintf(result, "%s%s", a, b);
	return result;
}

int process_spawn(pid_t *ppid, char *prog, char *argv[], char letter,
                  uid_t uid, gid_t gid, unsigned int delay)
{
	pid_t pid;
	BLOCK_SIGS

	pid = fork();
	if (!pid) {  /* child process... */
		/*DBG3("SPAWN %d %s %s", getpid(), argv[0], argv[1])*/

		RESET_SIG_ALL

		if (letter) {
			char cc[2] = {0,0};
			cc[0] = letter;
			setenv("MYINIT", cc, 1);
		}

		if (ROOT[0]) chdir(ROOT);
		if (gid != (gid_t)-1) setgid(gid);
		if (uid != (uid_t)-1) setuid(uid);

		if (delay > 0) sleep(delay);

		if (prog)
			execv(prog, argv);
		else
			execvp(argv[0], argv);

		fprintf(stderr, "cannot exec `%s': %s\n",
		                argv[0], strerror(errno));
		_exit(EXIT_FAILURE);
		raise(SIGKILL);
		for (;;) ;
	}

	if (pid <= 0) {
		COMPLAIN1("process_spawn cannot spawn `%s'", prog);
		pid = 0;
		return -1;
	}

	if (ppid) *ppid = pid;

	UNBLOCK_SIGS
	return pid;
}

int open_and_lock(const char *filename, int flags)
{
	int fd = open(filename, flags);
	if ( fd < 0 ) {
		COMPLAIN1("cannot open `%s'", filename);
		return -1;
	}
	if ( flock(fd, LOCK_EX) < 0 ) {
		COMPLAIN1("cannot lock `%s'", filename);
		close(fd);
		return -1;
	}
	return fd;
}

void unlock_and_close(int fd)
{
	flock(fd, LOCK_UN);
	close(fd);
}

char *get_init_home(void )
{
	char *init_home = getenv("MYINIT_HOME");
	if (!init_home) init_home = DEFAULT_HOME;
	return init_home;
}

char *make_input_fifo_filename(void )
{
	return concatenate(get_init_home(), 0, INPUT_FIFO_EXTENSION);
}
