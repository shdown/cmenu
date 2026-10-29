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

#include <stdexcept>
#include <string>

class EofError : public std::runtime_error
{
public:
    EofError()
        : std::runtime_error("Got end-of-file while reading")
    {}
};

class ParseError : public std::runtime_error
{
public:
    explicit ParseError(const std::string &msg)
        : std::runtime_error(msg)
    {}
};

class NcursesError : public std::runtime_error
{
public:
    explicit NcursesError(const std::string &msg)
        : std::runtime_error(msg)
    {}
};

class PhysMathError : public std::runtime_error
{
public:
    explicit PhysMathError(const std::string &msg)
        : std::runtime_error("Got error while calculating physical sizes: " + msg)
    {}
};

class CommandLineArgsError : public std::runtime_error
{
public:
    explicit CommandLineArgsError(const std::string &msg)
        : std::runtime_error(msg)
    {}
};

class ProtocolError : public std::runtime_error
{
public:
    ProtocolError(const std::string &msg, const std::string &line)
        : std::runtime_error("Protocol: " + msg + " at line '" + line + "'")
    {}
};

class FDNotAvailableError : public std::runtime_error
{
private:
    int m_fd;
public:
    explicit FDNotAvailableError(int fd)
        : std::runtime_error("File descriptor " + std::to_string(fd) + " not available")
        , m_fd{fd}
    {}

    int which_fd() const noexcept
    {
        return m_fd;
    }
};

class DuplicatedComboError : public CommandLineArgsError
{
public:
    explicit DuplicatedComboError(const std::string &msg)
        : CommandLineArgsError(msg)
    {}
};

class CMenuError : public std::runtime_error
{
public:
    explicit CMenuError(const std::string &msg)
        : std::runtime_error(msg)
    {}
};
