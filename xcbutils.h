/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2012, 2013 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#ifndef KWIN_XCB_UTILS_H
#define KWIN_XCB_UTILS_H

#include <kwinglobals.h>
#include "utils.h"

#include <QRect>

#include <xcb/xcb.h>

namespace KWin {

namespace Xcb {

typedef xcb_window_t WindowId;

template <typename Reply,
    typename Cookie,
    Reply *(*replyFunc)(xcb_connection_t*, Cookie, xcb_generic_error_t**),
    Cookie (*requestFunc)(xcb_connection_t*, xcb_window_t)>
class Wrapper
{
public:
    Wrapper()
        : m_retrieved(false)
        , m_window(XCB_WINDOW_NONE)
        , m_reply(NULL)
        {
            m_cookie.sequence = 0;
        }
    explicit Wrapper(WindowId window)
        : m_retrieved(false)
        , m_cookie(requestFunc(connection(), window))
        , m_window(window)
        , m_reply(NULL)
    {
    }
    explicit Wrapper(const Wrapper &other)
        : m_retrieved(other.m_retrieved)
        , m_cookie(other.m_cookie)
        , m_window(other.m_window)
        , m_reply(NULL)
    {
        takeFromOther(const_cast<Wrapper&>(other));
    }
    virtual ~Wrapper() {
        cleanup();
    }
    inline Wrapper &operator=(const Wrapper &other) {
        if (this != &other) {
            // if we had managed a reply, free it
            cleanup();
            // copy members
            m_retrieved = other.m_retrieved;
            m_cookie = other.m_cookie;
            m_window = other.m_window;
            m_reply = other.m_reply;
            // take over the responsibility for the reply pointer
            takeFromOther(const_cast<Wrapper&>(other));
        }
        return *this;
    }

    inline const Reply *operator->() {
        getReply();
        return m_reply;
    }
    inline bool isNull() {
        getReply();
        return m_reply == NULL;
    }
    inline operator bool() {
        return !isNull();
    }
    inline const Reply *data() {
        getReply();
        return m_reply;
    }
    inline WindowId window() const {
        return m_window;
    }
    inline bool isRetrieved() const {
        return m_retrieved;
    }
    /**
     * Returns the value of the reply pointer referenced by this object. The reply pointer of
     * this object will be reset to null. Calling any method which requires the reply to be valid
     * will crash.
     *
     * Callers of this function take ownership of the pointer.
     **/
    inline Reply *take() {
        getReply();
        Reply *ret = m_reply;
        m_reply = NULL;
        m_window = XCB_WINDOW_NONE;
        return ret;
    }

protected:
    void getReply() {
        if (m_retrieved || !m_cookie.sequence) {
            return;
        }
        m_reply = replyFunc(connection(), m_cookie, NULL);
        m_retrieved = true;
    }

private:
    inline void cleanup() {
        if (!m_retrieved && m_cookie.sequence) {
            xcb_discard_reply(connection(), m_cookie.sequence);
        } else if (m_reply) {
            free(m_reply);
        }
    }
    inline void takeFromOther(Wrapper &other) {
        if (m_retrieved) {
            m_reply = other.take();
        } else {
            //ensure that other object doesn't try to get the reply or discards it in the dtor
            other.m_retrieved = true;
            other.m_window = XCB_WINDOW_NONE;
        }
    }
    bool m_retrieved;
    Cookie m_cookie;
    WindowId m_window;
    Reply *m_reply;
};

typedef Wrapper<xcb_get_window_attributes_reply_t, xcb_get_window_attributes_cookie_t, &xcb_get_window_attributes_reply, &xcb_get_window_attributes_unchecked> WindowAttributes;


class WindowGeometry : public Wrapper<xcb_get_geometry_reply_t, xcb_get_geometry_cookie_t, &xcb_get_geometry_reply, &xcb_get_geometry_unchecked>
{
public:
    WindowGeometry() : Wrapper<xcb_get_geometry_reply_t, xcb_get_geometry_cookie_t, &xcb_get_geometry_reply, &xcb_get_geometry_unchecked>() {}
    explicit WindowGeometry(xcb_window_t window) : Wrapper<xcb_get_geometry_reply_t, xcb_get_geometry_cookie_t, &xcb_get_geometry_reply, &xcb_get_geometry_unchecked>(window) {}

    inline QRect rect() {
        const xcb_get_geometry_reply_t *geometry = data();
        if (!geometry) {
            return QRect();
        }
        return QRect(geometry->x, geometry->y, geometry->width, geometry->height);
    }
};

class ExtensionData
{
public:
    ExtensionData();
    int version;
    int eventBase;
    int errorBase;
    int majorOpcode;
    bool present;
    QByteArray name;
};

class Extensions
{
public:
    bool isShapeAvailable() const {
        return m_shape.version > 0;
    }
    bool isShapeInputAvailable() const;
    int shapeNotifyEvent() const;
    bool hasShape(xcb_window_t w) const;
    bool isRandrAvailable() const {
        return m_randr.present;
    }
    int randrNotifyEvent() const;
    bool isDamageAvailable() const {
        return m_damage.present;
    }
    int damageNotifyEvent() const;
    bool isCompositeAvailable() const {
        return m_composite.version > 0;
    }
    bool isCompositeOverlayAvailable() const;
    bool isRenderAvailable() const {
        return m_render.version > 0;
    }
    bool isFixesAvailable() const {
        return m_fixes.version > 0;
    }
    bool isFixesRegionAvailable() const;
    bool isSyncAvailable() const {
        return m_sync.present;
    }
    int syncAlarmNotifyEvent() const;
    QVector<ExtensionData> extensions() const;

    static Extensions *self();
    static void destroy();
private:
    Extensions();
    ~Extensions();
    void init();
    template <typename reply, typename T, typename F>
    void initVersion(T cookie, F f, ExtensionData *dataToFill);
    void extensionQueryReply(const xcb_query_extension_reply_t *extension, ExtensionData *dataToFill);

    ExtensionData m_shape;
    ExtensionData m_randr;
    ExtensionData m_damage;
    ExtensionData m_composite;
    ExtensionData m_render;
    ExtensionData m_fixes;
    ExtensionData m_sync;

    static Extensions *s_self;
};

} // namespace X11

} // namespace KWin
#endif // KWIN_X11_UTILS_H
