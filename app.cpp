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

#include "app.hpp"

#include <type_traits>
#include <string>
#include <string_view>
#include <utility>
#include <any>
#include <memory>
#include <vector>
#include <array>
#include <exception>
#include <functional>
#include <stdexcept>
#include <system_error>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>
#include <locale.h>
#include <curses.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>

#include "widget.hpp"
#include "widgets/list.hpp"
#include "widgets/info_bar.hpp"
#include "widgets/cc_bar.hpp"
#include "widgets/combo_bar.hpp"
#include "widgets/help_screen.hpp"
#include "bio.hpp"
#include "fd_utils.hpp"
#include "int_utils.hpp"
#include "poll_utils.hpp"
#include "cc_storage.hpp"
#include "combo_storage.hpp"
#include "parse_args.hpp"
#include "input_utils.hpp"
#include "style.hpp"
#include "phys.hpp"
#include "decode.hpp"
#include "strview_utils.hpp"
#include "cmenu_styles.hpp"
#include "errors.hpp"
#include "fd.hpp"
#include "my_assert.hpp"
#include "truncated_text.hpp"
#include "writer.hpp"

struct ImmutableParams
{
    size_t num_cols;

    bool bare_mode;
    bool arg_flag_persist;
    bool arg_flag_report_backslash;
    bool arg_flag_require_enter_after_combo;

    int esc_delay;

    Writer output_writer;

    Bio input_bio;

    CCStorage cc_storage;
    ComboStorage combo_storage;
};

struct App
{
    ImmutableParams imm;

    bool term_has_default_colors;
    bool term_has_colors;

    std::unique_ptr<List> list;
    std::unique_ptr<InfoBar> info_bar;
    std::unique_ptr<CCBar> cc_bar;
    std::unique_ptr<ComboBar> combo_bar;
    std::unique_ptr<HelpScreen> help_screen;

    Widget *focused_widget;

    PHYS width;
    PHYS height;

    bool quit_flag;

    std::unique_ptr<std::variant<size_t, std::string>> output_variant;

    explicit App(ImmutableParams &&imm_)
        : imm{std::move(imm_)}

        , term_has_default_colors{false}
        , term_has_colors{false}

        , list{}
        , info_bar{}
        , cc_bar{}
        , combo_bar{}
        , help_screen{}

        , focused_widget{nullptr}

        , width{0}
        , height{0}

        , quit_flag{false}

        , output_variant{}
    {
    }

    template<class T>
    bool is_focused(const std::unique_ptr<T> &ptr) const
    {
        static_assert(std::is_base_of_v<Widget, T>, "T must inherit from Widget");

        return focused_widget == ptr.get();
    }

    template<class T>
    void set_focused(const std::unique_ptr<T> &ptr)
    {
        static_assert(std::is_base_of_v<Widget, T>, "T must inherit from Widget");

        focused_widget = ptr.get();
    }

    Widget *which_focused()
    {
        MY_ASSERT(focused_widget != nullptr);
        return focused_widget;
    }

    bool has_output_writer() const
    {
        return !imm.output_writer.is_dummy();
    }
};

static void update_dims(App &A)
{
    int y, x;
    getmaxyx(stdscr, y, x);
    A.width = PHYS(x);
    A.height = PHYS(y);

    A.list->set_dims(A.width, A.height);
    A.info_bar->set_dims(A.width, A.height);
    if (A.cc_bar) {
        A.cc_bar->set_dims(A.width, A.height);
    }
    if (A.combo_bar) {
        A.combo_bar->set_dims(A.width, A.height);
    }
    if (A.help_screen) {
        A.help_screen->set_dims(A.width, A.height);
    }
}

static void set_ncurses_esc_delay(int esc_delay)
{
    if (esc_delay >= 0) {
        set_escdelay(esc_delay);
    } else if (getenv("ESCDELAY") == nullptr) {
        set_escdelay(50);
    }
}

static void begin_ncurses(App &A)
{
    initscr();

    if (start_color() == OK) {
        A.term_has_colors = has_colors() && COLORS >= 8;
    } else {
        A.term_has_colors = false;
    }

    cbreak();
    noecho();
    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);

    halfdelay(1);
    A.term_has_default_colors = (use_default_colors() == OK);

    set_ncurses_esc_delay(A.imm.esc_delay);
}

