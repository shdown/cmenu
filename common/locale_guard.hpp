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

#include <locale.h>
#include <errno.h>
#include <system_error>
#include "my_assert.hpp"

class LocaleGuard
{
private:
    locale_t m_native;
    locale_t m_old;

public:
    LocaleGuard(const char *locname)
        : m_native{}
        , m_old{}
    {
        MY_ASSERT(locname != nullptr);

        locale_t bad_locale = static_cast<locale_t>(0);

        m_native = newlocale(LC_ALL_MASK, locname, bad_locale);
        if (m_native == bad_locale) {
            throw std::system_error(errno, std::generic_category(), "newlocale() failed");
        }
        m_old = uselocale(m_native);
        if (m_old == bad_locale) {
            freelocale(m_native);
            throw std::system_error(errno, std::generic_category(), "uselocale() failed");
        }
    }

    LocaleGuard(const LocaleGuard &) = delete;
    LocaleGuard(LocaleGuard &&) = delete;

    LocaleGuard &operator =(const LocaleGuard &) = delete;
    LocaleGuard &operator =(LocaleGuard &&) = delete;

    ~LocaleGuard()
    {
        freelocale(m_native);
        uselocale(m_old);
    }
};

