#include "common.h"
#include "truncated_text.h"
#include "decode.h"
#include "style.h"
#include "parse_uint.h"
#include "print_uint.h"
#include "bio.h"

#include <wchar.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

typedef struct {
    // The columns of this entry; valid indices are [0; list->ncols).
    TruncatedText *cols;
} ListEntry;

typedef struct {
    // If negative, this column has fixed width of (-w).
    // If non-negative, this column has variable width of TOTAL_WIDTH * (w / list->vw_denom).
    int32_t w;

    // Current width, calculated according to the rule above with TOTAL_WIDTH = list->width.
    uint32_t cur_width;
} ListColumn;

typedef struct {
    // Number of columns.
    size_t ncols;

    // The descriptions of the columns; valid indices are [0; list->ncols).
    ListColumn *cols;

    // The headers of the columns; valid indices are [0; list->ncols).
    TruncatedText *headers;

    // The "denominator" for variable-width columns.
    uint32_t vw_denom;

    // The sum of widths of fixed-width columns.
    uint32_t fw_sum;

    ListEntry *entries;
    size_t size;
    size_t capacity;

    // Index of the selected entry. If size is zero, then selected is also zero.
    size_t selected;

    // Current height and width.
    uint32_t height;
    uint32_t width;

    InternedStyle style_header;
    InternedStyle style_highlight;
    InternedStyle style_entry;

    bool enable_custom;

    Bio infile;
    char *infile_buf;
    size_t infile_nbuf;

    int outfd;

    bool need_more_size;

    char info_buf[512];
} List;

static char global_errmsg[1024];

static void errmsgf(const char *fmt, ...)
{
    va_list vl;
    va_start(vl, fmt);
    vsnprintf(global_errmsg, sizeof(global_errmsg), fmt, vl);
    va_end(vl);
}

static int full_write(int fd, const char *buf, size_t nbuf, int *caught_signal)
{
    for (size_t nwritten = 0; nwritten < nbuf;) {
        ssize_t w = write(fd, buf + nwritten, nbuf - nwritten);
        if (w < 0) {
            if (errno == EINTR) {
                *caught_signal = 1;
                continue;
            }
            return -1;
        }
        nwritten += w;
    }
    return 0;
}

