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

#include "glib_stuff.hpp"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <stdlib.h>
#include <stdio.h>
#include "../parse_args.hpp"
#include "../app.hpp"
#include "../fd_utils.hpp"
#include "../errors.hpp"
#include "../common/fd.hpp"

static std::string global_my_name;

static void print_usage()
{
    fprintf(
        stderr,
        "USAGE: %s "
        "[-X:stderr-file=<STDERR_FILE>] "
        "[pass-through cmenu options...] "
        "[--] "
        "[<FILE>]\n",
        global_my_name.c_str()
    );
}

static std::vector<std::string_view> split_categories(std::string_view sv)
{
    std::vector<std::string_view> res;

    size_t prev = 0;
    bool esc = false;
    for (size_t i = 0; i < sv.size(); ++i) {
        if (esc) {
            esc = false;
            continue;
        }
        char c = sv[i];
        if (c == ';') {
            res.push_back(sv.substr(prev, i - prev));
            prev = i + 1;
        } else if (c == '\\') {
            esc = true;
        }
    }

    if (prev < sv.size()) {
        res.push_back(sv.substr(prev));
    }

    return res;
}

class FakeArgvFactory
{
private:
    std::vector<std::string> m_args;
    const FakeArgvFactory *m_base;

public:
    FakeArgvFactory() noexcept
        : m_args{}
        , m_base{nullptr}
    {}

    explicit FakeArgvFactory(const FakeArgvFactory *base) noexcept
        : m_args{}
        , m_base{base}
    {
        if (m_base) {
            MY_ASSERT(!m_base->m_base);
        }
    }

    void add(std::string_view arg)
    {
        m_args.emplace_back(arg);
    }

    size_t argc() const noexcept
    {
        return m_args.size() + (m_base ? m_base->m_args.size() : 0);
    }

    std::vector<char *> finalize() const
    {
        std::vector<char *> res;
        res.reserve(argc());
        if (m_base) {
            for (const std::string &arg : m_base->m_args) {
                res.push_back(const_cast<char *>(arg.data()));
            }
        }
        for (const std::string &arg : m_args) {
            res.push_back(const_cast<char *>(arg.data()));
        }
        return res;
    }
};

struct Category
{
    std::string spelling;
    std::string human_name;
};

static const std::vector<Category> KNOWN_CATEGORIES = {
    {"AudioVideo",      "Audio and video"},
    {"Development",     "Development"},
    {"Education",       "Education"},
    {"HealthFitness",   "Health and fitness"},
    {"Game",            "Games"},
    {"Graphics",        "Graphics"},
    {"Network",         "Network"},
    {"Office",          "Office"},
    {"Science",         "Science"},
    {"Settings",        "Settings"},
    {"System",          "System"},
    {"Utility",         "Utilities"},
};

static const std::string CATEGORY_OTHER_HUMAN_NAME = "Uncategorized";

static std::pair<bool, size_t> find_known_category_by_spelling(std::string_view spelling)
{
    for (size_t i = 0; i < KNOWN_CATEGORIES.size(); ++i) {
        if (KNOWN_CATEGORIES[i].spelling == spelling) {
            return {true, i};
        }
    }
    return {false, 0};
}

class QuitRequest : public std::runtime_error
{
public:
    QuitRequest()
        : std::runtime_error("user wants to quit without action")
    {}
};

using DAI = glib_stuff::DesktopAppInfo;

static void make_args(
    Args &dst,
    const FakeArgvFactory &base_faf,
    const std::vector<std::string> &extra_args)
{
    FakeArgvFactory my_faf{&base_faf};
    my_faf.add("-mode=bare");
    my_faf.add("-combo=q");
    for (const std::string &arg : extra_args) {
        my_faf.add(arg);
    }
    std::vector<char *> argv = my_faf.finalize();

    try {
        ::parse_args(argv.size(), argv.data(), dst, base_faf.argc());
    } catch (const CommandLineArgsError &e) {
        fprintf(stderr, "Bad pass-through cmenu option passed: %s\n", e.what());
        print_usage();
        exit(2);
    }
}

