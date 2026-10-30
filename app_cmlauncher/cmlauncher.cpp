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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <system_error>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <stdexcept>
#include <vector>
#include <utility>
#include "../common/strview_utils.hpp"
#include "../app.hpp"
#include "../parse_args.hpp"
#include "../errors.hpp"
#include "../fd_utils.hpp"
#include "varexp.hpp"
#include "cfile.hpp"

static const std::string_view WHITESPACE = " \t\r";

struct Entry
{
    std::string combo;
    std::string name;
    std::string cmd;
    bool dim;

    Entry()
        : combo{}
        , name{}
        , cmd{}
        , dim{false}
    {}
};

class YourLineIsBad : public std::runtime_error
{
public:
    explicit YourLineIsBad(const std::string &msg)
        : std::runtime_error(msg)
    {}
};

static inline std::string_view rtrim_whitespace(std::string_view sv) noexcept
{
    size_t end = sv.find_last_not_of(WHITESPACE);
    if (end == std::string_view::npos) {
        return {};
    }
    return sv.substr(0, end + 1);
}

static void set_field_combo(Entry &e, std::string_view v)
{
    v = rtrim_whitespace(v);

    if (v.empty()) {
        e.combo = "";
        return;
    }
    if (v.size() > 5) {
        throw YourLineIsBad("combo is too long (max size is 5)");
    }

    auto is_valid_ch = [](char c) -> bool {
        if ('a' <= c && c <= 'z') {
            return true;
        }
        if ('A' <= c && c <= 'Z') {
            return true;
        }
        if ('0' <= c && c <= '9') {
            return true;
        }
        if (c == '_') {
            return true;
        }
        return false;
    };

    for (char c : v) {
        if (!is_valid_ch(c)) {
            throw YourLineIsBad("combo contains prohibited symbol");
        }
    }
    e.combo = std::string(v);
}

static void set_field_name(Entry &e, std::string_view v)
{
    e.name = std::string(v);
}

static void set_field_cmd(Entry &e, std::string_view v)
{
    e.cmd = std::string(v);
}

static void set_field_dim(Entry &e, std::string_view v)
{
    v = rtrim_whitespace(v);
    if (v == "true") {
        e.dim = true;
    } else if (v == "false") {
        e.dim = false;
    } else {
        throw YourLineIsBad("cannot parse boolean value (must be either 'true' or 'false')");
    }
}

struct Field
{
    using CallbackType = void(*)(Entry &, std::string_view);

    std::string_view spelling;
    CallbackType callback;
    bool required;
    bool subject_to_varexp;
    bool already_set;

    Field(
            std::string_view spelling_,
            CallbackType callback_,
            bool required_,
            bool subject_to_varexp_)
        : spelling{spelling_}
        , callback{callback_}
        , required{required_}
        , subject_to_varexp{subject_to_varexp_}
        , already_set{false}
    {
    }
};

class IniParserError : public std::runtime_error
{
public:
    static inline constexpr uint64_t END_OF_FILE = -1;

private:
    static std::string m_format_msg(uint64_t line_num, const std::string &orig_msg)
    {
        std::string where;
        if (line_num == END_OF_FILE) {
            where = "end of file";
        } else {
            where = "line " + std::to_string(line_num);
        }
        return "error at " + where + ": " + orig_msg;
    }

public:
    IniParserError(uint64_t line_num, const std::string &orig_msg)
        : std::runtime_error(m_format_msg(line_num, orig_msg))
    {}
};

