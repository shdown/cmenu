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

#include "varexp.hpp"
#include <stddef.h>
#include <string>
#include <vector>
#include <string_view>

static inline bool is_escaped(std::string_view sv, size_t i) noexcept
{
    size_t j = i;
    while (j && sv[j - 1] == '@') {
        --j;
    }
    size_t num_esc = i - j;
    return num_esc % 2 == 1;
}

static std::string_view find_var(
        const std::vector<std::pair<std::string, std::string>> &vars,
        std::string_view var_to_find)
{
    for (const auto &[spelling, value] : vars) {
        if (spelling == var_to_find) {
            return value;
        }
    }
    if (varexp::is_valid_variable_name(var_to_find)) {
        throw varexp::VarexpError("unknown variable @{!" + std::string(var_to_find) + "}");
    } else {
        throw varexp::VarexpError("bad @{!var} expansion (invalid variable name)");
    }
}

static const std::string_view START_OF_EXP = "@{!";

namespace varexp
{

bool is_valid_variable_name(std::string_view sv) noexcept
{
    auto is_valid_symbol = [](char c) -> bool {
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
    };

    if (sv.empty()) {
        return false;
    }
    for (char c : sv) {
        if (!is_valid_symbol(c)) {
            return false;
        }
    }
    return true;
}

bool may_have_expansions(std::string_view sv) noexcept
{
    // Cheaper check first.
    if (sv.find('@') == std::string_view::npos) {
        return false;
    }

    if (sv.find(START_OF_EXP) == std::string_view::npos) {
        return false;
    }
    return true;
}

std::string expand(
        std::string_view sv,
        const std::vector<std::pair<std::string, std::string>> &vars)
{
    std::string res;

    size_t i = 0;
    for (;;) {
        size_t j = sv.find(START_OF_EXP, i);
        if (j == std::string_view::npos) {
            res.append(sv.substr(i));
            break;
        }
        if (is_escaped(sv, j)) {
            size_t new_i = j + START_OF_EXP.size();
            res.append(sv.substr(i, new_i - i));
            i = new_i;
            continue;
        }

        res.append(sv.substr(i, j - i));

        size_t var_begin = j + START_OF_EXP.size();
        size_t var_end = sv.find('}', j);
        if (var_end == std::string_view::npos) {
            throw varexp::VarexpError("unterminated @{!... variable expansion");
        }

        std::string_view repl = find_var(
            vars,
            sv.substr(var_begin, var_end - var_begin));
        res.append(repl);

        i = var_end + 1;
    }

    return res;
}

} // namespace varexp