static void end_ncurses()
{
    clear();
    endwin();
}

static inline void write_to_output_writer(App &A, std::string_view s, bool do_quit)
{
    MY_ASSERT(A.has_output_writer());

    A.imm.output_writer.enqueue(s);

    A.quit_flag |= do_quit;
}

static void do_redraw(App &A)
{
    erase();

    if (!A.is_focused(A.help_screen)) {
        A.list->draw();
    }

    if (A.is_focused(A.list)) {
        A.list->focus();
    } else {
        A.which_focused()->draw();
        A.which_focused()->focus();
    }

    refresh();
}

static void show_info(App &A, std::string_view msg, bool dismissable = true)
{
    A.info_bar->reset(msg, dismissable);
    A.set_focused(A.info_bar);
}

static std::vector<TruncatedText> read_entry_from_bio(const ImmutableParams &imm, Bio &bio)
{
    std::vector<TruncatedText> res;
    res.reserve(imm.num_cols);

    for (size_t i = 0; i < imm.num_cols; ++i) {
        std::string s = bio.read_line();
        res.push_back(TruncatedText{decode(s)});
    }

    return res;
}

static inline bool is_single_char_command(const std::string &line, char command)
{
    return line.size() == 1 && line[0] == command;
}

static std::pair<bool, uint64_t> is_space_and_number_command(const std::string &line, char command)
{
    if (line.size() < 3) {
        return {false, 0};
    }
    if (line[0] != command || line[1] != ' ') {
        return {false, 0};
    }
    std::string_view sv = strview_utils::from_string(line, 2);
    uint64_t res;
    try {
        res = parse_uint_as<uint64_t>(sv);
    } catch (const ParseError &) {
        return {false, 0};
    }
    return {true, res};
}

static inline void check_sane_index(uint64_t i, const std::string &line)
{
    if (i >= SIZE_MAX) {
        throw ProtocolError("index is insane", line);
    }
}

static void execute_command_from_bio(App &A, Bio &bio, const std::string &line, bool &should_shrink)
{
    if (is_single_char_command(line, 'v')) {
        write_to_output_writer(A, "1\n", false);

    } else if (auto [ok, ver] = is_space_and_number_command(line, 'V'); ok) {
        if (ver != 1) {
            throw ProtocolError("only protocol version 1 is supported", line);
        }

    } else if (is_single_char_command(line, 'w')) {
        show_info(A, "...", false);

    } else if (is_single_char_command(line, 'W')) {
        A.set_focused(A.list);

    } else if (is_single_char_command(line, '+')) {
        A.list->add(read_entry_from_bio(A.imm, bio));

    } else if (auto [ok, i] = is_space_and_number_command(line, '='); ok) {
        check_sane_index(i, line);
        if (!A.list->is_valid_index(i)) {
            throw ProtocolError("invalid list index", line);
        }
        A.list->set(i, read_entry_from_bio(A.imm, bio));

    } else if (auto [ok, i] = is_space_and_number_command(line, '-'); ok) {
        check_sane_index(i, line);
        if (!A.list->is_valid_index(i)) {
            throw ProtocolError("invalid list index", line);
        }
        A.list->del(i);
        should_shrink = true;

    } else if (is_single_char_command(line, 'x')) {
        A.list->clear();
        should_shrink = true;

    } else {
        throw ProtocolError("bad command", line);
    }
}

static void read_series_from_bio(App &A, Bio &bio)
{
    try {
        std::string header_line = bio.read_line();
        auto [ok, n] = is_space_and_number_command(header_line, 'n');
        if (!ok) {
            throw ProtocolError("expected command pack ('n NUMBER')", header_line);
        }

        bool should_shrink = false;
        for (uint64_t i = 0; i < n; ++i) {
            execute_command_from_bio(A, bio, bio.read_line(), should_shrink);
        }
        if (should_shrink) {
            A.list->shrink();
        }

    } catch (const EofError &) {
        A.quit_flag = true;
        return;
    }

    write_to_output_writer(A, "ok\n", false);
}

