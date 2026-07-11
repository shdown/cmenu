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
#include <wchar.h>
#include <curses.h>
#include <string>
#include <string_view>
#include <utility>
#include <any>
#include <functional>
#include <algorithm>

#include "../column_type.hpp"
#include "../phys.hpp"
#include "../widget.hpp"
#include "../publisher.hpp"
#include "../input_utils.hpp"
#include "../combo_storage.hpp"
#include "../style.hpp"
#include "../my_assert.hpp"
#include "../strview_utils.hpp"

class ComboBar : public Widget
{
private:
    static inline constexpr size_t M_NUM_RESERVE = 32;

    std::string m_combo;
    PHYS m_scr_width;
    PHYS m_y;
    bool m_minus_symbol;
    InternedStyle m_style_error;
    Publisher m_publisher;

    std::pair<bool, std::string_view> m_calc_suffix() const
    {
        size_t w = m_scr_width.raw();
        if (w < 2) {
            return {false, {}};
        }
        if (m_combo.size() + 1 <= w) {
            return {false, m_combo};
        }

        size_t offset_from_end = w - 2;
        return {true, strview_utils::from_string(m_combo, m_combo.size() - offset_from_end)};
    }

    void m_reset()
    {
        m_combo.clear();
        m_combo.shrink_to_fit();
        m_combo.reserve(M_NUM_RESERVE);

        m_minus_symbol = false;
    }

    void m_on_enter()
    {
        m_publisher.emit("enter", std::cref(m_combo));
        m_reset();
    }

    void m_on_leave()
    {
        m_publisher.emit("leave");
        m_reset();
    }

    void m_on_update()
    {
        m_publisher.emit("update", std::cref(m_combo));
    }

public:
    explicit ComboBar(InternedStyle style_error)
        : m_combo{}
        , m_scr_width{0}
        , m_y{0}
        , m_minus_symbol{false}
        , m_style_error{style_error}
        , m_publisher{}
    {
        m_combo.reserve(M_NUM_RESERVE);
    }

    virtual void subscribe(const std::string &event_name, Publisher::Handler &&handler) override
    {
        m_publisher.connect(event_name, std::move(handler));
    }

    void reset()
    {
        m_reset();
    }

    void add_char(char c)
    {
        MY_ASSERT(ComboStorage::is_valid_symbol(c));
        m_combo.push_back(c);
    }

    void set_minus_symbol(bool new_val)
    {
        m_minus_symbol = new_val;
    }

    virtual void set_dims(PHYS new_w, PHYS new_h) override
    {
        m_scr_width = new_w;
        m_y = new_h ? new_h - PHYS{1} : PHYS{0};
    }

    virtual bool handle_input(wint_t c) override
    {
        if (m_combo.empty()) {
            return false;
        }

        if (is_enter(c)) {
            m_on_enter();
            return true;

        } else if (is_backspace(c) || c == ctrl('h')) {
            m_combo.pop_back();
            if (m_combo.empty()) {
                m_on_leave();
            } else {
                m_on_update();
            }
            return true;

        } else if (c == ctrl('w') || c == ctrl('u')) {
            m_on_leave();
            return true;

        } else if ((c & 127) == c && ComboStorage::is_valid_symbol(c)) {
            m_combo.push_back(static_cast<char>(c));
            m_on_update();
            return true;

        } else {
            return false;
        }
    }

    virtual void draw() override
    {
        auto [with_lt_symbol, suffix] = m_calc_suffix();

        if (m_minus_symbol) {
            attr_set(m_style_error.a, m_style_error.cpn, nullptr);
        } else {
            attr_set(0, 0, nullptr);
        }

        PHYS x{0};
        if (with_lt_symbol) {
            mvaddnstr(m_y.to_int(), 0, "<", 1);
            ++x;
        }

        PHYS suffix_len = PHYS::from(suffix.size());

        mvaddnstr(m_y.to_int(), x.to_int(), suffix.data(), suffix_len.to_int());

        x += suffix_len;

        if (m_minus_symbol) {
            mvaddnstr(m_y.to_int(), x.to_int(), "-", 1);
        }
    }

    virtual void focus() override
    {
        auto [with_lt_symbol, suffix] = m_calc_suffix();

        PHYS x = PHYS::from(suffix.size());
        if (with_lt_symbol) {
            ++x;
        }

        move(m_y.to_int(), x.to_int());
    }
};
