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

#define _GNU_SOURCE 1
#include "poll_utils.hpp"
#include <poll.h>
#include <signal.h>
#include <errno.h>
#include <stddef.h>
#include <system_error>
#include <stdexcept>
#include <limits>

#include "probes.generated.h"

static void throw_for_errnum(int errnum)
{
    throw std::system_error(errnum, std::generic_category(), "unexpected failure of low-level C function");
}

static inline void checkcfunc_pth_style(int rc)
{
    if (rc != 0) {
        throw_for_errnum(rc);
    }
}

static inline void checkcfunc(int rc)
{
    if (rc < 0) {
        throw_for_errnum(errno);
    }
}

#if HAVE_PPOLL
static int do_protected_poll(struct pollfd *pfds, nfds_t num_pfds, const sigset_t *ss)
{
    return ppoll(pfds, num_pfds, nullptr, ss);
}
#else
static int do_protected_poll(struct pollfd *pfds, nfds_t num_pfds, const sigset_t *ss)
{
    sigset_t ss_orig;
    checkcfunc_pth_style(pthread_sigmask(SIG_SETMASK, ss, &ss_orig));

    int rc = poll(pfds, num_pfds, -1);
    int saved_errno = errno;

    checkcfunc_pth_style(pthread_sigmask(SIG_SETMASK, &ss_orig, nullptr));

    errno = saved_errno;
    return rc;
}
#endif

namespace poll_utils
{

int do_poll(struct pollfd *pfds, size_t num_pfds)
{
    if (num_pfds > std::numeric_limits<nfds_t>::max()) {
        throw std::overflow_error("number of fds for poll would overflow nfds_t");
    }

    sigset_t ss;

    checkcfunc_pth_style(pthread_sigmask(SIG_BLOCK, nullptr, &ss));
    checkcfunc(sigdelset(&ss, SIGWINCH));

    int rc = do_protected_poll(pfds, num_pfds, &ss);
    if (rc < 0 && errno == EINTR) {
        return 0;
    }
    return rc;
}

void setup_sigmask(sigset_t &old)
{
    sigset_t ss;
    checkcfunc(sigemptyset(&ss));
    checkcfunc(sigaddset(&ss, SIGWINCH));

    checkcfunc_pth_style(pthread_sigmask(SIG_BLOCK, &ss, &old));
}

void restore_sigmask(const sigset_t &old)
{
    checkcfunc_pth_style(pthread_sigmask(SIG_SETMASK, &old, nullptr));
}

} // namespace poll_utils
