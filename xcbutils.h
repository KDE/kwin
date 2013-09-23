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
#include <QRegion>
#include <QVector>

#include <xcb/xcb.h>
#include <xcb/composite.h>

namespace KWin {

namespace Xcb {

typedef xcb_window_t WindowId;

// forward declaration of methods
static void defineCursor(xcb_window_t window, xcb_cursor_t cursor);
static void setInputFocus(xcb_window_t window, uint8_t revertTo = XCB_INPUT_FOCUS_POINTER_ROOT, xcb_timestamp_t time = xTime());
static void moveWindow(xcb_window_t window, const QPoint &pos);
static void moveWindow(xcb_window_t window, uint32_t x, uint32_t y);

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
typedef Wrapper<xcb_composite_get_overlay_window_reply_t, xcb_composite_get_overlay_window_cookie_t, &xcb_composite_get_overlay_window_reply, &xcb_composite_get_overlay_window_unchecked> OverlayWindow;


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

class Tree : public Wrapper<xcb_query_tree_reply_t, xcb_query_tree_cookie_t, &xcb_query_tree_reply, &xcb_query_tree_unchecked>
{
public:
    explicit Tree(WindowId window) : Wrapper<xcb_query_tree_reply_t, xcb_query_tree_cookie_t, &xcb_query_tree_reply, &xcb_query_tree_unchecked>(window) {}

    inline WindowId *children() {
        return xcb_query_tree_children(data());
    }
    inline xcb_window_t parent() {
        if (isNull())
            return XCB_WINDOW_NONE;
        return (*this)->parent;
    }
};

inline xcb_get_input_focus_cookie_t get_input_focus(xcb_connection_t *c, xcb_window_t) {
    return xcb_get_input_focus(c);
}
class CurrentInput : public Wrapper<xcb_get_input_focus_reply_t, xcb_get_input_focus_cookie_t, &xcb_get_input_focus_reply, &get_input_focus>
{
public:
    CurrentInput() : Wrapper<xcb_get_input_focus_reply_t, xcb_get_input_focus_cookie_t, &xcb_get_input_focus_reply, &get_input_focus>(XCB_WINDOW_NONE) {}

    inline xcb_window_t window() {
        if (isNull())
            return XCB_WINDOW_NONE;
        return (*this)->focus;
    }
};

inline xcb_get_property_cookie_t get_transient_for(xcb_connection_t *c, xcb_window_t window)
{
    return xcb_get_property_unchecked(c, 0, window, XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 0, 1);
}

class TransientFor : public Wrapper<xcb_get_property_reply_t, xcb_get_property_cookie_t, &xcb_get_property_reply, &get_transient_for>
{
public:
    explicit TransientFor(WindowId window) : Wrapper<xcb_get_property_reply_t, xcb_get_property_cookie_t, &xcb_get_property_reply, &get_transient_for>(window) {}

