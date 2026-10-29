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

#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <unistd.h>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <utility>
#include "../common/my_assert.hpp"

namespace glib_stuff
{

namespace RefcntSemantics
{

    struct TagCopy {};
    struct TagSteal {};

} // namespace glib_stuff::RefcntSemantics

template<class U>
class ObjectWrapper
{
protected:
    U *m_raw;

    void m_destroy() noexcept
    {
        if (m_raw) {
            g_object_unref(m_raw);
        }
    }

public:
    using UnderlyingGlibOpaqueStructType = U;

    ObjectWrapper() noexcept
        : m_raw{nullptr}
    {}

    ObjectWrapper(U *existing, RefcntSemantics::TagCopy) noexcept
        : m_raw{reinterpret_cast<U *>(g_object_ref(existing))}
    {}

    ObjectWrapper(U *existing, RefcntSemantics::TagSteal) noexcept
        : m_raw{existing}
    {}

    ObjectWrapper(const ObjectWrapper &that) noexcept
        : m_raw{that.m_raw}
    {
        if (m_raw) {
            g_object_ref(m_raw);
        }
    }

    ObjectWrapper(ObjectWrapper &&that) noexcept
        : m_raw{that.m_raw}
    {
        that.m_raw = nullptr;
    }

    ObjectWrapper &operator =(const ObjectWrapper &that) noexcept
    {
        if (this != &that) {
            m_destroy();
            m_raw = that.m_raw;
            if (m_raw) {
                g_object_ref(m_raw);
            }
        }
        return *this;
    }

    ObjectWrapper &operator =(ObjectWrapper &&that) noexcept
    {
        if (this != &that) {
            m_destroy();
            m_raw = that.m_raw;
            that.m_raw = nullptr;
        }
        return *this;
    }

    bool valid() const noexcept
    {
        return m_raw != nullptr;
    }

    U *raw() const noexcept
    {
        return m_raw;
    }

    ~ObjectWrapper()
    {
        m_destroy();
    }
};

namespace details
{
    static inline std::string_view from_maybe_cstr(const char *cstr) noexcept
    {
        return {cstr ? cstr : ""};
    }

} // namespace glib_stuff::details

class DesktopAppInfo : public ObjectWrapper<GDesktopAppInfo>
{
    using ObjectWrapper<GDesktopAppInfo>::ObjectWrapper;

public:
    std::string_view name() const noexcept
    {
        MY_ASSERT(valid());

        return details::from_maybe_cstr(g_app_info_get_name(G_APP_INFO(m_raw)));
    }

    bool hidden() const noexcept
    {
        MY_ASSERT(valid());

        return g_desktop_app_info_get_is_hidden(m_raw);
    }

    bool no_display() const noexcept
    {
        MY_ASSERT(valid());

        return g_desktop_app_info_get_nodisplay(m_raw);
    }

    std::string_view categories() const noexcept
    {
        MY_ASSERT(valid());

        return details::from_maybe_cstr(g_desktop_app_info_get_categories(m_raw));
    }
};

class File : public ObjectWrapper<GFile>
{
    using ObjectWrapper<GFile>::ObjectWrapper;
};

class FileInfo : public ObjectWrapper<GFileInfo>
{
    using ObjectWrapper<GFileInfo>::ObjectWrapper;

public:
    std::string_view content_type() const noexcept
    {
        MY_ASSERT(valid());

        return details::from_maybe_cstr(g_file_info_get_content_type(m_raw));
    }
};

inline File new_file_from_path(const std::string &path) noexcept
{
    GFile *res = g_file_new_for_path(path.c_str());
    return {res, RefcntSemantics::TagSteal{}};
}

class Error : public std::runtime_error
{
public:
    explicit Error(const std::string &msg)
        : std::runtime_error("glib error: " + msg)
    {}
};

inline Error wrap_glib_error_slurp(GError *err)
{
    MY_ASSERT(err != nullptr);
    MY_ASSERT(err->message != nullptr);

    std::string msg_str;
    try {
        msg_str.assign(err->message);
    } catch (...) {
        g_error_free(err);
        throw;
    }
    g_error_free(err);

    return Error{msg_str};
}

inline FileInfo query_file_info(const File &file)
{
    MY_ASSERT(file.valid());

    GError *err = nullptr;
    GFileInfo *res = g_file_query_info(
        file.raw(),
        G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
        G_FILE_QUERY_INFO_NONE,
        nullptr,
        &err);

    if (!res) {
        throw wrap_glib_error_slurp(err);
    }

    return {res, RefcntSemantics::TagSteal{}};
}

class List
{
public:
    using DeleterFunc = void(*)(void *elem);

private:
    GList *m_list;
    DeleterFunc m_deleter;

