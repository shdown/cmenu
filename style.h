#pragma once

#include <curses.h>
#include <stddef.h>

typedef struct {
    attr_t a;
    short cpn;
} InternedStyle;

typedef struct {
    attr_t a;
    short fc;
    short bc;
} RawStyle;

int parse_style(const char *s, RawStyle *out, char *errbuf, size_t nerrbuf);

int intern_style(RawStyle style, short cpn, InternedStyle *out);
