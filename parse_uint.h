#pragma once

#include <stdint.h>
#include <stddef.h>

int64_t parse_uint(const char *s, size_t ns, int64_t max);

const char *parse_uint_strerror(int64_t r);
