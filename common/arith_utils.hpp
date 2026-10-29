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

#include <limits.h>
#include <type_traits>
#include <limits>

namespace arith_utils
{
    template<class T>
    static inline bool mul_is_ok(T &x, T y) noexcept
    {
        static_assert(std::is_unsigned_v<T>, "T must be unsigned");

        T max = std::numeric_limits<T>::max();
        if (y && x > max / y) {
            return false;
        }
        x *= y;
        return true;
    }

    template<class T>
    static inline bool add_is_ok(T &x, T y) noexcept
    {
        static_assert(std::is_unsigned_v<T>, "T must be unsigned");

        T res = x + y;
        if (res < y) {
            return false;
        }
        x = res;
        return true;
    }

    template<class T>
    static inline int to_int_saturate(T x) noexcept
    {
        static_assert(std::is_unsigned_v<T>, "T must be unsigned");

        if (x > INT_MAX) {
            return INT_MAX;
        }
        return x;
    }

} // namespace arith_utils
