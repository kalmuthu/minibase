#include <sys/file.h>
#include <fail.h>

/* XXX: GNU coreutils allow writing to several files at once.
   Is that useful for anything? */

ERRTAG = "tee";
ERRLIST = {
	REPORT(EAGAIN), REPORT(EBADF), REPORT(EFAULT), REPORT(EINTR),
	REPORT(EINVAL), REPORT(EIO), REPORT(EISDIR), REPORT(EDQUOT),
	REPORT(EFBIG), REPORT(ENOSPC), REPORT(EPERM), REPORT(EPIPE),
	REPORT(EEXIST), RESTASNUMBERS
};

static char buf[4*4096];

static void tee(int fd, const char* file)
{
	int rd;
	long ret;

	while((rd = sys_read(0, buf, sizeof(buf))) > 0) {
		if(fd <= 1)
			goto wr1;

		if((ret = sys_write(fd, buf, rd)) < 0) {
			warn("writing to", file, ret);
			fd = -1;
		}
	wr1:
		if((ret = sys_write(1, buf, rd)) < 0)
			fail("writing to stdout", NULL, ret);
	};
}

int main(int argc, char** argv)
{
	int i = 1;
	char* p;
	char opt[] = "-?";

	int flags = O_RDWR | O_CREAT | O_TRUNC;
	char* file = NULL;

	if(i < argc && argv[i][0] == '-')
		for(p = argv[i++] + 1; *p; p++) switch(*p) {
			case 'a': flags |= O_APPEND; break;
			case 'x': flags |= O_EXCL; break;
			case 'l': flags |= O_NOFOLLOW; break;
			case 'y': flags |= O_SYNC; break;
			default: opt[1] = *p;
				 fail("unknown option", opt, 0);
		}

	if(i < argc)
		file = argv[i++];
	else
		fail("need a file to write to", NULL, 0);
	if(i < argc)
		fail("too many arguments", NULL, 0);

	long fd = xchk(sys_open3(file, flags, 0666), "cannot open", file);

	tee(fd, file);

	return 0;
}
