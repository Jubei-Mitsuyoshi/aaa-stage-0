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

#include "servs.h"

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>

#include "util.h"


typedef struct service service;
typedef struct reqnode reqnode;
typedef struct request request;


enum service_state {
	SERVICE_STOPPED = 0,
	SERVICE_STARTING = 1,
	SERVICE_STARTED = 2,
	SERVICE_STOPPING = 3
};

struct service {
	service *next, **prevp;

	char *name;
	int refs;  /* all references: `top_refs' + `non_top_refs' = `refs' */
	int top_refs;

	enum service_state state;
	pid_t pid;

	reqnode *lst;
} *services = 0;

struct reqnode {
	reqnode *next, **prevp;  /* in same request */
	reqnode *next1, **prevp1;  /* for same service */
	request *req;
	service *srv;
};

struct request {
	request *next, **prevp;

	pid_t client_pid;
	bool toplevel;
	reqnode *lst;
	int nrequired, ntolerated;
	int nsucceeded, nfailed;
} *requests = 0;


/******************************************************************************/


/* TODO: some hash or binary tree or something */
static service *service_find(const char *name)
{
	service *srv;
	for (srv = services ; srv ; srv = srv->next) {
		if ( strcmp(name, srv->name) == 0 )
			return srv;
	}
	return 0;
}
static service *service_find_pid(pid_t pid)
{
	service *srv;
	for (srv = services ; srv ; srv = srv->next) {
		if ( srv->pid == pid )
			return srv;
	}
	return 0;
}

bool any_services(void )
{
	return services != 0;
}

bool service_has_pid(pid_t pid)
{
	return service_find_pid(pid) != 0;
}

static service *service_new(const char *name, bool toplevel)
{
	service *srv = new(service);
	if (!srv) { COMPLAIN("service_new"); return 0; }

	srv->name = strdup(name);
	srv->state = SERVICE_STOPPED;
	srv->pid = 0;
	srv->refs = 1;
	srv->top_refs = toplevel ? 1 : 0;
	srv->lst = 0;
	LIST_INS(srv,services);

	/*DBG1("SERVICE %s", srv->name)*/
	return srv;
}
static void service_delete(service *srv)
{
	reqnode *n, *next;

	/*DBG1("SERVICE_DELETE %s", srv->name)*/

	LIST_DEL(srv,services);
	for (n = srv->lst ; n ; n = next) { next = n->next;
		LIST_DEL1(n,srv->lst);
		n->srv = 0;
	}
	free(srv->name);
	free(srv);
}

/* `service_transition' will launch the initscript of a service to start it if
 * it is stopped, or stop it if it is started.  If the initscript fails, the
 * service is assumed to be stopped. */
static int service_transition(service *srv)
{
	char *argv[] = {0,0,0};
	pid_t pid;

	switch (srv->state) {

	case SERVICE_STOPPED:
		srv->state = SERVICE_STARTING;
		argv[1] = "start";
		break;

	case SERVICE_STARTED:
		srv->state = SERVICE_STOPPING;
		argv[1] = "stop";
		break;

	default:
		return -1;
	}

	argv[0] = concatenate(get_init_home(),'/',srv->name);
	pid = process_spawn(&srv->pid, argv[0], argv, 's',-1,-1,0);
	free(argv[0]);

	if (pid == -1) srv->state = SERVICE_STOPPED;

	return (pid != -1);
}

static service *service_start_p(const char *name, bool toplevel)
{
	service *srv = service_find(name);
	if (!srv) {
		srv = service_new(name, toplevel);
		if ( service_transition(srv) == -1 ) {
			service_delete(srv);
			return 0;
		}
		return srv;
	} else {
		++srv->refs;
		if (toplevel) ++srv->top_refs;

		/*DBG2("REF %s %d", name, srv->refs)*/
		return srv;
	}
}
int service_start(const char *name, bool toplevel)
{
	return service_start_p(name, toplevel) ? 0 : -1;
}

