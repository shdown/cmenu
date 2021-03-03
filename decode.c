#include "decode.h"
#include "common.h"
#include <stdlib.h>

wchar_t *decode_copy(const char *s)
{
    size_t nbuf = 1;
    wchar_t *buf = malloc_or_die(nbuf, sizeof(wchar_t));
    for (;;) {
        const char *x = s;
        mbstate_t state = {0};
        size_t r = mbsrtowcs(buf, &x, nbuf, &state);
        if (r == (size_t) -1) {
            free(buf);
            return NULL;
        }
        if (!x) {
            return buf;
        }
        buf = x2realloc_or_die(buf, &nbuf, sizeof(wchar_t));
    }
}