static size_t select_category(
    const FakeArgvFactory &base_faf,
    const std::vector<std::vector<DAI>> &Cs,
    size_t &selected_idx)
{
    Args my_args;
    make_args(my_args, base_faf, {
        "-column=@4:Num",
        "-column=:Category",
        "-default-selection=" + std::to_string(selected_idx),
    });

    std::vector<size_t> to_orig_idx;
    std::vector<std::vector<std::string>> entries;
    for (size_t i = 0; i < Cs.size(); ++i) {
        const auto &C = Cs[i];
        if (C.empty()) {
            continue;
        }
        to_orig_idx.push_back(i);

        const std::string &num = C.size() < 1000 ? std::to_string(C.size()) : "1K+";

        const std::string &name = (i == KNOWN_CATEGORIES.size())
            ? CATEGORY_OTHER_HUMAN_NAME
            : KNOWN_CATEGORIES[i].human_name;

        entries.push_back({ num, name });
    }

    std::optional<std::variant<size_t, std::string>> res =
        app::run_bare_mode_sync(std::move(my_args), entries);

    if (!res) {
        throw QuitRequest{};
    }
    if (!std::holds_alternative<size_t>(*res)) {
        throw QuitRequest{};
    }

    size_t j = std::get<size_t>(*res);
    selected_idx = j;
    return to_orig_idx.at(j);
}

static std::pair<bool, size_t> select_app_within_category(
    const FakeArgvFactory &base_faf,
    const std::vector<DAI> &C)
{
    Args my_args;
    make_args(my_args, base_faf, {
        "-column=:Application",
    });

    size_t total = C.size() + 1;

    std::vector<std::vector<std::string>> entries;
    std::vector<uint8_t> styles;

    entries.reserve(total);
    styles.reserve(total);

    entries.push_back({ ".." });
    styles.push_back(1);

    for (const auto &dai : C) {
        entries.push_back({ std::string(dai.name()) });
        styles.push_back(0);
    }

    std::optional<std::variant<size_t, std::string>> res =
        app::run_bare_mode_sync(std::move(my_args), entries, styles);

    if (!res) {
        throw QuitRequest{};
    }
    if (!std::holds_alternative<size_t>(*res)) {
        throw QuitRequest{};
    }

    size_t j = std::get<size_t>(*res);
    if (j == 0) {
        return {false, 0};
    }
    --j;
    return {true, j};
}

static void sort_apps(std::vector<DAI> &C)
{
    std::sort(C.begin(), C.end(), [](const DAI &a, const DAI &b) -> bool {
        return a.name() < b.name();
    });
}

static DAI mode_app_menu(const FakeArgvFactory &base_faf)
{
    std::vector<DAI> all_apps = glib_stuff::get_all_desktop_apps();

    std::vector<std::vector<DAI>> by_category(KNOWN_CATEGORIES.size() + 1);

    for (const auto &app : all_apps) {
        std::vector<std::string_view> spellings = split_categories(app.categories());

        bool has_known_category = false;
        for (std::string_view spelling : spellings) {
            if (auto [ok, i] = find_known_category_by_spelling(spelling); ok) {
                by_category[i].push_back(app);
                has_known_category = true;
            }
        }
        if (!has_known_category) {
            by_category.back().push_back(app);
        }
    }

    for (auto &C : by_category) {
        sort_apps(C);
    }

    try {
        size_t selected_category_idx = 0;
        for (;;) {
            size_t category_idx = select_category(base_faf, by_category, selected_category_idx);
            const auto &C = by_category.at(category_idx);

            auto [is_selected, app_idx] = select_app_within_category(base_faf, C);
            if (!is_selected) {
                continue;
            }
            return C.at(app_idx);
        }
    } catch (const QuitRequest &) {
        return {};
    }
}

static DAI mode_open_file(
    const FakeArgvFactory &base_faf,
    const glib_stuff::FileInfo &file_info)
{
    auto [apps_recommended, apps_fallback] =
        glib_stuff::get_desktop_apps_for_content_type(
            std::string(file_info.content_type()));

    sort_apps(apps_recommended);
    sort_apps(apps_fallback);

    Args my_args;
    make_args(my_args, base_faf, {
        "-column=:Open with",
    });

    std::vector<std::vector<std::string>> entries;
    entries.reserve(apps_recommended.size() + apps_fallback.size());

    for (const auto &dai : apps_recommended) {
        entries.push_back({ std::string(dai.name()) });
    }
    for (const auto &dai : apps_fallback) {
        entries.push_back({ std::string(dai.name()) });
    }

    std::optional<std::variant<size_t, std::string>> res =
        app::run_bare_mode_sync(std::move(my_args), entries);

    if (!res) {
        return {};
    }
    if (!std::holds_alternative<size_t>(*res)) {
        return {};
    }

    size_t j = std::get<size_t>(*res);
    if (j < apps_recommended.size()) {
        return apps_recommended.at(j);
    } else {
        j -= apps_recommended.size();
        return apps_fallback.at(j);
    }
}

