#include "common.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void die_out_of_memory(void)
{
    fputs("Out of memory.\n", stderr);
    abort();
}

void *realloc_or_die(void *p, size_t n, size_t m)
{
    if (m && n > SIZE_MAX / m) {
        die_out_of_memory();
    }
    void *r = realloc(p, n * m);
    if (!r && n && m) {
        die_out_of_memory();
    }
    return r;
}

void *malloc_or_die(size_t n, size_t m)
{
    return realloc_or_die(NULL, n, m);
}

void *memdup_or_die(const void *p, size_t n)
{
    void *q = malloc_or_die(n, 1);
    if (n) {
        memcpy(q, p, n);
    }
    return q;
}

void *x2realloc_or_die(void *p, size_t *n, size_t m)
{
    if (*n) {
        if (*n > SIZE_MAX / 2) {
            die_out_of_memory();
        }
        *n *= 2;
    } else {
        *n = 1;
    }
    return realloc_or_die(p, *n, m);
}
