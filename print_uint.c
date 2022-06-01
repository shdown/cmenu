#include "print_uint.h"

static inline size_t getndigits(uint64_t x)
{
    if (x < UINT64_C(10)) return 1;
    if (x < UINT64_C(100)) return 2;
    if (x < UINT64_C(1000)) return 3;
    if (x < UINT64_C(10000)) return 4;
    if (x < UINT64_C(100000)) return 5;
    if (x < UINT64_C(1000000)) return 6;
    if (x < UINT64_C(10000000)) return 7;
    if (x < UINT64_C(100000000)) return 8;
    if (x < UINT64_C(1000000000)) return 9;
    if (x < UINT64_C(10000000000)) return 10;
    if (x < UINT64_C(100000000000)) return 11;
    if (x < UINT64_C(1000000000000)) return 12;
    if (x < UINT64_C(10000000000000)) return 13;
    if (x < UINT64_C(100000000000000)) return 14;
    if (x < UINT64_C(1000000000000000)) return 15;
    if (x < UINT64_C(10000000000000000)) return 16;
    if (x < UINT64_C(100000000000000000)) return 17;
    if (x < UINT64_C(1000000000000000000)) return 18;
    if (x < UINT64_C(10000000000000000000)) return 19;
    return 20;
}

size_t print_uint(char *out, uint64_t x)
{
    size_t n = getndigits(x);

    size_t i = n;
    do {
        out[i - 1] = '0' + (x % 10);
        x /= 10;
    } while (--i);

    return n;
}
