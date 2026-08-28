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

#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <string_view>
#include <system_error>
#include <stdexcept>
#include <utility>
#include "fd.hpp"
#include "os_utils.hpp"
#include "strview_utils.hpp"
#include "my_assert.hpp"

class Writer
{
private:
    std::string m_queue;
    size_t m_queue_pos;
    FD m_fd;

    void m_clear_queue()
    {
        m_queue.clear();
        m_queue.shrink_to_fit();
        m_queue_pos = 0;
    }

    static size_t m_do_write(const FD &fd, std::string_view data, bool nonblocking, bool &got_epipe)
    {
        got_epipe = false;
        size_t nwritten = 0;
        while (nwritten < data.size()) {
            ssize_t w = write(fd.raw(), data.data() + nwritten, data.size() - nwritten);
            if (w < 0) {
                if (errno == EINTR) {
                    continue;
                } else if (errno == EPIPE) {
                    got_epipe = true;
                    break;
                } else if (nonblocking && IS_EAGAIN(errno)) {
                    break;
                } else {
                    throw std::system_error(errno, std::generic_category(), "write() failed");
                }
            }
            nwritten += w;
        }
        return nwritten;
    }

public:
    explicit Writer(FD &&fd = FD{})
        : m_queue{}
        , m_queue_pos{0}
        , m_fd{std::move(fd)}
    {
        if (m_fd.is_valid()) {
            set_nonblocking_flag(m_fd.raw(), true);
        }
    }

    bool is_dummy() const
    {
        return !m_fd.is_valid();
    }

    bool has_something_to_write() const
    {
        return !m_queue.empty();
    }

    void enqueue(std::string_view chunk)
    {
        MY_ASSERT(!is_dummy());

        m_queue.append(chunk);
    }

    FD &fd()
    {
        return m_fd;
    }

    const FD &fd() const
    {
        return m_fd;
    }

    int fd_for_poll() const
    {
        return has_something_to_write() ? m_fd.raw() : -1;
    }

    bool try_write_nonblocking()
    {
        MY_ASSERT(!is_dummy());

        std::string_view data = strview_utils::from_string(m_queue, m_queue_pos);

        bool got_epipe;
        size_t nwritten = m_do_write(m_fd, data, true, got_epipe);
        m_queue_pos += nwritten;
        if (m_queue_pos == m_queue.size()) {
            m_clear_queue();
        }

        return !got_epipe;
    }

    bool write_remainder_blocking()
    {
        MY_ASSERT(!is_dummy());

        set_nonblocking_flag(m_fd.raw(), false);

        std::string_view data = strview_utils::from_string(m_queue, m_queue_pos);

        bool got_epipe;
        size_t nwritten = m_do_write(m_fd, data, false, got_epipe);
        if (got_epipe) {
            return false;
        }

        MY_ASSERT(nwritten == data.size());

        return true;
    }
};
