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

#include <vector>
#include <string>
#include <utility>
#include <functional>
#include <stdexcept>
#include <any>

class Publisher
{
public:
    using Handler = std::function<void(std::any)>;

private:
    std::vector<std::pair<std::string, Handler>> m_map;

public:
    Publisher()
        : m_map{}
    {}

    void connect(std::string_view event_name, Handler &&handler)
    {
        for (const auto &[s, _] : m_map) {
            if (s == event_name) {
                throw std::logic_error(
                    "already subscribed to event " + std::string(event_name)
                );
            }
        }
        m_map.emplace_back(event_name, std::move(handler));
    }

    void emit(std::string_view event_name, std::any data = {})
    {
        for (const auto &[s, handler] : m_map) {
            if (s == event_name) {
                handler(data);
                break;
            }
        }
    }
};