    /**
     * @brief Fill given window pointer with the WM_TRANSIENT_FOR property of a window.
     * @param prop WM_TRANSIENT_FOR property value.
     * @returns @c true on success, @c false otherwise
     **/
    inline bool getTransientFor(WindowId *prop) {
        if (isNull()) {
            return false;
        }

        const xcb_get_property_reply_t *reply = data();
        if (!reply || reply->type != XCB_ATOM_WINDOW || reply->format != 32 || reply->length == 0)
            return false;

        *prop = *reinterpret_cast<WindowId *>(xcb_get_property_value(reply));
        return true;
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
    int fixesCursorNotifyEvent() const;
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

/**
 * This class is an RAII wrapper for an xcb_window_t. An xcb_window_t hold by an instance of this class
 * will be freed when the instance gets destroyed.
 *
 * Furthermore the class provides wrappers around some xcb methods operating on an xcb_window_t.
 **/
class Window
{
public:
    /**
     * Takes over responsibility of @p window. If @p window is not provided an invalid Window is
     * created. Use @link create to set an xcb_window_t later on.
     * @param window The window to manage.
     **/
    Window(xcb_window_t window = XCB_WINDOW_NONE);
    /**
     * Creates an xcb_window_t and manages it. It's a convenient method to create a window with
     * depth, class and visual being copied from parent and border being @c 0.
     * @param geometry The geometry for the window to be created
     * @param mask The mask for the values
     * @param values The values to be passed to xcb_create_window
     * @param parent The parent window
     **/
    Window(const QRect &geometry, uint32_t mask = 0, const uint32_t *values = NULL, xcb_window_t parent = rootWindow());
    /**
     * Creates an xcb_window_t and manages it. It's a convenient method to create a window with
     * depth and visual being copied from parent and border being @c 0.
     * @param geometry The geometry for the window to be created
     * @param class The window class
     * @param mask The mask for the values
     * @param values The values to be passed to xcb_create_window
     * @param parent The parent window
     **/
    Window(const QRect &geometry, uint16_t windowClass, uint32_t mask = 0, const uint32_t *values = NULL, xcb_window_t parent = rootWindow());
    ~Window();

    /**
     * Creates a new window for which the responsibility is taken over. If a window had been managed
     * before it is freed.
     *
     * Depth, class and visual are being copied from parent and border is @c 0.
     * @param geometry The geometry for the window to be created
     * @param mask The mask for the values
     * @param values The values to be passed to xcb_create_window
     * @param parent The parent window
     **/
    void create(const QRect &geometry, uint32_t mask = 0, const uint32_t *values = NULL, xcb_window_t parent = rootWindow());
    /**
     * Creates a new window for which the responsibility is taken over. If a window had been managed
     * before it is freed.
     *
     * Depth and visual are being copied from parent and border is @c 0.
     * @param geometry The geometry for the window to be created
     * @param class The window class
     * @param mask The mask for the values
     * @param values The values to be passed to xcb_create_window
     * @param parent The parent window
     **/
    void create(const QRect &geometry, uint16_t windowClass, uint32_t mask = 0, const uint32_t *values = NULL, xcb_window_t parent = rootWindow());
    /**
     * Frees the existing window and starts to manage the new @p window.
     **/
    void reset(xcb_window_t window = XCB_WINDOW_NONE);
    /**
     * @returns @c true if a window is managed, @c false otherwise.
     **/
    bool isValid() const;
    /**
     * Configures the window with a new geometry.
     * @param geometry The new window geometry to be used
     **/
    void setGeometry(const QRect &geometry);
    void setGeometry(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void move(const QPoint &pos);
    void move(uint32_t x, uint32_t y);
    void resize(const QSize &size);
    void resize(uint32_t width, uint32_t height);
    void raise();
    void map();
    void unmap();
    /**
     * Clears the window area. Same as xcb_clear_area with x, y, width, height being @c 0.
     **/
    void clear();
    void setBackgroundPixmap(xcb_pixmap_t pixmap);
    void defineCursor(xcb_cursor_t cursor);
    void focus(uint8_t revertTo = XCB_INPUT_FOCUS_POINTER_ROOT, xcb_timestamp_t time = xTime());
    operator xcb_window_t() const;
private:
    Window(const Window &other);
    xcb_window_t doCreate(const QRect &geometry, uint16_t windowClass, uint32_t mask = 0, const uint32_t *values = NULL, xcb_window_t parent = rootWindow());
    void destroy();
    xcb_window_t m_window;
};

inline
Window::Window(xcb_window_t window)
    : m_window(window)
{
}

inline
Window::Window(const QRect &geometry, uint32_t mask, const uint32_t *values, xcb_window_t parent)
    : m_window(doCreate(geometry, XCB_COPY_FROM_PARENT, mask, values, parent))
{
}

inline
Window::Window(const QRect &geometry, uint16_t windowClass, uint32_t mask, const uint32_t *values, xcb_window_t parent)
    : m_window(doCreate(geometry, windowClass, mask, values, parent))
{
}

inline
Window::~Window()
{
    destroy();
}

inline
void Window::destroy()
{
    if (!isValid()) {
        return;
    }
    xcb_destroy_window(connection(), m_window);
    m_window = XCB_WINDOW_NONE;
}

inline
bool Window::isValid() const
{
    return m_window != XCB_WINDOW_NONE;
}

inline
Window::operator xcb_window_t() const
{
    return m_window;
}

inline
void Window::create(const QRect &geometry, uint16_t windowClass, uint32_t mask, const uint32_t *values, xcb_window_t parent)
{
    destroy();
    m_window = doCreate(geometry, windowClass, mask, values, parent);
}

inline
void Window::create(const QRect &geometry, uint32_t mask, const uint32_t *values, xcb_window_t parent)
{
    create(geometry, XCB_COPY_FROM_PARENT, mask, values, parent);
}

inline
xcb_window_t Window::doCreate(const QRect &geometry, uint16_t windowClass, uint32_t mask, const uint32_t *values, xcb_window_t parent)
{
    xcb_window_t w = xcb_generate_id(connection());
    xcb_create_window(connection(), XCB_COPY_FROM_PARENT, w, parent,
                      geometry.x(), geometry.y(), geometry.width(), geometry.height(),
                      0, windowClass, XCB_COPY_FROM_PARENT, mask, values);
    return w;
}

inline
void Window::reset(xcb_window_t window)
{
    destroy();
    m_window = window;
}

inline
void Window::setGeometry(const QRect &geometry)
{
    setGeometry(geometry.x(), geometry.y(), geometry.width(), geometry.height());
}

inline
void Window::setGeometry(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!isValid()) {
        return;
    }
    const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    const uint32_t values[] = { x, y, width, height };
    xcb_configure_window(connection(), m_window, mask, values);
}

inline
void Window::move(const QPoint &pos)
{
    move(pos.x(), pos.y());
}

inline
void Window::move(uint32_t x, uint32_t y)
{
    if (!isValid()) {
        return;
    }
    moveWindow(m_window, x, y);
}

inline
void Window::resize(const QSize &size)
{
    resize(size.width(), size.height());
}

inline
void Window::resize(uint32_t width, uint32_t height)
{
    if (!isValid()) {
        return;
    }
    const uint16_t mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    const uint32_t values[] = { width, height };
    xcb_configure_window(connection(), m_window, mask, values);
}

inline
void Window::raise()
{
    const uint32_t values[] = { XCB_STACK_MODE_ABOVE };
    xcb_configure_window(connection(), m_window, XCB_CONFIG_WINDOW_STACK_MODE, values);
}

inline
void Window::map()
{
    if (!isValid()) {
        return;
    }
    xcb_map_window(connection(), m_window);
}

inline
void Window::unmap()
{
    if (!isValid()) {
        return;
    }
    xcb_unmap_window(connection(), m_window);
}

inline
void Window::clear()
{
    if (!isValid()) {
        return;
    }
    xcb_clear_area(connection(), false, m_window, 0, 0, 0, 0);
}

inline
void Window::setBackgroundPixmap(xcb_pixmap_t pixmap)
{
    if (!isValid()) {
        return;
    }
    const uint32_t values[] = {pixmap};
    xcb_change_window_attributes(connection(), m_window, XCB_CW_BACK_PIXMAP, values);
}

inline
void Window::defineCursor(xcb_cursor_t cursor)
{
    Xcb::defineCursor(m_window, cursor);
}

inline
void Window::focus(uint8_t revertTo, xcb_timestamp_t time)
{
    setInputFocus(m_window, revertTo, time);
}

// helper functions
static inline void moveResizeWindow(WindowId window, const QRect &geometry)
{
    const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    const uint32_t values[] = {
        static_cast<uint32_t>(geometry.x()),
        static_cast<uint32_t>(geometry.y()),
        static_cast<uint32_t>(geometry.width()),
        static_cast<uint32_t>(geometry.height())
    };
    xcb_configure_window(connection(), window, mask, values);
}

static inline void moveWindow(xcb_window_t window, const QPoint& pos)
{
    moveWindow(window, pos.x(), pos.y());
}

static inline void moveWindow(xcb_window_t window, uint32_t x, uint32_t y)
{
    const uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
    const uint32_t values[] = { x, y };
    xcb_configure_window(connection(), window, mask, values);
}

static inline WindowId createInputWindow(const QRect &geometry, uint32_t mask, const uint32_t *values)
{
    WindowId window = xcb_generate_id(connection());
    xcb_create_window(connection(), 0, window, rootWindow(),
                      geometry.x(), geometry.y(), geometry.width(), geometry.height(),
                      0, XCB_WINDOW_CLASS_INPUT_ONLY,
                      XCB_COPY_FROM_PARENT, mask, values);
    return window;
}

static inline void restackWindows(const QVector<xcb_window_t> &windows)
{
    if (windows.count() < 2) {
        // only one window, nothing to do
        return;
    }
    for (int i=1; i<windows.count(); ++i) {
        const uint16_t mask = XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE;
        const uint32_t stackingValues[] = {
            windows.at(i-1),
            XCB_STACK_MODE_BELOW
        };
        xcb_configure_window(connection(), windows.at(i), mask, stackingValues);
    }
}

static inline void restackWindowsWithRaise(const QVector<xcb_window_t> &windows)
{
    if (windows.isEmpty()) {
        return;
    }
    const uint32_t values[] = { XCB_STACK_MODE_ABOVE };
    xcb_configure_window(connection(), windows.first(), XCB_CONFIG_WINDOW_STACK_MODE, values);
    restackWindows(windows);
}

static inline int defaultDepth()
{
    static int depth = 0;
    if (depth != 0) {
        return depth;
    }
    int screen = QX11Info::appScreen();
    for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(connection()));
            it.rem;
            --screen, xcb_screen_next(&it)) {
        if (screen == 0) {
            depth = it.data->root_depth;
            break;
        }
    }
    return depth;
}

static inline xcb_rectangle_t fromQt(const QRect &rect)
{
    xcb_rectangle_t rectangle;
    rectangle.x = rect.x();
    rectangle.y = rect.y();
    rectangle.width  = rect.width();
    rectangle.height = rect.height();
    return rectangle;
}

static inline QVector<xcb_rectangle_t> regionToRects(const QRegion &region)
{
    const QVector<QRect> regionRects = region.rects();
    QVector<xcb_rectangle_t> rects(regionRects.count());
    for (int i=0; i<regionRects.count(); ++i) {
        rects[i] = Xcb::fromQt(regionRects.at(i));
    }
    return rects;
}

static inline void defineCursor(xcb_window_t window, xcb_cursor_t cursor)
{
    xcb_change_window_attributes(connection(), window, XCB_CW_CURSOR, &cursor);
}

static inline void setInputFocus(xcb_window_t window, uint8_t revertTo, xcb_timestamp_t time)
{
    xcb_set_input_focus(connection(), revertTo, window, time);
}

static inline void setTransientFor(xcb_window_t window, xcb_window_t transient_for_window)
{
    xcb_change_property(connection(), XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_TRANSIENT_FOR,
                        XCB_ATOM_WINDOW, 32, 1, &transient_for_window);
}

} // namespace X11

} // namespace KWin
#endif // KWIN_X11_UTILS_H
