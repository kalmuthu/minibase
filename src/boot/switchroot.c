#include <bits/magic.h>
#include <bits/stmode.h>

#include <sys/cwd.h>
#include <sys/exec.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/fsnod.h>
#include <sys/dents.h>
#include <sys/pid.h>
#include <sys/mount.h>

#include <format.h>
#include <string.h>
#include <fail.h>

/* Note this *may* happen to run with empty fds 0-2, but since it never
   opens anything r/w itself it's ok. At worst it will try to write its
   errors into a read-only fd pointing to a directory, failing silently. */

ERRTAG = "switchroot";
ERRLIST = {
	REPORT(EACCES), REPORT(EBADF), REPORT(EFAULT), REPORT(EINTR),
	REPORT(EIO), REPORT(ELOOP), REPORT(ENOENT), REPORT(ENOMEM),
	REPORT(ENOSYS), REPORT(ENOTDIR), REPORT(EINVAL), REPORT(ENOTDIR),
	REPORT(EBUSY), REPORT(EISDIR), REPORT(ENOMEM), REPORT(EPERM),
	REPORT(EROFS), RESTASNUMBERS
};

#define DEBUFSIZE 2000

struct root {
	long olddev;
	long newdev;
	char* newroot;
	int newrlen;
};

static inline int dotddot(const char* p)
{
	if(p[0] == '.' && !p[1])
		return 1;
	if(p[1] == '.' && !p[2])
		return 1;
	return 0;
}

static void delete_ent(struct root* ctx, char* dir, int dirfd, struct dirent* de);

/* Directory tree recursion is done here in (atfd, path) mode.
   The actualy file ops are done via the directory fd, but proper
   absolute path is also kept for error reporting and in this case
   for mount() calls. */

static void delete_rec(struct root* ctx, int dirfd, char* dir)
{
	char debuf[DEBUFSIZE];
	int delen = sizeof(debuf);
	long rd;

	while((rd = sys_getdents(dirfd, debuf, delen)) > 0)
	{
		char* ptr = debuf;
		char* end = debuf + rd;

		while(ptr < end)
		{
			struct dirent* de = (struct dirent*) ptr;
			ptr += de->reclen;

			if(!de->reclen)
				break;
			if(dotddot(de->name))
				continue;

			delete_ent(ctx, dir, dirfd, de);
		}
	};
};

static void make_path(char* buf, int len, char* dir, char* name)
{
	char* p = buf;
	char* e = buf + len - 1;

	p = fmtstr(p, e, dir);
	p = fmtstr(p, e, "/");
	p = fmtstr(p, e, name);
	*p = '\0';
}

static void move_mount(struct root* ctx, char* path)
{
	char newpath[ctx->newrlen + strlen(path) + 5];

	char* p = newpath;
	char* e = newpath + sizeof(newpath) - 1;

	p = fmtstr(p, e, ctx->newroot);
	p = fmtstr(p, e, path);
	*p = '\0';

	int ret;

	if((ret = sys_mount(path, newpath, NULL, MS_MOVE, NULL)) >= 0)
		return;

	warn("mount", newpath, ret);

	if((ret = sys_umount(path, MNT_DETACH)) >= 0)
		return;

	warn("umount", path, ret);
}

/* This one gets de@dirfd = "$dir/$de.name" but we don't know what kind
   of direntry it is yet. There is no need to call statat() however.
   We can just try to unlink it as a file, and if it happens to be a dir,
   unlink() whould fail with EISDIR.

   The check may be skipped if getdents promises us it's a dir. */

