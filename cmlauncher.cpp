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
#include <iostream>
#include "app.hpp"
#include "parse_args.hpp"
#include "errors.hpp"

namespace cfile
{

class CFile
{
private:
    FILE *m_f;

public:
    CFile()
        : m_f{nullptr}
    {}

    CFile(const char *path, const char *mode)
    {
        m_f = fopen(path, mode);
        if (!m_f) {
            throw std::system_error(errno, std::generic_category(), "cannot open file");
        }
    }

    CFile(CFile &&that)
        : m_f{that.m_f}
    {
        that.m_f = nullptr;
    }

    CFile(const CFile &) = delete;

    CFile &operator =(CFile &&that)
    {
        std::swap(m_f, that.m_f);
        return *this;
    }

    CFile &operator =(const CFile &) = delete;

    bool valid() const
    {
        return m_f != nullptr;
    }

    FILE *raw() const
    {
        return m_f;
    }

    ~CFile()
    {
        if (m_f) {
            fclose(m_f);
        }
    }
};

class CFileLine
{
private:
    char *m_buf;
    size_t m_num_alloc;
    size_t m_cur_len;

public:
    CFileLine()
        : m_buf{nullptr}
        , m_num_alloc{0}
        , m_cur_len{0}
    {}

    CFileLine(CFileLine &&that)
        : m_buf{that.m_buf}
        , m_num_alloc{that.m_num_alloc}
        , m_cur_len{that.m_cur_len}
    {
        that.m_buf = nullptr;
        that.m_num_alloc = 0;
        that.m_cur_len = 0;
    }

    CFileLine(const CFileLine &) = delete;

    CFileLine &operator =(CFileLine &&that)
    {
        std::swap(m_buf, that.m_buf);
        std::swap(m_num_alloc, that.m_num_alloc);
        std::swap(m_cur_len, that.m_cur_len);
        return *this;
    }

    CFileLine &operator =(const CFileLine &&) = delete;

    bool read_from(const CFile &f)
    {
        if (!f.valid()) {
            throw std::logic_error("trying to call CFileLine::read_from() on null CFile");
        }

        ssize_t r = getline(&m_buf, &m_num_alloc, f.raw());
        if (r <= 0) {
            if (feof(f.raw())) {
                return false;
            } else {
                throw std::system_error(errno, std::generic_category(), "getline() failed");
            }
        }
        if (r && m_buf[r - 1] == '\n') {
            --r;
        }
        m_cur_len = r;
        return true;
    }

    std::string_view view() const
    {
        return {m_buf, m_cur_len};
    }

    ~CFileLine()
    {
        free(m_buf);
    }
};

} // namespace cfile

struct Entry
{
    std::string combo;
    std::string name;
    std::string cmd;
};

class IniParserError : public std::runtime_error
{
public:
    explicit IniParserError(const std::string &msg)
        : std::runtime_error(msg)
    {}
};

static void set_field_combo(Entry &e, std::string_view v)
{
    if (v.empty()) {
        e.combo = "";
        return;
    }
    if (v.size() > 5) {
        throw IniParserError("combo is too long (max size is 5)");
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
            throw IniParserError("combo contains prohibited symbol");
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

struct Field
{
    using CallbackType = void(*)(Entry &, std::string_view);

    std::string_view spelling;
    CallbackType callback;
    bool already_set;
};

class BadLine : public std::runtime_error
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
    BadLine(uint64_t line_num, const std::string &orig_msg)
        : std::runtime_error(m_format_msg(line_num, orig_msg))
    {}
};

static std::vector<Entry> read_entries_from_file(const std::string &path)
{
    using namespace std::string_view_literals;

    cfile::CFile f(path.c_str(), "r");
    cfile::CFileLine line;

    std::vector<Entry> entries;

    bool cur_entry_is_valid = false;
    Entry cur_entry{};

    std::vector<Field> fields{
        {"combo"sv, set_field_combo, false},
        {"name"sv,  set_field_name,  false},
        {"cmd"sv,   set_field_cmd,   false},
    };

    auto sv_all_whitespace = [](std::string_view sv) -> bool {
        for (char c : sv) {
            if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
                return false;
            }
        }
        return true;
    };

    auto sv_starts_with_ch = [](std::string_view sv, char c) -> bool {
        return !sv.empty() && sv[0] == c;
    };

    auto sv_split_by_eq = [](std::string_view sv, std::string_view &out_k, std::string_view &out_v) -> bool {
        size_t pos = sv.find('=');
        if (pos == std::string_view::npos) {
            return false;
        }
        out_k = sv.substr(0, pos);
        out_v = sv.substr(pos + 1);
        return true;
    };

    auto apply_kv = [&](std::string_view k, std::string_view v) -> void {
        for (Field &field : fields) {
            if (field.spelling == k) {
                if (field.already_set) {
                    throw IniParserError("this field has already been set for the current entry");
                }
                field.callback(cur_entry, v);
                field.already_set = true;
                return;
            }
        }
        throw IniParserError("unknown field");
    };

    auto flush_current_entry = [&](uint64_t line_num) -> void {
        if (!cur_entry_is_valid) {
            return;
        }
        for (const Field &field : fields) {
            if (!field.already_set) {
                throw BadLine(
                    line_num,
                    "field '" + std::string(field.spelling) + "' was not set for previous entry");
            }
        }
        entries.push_back(cur_entry);
    };

    auto open_new_entry = [&]() -> void {
        cur_entry_is_valid = true;
        cur_entry = Entry{};
        for (Field &field : fields) {
            field.already_set = false;
        }
    };

    for (uint64_t line_num = 1; line.read_from(f); ++line_num) {
        std::string_view sv = line.view();
        if (sv_all_whitespace(sv)) {
            continue;
        }
        if (sv_starts_with_ch(sv, '#')) {
            continue;
        }
        if (sv_starts_with_ch(sv, '[')) {
            if (sv != "[entry]") {
                throw BadLine(line_num, "section must be '[entry]'");
            }
            flush_current_entry(line_num);
            open_new_entry();
            continue;
        }
        if (!cur_entry_is_valid) {
            throw BadLine(line_num, "field outside of section");
        }
        std::string_view k;
        std::string_view v;
        if (!sv_split_by_eq(sv, k, v)) {
            throw BadLine(line_num, "cannot parse this line");
        }
        try {
            apply_kv(k, v);
        } catch (const IniParserError &e) {
            throw BadLine(line_num, e.what());
        }
    }

    flush_current_entry(BadLine::END_OF_FILE);

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

static void parse_args(
    int argc,
    char **argv,
    FakeArgvFactory &out_faf,
    std::string &out_ini_path,
    bool &out_should_exec)
{
    out_should_exec = false;
    out_ini_path = "";

    out_faf.add("fake_argv0");

    bool dash_dash = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view sv = argv[i];
        if (!dash_dash && (!sv.empty() && sv[0] == '-')) {
            if (sv == "-L:exec") {
                out_should_exec = true;
            } else if (sv == "--") {
                dash_dash = true;
            } else {
                out_faf.add(sv);
            }
        } else {
            if (sv.empty()) {
                throw WrongUsageError("got empty <INI_FILE> argument");
            }
            if (!out_ini_path.empty()) {
                throw WrongUsageError("got multiple <INI_FILE> arguments");
            }
            out_ini_path = sv;
        }
    }

    if (out_ini_path.empty()) {
        throw WrongUsageError("no <INI_FILE> argument");
    }
}

} // namespace launcher_parse_args