static std::vector<std::vector<TruncatedText>> read_whole_list_from_bio(const ImmutableParams &imm, Bio &bio)
{
    std::string header_line = bio.read_line();
    auto [ok, n_u64] = is_space_and_number_command(header_line, 'N');
    if (!ok) {
        throw ProtocolError("expected command pack ('N NUMBER')", header_line);
    }

    if (n_u64 >= SIZE_MAX) {
        throw ProtocolError("number of entries is insane", header_line);
    }
    size_t n{static_cast<size_t>(n_u64)};

    std::vector<std::vector<TruncatedText>> res;
    res.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        std::string line = bio.read_line();
        if (!is_single_char_command(line, '+')) {
            throw ProtocolError("expected command '+'", line);
        }
        res.push_back(read_entry_from_bio(imm, bio));
    }
    return res;
}

static void read_from_bio(App &A, Bio &bio)
{
    do {
        read_series_from_bio(A, bio);
    } while (bio.has_something());
}

static bool poll_until_have_ncurses_input(App &A, wint_t &out_c)
{
    auto make_pollfd = [](int fd, short events) -> struct pollfd {
        struct pollfd res = {};
        res.fd = fd;
        res.events = events;
        return res;
    };

    int input_fd = A.imm.input_bio.fd().raw();
    int output_fd = A.imm.output_writer.fd_for_poll();

    std::array<struct pollfd, 3> pfds{
        make_pollfd(input_fd,  POLLIN),
        make_pollfd(output_fd, POLLOUT),
        make_pollfd(0,         POLLIN),
    };
    for (;;) {
        int poll_rc = poll_utils::do_poll(pfds.data(), pfds.size());

        if (pfds[0].revents) {
            read_from_bio(A, A.imm.input_bio);
            return false;
        }
        if (pfds[1].revents) {
            if (!A.imm.output_writer.try_write_nonblocking()) {
                A.quit_flag = true;
            }
            return false;
        }
        if (pfds[2].revents || poll_rc == 0) {
            int rc = get_wch(&out_c);
            if (rc != ERR) {
                return true;
            }
        }
    }
}

static bool handle_input(App &A, wint_t c)
{
    if (c == ctrl('l') || c == KEY_RESIZE) {
        update_dims(A);
        clear();
        return true;
    }
    if (c == ctrl('[')) {
        // ESC
        auto can_hide_info_bar = [&]() -> bool {
            if (A.is_focused(A.info_bar)) {
                return A.info_bar->dismissable();
            }
            return true;
        };
        if (can_hide_info_bar()) {
            A.set_focused(A.list);
        }
        return true;
    }

    bool handled = A.which_focused()->handle_input(c);
    if (!handled && A.is_focused(A.combo_bar)) {
        handled = A.list->handle_input(c);
    }
    return handled;
}

static void combo_bar_content_finished(App &A, std::string_view combo)
{
    if (A.has_output_writer()) {
        std::string s = "combo";

        char delimiter = A.imm.bare_mode ? ' ' : '\n';
        s.push_back(delimiter);

        s.append(combo);
        s.push_back('\n');

        write_to_output_writer(A, s, true);

    } else {
        A.output_variant = std::make_unique<std::variant<size_t, std::string>>(
            std::string(combo));
        A.quit_flag = true;
    }
}

static void combo_bar_content_updated(App &A, std::string_view combo)
{
    switch (A.imm.combo_storage.lookup(combo)) {

    case ComboStorage::LookupResult::EXACT:
        if (A.imm.arg_flag_require_enter_after_combo) {
            A.combo_bar->set_minus_symbol(false);
        } else {
            combo_bar_content_finished(A, combo);
        }
        break;

    case ComboStorage::LookupResult::NONE:
        A.combo_bar->set_minus_symbol(true);
        break;

    case ComboStorage::LookupResult::PREFIX:
        A.combo_bar->set_minus_symbol(false);
        break;
    }
}

static void event_list_info(App &A, std::any data)
{
    std::string_view info_str = std::any_cast<std::string_view>(data);
    show_info(A, info_str);
}

static void event_list_to_command_mode(App &A, std::any /*data*/)
{
    if (A.cc_bar) {
        A.cc_bar->reset();
        A.set_focused(A.cc_bar);
    } else {
        show_info(A, "Bare mode, custom commands are not supported");
    }
}

static void event_list_to_combo_mode(App &A, std::any data)
{
    MY_ASSERT(A.combo_bar);

    char c = std::any_cast<char>(data);
    A.combo_bar->reset();
    A.combo_bar->add_char(c);

    A.set_focused(A.combo_bar);

    std::string_view sv{&c, 1};
    combo_bar_content_updated(A, sv);
}

