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
#include <utility>
#include <stdio.h>
#include <curses.h>
#include "phys.hpp"
#include "cc_storage.hpp"
#include "combo_storage.hpp"
#include "column_type.hpp"
#include "style.hpp"

struct Args
{
    static inline constexpr RawStyle DEFAULT_ALT1_STYLE = {0, COLOR_RED, -1};

    int input_fd;
    int output_fd;

    std::vector<std::pair<ColumnType, PHYS>> CW_spec;
    std::vector<std::string> headers;

    RawStyle style_header;
    RawStyle style_hi;
    RawStyle style_entry;
    RawStyle style_error;

    std::vector<RawStyle> alt_styles;

    CCStorage cc_storage;
    ComboStorage combo_storage;

    int esc_delay;

    bool persist;
    bool bare_mode;
    bool require_enter_after_combo;
    bool report_backslash;
    bool backslash_requires_index;

    size_t default_selection;

    std::vector<std::pair<char, std::string>> cc_descr;
    std::string backslash_descr;

    Args()
        : input_fd{-1}
        , output_fd{-1}
        , CW_spec{}
        , headers{}

        , style_header{A_BOLD, COLOR_WHITE, COLOR_GREEN}
        , style_hi{    0,      COLOR_WHITE, COLOR_BLUE}
        , style_entry{ 0,      -1,          -1}
        , style_error{ 0,      COLOR_WHITE, COLOR_RED}
        , alt_styles{}

        , cc_storage{}
        , combo_storage{}
        , esc_delay{-1}

        , persist{false}
        , bare_mode{false}
        , require_enter_after_combo{false}
        , report_backslash{false}
        , backslash_requires_index{false}

        , default_selection{static_cast<size_t>(-1)}

        , cc_descr{}
        , backslash_descr{}
    {}
};

void parse_args(size_t argc, char **argv, Args &dst, size_t inherited_argc = 0);

void print_usage(FILE *stream, const std::string &my_name) noexcept;