static std::vector<Entry> read_entries_from_file(const std::string &path)
{
    cfile::CFile f(path.c_str(), "r");
    cfile::CFileLine line;

    std::vector<Entry> entries;
    std::vector<std::pair<std::string, std::string>> vars;

    bool cur_entry_is_valid = false;
    Entry cur_entry;

    std::vector<Field> fields{
        {"combo", set_field_combo, true,  false},
        {"name",  set_field_name,  true,  false},
        {"cmd",   set_field_cmd,   true,  true},
        {"dim",   set_field_dim,   false, false},
    };

    auto sv_all_whitespace = [](std::string_view sv) -> bool {
        return sv.find_first_not_of(WHITESPACE) == std::string_view::npos;
    };

    auto apply_kv_to_entry = [&](std::string_view k, std::string_view v) -> void {
        for (Field &field : fields) {
            if (field.spelling == k) {
                if (field.already_set) {
                    throw YourLineIsBad("this field has already been set for the current entry");
                }

                if (field.subject_to_varexp && varexp::may_have_expansions(v)) {
                    std::string v_expanded;
                    try {
                        v_expanded = varexp::expand(v, vars);
                    } catch (const varexp::VarexpError &e) {
                        throw YourLineIsBad(e.what());
                    }
                    field.callback(cur_entry, v_expanded);
                } else {
                    field.callback(cur_entry, v);
                }

                field.already_set = true;
                return;
            }
        }

        if (k == "variable") {
            throw YourLineIsBad("'variable' directives are only allowed in global section");
        } else {
            throw YourLineIsBad("unknown field");
        }
    };

    auto apply_kv_outside_of_entry = [&](std::string_view k, std::string_view v) -> void {
        if (k == "variable") {
            auto [ok, var_name, var_value] = strview_utils::split_once(v, '=');
            if (!ok) {
                throw YourLineIsBad("'variable' directive must be of form 'variable=NAME=VALUE'");
            }
            if (!varexp::is_valid_variable_name(var_name)) {
                throw YourLineIsBad("invalid variable name");
            }
            vars.emplace_back(var_name, var_value);
        } else {
            throw YourLineIsBad("unknown directive in global section");
        }
    };

    auto flush_current_entry = [&](uint64_t line_num) -> void {
        if (!cur_entry_is_valid) {
            return;
        }
        for (const Field &field : fields) {
            if (!field.already_set && field.required) {
                throw IniParserError(
                    line_num,
                    "field '" + std::string(field.spelling) + "' was not set for previous entry");
            }
        }
        entries.push_back(cur_entry);
    };

    auto open_new_entry = [&]() -> void {
        cur_entry_is_valid = true;
        cur_entry = Entry();
        for (Field &field : fields) {
            field.already_set = false;
        }
    };

    for (uint64_t line_num = 1; line.read_from(f); ++line_num) {
        std::string_view sv = line.view();
        if (sv_all_whitespace(sv)) {
            continue;
        }
        if (strview_utils::starts_with_ch(sv, '#')) {
            continue;
        }
        if (strview_utils::starts_with_ch(sv, '[')) {
            if (sv != "[entry]") {
                throw IniParserError(line_num, "section must be '[entry]'");
            }
            flush_current_entry(line_num);
            open_new_entry();
            continue;
        }

        auto [ok, k, v] = strview_utils::split_once(sv, '=');
        if (!ok) {
            throw IniParserError(line_num, "cannot parse this line");
        }

        try {
            if (cur_entry_is_valid) {
                apply_kv_to_entry(k, v);
            } else {
                apply_kv_outside_of_entry(k, v);
            }
        } catch (const YourLineIsBad &e) {
            throw IniParserError(line_num, e.what());
        }
    }

    flush_current_entry(IniParserError::END_OF_FILE);

    entries.shrink_to_fit();
    return entries;
}

class FakeArgvFactory
{
private:
    std::string m_buf;
    std::vector<size_t> m_offsets;

public:
    FakeArgvFactory()
        : m_buf{}
        , m_offsets{}
    {}

    size_t argc() const noexcept
    {
        return m_offsets.size();
    }

    void add(std::string_view sv)
    {
        m_offsets.push_back(m_buf.size());
        m_buf.append(sv);
        m_buf.push_back('\0');
    }

    std::vector<char *> finalize()
    {
        std::vector<char *> res;
        res.reserve(m_offsets.size());

        for (size_t offset : m_offsets) {
            res.push_back(m_buf.data() + offset);
        }

        return res;
    }
};