static void event_list_quit(App &A, std::any /*data*/)
{
    A.quit_flag = true;
}

static void event_list_enter(App &A, std::any /*data*/)
{
    if (A.list->empty()) {
        show_info(A, "No items");
        return;
    }

    if (A.has_output_writer()) {

        if (A.imm.arg_flag_persist) {
            std::string s = "enter\n";
            append_uint_to_str(s, A.list->selected_index());
            s.push_back('\n');

            write_to_output_writer(A, s, false);

        } else {
            std::string s = "result";

            char delimiter = A.imm.bare_mode ? ' ' : '\n';
            s.push_back(delimiter);

            append_uint_to_str(s, A.list->selected_index());
            s.push_back('\n');

            write_to_output_writer(A, s, true);
        }

    } else {
        A.output_variant = std::make_unique<std::variant<size_t, std::string>>(A.list->selected_index());
        A.quit_flag = true;
    }
}

static void event_list_backslash(App &A, std::any /*data*/)
{
    if (!A.imm.arg_flag_report_backslash) {
        show_info(A, "Backslash is not supported");
        return;
    }

    if (A.list->empty()) {
        show_info(A, "Backslash requires an index, but the list is empty");

    } else {
        std::string s = "backslash\n";
        append_uint_to_str(s, A.list->selected_index());
        s.push_back('\n');

        write_to_output_writer(A, s, false);
    }
}

static void event_info_bar_hide(App &A, std::any /*data*/)
{
    if (A.info_bar->dismissable()) {
        A.set_focused(A.list);
    }
}

static void event_cc_bar_leave(App &A, std::any /*data*/)
{
    A.set_focused(A.list);
}

static void event_cc_bar_enter(App &A, std::any data)
{
    char cmd_ch = std::any_cast<char>(data);

    bool with_index;
    if (!A.imm.cc_storage.lookup(cmd_ch, with_index)) {
        show_info(A, "No such custom command");
        return;
    }

    if (with_index) {
        if (A.list->empty()) {
            show_info(A, "This custom command requires an index, but the list is empty");
            return;
        } else {
            std::string s = "custom\n";
            s.push_back(cmd_ch);
            s.push_back('\n');
            append_uint_to_str(s, A.list->selected_index());
            s.push_back('\n');
            write_to_output_writer(A, s, false);
        }

    } else {
        std::string s = "custom\n";
        s.push_back(cmd_ch);
        s.push_back('\n');
        write_to_output_writer(A, s, false);
    }

    A.set_focused(A.list);
}

static void event_cc_bar_help(App &A, std::any /*data*/)
{
    MY_ASSERT(A.help_screen);

    if (A.help_screen->empty()) {
        show_info(A, "No help because no extra keys defined");
    } else {
        A.help_screen->reset();
        A.set_focused(A.help_screen);
    }
}

static void event_help_screen_leave(App &A, std::any /*data*/)
{
    A.set_focused(A.list);
}

static void event_combo_bar_enter(App &A, std::any data)
{
    std::string_view combo = std::any_cast<std::string_view>(data);

    if (A.imm.combo_storage.lookup(combo) == ComboStorage::LookupResult::EXACT) {
        combo_bar_content_finished(A, combo);

    } else {
        show_info(A, "No such combo");
    }
}

static void event_combo_bar_update(App &A, std::any data)
{
    std::string_view combo = std::any_cast<std::string_view>(data);
    combo_bar_content_updated(A, combo);
}

static void event_combo_bar_leave(App &A, std::any /*data*/)
{
    A.set_focused(A.list);
}

template<class T>
static void do_subscribe(
        App &A,
        std::unique_ptr<T> &widget,
        std::string_view event_name,
        void (*callback)(App &, std::any)
    )
{
    static_assert(std::is_base_of_v<Widget, T>, "T must inherit from Widget");

    widget->subscribe(event_name, [&A, callback](std::any data) -> void {
        callback(A, data);
    });
}

