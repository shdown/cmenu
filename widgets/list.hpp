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
#include <math.h>
#include <wchar.h>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <any>
#include <string_view>
#include <utility>

#include "../publisher.hpp"
#include "../style.hpp"
#include "../widget.hpp"
#include "../phys.hpp"
#include "../truncated_text.hpp"
#include "../my_assert.hpp"
#include "../input_utils.hpp"
#include "../arith_utils.hpp"
#include "../column_type.hpp"
#include "../combo_storage.hpp"
#include "../cmenu_styles.hpp"

namespace list_details
{
    inline PHYS sum_over_widths(
            const std::vector<std::pair<ColumnType, PHYS>> &vec,
            ColumnType filter_by_type)
    {
        PHYS sum{0};
        for (auto [type, w] : vec) {
            if (type == filter_by_type) {
                sum += w;
            }
        }
        return sum;
    }

    inline size_t add_limit(size_t a, size_t b, size_t limit)
    {
        if (!arith_utils::add_is_ok(a, b)) {
            return limit;
        }
        return std::min(a, limit);
    }

    inline size_t sub_wrap0(size_t a, size_t b)
    {
        if (a < b) {
            return 0;
        }
        return a - b;
    }

} // namespace list_details

class List : public Widget
{
private:
    struct ListStyles
    {
        InternedStyle style_header;
        InternedStyle style_entry;
        InternedStyle style_hi;

        explicit ListStyles(const CMenuStyles &styles)
            : style_header{styles.style_header}
            , style_entry{styles.style_entry}
            , style_hi{styles.style_hi}
        {}
    };

    // Number of columns.
    size_t m_num_cols;

    // Column widths.
    std::vector<std::pair<ColumnType, PHYS>> m_CW_spec;
    std::vector<PHYS> m_CW;

    // Headers and lines.
    std::vector<TruncatedText> m_headers;
    std::vector<std::vector<TruncatedText>> m_lines;

    // Styles.
    ListStyles m_styles;

    // Index of the selected entry. If size is zero, then selected is also zero.
    size_t m_selected;

    // Current width and height.
    PHYS m_scr_width;
    PHYS m_scr_height;

    // Set in 'draw()', used in 'focus()'.
    PHYS m_scr_selected_y;

    // The publisher.
    Publisher m_publisher;

    // Whether vi mode is enabled.
    bool m_vi_keys;

    void m_draw_line(PHYS y, std::vector<TruncatedText> &vec, InternedStyle style)
    {
        attr_set(style.a, style.cpn, nullptr);
        mvhline(y.to_int(), 0, ' ', m_scr_width.to_int());

        PHYS cur_x{0};
        for (size_t i = 0; i < m_num_cols; ++i) {
            TruncatedText &t = vec[i];
            PHYS w = m_CW[i];

            t.truncate_to_width(w);
            std::wstring_view content = t.result();

            mvaddnwstr(
                y.to_int(),
                cur_x.to_int(),
                content.data(),
                arith_utils::to_int_saturate(content.size())
            );
            cur_x += w;
        }
    }

    bool m_update_column_widths()
    {
        PHYS fw_sum = list_details::sum_over_widths(m_CW_spec, ColumnType::FIXED_WIDTH);
        if (m_scr_width < fw_sum) {
            return false;
        }
        PHYS total_for_vw = m_scr_width - fw_sum;
        PHYS vw_denom = list_details::sum_over_widths(m_CW_spec, ColumnType::VARIABLE_WIDTH);

        PHYS vw_sum{0};
        size_t vw_last_idx = -1;

        for (size_t i = 0; i < m_num_cols; ++i) {
            auto [spec_type, spec_w] = m_CW_spec[i];
            if (spec_type == ColumnType::VARIABLE_WIDTH) {
                PHYS w = phys_utils::scale(total_for_vw, spec_w, vw_denom);
                m_CW[i] = w;
                vw_sum += w;
                vw_last_idx = i;
            } else {
                m_CW[i] = spec_w;
            }
        }

        if (vw_last_idx != static_cast<size_t>(-1)) {
            m_CW[vw_last_idx] += total_for_vw - vw_sum;
        }

        return true;
    }

    void m_selection_to_end()
    {
        m_selected = m_lines.empty() ? 0 : m_lines.size() - 1;
    }

    void m_selection_to_home()
    {
        m_selected = 0;
    }

    void m_selection_up_one_line_wrap_around()
    {
        if (m_selected == 0) {
            m_selected = m_lines.empty() ? 0 : m_lines.size() - 1;
        } else {
            --m_selected;
        }
    }

    void m_selection_down_one_line_wrap_around()
    {
        ++m_selected;
        if (m_selected >= m_lines.size()) {
            m_selected = 0;
        }
    }

    void m_selection_up(PHYS lines)
    {
        if (m_selected < lines.raw()) {
            m_selected = 0;
        } else {
            m_selected -= lines.raw();
        }
    }

    void m_selection_down(PHYS lines)
    {
        if (m_lines.empty()) {
            m_selected = 0;
        } else {
            m_selected = list_details::add_limit(m_selected, lines.raw(), m_lines.size() - 1);
        }
    }

