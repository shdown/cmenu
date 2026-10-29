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

#pragma once

#include <string>
#include <string_view>
#include <limits>
#include <type_traits>
#include <stdexcept>
#include <stdint.h>

namespace int_utils
{

class CannotParse : public std::runtime_error
{
public:
    CannotParse(const std::string &msg)
        : std::runtime_error(msg)
    {}
};

uint64_t parse_uint_with_limit(std::string_view s, uint64_t max);

template<class T>
static inline T parse_uint_as(std::string_view s)
{
    static_assert(std::is_integral_v<T>, "T must be an integral type");

    return parse_uint_with_limit(s, std::numeric_limits<T>::max());
}

void append_uint_to_str(std::string &s, uint64_t x);

} // namespace int_utils
