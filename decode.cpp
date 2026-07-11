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

#include "decode.hpp"

#include <wchar.h>
#include <stddef.h>
#include <string>
#include <string_view>

std::wstring decode(std::string_view s)
{
    mbstate_t state = {};

    std::wstring res;
    res.reserve(16);

    size_t pos = 0;

    while (pos < s.size()) {
        wchar_t c;
        size_t rc = mbrtowc(&c, s.data() + pos, s.size() - pos, &state);
        if (
            rc == 0 ||
            rc == static_cast<size_t>(-1) ||
            rc == static_cast<size_t>(-2))
        {
            res.push_back(L'?');
            break;
        }
        res.push_back(c);
        pos += rc;
    }

    res.shrink_to_fit();
    return res;
}