static void prepare_to_run(App &A, Args &my_args)
{
    CMenuStyles my_styles{
        intern_style(my_args.style_header, 1),
        intern_style(my_args.style_hi,     2),
        intern_style(my_args.style_entry,  3),
        intern_style(my_args.style_error,  4),
    };

    std::vector<TruncatedText> headers;
    headers.reserve(A.imm.num_cols);
    for (const std::string &s : my_args.headers) {
        headers.push_back(TruncatedText(decode(s)));
    }

    bool vi_keys = !A.imm.bare_mode;

    A.list = std::make_unique<List>(
        std::move(my_args.CW_spec),
        std::move(headers),
        my_styles,
        vi_keys
    );
    do_subscribe(A, A.list, "info", event_list_info);
    do_subscribe(A, A.list, "to_command_mode", event_list_to_command_mode);
    do_subscribe(A, A.list, "to_combo_mode", event_list_to_combo_mode);
    do_subscribe(A, A.list, "quit", event_list_quit);
    do_subscribe(A, A.list, "enter", event_list_enter);
    do_subscribe(A, A.list, "backslash", event_list_backslash);

    A.info_bar = std::make_unique<InfoBar>(my_styles.style_error);
    do_subscribe(A, A.info_bar, "hide", event_info_bar_hide);

    if (A.imm.bare_mode) {
        A.combo_bar = std::make_unique<ComboBar>(my_styles.style_error);
        do_subscribe(A, A.combo_bar, "enter", event_combo_bar_enter);
        do_subscribe(A, A.combo_bar, "leave", event_combo_bar_leave);
        do_subscribe(A, A.combo_bar, "update", event_combo_bar_update);
    } else {
        A.cc_bar = std::make_unique<CCBar>();
        do_subscribe(A, A.cc_bar, "leave", event_cc_bar_leave);
        do_subscribe(A, A.cc_bar, "enter", event_cc_bar_enter);
        do_subscribe(A, A.cc_bar, "help", event_cc_bar_help);

        A.help_screen = std::make_unique<HelpScreen>(my_styles, vi_keys);
        if (A.imm.arg_flag_report_backslash) {
            A.help_screen->add_line("\\", my_args.backslash_descr);
        }
        for (const auto &[cmd, descr] : my_args.cc_descr) {
            std::string what = ":";
            what.push_back(cmd);
            A.help_screen->add_line(what, descr);
        }
        A.help_screen->done_adding_lines();
        do_subscribe(A, A.help_screen, "leave", event_help_screen_leave);
    }
}

static void do_run(App &A)
{
    A.set_focused(A.list);

    update_dims(A);
    do_redraw(A);

    while (!A.quit_flag) {
        wint_t c;
        if (poll_until_have_ncurses_input(A, c)) {
            if (handle_input(A, c)) {
                do_redraw(A);
            }
        } else {
            do_redraw(A);
        }
    }
}

static void do_check_fd(const std::string &what, int fd)
{
    try {
        fd_utils::check_fd(fd);
    } catch (const FDNotAvailableError &) {
        throw CMenuError(what + " (" + std::to_string(fd) + ") is not available");
    }
}

class SigmaskGuard
{
private:
    sigset_t m_old;

public:
    SigmaskGuard()
        : m_old{}
    {
        poll_utils::setup_sigmask(m_old);
    }

    SigmaskGuard(const SigmaskGuard &) = delete;
    SigmaskGuard(SigmaskGuard &&) = delete;

    SigmaskGuard &operator =(const SigmaskGuard &) = delete;
    SigmaskGuard &operator =(SigmaskGuard &&) = delete;

    ~SigmaskGuard()
    {
        poll_utils::restore_sigmask(m_old);
    }
};

class LocaleGuard
{
private:
    locale_t m_native;
    locale_t m_old;

public:
    LocaleGuard()
        : m_native{}
        , m_old{}
    {
        locale_t bad_locale = static_cast<locale_t>(0);

        m_native = newlocale(LC_ALL_MASK, "", bad_locale);
        if (m_native == bad_locale) {
            throw std::system_error(errno, std::generic_category(), "newlocale() failed");
        }
        m_old = uselocale(m_native);
        if (m_old == bad_locale) {
            freelocale(m_native);
            throw std::system_error(errno, std::generic_category(), "uselocale() failed");
        }
    }

    LocaleGuard(const LocaleGuard &) = delete;
    LocaleGuard(LocaleGuard &&) = delete;

    LocaleGuard &operator =(const LocaleGuard &) = delete;
    LocaleGuard &operator =(LocaleGuard &&) = delete;