static void spawn_cmd(const std::string &cmd, bool should_exec)
{
    std::vector<char *> argv = {
        const_cast<char *>("/bin/sh"),
        const_cast<char *>("-c"),
        const_cast<char *>(cmd.c_str()),
        nullptr,
    };
    char **argv_data = argv.data();

    if (should_exec) {
        if (execv("/bin/sh", argv_data) < 0) {
            throw std::system_error(errno, std::generic_category(), "execv() failed");
        }
    } else {
        fprintf(stderr, "forking...\n");
        pid_t child = fork();

        if (child < 0) {
            throw std::system_error(errno, std::generic_category(), "fork() failed");
        }
        if (child == 0) {
            setsid();
            execv("/bin/sh", argv_data);
            _exit(127);
        }
    }
}

namespace launcher_parse_args
{

class WrongUsageError : public std::runtime_error
{
public:
    WrongUsageError(const std::string &msg)
        : std::runtime_error(msg)
    {}
};

struct OurOptions
{
    std::string ini_path;
    bool should_exec;
    std::string stderr_file;

    OurOptions()
        : ini_path{} /*empty*/
        , should_exec{false}
        , stderr_file{} /*empty*/
    {}
};

static void parse_args(int argc, char **argv, FakeArgvFactory &out_faf, OurOptions &out_our_opts)
{
    out_faf.add("fake_argv0");

    bool dash_dash = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view sv = argv[i];
        if (!dash_dash && strview_utils::starts_with_ch(sv, '-')) {
            if (strview_utils::starts_with(sv, "-X:")) {
                if (sv == "-X:exec") {
                    out_our_opts.should_exec = true;
                } else if (auto [ok, suffix] = strview_utils::follow(sv, "-X:stderr-file="); ok) {
                    if (suffix.empty()) {
                        throw WrongUsageError("argument to '-X:stderr-file=' option is empty");
                    }
                    out_our_opts.stderr_file = suffix;
                } else {
                    throw WrongUsageError("unknown '-X:' option: " + std::string(sv));
                }
            } else if (sv == "--") {
                dash_dash = true;
            } else {
                out_faf.add(sv);
            }
        } else {
            if (sv.empty()) {
                throw WrongUsageError("got empty <INI_FILE> argument");
            }
            if (!out_our_opts.ini_path.empty()) {
                throw WrongUsageError("got multiple <INI_FILE> arguments");
            }
            out_our_opts.ini_path = sv;
        }
    }

    if (out_our_opts.ini_path.empty()) {
        throw WrongUsageError("no <INI_FILE> argument");
    }
}

} // namespace launcher_parse_args

static void do_redirect_std_fds(const std::string &stderr_file)
{
    if (stderr_file.empty()) {
        fd_utils::redirect_std_fds_to_safe_place("");
    } else {
        try {
            fd_utils::redirect_std_fds_to_safe_place(stderr_file);
        } catch (const std::system_error &) {
            fprintf(
                stderr,
                "WARNING: cannot open custom stderr file, redirecting to "
                "/dev/null instead: %s\n",
                stderr_file.c_str());
            fd_utils::redirect_std_fds_to_safe_place("");
        }
    }
}