static void cli_pause()
{
    using FcloseType = int(*)(FILE *);

    std::unique_ptr<FILE, FcloseType> f{
        fopen("/dev/tty", "r"),
        &fclose
    };
    if (!f) {
        return;
    }

    fprintf(stderr, "Press ENTER to continue...\n");

    int c;
    do {
        c = fgetc(f.get());
    } while (c != '\n' && c != '\r' && c != EOF);
}

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

namespace cmdesktopmenu_parse_arg
{

class WrongUsageError : public std::runtime_error
{
public:
    WrongUsageError(const std::string &msg)
        : std::runtime_error(msg)
    {}
};

static void parse_args(
    int argc, char **argv,
    FakeArgvFactory &out_faf,
    std::string &out_file_arg,
    std::string &out_stderr_file)
{
    out_file_arg = "";
    out_stderr_file = "";

    out_faf.add("fake_argv0");

    bool dash_dash = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view sv = argv[i];
        if (!dash_dash && strview_utils::starts_with_ch(sv, '-')) {
            if (strview_utils::starts_with(sv, "-X:")) {
                if (auto [ok, suffix] = strview_utils::follow(sv, "-X:stderr-file="); ok) {
                    if (suffix.empty()) {
                        throw WrongUsageError("argument to '-X:stderr-file=' option is empty");
                    }
                    out_stderr_file = suffix;
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
                throw WrongUsageError("got empty <FILE> argument");
            }
            if (!out_file_arg.empty()) {
                throw WrongUsageError("got multiple <FILE> arguments");
            }
            out_file_arg = sv;
        }
    }
}

} // namespace cmdesktopmenu_parse_arg

int main(int argc, char **argv)
{
    global_my_name = (argc == 0) ? "cmdesktopmenu" : argv[0];

    FakeArgvFactory base_faf;
    std::string file_arg;
    std::string stderr_file;

    try {
        cmdesktopmenu_parse_arg::parse_args(argc, argv, base_faf, file_arg, stderr_file);
    } catch (const cmdesktopmenu_parse_arg::WrongUsageError &e) {
        fprintf(stderr, "Wrong USAGE: %s\n", e.what());
        print_usage();
        return 2;
    }

    DAI target;
    glib_stuff::File target_file;

    app::sigpipe_ignore();

    if (file_arg.empty()) {
        try {
            target = mode_app_menu(base_faf);
        } catch (const std::runtime_error &e) {
            fprintf(stderr, "Caught error: %s\n", e.what());
            return 1;
        }
    } else {
        target_file = glib_stuff::new_file_from_path(file_arg);
        glib_stuff::FileInfo file_info;
        try {
            file_info = glib_stuff::query_file_info(target_file);
        } catch (const std::runtime_error &e) {
            fprintf(stderr, "Cannot query file info for '%s': %s\n", file_arg.c_str(), e.what());
            cli_pause();
            return 1;
        }

        try {
            target = mode_open_file(base_faf, file_info);
        } catch (const std::runtime_error &e) {
            fprintf(stderr, "Caught error: %s\n", e.what());
            cli_pause();
            return 1;
        }
    }

    if (!target.valid()) {
        return 0;
    }

    app::sigpipe_reset_handler_to_default();

    // 1. Some GUI applications don't start if their stdin/stdout/stderr are redirected to a
    //    dead PTY (if we are launched in a separate terminal window, the terminal closes once
    //    our process terminates). So we need to redirect all them to /dev/null (possibly except
    //    for stderr, which we try to redirect to 'stderr_file' if non-empty).
    //
    // 2. At least under urxvtc+urxvtd, the process running in the terminal gets killed with
    //    SIGHUP by urxvtd once all file descriptors connecting the process to the PTY are
    //    closed. So we need to open a "lifebuoy" fd to /dev/tty before redirecting
    //    stdin/stdout/stderr. It needs to be opened with O_CLOEXEC flag so that our child does
    //    not inherit it.

    FD do_not_kill_me_yet_fd;
    try {
        do_not_kill_me_yet_fd = fd_utils::open_tty_to_prevent_sighup();
    } catch (const std::runtime_error &e) {
        fprintf(stderr, "Cannot open do-not-kill-me-yet file descriptor: %s\n", e.what());
        return 1;
    }

    try {
        do_redirect_std_fds(stderr_file);
    } catch (const std::runtime_error &e) {
        fprintf(stderr, "Cannot redirect stdin/stdout/stderr: %s\n", e.what());
        return 1;
    }

    try {
        glib_stuff::launch(target, target_file);
    } catch (const std::runtime_error &e) {
        fprintf(stderr, "Launch failed: %s", e.what());
        return 1;
    }

    return 0;
}
