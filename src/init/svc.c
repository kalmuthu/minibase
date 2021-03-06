#include <bits/socket.h>
#include <bits/socket/unix.h>
#include <sys/file.h>
#include <sys/socket.h>

#include <string.h>
#include <format.h>
#include <fail.h>

#include "config.h"

ERRTAG = "svc";
ERRLIST = {
	REPORT(ENOENT), REPORT(ECONNREFUSED), REPORT(ELOOP), REPORT(ENFILE),
	REPORT(EMFILE), REPORT(EINTR), REPORT(EINVAL), REPORT(EACCES),
	REPORT(EPERM), REPORT(EIO), RESTASNUMBERS
}; 

/* Telinit (svc) sends commands to init (svcmon) via its control socket
   and reads init's output back. One command is sent per connection.
   In case there are multiple arguments, telinit sends one at a time
   and reconnects between sending them. There is no multiplexing
   of any sort, it's write-all followed by read-all for each command. */

static int runcmd(const char* cmd, int len);

/* For convenience, one-letter command codes (cc's) are given readable
   names within svc. These should be kept in sync with svcmon_cmds.c */

static struct cmdrec {
	char cc;
	char arg;
	char name[10];
} cmdtbl[] = {
	/* halt */
	{ 'h', 0, "halt",	},
	{ 'p', 0, "poweroff",	},
	{ 'r', 0, "reboot",	},
	/* process ops */
	{ 'x', 1, "restart",	},
	{ 'd', 1, "stop",	},
	{ 'e', 1, "start",	},
	{ 's', 1, "pause",	},
	{ 'c', 1, "resume",	},
	{ 'u', 1, "hup",	},
	/* state query */
	{ 'l', 0, "list",	},
	{ 'i', 1, "pidof",	},
	{ 'q', 1, "show",	},
	{ 'f', 2, "flush",	},
	/* reconfigure */
	{ 'z', 0, "reload",	},
	{  0  }
};

int main(int argc, char** argv)
{
	struct cmdrec* cr = NULL;
	char buf[NAMELEN+2];
	char* p = buf;
	char* e = buf + sizeof(buf);

	int hasarg = 0;
	char* ptr = buf;
	char* cmd = argv[1];
	char* cm1 = cmd + 1;
	int i;

	if(argc < 2)
		fail("too few arguments", NULL, 0);

	for(cr = cmdtbl; cr->cc; cr++) {
		if(!*cm1 && *cmd == cr->cc)
			break;
		if(!strcmp(cmd, cr->name))
			break;
	} if(!cr->cc)
		fail("unknown command", cmd, 0);

	*p++ = cr->cc; *p = '\0';
	hasarg = cr->arg;

	int ret = 0;

	if(!hasarg)
		ret = runcmd(ptr, p - buf);
	else if(hasarg == 2 && argc <= 2)
		ret = runcmd(ptr, p - buf);
	else for(i = 2; i < argc; i++) {
		char* q = fmtstr(p, e, argv[i]);
		ret |= runcmd(buf, q - buf);
	}

	return (ret ? 1 : 0);
}

static int opensocket(void)
{
	struct sockaddr_un addr = {
		.family = AF_UNIX,
		.path = SVCTL
	};

	if(addr.path[0] == '@')
		addr.path[0] = '\0';

	int fd = xchk(sys_socket(AF_UNIX, SOCK_STREAM, 0),
			"socket", "AF_UNIX SOCK_STREAM");

	xchk(sys_connect(fd, &addr, sizeof(addr)),
			"connect", SVCTL);

	return fd;
}

static void sendcmd(int fd, const char* cmd, int len)
{
	xchk(sys_write(fd, cmd, len), "write", SVCTL);
}

/* The tricky part here is demuxing init output, which can be error
   messages to be sent to stderr, or pidof/list output which is stdout
   kind of data.

   Init has a very specific reply pattern, it's eiter all-error
   or all-non-error, so a simple # indicator at the start of init
   output is used to choose the fd.

   As a side effect, this also determines telinit return code.
   Non-empty error message means there was an error, which empty
   output or any #-output means everything went well. */

static int recvreply(int fd)
{
	char buf[100];
	int rr;
	int out = 0;
	int ret = 0;
	int off = 0;

	while((rr = sys_read(fd, buf, sizeof(buf))) > 0) {
		if(!out) {
			if(buf[0] == '#') {
				off = 1;
				out = 1;
			} else {
				ret = -1;
				out = 2;
			}
		} else if(off) off = 0;

		sys_write(out, buf + off, rr - off);
	}

	return ret;
}

static int runcmd(const char* cmd, int len)
{
	int fd;
	int r = 0;

	fd = opensocket();
	sendcmd(fd, cmd, len);
	sys_shutdown(fd, SHUT_WR);
	r = recvreply(fd);
	sys_close(fd);

	return r;
};
