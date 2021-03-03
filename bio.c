#include "bio.h"
#include "common.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

static void append_chunk(
    char **pbuf, size_t *psize, size_t *pcapacity,
    const char *chunk, size_t nchunk)
{
    while (*pcapacity - *psize < nchunk) {
        *pbuf = x2realloc_or_die(*pbuf, pcapacity, sizeof(char));
    }
    if (nchunk) {
        memcpy(*pbuf + *psize, chunk, nchunk);
    }
    *psize += nchunk;
    if (*psize > (size_t) SSIZE_MAX) {
        die_out_of_memory();
    }
}

ssize_t bio_read_line(Bio *bio, char **pbuf, size_t *pcapacity, int *caught_signal)
{
    size_t psize = 0;

    if (bio->offset != bio->size) {
        char *s = bio->buf + bio->offset;
        size_t ns = bio->size - bio->offset;

        char *nl = memchr(s, '\n', ns);
        if (nl) {
            size_t nline = nl + 1 - s;
            append_chunk(
                pbuf, &psize, pcapacity,
                s, nline);
            bio->offset += nline;
            return psize;
        }
    }

    append_chunk(
        pbuf, &psize, pcapacity,
        bio->buf + bio->offset, bio->size - bio->offset);

    for (;;) {
        ssize_t r;
        while ((r = read(bio->fd, bio->buf, BIO_NBUF)) < 0 && errno == EINTR) {
            *caught_signal = 1;
        }
        if (r < 0) {
            bio->offset = 0;
            bio->size = 0;
            return -1;
        } else if (r == 0) {
            bio->offset = 0;
            bio->size = 0;
            return psize;
        } else {
            char *nl = memchr(bio->buf, '\n', r);
            if (nl) {
                size_t nline = nl + 1 - bio->buf;
                append_chunk(
                    pbuf, &psize, pcapacity,
                    bio->buf, nline);
                bio->offset = nline;
                bio->size = r;
                return psize;
            }
            append_chunk(
                pbuf, &psize, pcapacity,
                bio->buf, r);
        }
    }
}

int bio_has_something(Bio *bio)
{
    return bio->offset != bio->size;
}

void bio_reset(Bio *bio)
{
    bio->size = 0;
    bio->offset = 0;
    bio->fd = -1;
}
