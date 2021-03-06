#include <sys/file.h>
#include <sys/poll.h>
#include <sys/signal.h>

#include <format.h>
#include <sigset.h>
#include <fail.h>

#include "vtmux.h"

#define PFDS (CONSOLES + KEYBOARDS + 1)

int nfds;
struct pollfd pfds[PFDS];
static sigset_t defsigset;
int sigterm;
int sigchld;

static void sighandler(int sig)
{
	switch(sig) {
		case SIGINT:
		case SIGTERM: sigterm = 1; break;
		case SIGCHLD: sigchld = 1; break;
	}
}

void setup_signals(void)
{
	struct sigaction sa = {
		.handler = sighandler,
		.flags = SA_RESTART | SA_RESTORER,
		.restorer = sigreturn
	};

	int ret = 0;

	sigemptyset(&sa.mask);
	sigaddset(&sa.mask, SIGCHLD);
	ret |= sys_sigprocmask(SIG_BLOCK, &sa.mask, &defsigset);

	sigaddset(&sa.mask, SIGINT);
	sigaddset(&sa.mask, SIGTERM);
	sigaddset(&sa.mask, SIGHUP);

	ret |= sys_sigaction(SIGINT,  &sa, NULL);
	ret |= sys_sigaction(SIGTERM, &sa, NULL);
	ret |= sys_sigaction(SIGHUP,  &sa, NULL);

	/* SIGCHLD is only allowed to arrive in ppoll,
	   so SA_RESTART just does not make sense. */
	sa.flags &= ~SA_RESTART;
	ret |= sys_sigaction(SIGCHLD, &sa, NULL);

	if(ret) fail("signal init failed", NULL, 0);
}

/* Empty slots have everything = 0, but listening on fd 0
   aka stdin is not a good idea, so we skip those.

   The number of taken entries in pfds still has to match nconsoles
   so that check_polled_fds would be able to match fd to their
   respective consoles[] or keyboards[] slots. */

static int consfd(struct vtx* cvt)
{
	return cvt->pid > 0 ? cvt->ctlfd : -1;
}

static int keybfd(struct kbd* kb)
{
	return kb->fd > 0 ? kb->fd : -1;
}

void update_poll_fds(void)
{
	int i;
	int j = 0;

	for(i = 0; i < nconsoles && j < PFDS; i++)
		pfds[j++].fd = consfd(&consoles[i]);

	for(i = 0; i < nkeyboards && j < PFDS; i++)
		pfds[j++].fd = keybfd(&keyboards[i]);

	if(j < PFDS && inotifyfd > 0)
		pfds[j++].fd = inotifyfd;

	for(i = 0; i < j; i++)
		pfds[i].events = POLLIN;

	nfds = j;
	pollready = 1;
}

/* Failing fds are dealt with immediately, to avoid re-queueing
   them from ppoll. For keyboards, this is also the only place
   where the fds get closed. Control fds are closed either here
   or in closevt(). */

static void closectl(struct vtx* cvt)
{
	if(cvt->ctlfd > 0) {
		sys_close(cvt->ctlfd);
		cvt->ctlfd = -1;
	}
}

static void closekbd(struct kbd* kb)
{
	sys_close(kb->fd);
	kb->fd = 0;
	kb->dev = 0;
	kb->mod = 0;
}

void check_polled_fds(void)
{
	int j, k;

	for(j = 0; j < nfds; j++) {
		int revents = pfds[j].revents;
		int pollin = revents & POLLIN;
		int fd = pfds[j].fd;

		if(!revents)
			continue;
		if(!pollin)
			pfds[j].fd = -1;

		if((k = j) < nconsoles) {
			struct vtx* cvt = &consoles[k];

			if(pollin)
				handlectl(cvt, fd);
			else
				closectl(cvt);

		} else if((k = j - nconsoles) < nkeyboards) {
			struct kbd* kb = &keyboards[k];

			if(pollin)
				handlekbd(kb, fd);
			else
				closekbd(kb);
		} else {
			if(pollin)
				handleino(fd);
			else
				inotifyfd = -1; /* ugh */
		}
	}
}

void mainloop(void)
{
	while(!sigterm)
	{
		sigchld = 0;

		if(!pollready)
			update_poll_fds();

		int r = sys_ppoll(pfds, nfds, NULL, &defsigset);

		if(sigchld)
			waitpids();
		if(r == -EINTR)
			; /* signal has been caught and handled */
		else if(r < 0)
			fail("ppoll", NULL, r);
		else if(r > 0)
			check_polled_fds();
	}
}
