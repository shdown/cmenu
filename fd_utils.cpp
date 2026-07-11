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

#include "fd_utils.hpp"
#include <utility>
#include <unistd.h>
#include <fcntl.h>
#include <system_error>
#include <string>
#include <errno.h>
#include <sys/stat.h>
#include "fd.hpp"
#include "errors.hpp"

static FD do_open(const std::string &path, int flags)
{
    int res = open(path.c_str(), flags);
    if (res < 0) {
        throw std::system_error(
            errno,
            std::generic_category(),
            "open() on " + path + " failed"
        );
    }
    return FD{res};
}

static void do_duplicate_fd_into(int old_fd, int new_fd)
{
    if (dup2(old_fd, new_fd) < 0) {
        throw std::system_error(
            errno,
            std::generic_category(),
            "dup2() into " + std::to_string(new_fd) + " failed"
        );
    }
}

static FD do_duplicate_fd(int old_fd)
{
    int res = dup(old_fd);
    if (res < 0) {
        throw std::system_error(
            errno,
            std::generic_category(),
            "dup(" + std::to_string(old_fd) + ") failed"
        );
    }
    return FD{res};
}

static void ensure_std_fds_opened()
{
    for (;;) {
        FD fd = do_open("/dev/null", O_RDWR);
        if (fd.raw() > 2) {
            break;
        }
        // We purposefully leak 'fd' here.
        fd.release();
    }
}

namespace fd_utils
{

void check_fd(int fd)
{
    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        throw FDNotAvailableError(fd);
    }
}

void reset_std_fds()
{
    ensure_std_fds_opened();

    FD tty_fd = do_open("/dev/tty", O_RDWR);
    do_duplicate_fd_into(tty_fd.raw(), 0);
    do_duplicate_fd_into(tty_fd.raw(), 1);
}

std::pair<FD, FD> copy_stdin_and_stdout_fds()
{
    check_fd(0);
    check_fd(1);

    ensure_std_fds_opened();

    FD stdin_fd = do_duplicate_fd(0);
    FD stdout_fd = do_duplicate_fd(1);

    return {std::move(stdin_fd), std::move(stdout_fd)};
}

} // namespace fd_utils
