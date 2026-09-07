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
#include <curses.h>
#include <wchar.h>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <any>
#include <functional>
#include <algorithm>
#include "../phys.hpp"
#include "../arith_utils.hpp"
#include "../strview_utils.hpp"
#include "../truncated_text.hpp"
#include "../decode.hpp"
#include "../style.hpp"
#include "../publisher.hpp"
#include "../cmenu_styles.hpp"
#include "../column_type.hpp"
#include "../widget.hpp"
#include "list.hpp"

class HelpScreen : public Widget
{
private:
    std::unique_ptr<List> m_list;
    std::string m_info;
    InternedStyle m_my_style;
    PHYS m_scr_width;
    PHYS m_scr_height;
    Publisher m_publisher;

    static std::unique_ptr<List> m_make_list(const CMenuStyles &styles, bool vi_keys)
    {
        std::vector<std::pair<ColumnType, PHYS>> CW_spec{
            {ColumnType::FIXED_WIDTH, PHYS{5}},
            {ColumnType::VARIABLE_WIDTH, PHYS{1}},
        };

        std::vector<TruncatedText> headers{
            TruncatedText(L"Keys"),
            TruncatedText(L"Description"),
        };

        return std::make_unique<List>(
            std::move(CW_spec),
            std::move(headers),
            styles,
            vi_keys
        );
    }

    void m_set_info(std::string_view suffix)
    {
        m_info.clear();
        m_info.shrink_to_fit();

        m_info = "[Help screen; press ESC]";
        if (!suffix.empty()) {
            m_info += " ";
            m_info += suffix;
        }
    }

    void m_leave()
    {
        m_publisher.emit("leave");
    }

public:
    HelpScreen(const CMenuStyles &styles, bool vi_keys)
        : m_list{m_make_list(styles, vi_keys)}
        , m_info{}
        , m_my_style{styles.style_error}
        , m_scr_width{0}
        , m_scr_height{0}
        , m_publisher{}
    {
        m_set_info("");

        m_list->subscribe("info", [this](std::any data) -> void {
            std::string_view info_str = std::any_cast<std::string_view>(data);
            this->m_set_info(info_str);
        });

        m_list->subscribe("quit", [this](std::any /*data*/) -> void {
            this->m_leave();
        });

        m_list->subscribe("enter", [this](std::any /*data*/) -> void {
            this->m_leave();
        });
    }

    void add_line(std::string_view keys, std::string_view descr)
    {
        m_list->add({
            TruncatedText(decode(keys)),
            TruncatedText(descr.empty() ? L"(not documented)" : decode(descr)),
        });
    }

    void done_adding_lines()
    {
        m_list->shrink();
    }

    bool empty() const
    {
        return m_list->empty();
    }

    virtual void subscribe(std::string_view event_name, Publisher::Handler &&handler) override
    {
        m_publisher.connect(event_name, std::move(handler));
    }

    virtual void draw() override
    {
        m_list->draw();

        if (m_scr_height >= PHYS{2}) {
            attr_set(m_my_style.a, m_my_style.cpn, nullptr);

            PHYS y = m_scr_height - PHYS{1};

            mvhline(y.to_int(), 0, ' ', m_scr_width.to_int());

            size_t w = std::min<size_t>(m_info.size(), m_scr_width.raw());
            std::string_view content = strview_utils::from_string(m_info, 0, w);

            mvaddnstr(
                y.to_int(),
                0,
                content.data(),
                arith_utils::to_int_saturate(content.size())
            );
        }
    }

    virtual void focus() override
    {
        m_list->focus();
    }

    virtual bool handle_input(wint_t c) override
    {
        return m_list->handle_input(c);
    }

    virtual void set_dims(PHYS new_w, PHYS new_h) override
    {
        m_scr_width = new_w;
        m_scr_height = new_h;

        m_list->set_dims(
            new_w,
            new_h >= PHYS{2} ? new_h - PHYS{1} : new_h
        );
    }

    void reset()
    {
        m_list->reset_selection();
        m_set_info("");
    }
};
