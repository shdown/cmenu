#include "style.h"
#include "parse_uint.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

static short parse_color(const char *s, size_t ns, char *errbuf, size_t nerrbuf)
{
    short r = parse_uint(s, ns, SHRT_MAX);
    if (r < 0) {
        int truncated_ns = ns < 1024 ? ns : 1024;
        snprintf(errbuf, nerrbuf, "invalid color number: %s: '%.*s'", parse_uint_strerror(r), truncated_ns, s);
        return -1;
    }
    return r;
}

static bool span_eq(const char *s, const char *buf, size_t nbuf)
{
    return nbuf == strlen(s) && (nbuf == 0 || memcmp(s, buf, nbuf) == 0);
}

static int parse_segment(RawStyle *rs, const char *s, size_t ns, char *errbuf, size_t nerrbuf)
{
    if (span_eq("normal", s, ns)) {
        return 0;
    }
    if (span_eq("bold", s, ns)) {
        rs->a |= A_BOLD;
        return 0;
    }
    if (span_eq("blink", s, ns)) {
        rs->a |= A_BLINK;
        return 0;
    }
    if (span_eq("dim", s, ns)) {
        rs->a |= A_DIM;
        return 0;
    }
    if (span_eq("reverse", s, ns)) {
        rs->a |= A_REVERSE;
        return 0;
    }
    if (span_eq("standout", s, ns)) {
        rs->a |= A_STANDOUT;
        return 0;
    }
    if (span_eq("underline", s, ns)) {
        rs->a |= A_UNDERLINE;
        return 0;
    }
    if (ns >= 2 && memcmp("f=", s, 2) == 0) {
        short c = parse_color(s + 2, ns - 2, errbuf, nerrbuf);
        if (c < 0)
            return -1;
        rs->fc = c;
        return 0;
    }
    if (ns >= 2 && memcmp("b=", s, 2) == 0) {
        short c = parse_color(s + 2, ns - 2, errbuf, nerrbuf);
        if (c < 0)
            return -1;
        rs->bc = c;
        return 0;
    }

    int truncated_ns = ns < 1024 ? ns : 1024;
    snprintf(errbuf, nerrbuf, "invalid style segment: '%.*s'", truncated_ns, s);
    return -1;
}

int parse_style(const char *s, RawStyle *out, char *errbuf, size_t nerrbuf)
{
    RawStyle rs = {0, -1, -1};
    for (;;) {
        const char *t = strchr(s, ',');
        if (t) {
            if (parse_segment(&rs, s, t - s, errbuf, nerrbuf) < 0)
                return -1;
            s = t + 1;
        } else {
            if (parse_segment(&rs, s, strlen(s), errbuf, nerrbuf) < 0)
                return -1;
            break;
        }
    }
    *out = rs;
    return 0;
}

int intern_style(RawStyle style, short cpn, InternedStyle *out)
{
    int ncolors = COLORS;
    if (style.fc >= ncolors || style.bc >= ncolors) {
        goto error;
    }
    if (init_pair(cpn, style.fc, style.bc) != OK) {
        goto error;
    }
    *out = (InternedStyle) {.a = style.a, .cpn = cpn};
    return 0;
error:
    *out = (InternedStyle) {.a = 0, .cpn = 0};
    return -1;
}
