/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <https://unlicense.org>
 */

#include "truncated_text.hpp"
#include <wchar.h>
#include <stdint.h>

#include "probes.generated.h"

#if HAVE_WCWIDTH
static inline int my_wcwidth(wchar_t c)
{
    return wcwidth(c);
}
#else
static inline int my_wcwidth(wchar_t c)
{
    if (c == L'\0') {
        return 0;
    } else if ((c > 0 && c < 32) || c == 127) {
        return -1;
    } else {
        return 1;
    }
}
#endif

void TruncatedText::truncate_to_width(PHYS width)
{
    if (m_target_width == width) {
        return;
    }

    uint64_t cur_w = 0;
    size_t i = 0;

    for (; i < m_str.size(); ++i) {
        wchar_t c = m_str[i];
        int w = my_wcwidth(m_str[i]);
        if (w < 0 || c == L'\0') {
            m_str[i] = L'.';
            w = 1;
        }
        cur_w += w;
        if (cur_w > width.raw()) {
            break;
        }
    }

    m_truncated_n = i;
    m_target_width = width;
}
