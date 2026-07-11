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

#include "int_utils.hpp"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include "errors.hpp"
#include "arith_utils.hpp"

uint64_t parse_uint_with_limit(std::string_view s, uint64_t max)
{
    if (s.empty()) {
        throw ParseError("empty span");
    }

    uint64_t r = 0;

    for (char c : s) {
        unsigned char digit = c - '0';
        if (digit > 9) {
            throw ParseError("invalid digit");
        }
        if (!arith_utils::mul_is_ok<uint64_t>(r, 10)) {
            throw ParseError("overflow");
        }
        if (!arith_utils::add_is_ok<uint64_t>(r, digit)) {
            throw ParseError("overflow");
        }
    }
    if (r > max) {
        throw ParseError("overflow");
    }
    return r;
}

static inline int get_num_digits(uint64_t x)
{
    int res = 0;
    do {
        x /= 10;
        ++res;
    } while (x);
    return res;
}

void append_uint_to_str(std::string &s, uint64_t x)
{
    int ndigits = get_num_digits(x);

    size_t old_size = s.size();
    s.resize(old_size + ndigits);

    for (int i = ndigits - 1; i >= 0; --i) {
        s[old_size + i] = '0' + (x % 10);
        x /= 10;
    }
}
