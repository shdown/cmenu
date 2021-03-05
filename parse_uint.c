#include "parse_uint.h"
#include <limits.h>

enum {
    E_EMPTY    = -1,
    E_BAD      = -2,
    E_OVERFLOW = -3,
};

int64_t parse_uint(const char *s, size_t ns, int64_t max)
{
    if (!ns)
        return E_EMPTY;
    int64_t r = 0;
    for (size_t i = 0; i < ns; ++i) {
        int digit = s[i] - '0';
        if (digit < 0 || digit > 9)
            return E_BAD;
        if (r > INT64_MAX / 10)
            return E_OVERFLOW;
        r *= 10;
        if (r > max - digit)
            return E_OVERFLOW;
        r += digit;
    }
    return r;
}

const char *parse_uint_strerror(int64_t r)
{
    switch (r) {
    case E_EMPTY:
        return "the span is empty";
    case E_BAD:
        return "contains non-digit characters";
    case E_OVERFLOW:
        return "overflow";
    }
    return "(not a valid error code)";
}
