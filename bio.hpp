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
#include <sys/types.h>
#include <errno.h>
#include <array>
#include <string>
#include <system_error>
#include <utility>
#include <stdexcept>
#include "fd.hpp"
#include "errors.hpp"

class Bio
{
private:
    static inline constexpr size_t M_BUF_SIZE = 1024;

    std::array<char, M_BUF_SIZE> m_buf;
    size_t m_buf_pos;
    size_t m_buf_end;
    FD m_fd;

    size_t m_raw_read()
    {
        if (!m_fd.is_valid()) {
            throw std::logic_error("trying to read from a dummy Bio");
        }

        ssize_t r;
        while ((r = read(m_fd.raw(), m_buf.data(), M_BUF_SIZE)) < 0 && errno == EINTR) {
            // do nothing
        }
        if (r > 0) {
            return r;
        } else if (r == 0) {
            throw EofError{};
        } else {
            throw std::system_error(errno, std::generic_category(), "read() failed");
        }
    }

    char m_next_byte()
    {
        if (m_buf_pos == m_buf_end) {
            size_t nread = m_raw_read();
            m_buf_pos = 0;
            m_buf_end = nread;
        }
        return m_buf[m_buf_pos++];
    }

public:
    explicit Bio(FD &&fd = FD{})
        : m_buf{}
        , m_buf_pos{0}
        , m_buf_end{0}
        , m_fd{std::move(fd)}
    {}

    Bio(Bio &&) = default;

    Bio(const Bio &) = delete;

    Bio &operator =(Bio &&) = default;

    Bio &operator =(const Bio &) = delete;

    std::string read_line()
    {
        std::string line;
        line.reserve(16);
        for (;;) {
            char c = m_next_byte();
            if (c == '\n') {
                break;
            }
            line.push_back(c);
        }
        line.shrink_to_fit();
        return line;
    }

    bool has_something() const
    {
        return m_buf_pos != m_buf_end;
    }

    FD &fd()
    {
        return m_fd;
    }

    const FD &fd() const
    {
        return m_fd;
    }
};
