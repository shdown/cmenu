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

#include "parse_args.hpp"
#include <stddef.h>
#include <stdint.h>
#include <ostream>
#include <vector>
#include <string_view>
#include <utility>
#include <functional>
#include "errors.hpp"
#include "strview_utils.hpp"
#include "int_utils.hpp"
#include "column_type.hpp"
#include "phys.hpp"

using namespace std::string_view_literals;

using Callback = std::function<void(Args &dst, std::string_view value)>;

static void parse_input_fd(Args &dst, std::string_view value)
{
    dst.input_fd = parse_uint_as<int>(value);
    if (dst.input_fd < 3) {
        throw ParseError("this is stdin/stdout/stderr fd");
    }
}

static void parse_output_fd(Args &dst, std::string_view value)
{
    dst.output_fd = parse_uint_as<int>(value);
    if (dst.output_fd < 3) {
        throw ParseError("this is stdin/stdout/stderr fd");
    }
}

static void parse_column(Args &dst, std::string_view value)
{
    size_t delim_pos = value.find(':');
    if (delim_pos == std::string_view::npos) {
        throw ParseError("column specifier contains no ':'");
    }

    ColumnType column_type;
    uint32_t w;

    if (delim_pos == 0) {
        // -column=:HEADER
        column_type = ColumnType::VARIABLE_WIDTH;
        w = 1;

    } else if (!value.empty() && value[0] == '@') {
        // -column=@123:HEADER
        column_type = ColumnType::FIXED_WIDTH;
        w = parse_uint_as<uint32_t>(value.substr(1, delim_pos - 1));

    } else {
        // -column=123:HEADER
        column_type = ColumnType::VARIABLE_WIDTH;
        w = parse_uint_as<uint32_t>(value.substr(0, delim_pos));
    }

    if (w == 0) {
        throw ParseError("column width is zero");
    }

    dst.CW_spec.push_back({column_type, PHYS{w}});
    dst.headers.emplace_back(value.substr(delim_pos + 1));
}

static void parse_style_header(Args &dst, std::string_view value)
{
    dst.style_header = parse_style(value);
}

static void parse_style_hi(Args &dst, std::string_view value)
{
    dst.style_hi = parse_style(value);
}

static void parse_style_entry(Args &dst, std::string_view value)
{
    dst.style_entry = parse_style(value);
}

static void parse_style_error(Args &dst, std::string_view value)
{
    dst.style_error = parse_style(value);
}

static void parse_escdelay(Args &dst, std::string_view value)
{
    dst.esc_delay = parse_uint_as<int>(value);
}

static void parse_mode(Args &dst, std::string_view value)
{
    if (value == "bare"sv) {
        dst.bare_mode = true;
    } else if (value == "normal"sv) {
        dst.bare_mode = false;
    } else {
        throw ParseError("unknown mode (expected either 'bare' or 'normal')");
    }
}

static void parse_cc(Args &dst, std::string_view value)
{
    std::string_view spec;
    std::string_view descr;

    if (size_t i = value.find(':'); i != std::string_view::npos) {
        spec = value.substr(0, i);
        descr = value.substr(i + 1);
    } else {
        spec = value;
        descr = std::string_view{};
    }

    char c;
    bool with_index;

    if (spec.size() == 2 && spec[0] == '%') {
        c = spec[1];
        with_index = true;

    } else if (spec.size() == 1) {
        c = spec[0];
        with_index = false;

    } else {
        throw ParseError("invalid custom command specifier");
    }

    if (!CCStorage::is_valid_symbol(c)) {
        throw ParseError("specified symbol is not a valid custom command symbol");
    }

    if (!dst.cc_storage.add(c, with_index)) {
        throw ParseError("this symbol has already been used for another custom command");
    }

    dst.cc_descr.emplace_back(c, descr);
}

static void parse_combo(Args &dst, std::string_view value)
{
    if (value.empty()) {
        throw ParseError("empty combo");
    }
    for (char c : value) {
        if (!ComboStorage::is_valid_symbol(c)) {
            throw ParseError("specified combo contains a prohibited symbol");
        }
    }
    dst.combo_storage.add(std::string(value));
}

static inline bool parse_flag(std::string_view value)
{
    if (value == "1"sv) {
        return true;
    } else if (value == "0"sv) {
        return false;
    } else {
        throw ParseError("expected either '0' or '1'");
    }
}

static void parse_require_enter_after_combo(Args &dst, std::string_view value)
{
    dst.require_enter_after_combo = parse_flag(value);
}

