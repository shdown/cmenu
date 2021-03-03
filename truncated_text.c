#include "truncated_text.h"

void truncate_text_to_width(TruncatedText *t, uint32_t width)
{
    if (t->target_width == width)
        return;

    wchar_t *s = t->s;
    size_t n = t->n;

    uint64_t cur_w = 0;
    size_t i = 0;

    for (; i < n; ++i) {
        int w = wcwidth(s[i]);
        if (w < 0) {
            s[i] = L'.';
            w = 1;
        }
        cur_w += w;
        if (cur_w > width) {
            break;
        }
    }

    t->truncated_n = i;
    t->target_width = width;
}
