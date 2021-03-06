#include <format.h>

char* parselong(char* buf, long* np)
{
	long d, n = 0;
	char* p;

	for(p = buf; *p; p++)
		if(*p >= '0' && (d = *p - '0') < 10)
			n = n*10 + d;
		else
			break;

	*np = n;
	return p;
}
