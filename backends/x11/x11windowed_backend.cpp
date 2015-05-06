/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>

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
#include "x11windowed_backend.h"
#include "scene_qpainter_x11_backend.h"
#include "screens_x11windowed.h"
#include "logging.h"
#include "wayland_server.h"
#include "xcbutils.h"
#ifdef KWIN_HAVE_EGL
#if HAVE_X11_XCB
#include "eglonxbackend.h"
#endif
#endif
#include <kwinxrenderutils.h>
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QSocketNotifier>
// kwayland
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/surface_interface.h>
// system
#include <linux/input.h>
#if HAVE_X11_XCB
#include <X11/Xlib-xcb.h>
#endif

namespace KWin
{

X11WindowedBackend::X11WindowedBackend(const QByteArray &display, const QSize &size, QObject *parent)
    : AbstractBackend(parent)
    , m_displayName(display)
    , m_size(size)
{
}

X11WindowedBackend::~X11WindowedBackend()
{
    if (m_connection) {
        if (m_window) {
            xcb_unmap_window(m_connection, m_window);
            xcb_destroy_window(m_connection, m_window);
        }
        if (m_cursor) {
            xcb_free_cursor(m_connection, m_cursor);
        }
        xcb_disconnect(m_connection);
    }
}

void X11WindowedBackend::init()
{
    int screen = 0;
    xcb_connection_t *c = nullptr;
#if HAVE_X11_XCB
    Display *xDisplay = XOpenDisplay(m_displayName.constData());
    if (xDisplay) {
        c = XGetXCBConnection(xDisplay);
        XSetEventQueueOwner(xDisplay, XCBOwnsEventQueue);
        screen = XDefaultScreen(xDisplay);
    }
#else
    c = xcb_connect(m_displayName.constData(), &screen);
#endif
    if (c && !xcb_connection_has_error(c)) {
        m_connection = c;
        m_screenNumber = screen;
#if HAVE_X11_XCB
        m_display = xDisplay;
#endif
        for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(m_connection));
            it.rem;
            --screen, xcb_screen_next(&it)) {
            if (screen == m_screenNumber) {
                m_screen = it.data;
            }
        }
        XRenderUtils::init(m_connection, m_screen->root);
        createWindow();
        startEventReading();
        setReady(true);
        waylandServer()->seat()->setHasPointer(true);
        waylandServer()->seat()->setHasKeyboard(true);
    }
}

void X11WindowedBackend::createWindow()
{
    Q_ASSERT(m_window == XCB_WINDOW_NONE);
    Xcb::Atom protocolsAtom(QByteArrayLiteral("WM_PROTOCOLS"), false, m_connection);
    Xcb::Atom deleteWindowAtom(QByteArrayLiteral("WM_DELETE_WINDOW"), false, m_connection);
    m_window = xcb_generate_id(m_connection);
    uint32_t mask = XCB_CW_EVENT_MASK;
    const uint32_t values[] = {
        XCB_EVENT_MASK_KEY_PRESS |
        XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_ENTER_WINDOW |
        XCB_EVENT_MASK_LEAVE_WINDOW |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_EXPOSURE
    };
    xcb_create_window(m_connection, XCB_COPY_FROM_PARENT, m_window, m_screen->root,
                      0, 0, m_size.width(), m_size.height(),
                      0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, mask, values);
    xcb_map_window(m_connection, m_window);

    m_protocols = protocolsAtom;
    m_deleteWindowProtocol = deleteWindowAtom;
    xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, m_window, m_protocols, XCB_ATOM_ATOM, 32, 1, &m_deleteWindowProtocol);

    xcb_flush(m_connection);
}

void X11WindowedBackend::startEventReading()
{
    QSocketNotifier *notifier = new QSocketNotifier(xcb_get_file_descriptor(m_connection), QSocketNotifier::Read, this);
    auto processXcbEvents = [this] {
        while (auto event = xcb_poll_for_event(m_connection)) {
            handleEvent(event);
            free(event);
        }
        xcb_flush(m_connection);
    };
    connect(notifier, &QSocketNotifier::activated, this, processXcbEvents);
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, processXcbEvents);
    connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::awake, this, processXcbEvents);
}

void X11WindowedBackend::handleEvent(xcb_generic_event_t *e)
{
    const uint8_t eventType = e->response_type & ~0x80;
    switch (eventType) {
    case XCB_BUTTON_PRESS:
    case XCB_BUTTON_RELEASE:
        handleButtonPress(reinterpret_cast<xcb_button_press_event_t*>(e));
        break;
    case XCB_MOTION_NOTIFY: {
            auto event = reinterpret_cast<xcb_motion_notify_event_t*>(e);
            pointerMotion(QPointF(event->event_x, event->event_y), event->time);
        }
        break;
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
            auto event = reinterpret_cast<xcb_key_press_event_t*>(e);
            if (eventType == XCB_KEY_PRESS) {
                keyboardKeyPressed(event->detail - 8, event->time);
            } else {
                keyboardKeyReleased(event->detail - 8, event->time);
            }
        }
        break;
    case XCB_CONFIGURE_NOTIFY:
        updateSize(reinterpret_cast<xcb_configure_notify_event_t*>(e));
        break;
    case XCB_ENTER_NOTIFY: {
            auto event = reinterpret_cast<xcb_enter_notify_event_t*>(e);
            pointerMotion(QPointF(event->event_x, event->event_y), event->time);
        }
        break;
    case XCB_CLIENT_MESSAGE:
        handleClientMessage(reinterpret_cast<xcb_client_message_event_t*>(e));
        break;
    case XCB_EXPOSE:
        handleExpose(reinterpret_cast<xcb_expose_event_t*>(e));
        break;
    default:
        break;
    }
}

