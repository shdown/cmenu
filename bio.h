#pragma once

#include <stddef.h>
#include <sys/types.h>

enum { BIO_NBUF = 1024 };

typedef struct {
    char buf[BIO_NBUF];
    size_t size;
    size_t offset;
    int fd;
} Bio;

ssize_t bio_read_line(Bio *bio, char **pbuf, size_t *pcapacity, int *caught_signal);

int bio_has_something(Bio *bio);

void bio_reset(Bio *bio);
