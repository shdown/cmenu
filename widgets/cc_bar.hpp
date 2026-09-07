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

#include <string>
#include <any>
#include <utility>
#include <curses.h>
#include <wchar.h>
#include <array>
#include "../widget.hpp"
#include "../publisher.hpp"
#include "../input_utils.hpp"
#include "../cc_storage.hpp"
#include "../phys.hpp"

class CCBar : public Widget
{
private:
    char m_modebyte;
    bool m_has_enough_space;
    Publisher m_publisher;
    PHYS m_y;

public:
    CCBar()
        : m_modebyte{'\0'}
        , m_has_enough_space{false}
        , m_publisher{}
        , m_y{0}
    {}

    void reset()
    {
        m_modebyte = '\0';
    }

    virtual void subscribe(std::string_view event_name, Publisher::Handler &&handler) override
    {
        m_publisher.connect(event_name, std::move(handler));
    }

    virtual void draw() override
    {
        if (!m_has_enough_space) {
            return;
        }

        attr_set(0, 0, nullptr);

        std::array<wchar_t, 2> str = {
            L':',
            static_cast<wchar_t>(m_modebyte),
        };
        mvaddnwstr(m_y.to_int(), 0, str.data(), m_modebyte ? 2 : 1);
    }

    virtual void focus() override
    {
        if (!m_has_enough_space) {
            return;
        }

        int x = m_modebyte ? 2 : 1;
        move(m_y.to_int(), x);
    }

    virtual bool handle_input(wint_t c) override
    {
        if (is_backspace(c) || c == ctrl('h')) {
            if (m_modebyte) {
                m_modebyte = '\0';
            } else {
                m_publisher.emit("leave");
            }
            return true;

        } else if (c == ctrl('w') || c == ctrl('u')) {
            m_modebyte = '\0';
            return true;

        } else if (c == '?') {
            m_modebyte = '?';
            return true;

        } else if ((c & 127) == c && CCStorage::is_valid_symbol(c)) {
            m_modebyte = c;
            return true;

        } else if (is_enter(c) && m_modebyte != '\0') {
            if (m_modebyte == '?') {
                m_publisher.emit("help");
            } else {
                m_publisher.emit("enter", m_modebyte);
            }
            return true;

        } else {
            return false;
        }
    }

    virtual void set_dims(PHYS new_w, PHYS new_h) override
    {
        m_has_enough_space = new_w >= PHYS{3};
        m_y = new_h ? new_h - PHYS{1} : PHYS{0};
    }
};