    ~LocaleGuard()
    {
        freelocale(m_native);
        uselocale(m_old);
    }
};

template<class F>
static void guard_and_do_run(App &A, F &&prepare)
{
    SigmaskGuard sigmask_guard{};

    begin_ncurses(A);

    auto finish = [&]() -> void {
        end_ncurses();
        if (A.has_output_writer()) {
            A.imm.output_writer.write_remainder_blocking();
        }
    };

    try {
        prepare();
        do_run(A);
    } catch (...) {
        finish();
        throw;
    }
    finish();
}

static ImmutableParams imm_from_args(Args &my_args, FD &&fd_in, FD &&fd_out)
{
    return ImmutableParams{
        my_args.headers.size(),
        my_args.bare_mode,
        my_args.persist,
        my_args.report_backslash,
        my_args.require_enter_after_combo,
        my_args.esc_delay,
        Writer{std::move(fd_out)},
        Bio{std::move(fd_in)},
        std::move(my_args.cc_storage),
        std::move(my_args.combo_storage),
    };
}

static inline void set_sigpipe_handler(struct sigaction *my_sa)
{
    if (sigemptyset(&my_sa->sa_mask) < 0) {
        throw std::system_error(errno, std::generic_category(), "sigemptyset() failed");
    }
    if (sigaction(SIGPIPE, my_sa, nullptr) < 0) {
        throw std::system_error(errno, std::generic_category(), "sigaction() failed");
    }
}

namespace app
{

void sigpipe_ignore()
{
    struct sigaction my_sa = {};
    my_sa.sa_handler = SIG_IGN;
    set_sigpipe_handler(&my_sa);
}

void sigpipe_reset_handler_to_default()
{
    struct sigaction my_sa = {};
    my_sa.sa_handler = SIG_DFL;
    set_sigpipe_handler(&my_sa);
}

void run_normal_mode(Args &&my_args)
{
    FD input_fd{my_args.input_fd};
    FD output_fd{my_args.output_fd};

    do_check_fd("Input FD", input_fd.raw());
    do_check_fd("Output FD", output_fd.raw());

    LocaleGuard locale_guard{};

    App A{imm_from_args(
        my_args,
        std::move(input_fd),
        std::move(output_fd)
    )};

    guard_and_do_run(A, [&]() -> void {
        prepare_to_run(A, my_args);
    });
}

void run_bare_mode(Args &&my_args, FD &&fd_in, FD &&fd_out)
{
    LocaleGuard locale_guard{};

    ImmutableParams imm = imm_from_args(my_args, FD{}, std::move(fd_out));

    std::vector<std::vector<TruncatedText>> whole_list;

    {
        Bio my_bio{std::move(fd_in)};
        try {
            whole_list = read_whole_list_from_bio(imm, my_bio);
        } catch (const EofError &) {
            throw CMenuError("Unexpected end-of-file while reading from stdin");
        } catch (const ProtocolError &e) {
            throw CMenuError(std::string("Protocol error while reading from stdin: ") + e.what());
        }
    }

    App A{std::move(imm)};

    guard_and_do_run(A, [&]() -> void {

        prepare_to_run(A, my_args);

        for (auto &entry : whole_list) {
            A.list->add(std::move(entry));
        }
        A.list->shrink();
    });
}

std::optional<std::variant<size_t, std::string>> run_bare_mode_sync(
        Args &&my_args,
        const std::vector<std::vector<std::string>> &entries)
{
    LocaleGuard locale_guard{};

    App A{imm_from_args(my_args, FD{}, FD{})};

    guard_and_do_run(A, [&]() -> void {

        prepare_to_run(A, my_args);

        size_t num_cols = A.imm.num_cols;
        for (const auto &entry : entries) {
            if (entry.size() != num_cols) {
                throw std::logic_error("element of entries vector has wrong number of columns");
            }
            std::vector<TruncatedText> tt_entries;
            tt_entries.reserve(num_cols);
            for (const auto &col : entry) {
                tt_entries.push_back(TruncatedText(decode(col)));
            }
            A.list->add(std::move(tt_entries));
        }
        A.list->shrink();
    });

    if (A.output_variant) {
        return {*A.output_variant};
    } else {
        return {};
    }
}

} // namespace app