static void service_stop_p(service *srv, bool toplevel)
{
	if (!srv->refs)
		{ SHOUT1("no references for service `%s'", srv->name); return; }
	if (toplevel && !srv->top_refs)
		{ SHOUT1("no top-level references for service %s", srv->name); return; }

	--srv->refs;
	if (toplevel) --srv->top_refs;

	if ( !srv->refs && srv->state == SERVICE_STARTED ) {
		/*DBG1("STOP %s", srv->name)*/
		if ( service_transition(srv) == -1 )
			service_delete(srv);
	} else {
		/*DBG2("UNREF %s %d", srv->name, srv->refs)*/
	}
}
void service_stop(const char *name, bool toplevel)
{
	service *srv = service_find(name);
	if (!srv) { SHOUT1("no such service `%s'", name); return; }

	service_stop_p(srv, toplevel);
}

void service_stop_all(void )
{
	service *srv, *next;
	for (srv = services ; srv ; srv = next) { next = srv->next;
		srv->refs -= srv->top_refs;
		srv->top_refs = 0;
		if ( !srv->refs && srv->state == SERVICE_STARTED )
			if ( service_transition(srv) == -1 )
				service_delete(srv);
	}
}


/******************************************************************************/


static reqnode *reqnode_new(request *req, service *srv)
{
	reqnode *n = new(reqnode);
	if (!n) return 0;
	n->req = req;
	n->srv = srv;
	if (req) LIST_INS(n,req->lst);
	if (srv) LIST_INS1(n,srv->lst);

	/*DBG2("NODE_NEW %d %s", req->client_pid, srv->name)*/
	return n;
}
static void reqnode_delete(reqnode *n)
{
	/*DBG2("NODE_DELETE %d %s", n->req->client_pid, n->srv->name)*/

	if (n->req) LIST_DEL(n,n->req->lst);
	if (n->srv) LIST_DEL1(n,n->srv->lst);
	free(n);
}


/******************************************************************************/


static request *request_new(pid_t pid, bool toplevel)
{
	request *req = new(request);
	if (!req) {
		COMPLAIN("request_new");
		return 0;
	}
	req->client_pid = pid;
	req->toplevel = toplevel;
	req->lst = 0;
	req->nrequired = 0;
	req->ntolerated = 0;
	req->nsucceeded = 0;
	req->nfailed = 0;
	LIST_INS(req,requests);

	/*DBG1("REQUEST_NEW %d", pid)*/
	return req;
}
static void request_delete(request *req)
{
	reqnode *n, *next;
	/*DBG1("REQUEST_DELETE %d", req->client_pid)*/

	LIST_DEL(req,requests);
	for (n = req->lst ; n ; n = next) { next = n->next;
		reqnode_delete(n);
	}
	free(req);
}

static void request_done(request *req, bool success)
{
	reqnode *n, *next;
	service *srv;
	/*DBG2("REQUEST %d %s", req->client_pid,success?"SUCCESS":"FAILURE");*/
	for (n = req->lst ; n ; n = next) { next = n->next;
		srv = n->srv;
		reqnode_delete(n);
		if (!success) {
			if (srv->refs == 1 && srv->state == SERVICE_STARTED)
				SHOUT1("stopping service `%s' because of "
				       "all-or-nothing request", srv->name);

			service_stop_p(srv, req->toplevel);
		}
	}
	request_delete(req);
	kill(req->client_pid, success ? SIGUSR1 : SIGUSR2);
}

