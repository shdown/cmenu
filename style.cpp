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

#include "style.hpp"
#include <vector>
#include <string>
#include <string_view>
#include "common/int_utils.hpp"
#include "common/strview_utils.hpp"
#include "errors.hpp"

static short parse_color_num(std::string_view s)
{
    try {
        return int_utils::parse_uint_as<short>(s);
    } catch (const int_utils::CannotParse &e) {
        throw ParseError(std::string("cannot parse f= or b= segment: ") + e.what());
    }
}

static void parse_segment(RawStyle &rs, std::string_view s)
{
    if (s == "normal") {
        return;
    }
    if (s == "bold") {
        rs.a |= A_BOLD;
        return;
    }
    if (s == "blink") {
        rs.a |= A_BLINK;
        return;
    }
    if (s == "dim") {
        rs.a |= A_DIM;
        return;
    }
    if (s == "reverse") {
        rs.a |= A_REVERSE;
        return;
    }
    if (s == "standout") {
        rs.a |= A_STANDOUT;
        return;
    }
    if (s == "underline") {
        rs.a |= A_UNDERLINE;
        return;
    }

    if (auto [ok, suffix] = strview_utils::follow(s, "f="); ok) {
        rs.fc = parse_color_num(suffix);
        return;
    }

    if (auto [ok, suffix] = strview_utils::follow(s, "b="); ok) {
        rs.bc = parse_color_num(suffix);
        return;
    }

    throw ParseError("unknown style segment: " + std::string(s));
}

RawStyle parse_style(std::string_view s)
{
    RawStyle rs{0, -1, -1};

    auto segments = strview_utils::split(s, ',');
    for (auto segment : segments) {
        parse_segment(rs, segment);
    }

    return rs;
}

InternedStyle intern_style(RawStyle style, short cpn)
{
    int ncolors = COLORS;
    if (style.fc >= ncolors || style.bc >= ncolors) {
        throw NcursesError("implementation does not have enough colors");
    }

    if (init_pair(cpn, style.fc, style.bc) != OK) {
        throw NcursesError("init_pair() failed");
    }

    return InternedStyle{style.a, cpn};
}