int main(int argc, char **argv)
{
    auto print_usage = [&]() -> void {
        std::string my_name = (argc == 0) ? "cmlauncher" : argv[0];
        std::cerr << "USAGE: " << my_name;
        std::cerr << " [-L:exec]";
        std::cerr << " [pass-through cmenu options...]";
        std::cerr << " [--] <INI_FILE>" << std::endl;
    };

    FakeArgvFactory faf;
    std::string ini_path;
    bool should_exec;
    try {
        launcher_parse_args::parse_args(argc, argv, faf, ini_path, should_exec);
    } catch (const launcher_parse_args::WrongUsageError &e) {
        std::cerr << "Wrong USAGE: " << e.what() << std::endl;
        print_usage();
        return 2;
    }

    std::vector<Entry> entries;
    try {
        entries = read_entries_from_file(ini_path);
    } catch (const BadLine &e) {
        std::cerr << "Cannot parse " << ini_path << ": " << e.what() << std::endl;
        return 1;
    } catch (const std::system_error &e) {
        std::cerr << "Error reading from " << ini_path << ": " << e.what() << std::endl;
        return 1;
    }

    faf.add("-mode=bare");
    faf.add("-column=@6:Combo");
    faf.add("-column=:Name");

    std::vector<std::vector<std::string>> columns;
    columns.reserve(entries.size());

    size_t num_combos = 0;
    for (const Entry &e : entries) {
        columns.push_back({e.combo, e.name});
        if (!e.combo.empty()) {
            std::string new_arg = "-combo=" + e.combo;
            faf.add(new_arg);
            ++num_combos;
        }
    }

    std::vector<char *> fake_argv = faf.finalize();
    if (fake_argv.size() > static_cast<size_t>(INT_MAX)) {
        std::cerr << "Fake argv size would overflow int." << std::endl;
        return 1;
    }

    Args my_args;
    try {
        parse_args(fake_argv.size(), fake_argv.data(), my_args);
    } catch (const CommandLineArgsError &e) {
        std::cerr << "Bad pass-through cmenu option passed: " << e.what() << std::endl;
        print_usage();
        return 2;
    }

    if (!my_args.bare_mode) {
        std::cerr << "Passing '-mode=normal' is prohibited." << std::endl;
        return 1;
    }
    if (my_args.headers.size() != 2) {
        std::cerr << "Passing '-column=...' arguments is prohibited." << std::endl;
        return 1;
    }
    if (my_args.combo_storage.num_unique() != num_combos) {
        std::cerr << "Passing '-combo=...' arguments is prohibited." << std::endl;
        return 1;
    }

    app::sigpipe_ignore();

    std::optional<std::variant<size_t, std::string>> res;

    try {
        res = app::run_bare_mode_sync(std::move(my_args), columns);
    } catch (const std::runtime_error &e) {
        std::cerr << "Caught error: " << e.what() << std::endl;
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

    app::sigpipe_reset_handler_to_default();

    try {
        spawn_cmd(cmd, should_exec);
    } catch (const std::runtime_error &e) {
        std::cerr << "Cannot spawn command: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
