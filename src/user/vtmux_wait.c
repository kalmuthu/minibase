#include <sys/alarm.h>
#include <sys/kill.h>
#include <sys/file.h>
#include <sys/wait.h>

#include <format.h>
#include <null.h>

#include "vtmux.h"

/* Non-terminal SIGCHLD handler. Close fds, deallocate VT,
   and do whatever else cleanup necessary.

   Most clients should die while active, but inactive ones may die
   as well. Background deaths should not cause VT switching.

   In case of abnormal exit, let the user read whatever the failed
   process might have printed to its stderr.

   Successful exit means logout and return to greeter. Except on
   a fixed VT, then it is probably better to restart the client.
   There's no such thing as "logout" on fixed VTs, and no login
   either, so no point in activating greeter VT.

   Restarts are not timed. Abnormal exits require user intervention,
   and normal exits are presumed to not happen too fast.

   Greeter may, and probably should, exit with 0 status if it is not
   being used for some time. There's no point in keeping it running
   in background, it will be re-started on request anyway. */

static struct vtx* find_pid_rec(int pid)
{
	int i;

	for(i = 0; i < nconsoles; i++)
		if(consoles[i].pid == pid)
			return &consoles[i];

	return NULL;
}

static void report_cause(int fd, int status)
{
	char msg[32];
	char* p = msg;
	char* e = msg + sizeof(msg) - 1;

	if(WIFEXITED(status)) {
		p = fmtstr(p, e, "Exit ");
		p = fmtint(p, e, WEXITSTATUS(status));
	} else {
		p = fmtstr(p, e, "Signal ");
		p = fmtint(p, e, WTERMSIG(status));
	}

	*p++ = '\n';
	sys_write(fd, msg, p - msg);
}

void waitpids(void)
{
	int status;
	int pid;
	struct vtx* active = NULL;

	while((pid = sys_waitpid(-1, &status, WNOHANG)) > 0)
	{
		struct vtx* cvt = find_pid_rec(pid);

		if(!cvt)
			continue;
		if(status)
			report_cause(cvt->ttyfd, status);
		if(cvt->tty == activetty && !status)
			active = cvt;

		closevt(cvt, !!status);
	}

	if(!active)
		return;
	if(active->pin)
		switchto(active->tty); /* try to restart it */
	else
		switchto(consoles[0].tty); /* greeter */
}

/* Shutdown routines: wait for VT clients to die before exiting. */

static int countrunning(void)
{
	int i;
	int count = 0;

	for(i = 0; i < nconsoles; i++)
		if(consoles[i].pid > 0)
			count++;

	return count;
}

static void markdead(int pid)
{
	int i;

	for(i = 0; i < nconsoles; i++)
		if(consoles[i].pid == pid)
			consoles[i].pid = -1;
}

static void killall(void)
{
	int i;

	for(i = 0; i < nconsoles; i++)
		if(consoles[i].pid > 0)
			sys_kill(consoles[i].pid, SIGTERM);
}

void shutdown(void)
{
	int status;
	int pid;

	sys_alarm(5);
	killall();

	while(countrunning() > 0)
		if((pid = sys_waitpid(-1, &status, 0)) > 0)
			markdead(pid);
		else break;

	unlock_switch();
}
