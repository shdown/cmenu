#include "print_uint.h"

int print_uint(char *out, size_t nout, uint64_t x)
{
    char buf[32];
    int nbuf = 0;

    if (!x) {
        buf[nbuf++] = '0';
    } else {
        do {
            buf[nbuf++] = '0' + (x % 10);
            x /= 10;
        } while (x);
    }

    if (nout < (size_t) nbuf) {
        return -1;
    }
    for (int i = nbuf - 1; i >= 0; --i) {
        *out++ = buf[i];
    }
    return nbuf;
}