static void delete_ent(struct root* ctx, char* dir, int dirfd, struct dirent* de)
{
	int fd, ret;
	struct stat st;
	char* name = de->name;
	char path[strlen(dir) + strlen(name) + 5];

	make_path(path, sizeof(path), dir, name);

	if(de->type == DT_DIR)
		goto dir;
	if((ret = sys_unlinkat(dirfd, name, 0)) >= 0)
		return;
	if(ret != EISDIR)
		goto err;
dir:
	if((fd = sys_openat(dirfd, name, O_DIRECTORY)) < 0)
		return;
	if((ret = sys_fstatat(fd, "", &st, AT_EMPTY_PATH)) < 0)
		goto out;

	if(st.st_dev == ctx->olddev) {
		delete_rec(ctx, fd, path);
		ret = sys_unlinkat(dirfd, name, AT_REMOVEDIR);
	} else if(st.st_dev == ctx->newdev) {
		/* do nothing */
		ret = 0;
	} else {
		move_mount(ctx, path);
		ret = 0;
	}
out:
	sys_close(fd);
err:
	if(ret) warn(NULL, path, ret);
}

/* Before starting directory tree recursion, we need to figure out
   what the boundaries are. Only the stuff on oldroot should be unlinked,
   and anything that's neither oldroot nor newroot has to be move-mounted
   onto the newroot.
  
   The tricky part: if newroot is not mounted directly under oldroot,
   the move-mount code will try to move newroot's parent into newroot. */

static int stat_old_new_root(struct root* ctx, char* newroot)
{
	struct stat st;

	ctx->newroot = newroot;
	ctx->newrlen = strlen(newroot);

	xchk(sys_stat(newroot, &st), "stat", newroot);

	ctx->newdev = st.st_dev;

	int fd = xchk(sys_open("/", O_DIRECTORY), "open", "/");

	xchk(sys_fstatat(fd, "", &st, AT_EMPTY_PATH), "stat", "/");

	ctx->olddev = st.st_dev;

	if(ctx->newdev == ctx->olddev)
		fail("new root is on the same fs", NULL, 0);

	/* . = newroot, so .. is its parent directory */
	xchk(sys_stat("..", &st), "stat", "..");

	if(st.st_dev != ctx->olddev)
		fail(newroot, "is not directly under /", 0);

	return fd;
}

/* Avoid deleting any actual permanent files as hard as possible. */

static int check_ramfs(void)
{
	struct statfs st;

	if(sys_statfs("/", &st) < 0)
		return -1;
	if(st.type != RAMFS_MAGIC && st.type != TMPFS_MAGIC)
		return -1;

	return 0;
};

static void maybe_reopen_fds(struct root* ctx)
{
	struct stat st;
	int fd, i;

	if((sys_fstat(0, &st)) < 0)
		return;
	if(st.st_dev != ctx->olddev)
		return;
	if((fd = sys_open("/dev/console", O_RDWR)) < 0)
		return;

	for(i = 0; i <= 2; i++)
		if(fd != i)
			sys_dup2(fd, i);
	if(fd > 2)
		sys_close(fd);
}

static void changeroot(char* newroot)
{
	struct root ctx;

	if(sys_getpid() != 1)
		fail("not running as pid 1", NULL, 0);
	if(check_ramfs())
		fail("not running on ramfs", NULL, 0);

	xchk(sys_chdir(newroot), "chdir", newroot);

	int rfd = stat_old_new_root(&ctx, newroot);

	delete_rec(&ctx, rfd, "");
	sys_close(rfd);

	xchk(sys_mount(".", "/", NULL, MS_MOVE, NULL), "mount", ". to /");
	xchk(sys_chroot("."), "chroot", ".");
	xchk(sys_chdir("/"), "chdir", "/");

	maybe_reopen_fds(&ctx);
}

/* Usage: switchroot /newroot [/sbin/init ...] */

int main(int argc, char** argv)
{
	if(argc < 2)
		fail("no newroot to switch to", NULL, 0);

	changeroot(argv[1]);

	if(argc >= 3) {
		argv += 2;
	} else { /* no init has been supplied */
		argv[0] = "/sbin/init";
		argv[1] = NULL;
	}

	long ret = sys_execve(*argv, argv, NULL);
	fail("cannot exec", *argv, ret);
}
