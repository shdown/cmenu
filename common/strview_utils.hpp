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

#include <stddef.h>
#include <limits.h>
#include <string_view>
#include <algorithm>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include "my_assert.hpp"

#define SV_FMT_ARG(sv) strview_utils::size_as_int_or_throw((sv)), (sv).data()

namespace strview_utils
{
    std::vector<std::string_view> split(std::string_view sv, char delim);

    static inline std::tuple<bool, std::string_view, std::string_view> split_once(
            std::string_view sv,
            char delim
        ) noexcept
    {
        size_t pos = sv.find(delim);
        if (pos == std::string_view::npos) {
            return {false, {}, {}};
        }
        return {
            true,
            sv.substr(0, pos),
            sv.substr(pos + 1),
        };
    }

    static inline std::pair<bool, std::string_view> follow(
            std::string_view sv,
            std::string_view prefix
        ) noexcept
    {
        if (sv.size() < prefix.size()) {
            return {false, {}};
        }
        if (prefix != sv.substr(0, prefix.size())) {
            return {false, {}};
        }
        return {true, sv.substr(prefix.size())};
    }

    static inline bool starts_with(std::string_view sv, std::string_view prefix) noexcept
    {
        auto [ok, _] = follow(sv, prefix);
        return ok;
    }

    static inline bool starts_with_ch(std::string_view sv, char ch) noexcept
    {
        return !sv.empty() && sv[0] == ch;
    }

    template<class CharT, class Traits, class Allocator>
    static inline std::basic_string_view<CharT, Traits> from_basic_string(
            const std::basic_string<CharT, Traits, Allocator> &s,
            size_t offset,
            size_t count = -1
        ) noexcept
    {
        MY_ASSERT(offset <= s.size());
        size_t result_len = std::min<size_t>(count, s.size() - offset);
        return {s.data() + offset, result_len};
    }

    static inline std::string_view from_string(const std::string &s, size_t offset, size_t count = -1) noexcept
    {
        return from_basic_string(s, offset, count);
    }

    static inline std::wstring_view from_wstring(const std::wstring &s, size_t offset, size_t count = -1) noexcept
    {
        return from_basic_string(s, offset, count);
    }

    static inline int size_as_int_or_throw(std::string_view sv)
    {
        size_t size = sv.size();
        if (size > static_cast<size_t>(INT_MAX)) {
            throw std::overflow_error("size of string_view would overflow int");
        }
        return size;
    }

} // namespace strview_utils
