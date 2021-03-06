#include <sys/file.h>
#include <sys/access.h>

#include <string.h>
#include <format.h>
#include <exit.h>
#include <null.h>

#define TAG "which"

static void warn(const char* obj, const char* msg)
{
	char buf[100];
	char* end = buf + sizeof(buf) - 1;
	char* p = buf;

	p = fmtstr(p, end, TAG ": ");

	if(!obj) goto no;

	p = fmtstr(p, end, obj);
	p = fmtstr(p, end, " ");
no:
	p = fmtstr(p, end, msg);
	p = fmtstr(p, end, "\n");

	sys_write(2, buf, p - buf);
}

static char* xgetenv(char** envp, char* var)
{
	char** p;
	int len = strlen(var);

	for(p = envp; *p; p++)
		if(!strncmp(*p, var, len))
			return *p + len;

	warn(NULL, "$PATH is not set");
	_exit(-1);
}

static int execheck(const char* dir, int dirlen, const char* cmd, int cmdlen)
{
	char path[dirlen + cmdlen + 4];
	char* p = path;

	memcpy(p, dir, dirlen); p += dirlen;
	*p++ = '/';
	memcpy(p, cmd, cmdlen); p += cmdlen;
	*p = '\0';

	if(sys_access(path, X_OK) < 0)
		return 0;
	
	*p++ = '\n';
	sys_write(1, path, p - path);

	return -1;
}

static int which(const char* path, const char* cmd, int cmdlen)
{ 
	const char* pend = path + strlen(path);
	const char* p;
	const char* q;

	for(p = path; p < pend; p = q + 1) {
		for(q = p; *q && *q != ':'; q++)
			;
		if(execheck(p, q - p, cmd, cmdlen))
			return 0;
	}

	warn(cmd, "not found in $PATH");
	return -1;
}

int main(int argc, char** argv, char** envp)
{
	int i;
	char* path = xgetenv(envp, "PATH=");
	int ret = 0;

	for(i = 1; i < argc; i++)
		ret |= which(path, argv[i], strlen(argv[i]));

	return ret;
}