static void parse_persist(Args &dst, std::string_view value)
{
    dst.persist = parse_flag(value);
}

static void parse_report_backslash(Args &dst, std::string_view value)
{
    if (value == "0"sv) {
        dst.report_backslash = false;
    } else if (value == "1"sv) {
        dst.report_backslash = true;
    } else {
        auto [ok, descr] = strview_utils::follow(value, "1:"sv);
        if (!ok) {
            throw ParseError("expected either '0', '1' or '1:<DESCRIPTION>'");
        }
        dst.report_backslash = true;
        dst.backslash_descr = std::string(descr);
    }
}

struct Option
{
    std::string_view prefix;
    Callback callback;
    std::string_view metavar;
};

static const std::vector<Option> VALID_OPTIONS{
    {"-infd="sv, parse_input_fd, "<FD>"sv},
    {"-outfd="sv, parse_output_fd, "<FD>"sv},
    {"-column="sv, parse_column, "<COLUMN_SPEC>"sv},
    {"-style-header="sv, parse_style_header, "<STYLE_SPEC>"sv},
    {"-style-hi="sv, parse_style_hi, "<STYLE_SPEC>"sv},
    {"-style-entry="sv, parse_style_entry, "<STYLE_SPEC>"sv},
    {"-style-error="sv, parse_style_error, "<STYLE_SPEC>"sv},
    {"-escdelay="sv, parse_escdelay, "<MILLISECONDS>"sv},
    {"-mode="sv, parse_mode, "{normal | bare}"sv},
    {"-command="sv, parse_cc, "<CUSTOM_COMMAND_SPEC>"sv},
    {"-combo="sv, parse_combo, "<COMBO>"sv},
    {"-require-enter-after-combo="sv, parse_require_enter_after_combo, "{0 | 1}"sv},
    {"-report-backslash="sv, parse_report_backslash, "{0 | 1 | 1:<DESCRIPTION>}"sv},
    {"-persist="sv, parse_persist, "{0 | 1}"sv},
};

void parse_args(int argc, char **argv, Args &dst)
{
    for (int arg_i = 1; arg_i < argc; ++arg_i) {
        std::string_view cur_arg{argv[arg_i]};

        bool found = false;
        for (const Option &opt : VALID_OPTIONS) {
            if (auto [ok, suffix] = strview_utils::follow(cur_arg, opt.prefix); ok) {
                try {
                    opt.callback(dst, suffix);
                } catch (const ParseError &e) {
                    throw CommandLineArgsError(
                        "cannot parse option '" + std::string(cur_arg) + "': " + e.what()
                    );
                }
                found = true;
                break;
            }
        }

        if (!found) {
            throw CommandLineArgsError(
                "unknown option '" + std::string(cur_arg) + "'"
            );
        }
    }

    if (dst.CW_spec.empty()) {
        throw CommandLineArgsError("no columns specifications passed");
    }

    if (dst.bare_mode) {
        if (dst.input_fd >= 0) {
            throw CommandLineArgsError("-infd is only allowed in normal mode");
        }
        if (dst.output_fd >= 0) {
            throw CommandLineArgsError("-outfd is only allowed in normal mode");
        }

        if (!dst.cc_storage.is_empty()) {
            throw CommandLineArgsError("-command= options are not allowed in bare mode");
        }

        if (dst.persist) {
            throw CommandLineArgsError("-persist=1 is not compatible with -mode=bare");
        }
        if (dst.report_backslash) {
            throw CommandLineArgsError("-report-backslash=1 is not compatible with -mode=bare");
        }

        try {
            dst.combo_storage.freeze();
        } catch (const DuplicatedComboError &e) {
            throw CommandLineArgsError(e.what());
        }

    } else {
        if (dst.input_fd < 0) {
            throw CommandLineArgsError("-infd was not passed");
        }
        if (dst.output_fd < 0) {
            throw CommandLineArgsError("-outfd was not passed");
        }

        if (!dst.combo_storage.is_empty()) {
            throw CommandLineArgsError("-combo= options are only allowed in bare mode (pass '-mode=bare')");
        }

        if (dst.require_enter_after_combo) {
            throw CommandLineArgsError("-require-enter-after-combo=1 is only allowed in bare mode (pass '-mode=bare')");
        }
    }
}

void print_usage(std::ostream &os, const std::string &my_name)
{
    os << "USAGE: " << my_name << " OPTIONS..." << std::endl;
    os << "Available OPTIONS:" << std::endl;

    for (const Option &opt : VALID_OPTIONS) {
        os << " " << opt.prefix << opt.metavar << std::endl;
    }
}
