#include <bits/ints.h>

void pbkdf2_sha1(void* psk, int len,
                 void* pass, int passlen,
                 void* salt, int saltlen, int iters);