    void m_emit_pos_info_event()
    {
        std::string res = "--- ";

        if (m_lines.empty()) {
            res.append("Empty");
        } else {
            res.append(std::to_string(m_selected + 1));
            res.append("/");
            res.append(std::to_string(m_lines.size()));

            res.append(" (");

            if (m_selected == 0) {
                res.append("First");
            } else if (m_selected == m_lines.size() - 1) {
                res.append("Last");
            } else {
                double pct = std::min((m_selected + 1) * 100.0 / m_lines.size(), 100.0);
                res.append(std::to_string(static_cast<int>(round(pct))));
                res.append("%");
            }

            res.append(")");
        }

        res.append(" ---");

        m_publisher.emit("info", std::string_view(res));
    }

public:
    List(
            std::vector<std::pair<ColumnType, PHYS>> &&CW_spec,
            std::vector<TruncatedText> &&headers,
            const CMenuStyles &styles,
            bool vi_keys)

        : m_num_cols{CW_spec.size()}
        , m_CW_spec{std::move(CW_spec)}
        , m_CW(m_num_cols, PHYS{0})
        , m_headers(std::move(headers))
        , m_lines{}
        , m_styles{styles}
        , m_selected{0}
        , m_scr_width{0}
        , m_scr_height{0}
        , m_scr_selected_y{0}
        , m_publisher{}
        , m_vi_keys{vi_keys}
    {}

    bool empty() const
    {
        return m_lines.empty();
    }

    bool is_valid_index(size_t i) const
    {
        return i < m_lines.size();
    }

    size_t selected_index() const
    {
        MY_ASSERT(!empty());
        return m_selected;
    }

    size_t num_columns() const
    {
        return m_num_cols;
    }

    void reset_selection()
    {
        m_selected = 0;
    }

    void add(std::vector<TruncatedText> &&new_entry)
    {
        m_lines.push_back(std::move(new_entry));
    }

    void set(size_t idx, std::vector<TruncatedText> &&new_entry)
    {
        MY_ASSERT(is_valid_index(idx));
        m_lines[idx] = std::move(new_entry);
    }

    void del(size_t idx)
    {
        MY_ASSERT(is_valid_index(idx));
        m_lines.erase(m_lines.begin() + idx);
    }

    void clear()
    {
        m_lines.clear();
    }

    void shrink()
    {
        m_lines.shrink_to_fit();
    }

    virtual void draw() override
    {
        bool we_have_enough_size = m_update_column_widths();

        if (m_scr_height < PHYS{3} || m_scr_width < PHYS{3} || !we_have_enough_size) {

            attr_set(0, 0, nullptr);
            mvaddstr(0, 0, "(Need more size)");

            m_scr_selected_y = PHYS{0};
            return;
        }

        m_draw_line(PHYS{0}, m_headers, m_styles.style_header);

        if (m_lines.empty()) {
            m_scr_selected_y = PHYS{1};
            return;
        }

        size_t idx_from = list_details::sub_wrap0(m_selected, m_scr_height.raw() / 2);
        size_t idx_to = list_details::add_limit(idx_from, m_scr_height.raw() - 1, m_lines.size());

        for (size_t i = idx_from; i < idx_to; ++i) {
            PHYS cur_y = PHYS::from(i - idx_from + 1);
            InternedStyle style;
            if (m_selected == i) {
                m_scr_selected_y = cur_y;
                style = m_styles.style_hi;
            } else {
                style = m_styles.style_entry;
            }
            m_draw_line(cur_y, m_lines[i], style);
        }
    }

    virtual void focus() override
    {
        move(m_scr_selected_y.to_int(), 0);
    }

    virtual void subscribe(std::string_view event_name, Publisher::Handler &&handler) override
    {
        m_publisher.connect(event_name, std::move(handler));
    }

    virtual bool handle_input(wint_t c) override
    {
        if (c == KEY_UP || c == ctrl('p') || (m_vi_keys && c == 'k')) {
            m_selection_up_one_line_wrap_around();
            return true;
        }
        if (c == KEY_DOWN || c == ctrl('n') || (m_vi_keys && c == 'j')) {
            m_selection_down_one_line_wrap_around();
            return true;
        }
        if (c == KEY_HOME || (m_vi_keys && c == 'g')) {
            m_selection_to_home();
            return true;
        }
        if (c == KEY_END || (m_vi_keys && c == 'G')) {
            m_selection_to_end();
            return true;
        }
        if (c == ctrl('g')) {
            m_emit_pos_info_event();
            return true;
        }
        if (c == KEY_NPAGE || c == ctrl('f')) {
            if (m_scr_height) {
                m_selection_down(m_scr_height - PHYS{1});
            }
            return true;
        }
        if (c == KEY_PPAGE || c == ctrl('b')) {
            if (m_scr_height) {
                m_selection_up(m_scr_height - PHYS{1});
            }
            return true;
        }
        if (c == ctrl('d')) {
            m_selection_down(PHYS{m_scr_height.raw() / 2});
            return true;
        }
        if (c == ctrl('u')) {
            m_selection_up(PHYS{m_scr_height.raw() / 2});
            return true;
        }
        if (c == ':') {
            m_publisher.emit("to_command_mode");
            return true;
        }
        if (c == '\\') {
            m_publisher.emit("backslash");
            return true;
        }
        if (c == ctrl('x') || (m_vi_keys && c == 'q')) {
            m_publisher.emit("quit");
            return true;
        }
        if (is_enter(c)) {
            m_publisher.emit("enter");
            return true;
        }

        if (!m_vi_keys && c == (c & 127) && ComboStorage::is_valid_symbol(c)) {
            m_publisher.emit("to_combo_mode", static_cast<char>(c));
            return true;
        }

        return false;
    }

    virtual void set_dims(PHYS new_w, PHYS new_h) override
    {
        m_scr_width = new_w;
        m_scr_height = new_h;
    }
};
