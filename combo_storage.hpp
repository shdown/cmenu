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

#include <vector>
#include <string>
#include <algorithm>
#include "common/strview_utils.hpp"
#include "common/my_assert.hpp"
#include "errors.hpp"

class ComboStorage
{
public:
    enum class LookupResult
    {
        EXACT,
        PREFIX,
        NONE,
    };

    static bool is_valid_symbol(char c) noexcept
    {
        if ('0' <= c && c <= '9') {
            return true;
        }
        if ('a' <= c && c <= 'z') {
            return true;
        }
        if ('A' <= c && c <= 'Z') {
            return true;
        }
        if (c == '_') {
            return true;
        }
        return false;
    }

private:
    std::vector<std::string> m_data;
    bool m_frozen;

public:
    ComboStorage()
        : m_data{}
        , m_frozen{false}
    {}

    bool is_empty() const noexcept
    {
        return m_data.empty();
    }

    void add(std::string_view sv)
    {
        m_data.emplace_back(sv);
    }

    void freeze()
    {
        m_data.shrink_to_fit();
        std::sort(m_data.begin(), m_data.end());
        m_frozen = true;
        for (size_t i = 1; i < m_data.size(); ++i) {
            if (m_data[i] == m_data[i - 1]) {
                throw DuplicatedComboError(m_data[i]);
            }
        }
    }

    LookupResult lookup(std::string_view sv) const noexcept
    {
        MY_ASSERT(m_frozen);

        auto it = std::lower_bound(m_data.begin(), m_data.end(), sv);
        if (it == m_data.end()) {
            return LookupResult::NONE;
        }
        if (*it == sv) {
            return LookupResult::EXACT;
        }
        if (strview_utils::starts_with(*it, sv)) {
            return LookupResult::PREFIX;
        }
        return LookupResult::NONE;
    }
};
