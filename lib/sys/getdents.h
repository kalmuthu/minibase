#include <syscall.h>
#include <bits/dirent.h>

inline static long sysgetdents64(int fd, void* dp, int count)
{
	return syscall3(__NR_getdents64, fd, (long)dp, count);
}
