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

#include <stdint.h>
#include <stddef.h>
#include <array>
#include <algorithm>
#include "my_assert.hpp"

class CCStorage
{
public:
    static bool is_valid_symbol(char c)
    {
        if ('0' <= c && c <= '9') {
            return true;
        }
        if ('a' <= c && c <= 'z') {
            return true;
        }
        if ('A' <= c && c <= 'Z') {
            return true;
        }
        if (c == '_') {
            return true;
        }
        return false;
    }

private:
    enum class State : uint8_t
    {
        NONE,
        WITH_INDEX,
        WITHOUT_INDEX,
    };

    static size_t m_into_index(char c)
    {
        MY_ASSERT(static_cast<unsigned char>(c) < 128);
        return c;
    }

    std::array<State, 128> m_states;

public:
    CCStorage()
        : m_states{}
    {
        std::fill(m_states.begin(), m_states.end(), State::NONE);
    }

    CCStorage(CCStorage &&) = default;
    CCStorage(const CCStorage &) = delete;

    CCStorage &operator =(CCStorage &&) = default;
    CCStorage &operator =(const CCStorage &) = delete;

    bool is_empty() const
    {
        for (State st : m_states) {
            if (st != State::NONE) {
                return false;
            }
        }
        return true;
    }

    bool add(char c, bool with_index)
    {
        size_t i = m_into_index(c);
        if (m_states[i] != State::NONE) {
            return false;
        }
        m_states[i] = with_index ? State::WITH_INDEX : State::WITHOUT_INDEX;
        return true;
    }

    bool lookup(char c, bool &out_with_index) const
    {
        size_t i = m_into_index(c);
        if (m_states[i] == State::NONE) {
            return false;
        }
        out_with_index = m_states[i] == State::WITH_INDEX;
        return true;
    }
};