void service_request(pid_t pid, pid_t ppid, char **names, int nrequired,
                     bool toplevel)
{
	int i = 0;
	request *req = 0;
	service *srv = 0;

#if 0
#if DEBUG
	srv = service_find_pid(ppid);
	fprintf(stderr, "%s: REQUEST %s %d %c ", myname, srv?srv->name:0,
	                                         pid, (nrequired>0)?'+':'-');
	spew_argv(0, names);
	fputc('\n', stderr);
#endif
#endif

	if (nrequired < 0) {
		for (i = 0 ; names[i] ; ++i)
			service_stop(names[i], toplevel);
		kill(pid, SIGUSR1);

	} else if (nrequired > 0) {
		req = request_new(pid, toplevel);
		if (!req) { kill(pid, SIGUSR2); return; }

		req->nrequired = nrequired;
		for (i = 0 ; names[i] ; ++i) ;
		req->ntolerated = i-req->nrequired;

		for (i = 0 ; names[i] ; ++i) {
			srv = service_start_p(names[i], toplevel);

			if (srv && reqnode_new(req,srv)) {
				if (srv->state == SERVICE_STARTED)
					++req->nsucceeded;
			} else {
				if (srv) service_stop_p(srv, toplevel);

				if (++req->nfailed > req->ntolerated) {
					request_done(req, false);
					break;
				}
			}
		}

		if (req->nsucceeded >= req->nrequired)
			request_done(req, true);
	}
}


/******************************************************************************/


static void service_success(service *srv)
{
	reqnode *n, *next1;
	request *req;
	for (n = srv->lst ; n ; n = next1) { next1 = n->next1;
		req = n->req;
		if ( ++req->nsucceeded >= req->nrequired )
			request_done(req, true);
	}
}
static void service_failure(service *srv)
{
	reqnode *n, *next;
	request *req;
	for (n = srv->lst ; n ; n = next) { next = n->next1;
		req = n->req;
		if ( ++req->nfailed > req->ntolerated )
			request_done(n->req, false);
	}
}

void dead_initscript(pid_t pid, int status)
{
	service *srv = service_find_pid(pid);
	if (!srv) return;

	srv->pid = 0;

	switch (srv->state) {

	case SERVICE_STARTING:
		if (status == EXIT_SUCCESS) {
			srv->state = SERVICE_STARTED;
			if (srv->refs) {
				/*DBG2("SERVICE %s SUCCESS %d", srv->name, srv->refs)*/
				service_success(srv);
			} else {
				SHOUT1("service `%s' started, but not wanted "
				       "anymore", srv->name);

				service_failure(srv);
				if ( service_transition(srv) == -1 )
					service_delete(srv);
			}
		} else {
			SHOUT1("service `%s' failed to start!", srv->name);

			srv->state = SERVICE_STOPPED;
			service_failure(srv);
			service_delete(srv);
		}
		break;

	case SERVICE_STOPPING:
		srv->state = SERVICE_STOPPED;
		if (srv->refs) {
			/*DBG2("STOPPED %s REFS %d", srv->name, srv->refs)*/
			if ( service_transition(srv) == -1 )
				service_delete(srv);
		} else {
			/*DBG1("STOPPED %s", srv->name)*/
			service_delete(srv);
		}
		break;

	default:  /* this should never happen */
		SHOUT1("unexpected initscript completion for service `%s'",
		       srv->name);
	}
}

#if DEBUG
void services_debug_report(void )
{
	service *serv;
	request *req;

	fputs("\nSERVICES:\n", stderr);

	for (serv = services ; serv ; serv = serv->next) {
		char c;
		switch (serv->state) {
		case SERVICE_STARTING: c = '+'; break;
		case SERVICE_STARTED: c = ' '; break;
		case SERVICE_STOPPING: c = '-'; break;
		default: c = '!'; break;
		}

		fprintf(stderr, "%3d%3d %c %s\n",
		        serv->refs, serv->top_refs, c, serv->name);
	}

	fputs("\nREQUESTS:\n", stderr);

	for (req = requests ; req ; req = req->next) {
		reqnode *n;
		fprintf(stderr, "%d: ", req->client_pid);
		for (n = req->lst ; n ; n = n->next) {
			if (n != req->lst)
				fputc(' ', stderr);

			if (n->srv && n->srv->name)
				fprintf(stderr, "%s", n->srv->name);
			else
				fputs("(deleted)", stderr);
		}
		fputc('\n', stderr);
	}
}
#endif