int main(int argc, char **argv)
{
    auto print_usage = [&]() -> void {
        std::string my_name = (argc == 0) ? "cmlauncher" : argv[0];
        fprintf(
            stderr,
            "USAGE: %s "
            "[-X:exec] "
            "[-X:stderr-file=<STDERR_FILE>] "
            "[pass-through cmenu options...] "
            "[--] "
            "<INI_FILE>\n",
            my_name.c_str()
        );
    };

    FakeArgvFactory faf;
    launcher_parse_args::OurOptions our_opts;
    try {
        launcher_parse_args::parse_args(argc, argv, faf, our_opts);
    } catch (const launcher_parse_args::WrongUsageError &e) {
        fprintf(stderr, "Wrong USAGE: %s\n", e.what());
        print_usage();
        return 2;
    }

    std::vector<Entry> entries;
    try {
        entries = read_entries_from_file(our_opts.ini_path);
    } catch (const IniParserError &e) {
        fprintf(stderr, "Cannot parse %s: %s\n", our_opts.ini_path.c_str(), e.what());
        return 1;
    } catch (const std::system_error &e) {
        fprintf(stderr, "Error reading from %s: %s\n", our_opts.ini_path.c_str(), e.what());
        return 1;
    }

    size_t inherited_argc = faf.argc();

    faf.add("-mode=bare");
    faf.add("-column=@6:Combo");
    faf.add("-column=:Name");

    std::vector<std::vector<std::string>> columns;
    columns.reserve(entries.size());

    std::vector<uint8_t> styles;
    styles.reserve(entries.size());

    for (const Entry &e : entries) {
        columns.push_back({e.combo, e.name});
        styles.push_back(e.dim ? 1 : 0);

        if (!e.combo.empty()) {
            std::string new_arg = "-combo=" + e.combo;
            faf.add(new_arg);
        }
    }

    std::vector<char *> fake_argv = faf.finalize();

    Args my_args;
    try {
        parse_args(fake_argv.size(), fake_argv.data(), my_args, inherited_argc);
    } catch (const DuplicatedComboError &e) {
        fprintf(stderr, "Duplicated combo error: %s\n", e.what());
        fprintf(stderr, "You have a duplicated combo in your INI file.\n");
        return 1;

    } catch (const CommandLineArgsError &e) {
        fprintf(stderr, "Bad pass-through cmenu option passed: %s\n", e.what());
        print_usage();
        return 2;
    }

    app::sigpipe_ignore();

    std::optional<std::variant<size_t, std::string>> res;

    try {
        res = app::run_bare_mode_sync(std::move(my_args), columns, styles);
    } catch (const std::runtime_error &e) {
        fprintf(stderr, "Caught error: %s\n", e.what());
        return 1;
    }

    if (!res) {
        return 0;
    }

    auto find_by_combo = [&](const std::string &combo) -> size_t {
        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].combo == combo) {
                return i;
            }
        }
        throw std::logic_error("cannot find entry with given combo");
    };

    size_t res_idx = std::visit([&](auto &&arg) -> size_t {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, size_t>) {
            return arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return find_by_combo(arg);
        } else {
            static_assert(false, "non-exhaustive visitor");
        }
    }, *res);

    const std::string &cmd = entries.at(res_idx).cmd;

    fd_utils::reset_stdin_and_stdout_fds();
    app::sigpipe_reset_handler_to_default();

    FD do_not_kill_me_yet_fd;

    if (!our_opts.should_exec) {

        // 1. Some GUI applications don't start if their stdin/stdout/stderr are redirected to a
        //    dead PTY (if we are launched in a separate terminal window, the terminal closes once
        //    our process terminates). So we need to redirect all them to /dev/null (possibly except
        //    for stderr, which we try to redirect to 'our_opts.stderr_file' if non-empty).
        //
        // 2. At least under urxvtc+urxvtd, the process running in the terminal gets killed with
        //    SIGHUP by urxvtd once all file descriptors connecting the process to the PTY are
        //    closed. So we need to open a "lifebuoy" fd to /dev/tty before redirecting
        //    stdin/stdout/stderr. It needs to be opened with O_CLOEXEC flag so that our child does
        //    not inherit it.

        try {
            do_not_kill_me_yet_fd = fd_utils::open_tty_to_prevent_sighup();
        } catch (const std::runtime_error &e) {
            fprintf(stderr, "Cannot open do-not-kill-me-yet file descriptor: %s\n", e.what());
            return 1;
        }

        try {
            do_redirect_std_fds(our_opts.stderr_file);
        } catch (const std::runtime_error &e) {
            fprintf(stderr, "Cannot redirect stdin/stdout/stderr: %s\n", e.what());
            return 1;
        }
    }

    try {
        spawn_cmd(cmd, our_opts.should_exec);
    } catch (const std::runtime_error &e) {
        fprintf(stderr, "Cannot spawn command: %s\n", e.what());
        return 1;
    }

    return 0;
}
