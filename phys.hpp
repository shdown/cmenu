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

#include <stdint.h>
#include <limits.h>
#include <type_traits>
#include "errors.hpp"
#include "arith_utils.hpp"

class PHYS
{
private:
    uint32_t m_val;

public:
    explicit PHYS(uint32_t val)
        : m_val{val}
    {}

    template<class T>
    static PHYS from(T x)
    {
        static_assert(std::is_unsigned_v<T>, "T must be unsigned");

        if (x > UINT32_MAX) {
            throw PhysMathError("overflow");
        }

        return PHYS{static_cast<uint32_t>(x)};
    }

    PHYS operator +(PHYS other) const
    {
        uint32_t res = m_val;
        if (!arith_utils::add_is_ok(res, other.m_val)) {
            throw PhysMathError("overflow");
        }
        return PHYS{res};
    }

    PHYS operator -(PHYS other) const
    {
        if (m_val < other.m_val) {
            throw PhysMathError("underflow");
        }
        return PHYS{m_val - other.m_val};
    }

    PHYS &operator +=(PHYS other)
    {
        uint32_t res = m_val;
        if (!arith_utils::add_is_ok(res, other.m_val)) {
            throw PhysMathError("overflow");
        }
        m_val = res;
        return *this;
    }

    PHYS &operator -=(PHYS other)
    {
        if (m_val < other.m_val) {
            throw PhysMathError("underflow");
        }
        m_val -= other.m_val;
        return *this;
    }

    PHYS &operator ++()
    {
        return (*this += PHYS{1});
    }

    PHYS operator ++(int)
    {
        PHYS copy = *this;
        ++*this;
        return copy;
    }

    PHYS &operator --()
    {
        return (*this -= PHYS{1});
    }

    PHYS operator --(int)
    {
        PHYS copy = *this;
        --*this;
        return copy;
    }

    bool operator  <(PHYS other) const { return m_val <  other.m_val; }
    bool operator  >(PHYS other) const { return m_val >  other.m_val; }

    bool operator <=(PHYS other) const { return m_val <= other.m_val; }
    bool operator >=(PHYS other) const { return m_val >= other.m_val; }

    bool operator ==(PHYS other) const { return m_val == other.m_val; }
    bool operator !=(PHYS other) const { return m_val != other.m_val; }

    explicit operator bool() const
    {
        return m_val != 0;
    }

    bool operator !() const
    {
        return m_val == 0;
    }

    uint32_t raw() const
    {
        return m_val;
    }

    int to_int() const
    {
        if (m_val > INT_MAX) {
            throw PhysMathError("overflow");
        }
        return m_val;
    }
};

namespace phys_utils
{
    static inline PHYS scale(PHYS a, PHYS b, PHYS denom)
    {
        if (!denom) {
            throw PhysMathError("division by zero");
        }
        uint64_t res = static_cast<uint64_t>(a.raw()) * b.raw() / denom.raw();
        return PHYS::from(res);
    }

} // namespace phys_utils
