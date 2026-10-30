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
#include <vector>
#include <string_view>
#include <utility>
#include <functional>
#include "common/strview_utils.hpp"
#include "common/int_utils.hpp"
#include "errors.hpp"
#include "column_type.hpp"
#include "phys.hpp"
#include "cmenu_styles.hpp"

using Callback = void(*)(Args &dst, std::string_view value);

template<class T>
static inline T parse_uint_rethrow(std::string_view s)
{
    try {
        return int_utils::parse_uint_as<T>(s);
    } catch (const int_utils::CannotParse &e) {
        throw ParseError(e.what());
    }
}

static void parse_input_fd(Args &dst, std::string_view value)
{
    dst.input_fd = parse_uint_rethrow<int>(value);
    if (dst.input_fd < 3) {
        throw ParseError("this is stdin/stdout/stderr fd");
    }
}

static void parse_output_fd(Args &dst, std::string_view value)
{
    dst.output_fd = parse_uint_rethrow<int>(value);
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

    } else if (strview_utils::starts_with_ch(value, '@')) {
        // -column=@123:HEADER
        column_type = ColumnType::FIXED_WIDTH;
        w = parse_uint_rethrow<uint32_t>(value.substr(1, delim_pos - 1));

    } else {
        // -column=123:HEADER
        column_type = ColumnType::VARIABLE_WIDTH;
        w = parse_uint_rethrow<uint32_t>(value.substr(0, delim_pos));
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

static void parse_style_alt(Args &dst, std::string_view value)
{
    dst.alt_styles.push_back(parse_style(value));
    if (dst.alt_styles.size() > CMenuStyles::MAX_ALT_STYLES) {
        throw ParseError(
            "too many alt styles (limit is " +
            std::to_string(CMenuStyles::MAX_ALT_STYLES) +
            ")"
        );
    }
}

static void parse_escdelay(Args &dst, std::string_view value)
{
    dst.esc_delay = parse_uint_rethrow<int>(value);
}

static void parse_mode(Args &dst, std::string_view value)
{
    if (value == "bare") {
        dst.bare_mode = true;
    } else if (value == "normal") {
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
    if (value == "1") {
        return true;
    } else if (value == "0") {
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
    if (value == "0") {
        dst.report_backslash = false;
        return;
    }

    dst.report_backslash = true;
    dst.backslash_requires_index = false;
    if (strview_utils::starts_with_ch(value, '%')) {
        dst.backslash_requires_index = true;
        value = value.substr(1);
    }

    if (value == "1") {
        return;
    }
    auto [ok, descr] = strview_utils::follow(value, "1:");
    if (!ok) {
        throw ParseError(
            "expected either '0' or '[%]1' or '[%]1:<DESCRIPTION>', where [%] means optional percent sign"
        );
    }

    dst.backslash_descr = std::string(descr);
}

static void parse_default_selection(Args &dst, std::string_view value)
{
    uint64_t idx = parse_uint_rethrow<uint64_t>(value);
    dst.default_selection = std::min<uint64_t>(idx, static_cast<size_t>(-1));
}

struct Option
{
    enum class Kind
    {
        NOT_INHERITABLE,
        INHERITABLE,
    };

    std::string_view prefix;
    Callback callback;
    std::string_view metavar;
    Kind kind;

    Option(
        std::string_view prefix_,
        Callback callback_,
        std::string_view metavar_,
        Kind kind_ = Kind::NOT_INHERITABLE
    ) noexcept
        : prefix{prefix_}
        , callback{callback_}
        , metavar{metavar_}
        , kind{kind_}
    {}
};

static const std::vector<Option> VALID_OPTIONS{
    {"-infd=", parse_input_fd, "<FD>"},
    {"-outfd=", parse_output_fd, "<FD>"},
    {"-column=", parse_column, "<COLUMN_SPEC>"},
    {"-style-header=", parse_style_header, "<STYLE_SPEC>", Option::Kind::INHERITABLE},
    {"-style-hi=", parse_style_hi, "<STYLE_SPEC>", Option::Kind::INHERITABLE},
    {"-style-entry=", parse_style_entry, "<STYLE_SPEC>", Option::Kind::INHERITABLE},
    {"-style-error=", parse_style_error, "<STYLE_SPEC>", Option::Kind::INHERITABLE},
    {"-style-alt=", parse_style_alt, "<STYLE_SPEC>", Option::Kind::INHERITABLE},
    {"-escdelay=", parse_escdelay, "<MILLISECONDS>", Option::Kind::INHERITABLE},
    {"-mode=", parse_mode, "{normal | bare}"},
    {"-command=", parse_cc, "<CUSTOM_COMMAND_SPEC>"},
    {"-combo=", parse_combo, "<COMBO>"},
    {"-require-enter-after-combo=", parse_require_enter_after_combo, "{0 | 1}"},
    {"-report-backslash=", parse_report_backslash, "{0 | 1 | 1:<DESCRIPTION> | %1 | %1:<DESCRIPTION>}"},
    {"-default-selection=", parse_default_selection, "<INDEX>"},
    {"-persist=", parse_persist, "{0 | 1}"},
};

void parse_args(size_t argc, char **argv, Args &dst, size_t inherited_argc)
{
    for (size_t arg_i = 1; arg_i < argc; ++arg_i) {
        std::string_view cur_arg{argv[arg_i]};
        bool cur_arg_inherited = arg_i < inherited_argc;

        bool found = false;
        for (const Option &opt : VALID_OPTIONS) {
            if (auto [ok, suffix] = strview_utils::follow(cur_arg, opt.prefix); ok) {

                if (opt.kind == Option::Kind::NOT_INHERITABLE && cur_arg_inherited) {
                    throw CommandLineArgsError("option " + std::string(opt.prefix) + " cannot be inherited");
                }

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

        dst.combo_storage.freeze();

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

        if (dst.default_selection != static_cast<size_t>(-1)) {
            throw CommandLineArgsError("-default-selection= option is only allowed in bare mode (pass '-mode=bare')");
        }
    }

    if (dst.alt_styles.empty()) {
        dst.alt_styles.push_back(Args::DEFAULT_ALT1_STYLE);
    }
}

void print_usage(FILE *stream, const std::string &my_name) noexcept
{
    fprintf(stream, "USAGE: %s OPTIONS...\n", my_name.c_str());
    fprintf(stream, "Available OPTIONS:\n");

    for (const Option &opt : VALID_OPTIONS) {
        fprintf(stream, " %.*s%.*s\n", SV_FMT_ARG(opt.prefix), SV_FMT_ARG(opt.metavar));
    }
}
