#pragma once

#include <stddef.h>

void die_out_of_memory(void);

void *malloc_or_die(size_t n, size_t m);

void *realloc_or_die(void *p, size_t n, size_t m);

void *x2realloc_or_die(void *p, size_t *n, size_t m);

void *memdup_or_die(const void *p, size_t n);