static int say(List *list, const char *s, int *caught_signal)
{
    if (full_write(list->outfd, s, strlen(s), caught_signal) < 0) {
        errmsgf("Cannot write to output fd: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static void list_entry_free(List *list, ListEntry entry)
{
    for (size_t i = 0; i < list->ncols; ++i) {
        free(entry.cols[i].s);
    }
    free(entry.cols);
}

static void list_add(List *list, ListEntry entry)
{
    if (list->size == list->capacity) {
        list->entries = x2realloc_or_die(list->entries, &list->capacity, sizeof(ListEntry));
    }
    list->entries[list->size++] = entry;
}

static bool list_del(List *list, uint64_t idx)
{
    if (idx >= list->size)
        return false;

    list_entry_free(list, list->entries[idx]);

    if (list->selected > 0 && list->selected >= idx)
        --list->selected;

    for (size_t i = idx + 1; i < list->size; ++i)
        list->entries[i - 1] = list->entries[i];

    --list->size;

    return true;
}

static bool list_set(List *list, uint64_t idx, ListEntry entry)
{
    if (idx >= list->size)
        return false;

    list_entry_free(list, list->entries[idx]);

    list->entries[idx] = entry;

    return true;
}

static void list_clear(List *list)
{
    for (size_t i = 0; i < list->size; ++i)
        list_entry_free(list, list->entries[i]);
    list->size = 0;
    list->selected = 0;
}

static void list_selection_up(List *list, uint32_t lines)
{
    if (list->selected < lines) {
        list->selected = 0;
    } else {
        list->selected -= lines;
    }
}

static void list_selection_down(List *list, uint32_t lines)
{
    if (list->size == 0) {
        list->selected = 0;
    } else {
        uint64_t s = ((uint64_t) list->selected) + lines;
        list->selected = s < list->size ? s : list->size - 1;
    }
}

static void update_column_widths(List *list)
{
    uint32_t total_vw = list->width;
    if (total_vw < list->fw_sum) {
        list->need_more_size = true;
        return;
    }
    total_vw -= list->fw_sum;

    uint32_t vw_sum = 0;
    size_t last_vw = -1;

    for (size_t i = 0; i < list->ncols; ++i) {
        int32_t w = list->cols[i].w;
        if (w >= 0) {
            uint32_t cur_width = ((uint64_t) total_vw) * w / list->vw_denom;
            vw_sum += cur_width;
            last_vw = i;
            list->cols[i].cur_width = cur_width;
        } else {
            list->cols[i].cur_width = -w;
        }
    }

    if (last_vw != (size_t) -1) {
        list->cols[last_vw].cur_width += total_vw - vw_sum;
    }

    list->need_more_size = false;
}

static void draw_row(List *list, int y, TruncatedText *cols)
{
    uint32_t cur_x = 0;
    for (size_t i = 0; i < list->ncols; ++i) {
        uint32_t w = list->cols[i].cur_width;
        TruncatedText *t = &cols[i];
        truncate_text_to_width(t, w);
        mvaddnwstr(y, cur_x, t->s, t->truncated_n);
        cur_x += w;
    }
}

static void draw_row_styled(List *list, int y, TruncatedText *cols, InternedStyle style)
{
    attr_set(style.a, style.cpn, NULL);
    mvhline(y, 0, ' ', list->width);
    draw_row(list, y, cols);
}

static void redraw(List *list, bool requery_size)
{
    erase();

    if (requery_size) {
        int height;
        int width;
        getmaxyx(stdscr, height, width);
        list->height = height;
        list->width = width;
        update_column_widths(list);
    }

    if (list->height < 3 || list->need_more_size) {
        attr_set(0, 0, NULL);
        mvaddstr(0, 0, "(Need more size)");
        goto done;
    }

    draw_row_styled(list, 0, list->headers, list->style_header);

    if (list->info_buf[0]) {
        attr_set(0, 0, NULL);
        mvaddstr(0, 0, list->info_buf);
    }

    if (list->size) {
        int64_t idx_from = ((int64_t) list->selected) - ((int64_t) (list->height / 2));
        if (idx_from < 0)
            idx_from = 0;

        uint64_t idx_to = idx_from + list->height - 1;
        if (idx_to > list->size)
            idx_to = list->size;

        int selected_y = 0;

        for (size_t i = idx_from; i < idx_to; ++i) {
            int y = i - idx_from + 1;
            if (list->selected == i) {
                selected_y = y;
                draw_row_styled(list, y, list->entries[i].cols, list->style_highlight);
            } else {
                draw_row_styled(list, y, list->entries[i].cols, list->style_entry);
            }
        }

        move(selected_y, 0);

    } else {
        move(1, 0);
    }

done:
    refresh();
}

static void print_result(List *list, bool custom, int *exitcode)
{
    int caught_signal = 0;

    if (custom) {
        if (say(list, "custom\n", &caught_signal) < 0) {
            goto error;
        }
    } else {
        if (say(list, "result\n", &caught_signal) < 0) {
            goto error;
        }
        char buf[32];
        int n = print_uint(buf, sizeof(buf), list->selected);
        buf[n++] = '\n';
        buf[n++] = '\0';
        if (say(list, buf, &caught_signal) < 0) {
            goto error;
        }
    }
    return;

error:
    *exitcode = 1;
}

#define ctrl(x) ((x) & 0x1F)

static int handle_input(List *list, bool *requery_size, int *exitcode)
{
    int c = getch();
    switch (c) {
    case KEY_UP:
    case 'k':
    case ctrl('p'):
        --list->selected;
        if (list->selected == (size_t) -1) {
            list->selected = list->size ? list->size - 1 : 0;
        }
        return 0;

    case KEY_DOWN:
    case 'j':
    case ctrl('n'):
        ++list->selected;
        if (list->selected >= list->size) {
            list->selected = 0;
        }
        return 0;

    case KEY_HOME:
    case 'g':
        list->selected = 0;
        return 0;

    case KEY_END:
    case 'G':
        list->selected = list->size ? list->size - 1 : 0;
        return 0;

    case ctrl('g'):
        snprintf(
            list->info_buf, sizeof(list->info_buf),
            "--- %zu/%zu --- (ESC to hide this)", list->selected + 1, list->size);
        return 0;

    case ctrl('['):
        list->info_buf[0] = '\0';
        return 0;

    case KEY_NPAGE:
    case ctrl('f'):
        list_selection_down(list, list->height);
        return 0;

    case KEY_PPAGE:
    case ctrl('b'):
        list_selection_up(list, list->height);
        return 0;

    case ctrl('d'):
        list_selection_down(list, list->height / 2);
        return 0;

    case ctrl('u'):
        list_selection_up(list, list->height / 2);
        return 0;

    case ctrl('l'):
        *requery_size = true;
        return 0;

    case KEY_ENTER:
    case '\n':
    case '\r':
        if (list->size) {
            print_result(list, false, exitcode);
            return -1;
        }
        return 0;

    case 'c':
        if (list->enable_custom) {
            print_result(list, true, exitcode);
            return -1;
        }
        return 0;

    case KEY_RESIZE:
        *requery_size = true;
        return 0;

    case 'q':
        return -1;

    case ERR:
    default:
        return 0;
    }
}

static inline TruncatedText truncated_text_from_cstr(const char *s)
{
    wchar_t *ws = decode_copy(s);
    if (!ws) {
        const wchar_t wmsg[] = L"(encoding error)";
        ws = memdup_or_die(wmsg, sizeof(wmsg));
    }
    size_t nws = wcslen(ws);
    if (nws > INT_MAX) {
        nws = INT_MAX;
        ws[nws] = L'\0';
    }
    return (TruncatedText) {
        .s = ws,
        .n = nws,
    };
}

static char *read_line_from_infile(List *list, int *caught_signal)
{
    ssize_t r = bio_read_line(&list->infile, &list->infile_buf, &list->infile_nbuf, caught_signal);
    if (r < 0) {
        return NULL;
    }
    char *line = list->infile_buf;
    if (r == 0 || line[r - 1] != '\n') {
        errno = 0;
        return NULL;
    }
    line[r - 1] = '\0';
    return line;
}

static int read_entry_from_infile(List *list, ListEntry *entry, int *caught_signal)
{
    size_t ncols = list->ncols;
    TruncatedText *cols = malloc_or_die(sizeof(TruncatedText), ncols);
    size_t col_i = 0;
    for (; col_i < ncols; ++col_i) {
        char *line = read_line_from_infile(list, caught_signal);
        if (!line) {
            if (errno == 0) {
                errmsgf("Unterminated '+' command (got EOF).\n");
                goto fail;
            } else {
                errmsgf("Cannot read line from input fd: %s\n", strerror(errno));
                goto fail;
            }
        }
        cols[col_i] = truncated_text_from_cstr(line);
    }
    *entry = (ListEntry) {cols};
    return 0;
fail:
    for (size_t i = 0; i < col_i; ++i) {
        free(cols[i].s);
    }
    free(cols);
    return -1;
}

static int handle_infile_command(List *list, int *caught_signal)
{
    char *line = read_line_from_infile(list, caught_signal);
    if (!line) {
        if (errno == 0) {
            errmsgf("Expected a command, got EOF.\n");
            return -1;
        } else {
            errmsgf("Cannot read line from input fd: %s\n", strerror(errno));
            return -1;
        }
    }

    if (line[0] == '+' && line[1] == '\0') {
        ListEntry entry;
        if (read_entry_from_infile(list, &entry, caught_signal) < 0) {
            return -1;
        }
        list_add(list, entry);
        return 0;

    } else if (line[0] == '=' && line[1] == ' ') {
        const char *v = line + 2;
        int64_t r = parse_uint(v, strlen(v), INT64_MAX);
        if (r < 0) {
            errmsgf("Cannot parse '=' index: %s\n", parse_uint_strerror(r));
            return -1;
        }
        ListEntry entry;
        if (read_entry_from_infile(list, &entry, caught_signal) < 0) {
            return -1;
        }
        list_set(list, r, entry);
        return 0;

    } else if (line[0] == '-' && line[1] == ' ') {
        const char *v = line + 2;
        int64_t r = parse_uint(v, strlen(v), INT64_MAX);
        if (r < 0) {
            errmsgf("Cannot parse '-' index: %s\n", parse_uint_strerror(r));
            return -1;
        }
        list_del(list, r);
        return 0;

    } else if (line[0] == 'x' && line[1] == '\0') {
        list_clear(list);
        return 0;

    } else {
        errmsgf("Invalid command: %s\n", line);
        return -1;
    }
}

static int handle_infile_line(List *list, bool *close_infile, int *caught_signal)
{
    char *line = read_line_from_infile(list, caught_signal);
    if (!line) {
        if (errno == 0) {
            *close_infile = true;
            return 0;
        } else {
            errmsgf("Cannot read line from input fd: %s\n", strerror(errno));
            return -1;
        }
    }

    if (line[0] == 'n' && line[1] == ' ') {
        const char *v = line + 2;
        int64_t n = parse_uint(v, strlen(v), INT64_MAX);
        if (n < 0) {
            errmsgf("Cannot parse 'n' number: %s\n", parse_uint_strerror(n));
            return -1;
        }
        for (int64_t i = 0; i < n; ++i) {
            if (handle_infile_command(list, caught_signal) < 0) {
                return -1;
            }
        }
        if (say(list, "ok\n", caught_signal) < 0) {
            return -1;
        }
        return 0;

    } else {
        errmsgf("Invalid line (expected 'n NUMBER'): %s\n", line);
        return -1;
    }
}

static int reset_std_fds(void)
{
    int tty_fd;
    for (;;) {
        tty_fd = open("/dev/tty", O_RDWR);
        if (tty_fd < 0) {
            perror("open: /dev/tty");
            return -1;
        }
        if (tty_fd > 2) {
            break;
        }
    }

    if (dup2(tty_fd, 0) < 0) {
        perror("dup2: tty_fd -> 0");
        return -1;
    }
    if (dup2(tty_fd, 1) < 0) {
        perror("dup2: tty_fd -> 1");
        return -1;
    }
    close(tty_fd);
    return 0;
}

static int check_fd(int fd, const char *what)
{
    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        fprintf(stderr, "Cannot fstat() %s: %s\n", what, strerror(errno));
        return -1;
    }
    return 0;
}

static inline const char *strfollow(const char *s, const char *prefix)
{
    size_t nprefix = strlen(prefix);
    if (strncmp(s, prefix, nprefix) == 0)
        return s + nprefix;
    return NULL;
}

typedef struct {
    const char **data;
    size_t size;
    size_t capacity;
} StringVec;

static inline StringVec string_vec_new(void)
{
    return (StringVec) {NULL, 0, 0};
}

static inline void string_vec_push(StringVec *sv, const char *s)
{
    if (sv->size == sv->capacity) {
        sv->data = x2realloc_or_die(sv->data, &sv->capacity, sizeof(const char *));
    }
    sv->data[sv->size++] = s;
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    StringVec column_args = string_vec_new();
    bool enable_custom = false;
    RawStyle style_header = {.a = A_BOLD, .fc = COLOR_WHITE, .bc = COLOR_GREEN};
    RawStyle style_hi     = {.a = 0,      .fc = COLOR_WHITE, .bc = COLOR_BLUE};
    RawStyle style_entry  = {.a = 0,      .fc = -1,          .bc = -1};
    int infd = -1;
    int outfd = -1;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *v;

        if ((v = strfollow(arg, "-style-header="))) {
            char err[256];
            if (parse_style(v, &style_header, err, sizeof(err)) < 0) {
                fprintf(stderr, "Invalid -style-header= argument: %s\n", err);
                return 2;
            }

        } else if ((v = strfollow(arg, "-style-entry="))) {
            char err[256];
            if (parse_style(v, &style_entry, err, sizeof(err)) < 0) {
                fprintf(stderr, "Invalid -style-entry= argument: %s\n", err);
                return 2;
            }

        } else if ((v = strfollow(arg, "-style-hi="))) {
            char err[256];
            if (parse_style(v, &style_entry, err, sizeof(err)) < 0) {
                fprintf(stderr, "Invalid -style-hi= argument: %s\n", err);
                return 2;
            }

        } else if ((v = strfollow(arg, "-column="))) {
            string_vec_push(&column_args, v);

        } else if ((v = strfollow(arg, "-infd="))) {
            infd = parse_uint(v, strlen(v), INT_MAX);
            if (infd < 0) {
                fprintf(stderr, "Invalid -infd= argument: %s.\n", parse_uint_strerror(infd));
                return 2;
            }

        } else if ((v = strfollow(arg, "-outfd="))) {
            outfd = parse_uint(v, strlen(v), INT_MAX);
            if (outfd < 0) {
                fprintf(stderr, "Invalid -outfd= argument: %s.\n", parse_uint_strerror(outfd));
                return 2;
            }

        } else if (strcmp(arg, "-enable-custom") == 0) {
            enable_custom = true;

        } else {
            fprintf(stderr, "Unknown option: '%s'.\n", arg);
            return 2;
        }
    }

    if (!column_args.size) {
        fprintf(stderr, "No -column= arguments found.\n");
        return 2;
    }

    if (infd < 0) {
        fprintf(stderr, "No -infd= argument found.\n");
        return 2;
    }

    if (outfd < 0) {
        fprintf(stderr, "No -outfd= argument found.\n");
        return 2;
    }

    size_t ncols = column_args.size;
    ListColumn *cols = malloc_or_die(sizeof(ListColumn), ncols);
    TruncatedText *headers = malloc_or_die(sizeof(TruncatedText), ncols);
    uint32_t vw_denom = 0;
    uint32_t fw_sum = 0;

    for (size_t i = 0; i < ncols; ++i) {
        const char *arg = column_args.data[i];
        const char *colon = strchr(arg, ':');
        if (!colon) {
            fprintf(stderr, "Invalid -column= argument (no ':' found): '%s'.\n", arg);
            return 2;
        }

        int32_t w = 1;
        if (colon != arg) {
            const char *number_start = arg;
            bool negate = false;
            if (arg[0] == '@') {
                ++number_start;
                negate = true;
            }
            int32_t r = parse_uint(number_start, colon - number_start, INT32_MAX);
            if (r < 0) {
                fprintf(stderr, "Cannot parse column width in -column='%s': %s.\n", arg, parse_uint_strerror(r));
                return 2;
            }
            w = negate ? -r : r;
        }

        headers[i] = truncated_text_from_cstr(colon + 1);
        cols[i] = (ListColumn) {.w = w};

        if (w >= 0) {
            uint32_t addend = w;
            vw_denom += addend;
            if (vw_denom < addend) {
                fprintf(stderr, "Total width of the (variable-width) columns would overflow uint32_t.\n");
                return 2;
            }
        } else {
            uint32_t addend = -w;
            fw_sum += addend;
            if (fw_sum < addend) {
                fprintf(stderr, "Total width of the fixed-width columns would overflow uint32_t.\n");
                return 2;
            }
        }
    }
    if (fw_sum == 0) {
        fw_sum = 1;
    }

    if (reset_std_fds() < 0) {
        return 1;
    }

    if (check_fd(infd, "input fd") < 0) {
        return 1;
    }

    if (check_fd(outfd, "output fd") < 0) {
        return 1;
    }

    initscr();
    start_color();
    cbreak();
    noecho();
    nonl();
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    set_escdelay(50);
    halfdelay(1);

    int ret = 0;

    List list = {
        .ncols = ncols,
        .cols = cols,
        .headers = headers,
        .vw_denom = vw_denom,
        .fw_sum = fw_sum,
        .enable_custom = enable_custom,
        .infile = {
            .fd = infd,
        },
        .outfd = outfd,
    };

    intern_style(style_header, 1, &list.style_header);
    intern_style(style_hi,     2, &list.style_highlight);
    intern_style(style_entry,  3, &list.style_entry);

    struct pollfd pfds[2] = {
        {.fd = infd, .events = POLLIN},
        {.fd = 0,    .events = POLLIN},
    };

    bool requery_size = true;
again:
    redraw(&list, requery_size);
    requery_size = false;

    if (bio_has_something(&list.infile)) {
        goto handle_infile_input;
    }
    if (poll(pfds, 2, -1) < 0) {
        if (errno == EINTR) {
            goto handle_ncurses_input;
        } else {
            errmsgf("poll: %s\n", strerror(errno));
            ret = 1;
            goto done;
        }
    }
    if (pfds[0].revents) {
        goto handle_infile_input;
    }
    if (pfds[1].revents) {
        goto handle_ncurses_input;
    }
    goto again;

handle_infile_input:
    (void) 0;
    bool close_infile = false;
    int caught_signal = 0;
    if (handle_infile_line(&list, &close_infile, &caught_signal) < 0) {
        ret = 1;
        goto done;
    }
    if (close_infile) {
        close(list.infile.fd);
        pfds[0].fd = -1;
        bio_reset(&list.infile);
    }
    if (caught_signal) {
        goto handle_ncurses_input;
    }
    goto again;

handle_ncurses_input:
    if (handle_input(&list, &requery_size, &ret) < 0) {
        goto done;
    }
    goto again;

done:
    endwin();
    if (global_errmsg[0]) {
        fputs(global_errmsg, stderr);
    }
    return ret;
}
