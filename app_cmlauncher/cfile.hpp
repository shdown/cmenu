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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string_view>
#include <system_error>
#include <utility>
#include "../common/my_assert.hpp"

namespace cfile
{

class CFile
{
private:
    FILE *m_f;

public:
    CFile() noexcept
        : m_f{nullptr}
    {}

    CFile(const char *path, const char *mode)
    {
        MY_ASSERT(path != nullptr);
        MY_ASSERT(mode != nullptr);

        m_f = fopen(path, mode);
        if (!m_f) {
            throw std::system_error(errno, std::generic_category(), "cannot open file");
        }
    }

    CFile(CFile &&that) noexcept
        : m_f{that.m_f}
    {
        that.m_f = nullptr;
    }

    CFile(const CFile &) = delete;

    CFile &operator =(CFile &&that) noexcept
    {
        std::swap(m_f, that.m_f);
        return *this;
    }

    CFile &operator =(const CFile &) = delete;

    bool valid() const noexcept
    {
        return m_f != nullptr;
    }

    FILE *raw() const noexcept
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
        MY_ASSERT(f.valid());

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

    std::string_view view() const noexcept
    {
        return {m_buf, m_cur_len};
    }

    ~CFileLine()
    {
        free(m_buf);
    }
};

} // namespace cfile