void X11WindowedBackend::handleClientMessage(xcb_client_message_event_t *event)
{
    if (event->window != m_window) {
        return;
    }
    if (event->type == m_protocols && m_protocols != XCB_ATOM_NONE) {
        if (event->data.data32[0] == m_deleteWindowProtocol && m_deleteWindowProtocol != XCB_ATOM_NONE) {
            qCDebug(KWIN_X11WINDOWED) << "Backend window is going to be closed, shutting down.";
            QCoreApplication::quit();
        }
    }
}

void X11WindowedBackend::handleButtonPress(xcb_button_press_event_t *event)
{
    if (!input()) {
        return;
    }
    bool const pressed = (event->response_type & ~0x80) == XCB_BUTTON_PRESS;
    if (event->detail >= XCB_BUTTON_INDEX_4 && event->detail <= 7) {
        // wheel
        if (!pressed) {
            return;
        }
        const int delta = (event->detail == XCB_BUTTON_INDEX_4 || event->detail == 6) ? -1 : 1;
        static const qreal s_defaultAxisStepDistance = 10.0;
        if (event->detail > 5) {
            pointerAxisHorizontal(delta * s_defaultAxisStepDistance, event->time);
        } else {
            pointerAxisVertical(delta * s_defaultAxisStepDistance, event->time);
        }
        return;
    }
    uint32_t button = 0;
    switch (event->detail) {
    case XCB_BUTTON_INDEX_1:
        button = BTN_LEFT;
        break;
    case XCB_BUTTON_INDEX_2:
        button = BTN_MIDDLE;
        break;
    case XCB_BUTTON_INDEX_3:
        button = BTN_RIGHT;
        break;
    default:
        button = event->detail + BTN_LEFT - 1;
        return;
    }
    pointerMotion(QPointF(event->event_x, event->event_y), event->time);
    if (pressed) {
        pointerButtonPressed(button, event->time);
    } else {
        pointerButtonReleased(button, event->time);
    }
}

void X11WindowedBackend::handleExpose(xcb_expose_event_t *event)
{
    repaint(QRect(event->x, event->y, event->width, event->height));
}

void X11WindowedBackend::updateSize(xcb_configure_notify_event_t *event)
{
    if (event->window != m_window) {
        return;
    }
    QSize s = QSize(event->width, event->height);
    if (s != m_size) {
        m_size = s;
        emit sizeChanged();
    }
}

void X11WindowedBackend::installCursorFromServer()
{
    if (!waylandServer() || !waylandServer()->seat()->focusedPointer()) {
        return;
    }
    auto c = waylandServer()->seat()->focusedPointer()->cursor();
    if (c) {
        auto cursorSurface = c->surface();
        if (!cursorSurface.isNull()) {
            auto buffer = cursorSurface.data()->buffer();
            if (buffer) {
                // TODO: cache generated cursors?
                const xcb_pixmap_t pix = xcb_generate_id(m_connection);
                const xcb_gcontext_t gc = xcb_generate_id(m_connection);
                const xcb_cursor_t cid = xcb_generate_id(m_connection);

                xcb_create_pixmap(m_connection, 32, pix, m_screen->root, buffer->size().width(), buffer->size().height());
                xcb_create_gc(m_connection, gc, pix, 0, nullptr);

                const QImage img = buffer->data();
                xcb_put_image(m_connection, XCB_IMAGE_FORMAT_Z_PIXMAP, pix, gc, img.width(), img.height(), 0, 0, 0, 32, img.byteCount(), img.constBits());

                XRenderPicture pic(pix, 32);
                xcb_render_create_cursor(m_connection, cid, pic, c->hotspot().x(), c->hotspot().y());
                xcb_change_window_attributes(m_connection, m_window, XCB_CW_CURSOR, &cid);

                xcb_free_pixmap(m_connection, pix);
                xcb_free_gc(m_connection, gc);
                if (m_cursor) {
                    xcb_free_cursor(m_connection, m_cursor);
                }
                m_cursor = cid;
                xcb_flush(m_connection);
                return;
            }
        }
    }
    // TODO: unset cursor
}

xcb_window_t X11WindowedBackend::rootWindow() const
{
    if (!m_screen) {
        return XCB_WINDOW_NONE;
    }
    return m_screen->root;
}

Screens *X11WindowedBackend::createScreens(QObject *parent)
{
    return new X11WindowedScreens(this, parent);
}

OpenGLBackend *X11WindowedBackend::createOpenGLBackend()
{
#ifdef KWIN_HAVE_EGL
#if HAVE_X11_XCB
    return  new EglOnXBackend(connection(), display(), rootWindow(), screenNumer(), window());
#endif
#endif
    return nullptr;
}

QPainterBackend *X11WindowedBackend::createQPainterBackend()
{
    return new X11WindowedQPainterBackend(this);
}

}