    void m_destroy() noexcept
    {
        if (m_deleter) {
            for (GList *node = m_list; node; node = node->next) {
                m_deleter(node->data);
            }
        }
        g_list_free(m_list);
    }

public:
    List() noexcept
        : m_list{nullptr}
        , m_deleter{nullptr}
    {}

    List(GList *existing, DeleterFunc deleter) noexcept
        : m_list{existing}
        , m_deleter{deleter}
    {}

    List(const List &) = delete;

    List(List &&that) noexcept
        : m_list{that.m_list}
        , m_deleter{that.m_deleter}
    {}

    List &operator =(const List &) = delete;

    List &operator =(List &&that) noexcept
    {
        if (this != &that) {
            m_destroy();
            m_list = that.m_list;
            m_deleter = that.m_deleter;
            that.m_list = nullptr;
            that.m_deleter = nullptr;
        }
        return *this;
    }

    GList *raw() const noexcept
    {
        return m_list;
    }

    template<class F>
    void for_each(F &&f) const
    {
        for (GList *node = m_list; node; node = node->next) {
            void *const elem = node->data;
            if (!f(elem)) {
                break;
            }
        }
    }

    void append(void *elem) noexcept
    {
        m_list = g_list_append(m_list, elem);
    }

    ~List()
    {
        m_destroy();
    }
};

template<class F>
inline void for_each_desktop_app(F &&f)
{
    auto deleter = [](void *elem) -> void { g_object_unref(static_cast<GObject *>(elem)); };

    List list{
        g_app_info_get_all(),
        deleter,
    };

    list.for_each([&](void *elem) -> bool {
        GAppInfo *raw_ai = static_cast<GAppInfo *>(elem);
        if (G_IS_DESKTOP_APP_INFO(raw_ai)) {
            f(DesktopAppInfo{
                G_DESKTOP_APP_INFO(raw_ai),
                RefcntSemantics::TagCopy{},
            });
        }
        return true;
    });
}

inline std::vector<DesktopAppInfo> get_all_desktop_apps()
{
    std::vector<DesktopAppInfo> res;
    for_each_desktop_app([&](DesktopAppInfo &&app_info) -> void {
        if (!app_info.hidden() && !app_info.no_display()) {
            res.push_back(std::move(app_info));
        }
    });
    return res;
}

inline std::pair<std::vector<DesktopAppInfo>, std::vector<DesktopAppInfo>>
    get_desktop_apps_for_content_type(const std::string &content_type)
{
    if (content_type.empty()) {
        return {{}, {}};
    }

    std::vector<DesktopAppInfo> recommended;
    std::vector<DesktopAppInfo> fallback;

    for_each_desktop_app([&](DesktopAppInfo &&app_info) -> void {

        if (app_info.hidden()) {
            return;
        }

        const char **supported_types =
            g_app_info_get_supported_types(G_APP_INFO(app_info.raw()));

        if (!supported_types) {
            return;
        }

        bool supported = false;
        bool supported_directly = false;
        for (const char **cur = supported_types; *cur; ++cur) {
            if (content_type == *cur) {
                supported = true;
                supported_directly = true;
                break;
            } else if (!supported) {
                if (g_content_type_is_a(content_type.c_str(), *cur)) {
                    supported = true;
                }
            }
        }
        if (supported) {
            std::vector<DesktopAppInfo> &dst = supported_directly ? recommended : fallback;
            dst.push_back(std::move(app_info));
        }
    });

    return {std::move(recommended), std::move(fallback)};
}

inline void launch(const DesktopAppInfo &app_info, const File &on_file = {})
{
    MY_ASSERT(app_info.valid());

    auto deleter = [](void *uri) -> void { g_free(uri); };

    List uris{nullptr, deleter};
    if (on_file.valid()) {
        char *uri = g_file_get_uri(on_file.raw());
        uris.append(static_cast<void *>(uri));
    }

    GError *err = nullptr;
    bool is_ok = g_desktop_app_info_launch_uris_as_manager(
        app_info.raw(),
        uris.raw(),
        nullptr,
        G_SPAWN_DEFAULT,
        [](gpointer) -> void { setsid(); }, nullptr,
        nullptr, nullptr,
        &err);

    if (!is_ok) {
        throw wrap_glib_error_slurp(err);
    }
}

} // namespace glib_stuff
