#include <bits/syscall.h>
#include <syscall3.h>

inline static long sysunlinkat(int atfd, const char* name, int flags)
{
	return syscall3(__NR_unlinkat, atfd, (long)name, flags);
}