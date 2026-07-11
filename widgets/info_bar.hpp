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

#include <wchar.h>
#include <stddef.h>
#include <string>
#include <algorithm>
#include <utility>
#include <curses.h>
#include "../phys.hpp"
#include "../widget.hpp"
#include "../style.hpp"
#include "../input_utils.hpp"
#include "../publisher.hpp"

class InfoBar : public Widget
{
private:
    std::string m_text;
    bool m_dismissable;
    PHYS m_scr_width;
    InternedStyle m_style;
    Publisher m_publisher;

public:
    explicit InfoBar(InternedStyle style_error)
        : m_text{}
        , m_dismissable{true}
        , m_scr_width{0}
        , m_style{style_error}
        , m_publisher{}
    {}

    virtual void subscribe(const std::string &event_name, Publisher::Handler &&handler) override
    {
        m_publisher.connect(event_name, std::move(handler));
    }

    void reset(const std::string &text, bool dismissable)
    {
        m_dismissable = dismissable;
        m_text = text;
        if (dismissable) {
            m_text += " (ESC to hide this)";
        }
    }

    bool dismissable() const
    {
        return m_dismissable;
    }

    virtual void set_dims(PHYS new_w, PHYS /*new_h*/) override
    {
        m_scr_width = new_w;
    }

    virtual bool handle_input(wint_t c) override
    {
        if (is_enter(c)) {
            if (m_dismissable) {
                m_publisher.emit("hide");
            }
            return true;
        }
        return false;
    }

    virtual void draw() override
    {
        attr_set(m_style.a, m_style.cpn, nullptr);
        mvhline(0, 0, ' ', m_scr_width.to_int());

        size_t w = std::min<size_t>(m_scr_width.raw(), m_text.size());
        mvaddnstr(0, 0, m_text.data(), PHYS::from(w).to_int());
    }

    virtual void focus() override
    {
        move(0, 0);
    }
};
