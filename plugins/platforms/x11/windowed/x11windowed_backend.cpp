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
#include "logging.h"
#include "wayland_server.h"
#include "xcbutils.h"
#include "egl_x11_backend.h"
#include "screens.h"
#include <kwinxrenderutils.h>
// KDE
#include <KLocalizedString>
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QSocketNotifier>
// kwayland
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/display.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/surface_interface.h>
// xcb
#include <xcb/xcb_keysyms.h>
// system
#include <linux/input.h>
#include <X11/Xlib-xcb.h>

namespace KWin
{

X11WindowedBackend::X11WindowedBackend(QObject *parent)
    : Platform(parent)
{
    setSupportsPointerWarping(true);
    connect(this, &X11WindowedBackend::sizeChanged, this, &X11WindowedBackend::screenSizeChanged);
}

X11WindowedBackend::~X11WindowedBackend()
{
    if (m_connection) {
        if (m_keySymbols) {
            xcb_key_symbols_free(m_keySymbols);
        }
        for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
            xcb_unmap_window(m_connection, (*it).window);
            xcb_destroy_window(m_connection, (*it).window);
            delete (*it).winInfo;
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
    Display *xDisplay = XOpenDisplay(deviceIdentifier().constData());
    if (xDisplay) {
        c = XGetXCBConnection(xDisplay);
        XSetEventQueueOwner(xDisplay, XCBOwnsEventQueue);
        screen = XDefaultScreen(xDisplay);
    }
    if (c && !xcb_connection_has_error(c)) {
        m_connection = c;
        m_screenNumber = screen;
        m_display = xDisplay;
        for (xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(m_connection));
            it.rem;
            --screen, xcb_screen_next(&it)) {
            if (screen == m_screenNumber) {
                m_screen = it.data;
            }
        }
        XRenderUtils::init(m_connection, m_screen->root);
        createWindow();
        connect(kwinApp(), &Application::workspaceCreated, this, &X11WindowedBackend::startEventReading);
        connect(this, &X11WindowedBackend::cursorChanged, this,
            [this] {
                createCursor(softwareCursor(), softwareCursorHotspot());
            }
        );
        setReady(true);
        waylandServer()->seat()->setHasPointer(true);
        waylandServer()->seat()->setHasKeyboard(true);
        emit screensQueried();
    } else {
        emit initFailed();
    }
}

void X11WindowedBackend::createWindow()
{
    Xcb::Atom protocolsAtom(QByteArrayLiteral("WM_PROTOCOLS"), false, m_connection);
    Xcb::Atom deleteWindowAtom(QByteArrayLiteral("WM_DELETE_WINDOW"), false, m_connection);
    for (int i = 0; i < initialOutputCount(); ++i) {
        Output o;
        o.window = xcb_generate_id(m_connection);
        uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        const uint32_t values[] = {
            m_screen->black_pixel,
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
        o.scale = initialOutputScale();
        o.size = initialWindowSize() * o.scale;
        if (!m_windows.isEmpty()) {
            const auto &p = m_windows.last();
            o.internalPosition = QPoint(p.internalPosition.x() + p.size.width() / p.scale, 0);
        }
        xcb_create_window(m_connection, XCB_COPY_FROM_PARENT, o.window, m_screen->root,
                        0, 0, o.size.width(), o.size.height(),
                        0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, mask, values);

        o.winInfo = new NETWinInfo(m_connection, o.window, m_screen->root, NET::WMWindowType, NET::Properties2());
        o.winInfo->setWindowType(NET::Normal);
        o.winInfo->setPid(QCoreApplication::applicationPid());
        QIcon windowIcon = QIcon::fromTheme(QStringLiteral("kwin"));
        auto addIcon = [&o, &windowIcon] (const QSize &size) {
            if (windowIcon.actualSize(size) != size) {
                return;
            }
            NETIcon icon;
            icon.data = windowIcon.pixmap(size).toImage().bits();
            icon.size.width = size.width();
            icon.size.height = size.height();
            o.winInfo->setIcon(icon, false);
        };
        addIcon(QSize(16, 16));
        addIcon(QSize(32, 32));
        addIcon(QSize(48, 48));

        xcb_map_window(m_connection, o.window);

        m_protocols = protocolsAtom;
        m_deleteWindowProtocol = deleteWindowAtom;
        xcb_change_property(m_connection, XCB_PROP_MODE_REPLACE, o.window, m_protocols, XCB_ATOM_ATOM, 32, 1, &m_deleteWindowProtocol);

        m_windows << o;
    }

    updateWindowTitle();

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
            auto it = std::find_if(m_windows.constBegin(), m_windows.constEnd(), [event] (const Output &o) { return o.window == event->event; });
            if (it == m_windows.constEnd()) {
                break;
            }
            //generally we don't need to normalise input to the output scale; however because we're getting input
            //from a host window that doesn't understand scaling, we need to apply it ourselves so the cursor matches
            pointerMotion(QPointF(event->root_x - (*it).xPosition.x() + (*it).internalPosition.x(),
                                    event->root_y - (*it).xPosition.y() + (*it).internalPosition.y()) / it->scale,
                                    event->time);
        }
        break;
    case XCB_KEY_PRESS:
    case XCB_KEY_RELEASE: {
            auto event = reinterpret_cast<xcb_key_press_event_t*>(e);
            if (eventType == XCB_KEY_PRESS) {
                if (!m_keySymbols) {
                    m_keySymbols = xcb_key_symbols_alloc(m_connection);
                }
                const xcb_keysym_t kc = xcb_key_symbols_get_keysym(m_keySymbols, event->detail, 0);
                if (kc == XK_Control_R) {
                    grabKeyboard(event->time);
                }
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
            auto it = std::find_if(m_windows.constBegin(), m_windows.constEnd(), [event] (const Output &o) { return o.window == event->event; });
            if (it == m_windows.constEnd()) {
                break;
            }
            pointerMotion(QPointF(event->root_x - (*it).xPosition.x() + (*it).internalPosition.x(),
                                    event->root_y - (*it).xPosition.y() + (*it).internalPosition.y()) / it->scale,
                                    event->time);
        }
        break;
    case XCB_CLIENT_MESSAGE:
        handleClientMessage(reinterpret_cast<xcb_client_message_event_t*>(e));
        break;
    case XCB_EXPOSE:
        handleExpose(reinterpret_cast<xcb_expose_event_t*>(e));
        break;
    case XCB_MAPPING_NOTIFY:
        if (m_keySymbols) {
            xcb_refresh_keyboard_mapping(m_keySymbols, reinterpret_cast<xcb_mapping_notify_event_t*>(e));
        }
        break;
    default:
        break;
    }
}

void X11WindowedBackend::grabKeyboard(xcb_timestamp_t time)
{
    const bool oldState = m_keyboardGrabbed;
    if (m_keyboardGrabbed) {
        xcb_ungrab_keyboard(m_connection, time);
        xcb_ungrab_pointer(m_connection, time);
        m_keyboardGrabbed = false;
    } else {
        const auto c = xcb_grab_keyboard_unchecked(m_connection, false, window(), time,
                                                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        ScopedCPointer<xcb_grab_keyboard_reply_t> grab(xcb_grab_keyboard_reply(m_connection, c, nullptr));
        if (grab.isNull()) {
            return;
        }
        if (grab->status == XCB_GRAB_STATUS_SUCCESS) {
            const auto c = xcb_grab_pointer_unchecked(m_connection, false, window(),
                                                      XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                                                      XCB_EVENT_MASK_POINTER_MOTION |
                                                      XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW,
                                                      XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                                                      window(), XCB_CURSOR_NONE, time);
            ScopedCPointer<xcb_grab_pointer_reply_t> grab(xcb_grab_pointer_reply(m_connection, c, nullptr));
            if (grab.isNull() || grab->status != XCB_GRAB_STATUS_SUCCESS) {
                xcb_ungrab_keyboard(m_connection, time);
                return;
            }
            m_keyboardGrabbed = true;
        }
    }
    if (oldState != m_keyboardGrabbed) {
        updateWindowTitle();
        xcb_flush(m_connection);
    }
}

void X11WindowedBackend::updateWindowTitle()
{
    const QString grab = m_keyboardGrabbed ? i18n("Press right control to ungrab input") : i18n("Press right control key to grab input");
    const QString title = QStringLiteral("%1 (%2) - %3").arg(i18n("KDE Wayland Compositor"))
                                                        .arg(waylandServer()->display()->socketName())
                                                        .arg(grab);
    for (auto it = m_windows.constBegin(); it != m_windows.constEnd(); ++it) {
        (*it).winInfo->setName(title.toUtf8().constData());
    }
}

void X11WindowedBackend::handleClientMessage(xcb_client_message_event_t *event)
{
    auto it = std::find_if(m_windows.begin(), m_windows.end(), [event] (const Output &o) { return o.window == event->window; });
    if (it == m_windows.end()) {
        return;
    }
    if (event->type == m_protocols && m_protocols != XCB_ATOM_NONE) {
        if (event->data.data32[0] == m_deleteWindowProtocol && m_deleteWindowProtocol != XCB_ATOM_NONE) {
            if (m_windows.count() == 1) {
                qCDebug(KWIN_X11WINDOWED) << "Backend window is going to be closed, shutting down.";
                QCoreApplication::quit();
            } else {
                // remove the window
                qCDebug(KWIN_X11WINDOWED) << "Removing one output window.";
                auto o = *it;
                it = m_windows.erase(it);
                xcb_unmap_window(m_connection, o.window);
                xcb_destroy_window(m_connection, o.window);
                delete o.winInfo;

                // update the sizes
                int x = o.internalPosition.x();
                for (; it != m_windows.end(); ++it) {
                    (*it).internalPosition.setX(x);
                    x += (*it).size.width();
                }
                QMetaObject::invokeMethod(screens(), "updateCount");
            }
        }
    }
}

void X11WindowedBackend::handleButtonPress(xcb_button_press_event_t *event)
{
    auto it = std::find_if(m_windows.constBegin(), m_windows.constEnd(), [event] (const Output &o) { return o.window == event->event; });
    if (it == m_windows.constEnd()) {
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

    pointerMotion(QPointF(event->root_x - (*it).xPosition.x() + (*it).internalPosition.x(),
                            event->root_y - (*it).xPosition.y() + (*it).internalPosition.y()) / it->scale,
                            event->time);
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
    auto it = std::find_if(m_windows.begin(), m_windows.end(), [event] (const Output &o) { return o.window == event->window; });
    if (it == m_windows.end()) {
        return;
    }
    (*it).xPosition = QPoint(event->x, event->y);
    QSize s = QSize(event->width, event->height);
    if (s != (*it).size) {
        (*it).size = s;
        int x = (*it).internalPosition.x() + (*it).size.width() / (*it).scale;
        it++;
        for (; it != m_windows.end(); ++it) {
            (*it).internalPosition.setX(x);
            x += (*it).size.width() / (*it).scale;
        }
        emit sizeChanged();
    }
}

void X11WindowedBackend::createCursor(const QImage &img, const QPoint &hotspot)
{
    const xcb_pixmap_t pix = xcb_generate_id(m_connection);
    const xcb_gcontext_t gc = xcb_generate_id(m_connection);
    const xcb_cursor_t cid = xcb_generate_id(m_connection);

    xcb_create_pixmap(m_connection, 32, pix, m_screen->root, img.width(), img.height());
    xcb_create_gc(m_connection, gc, pix, 0, nullptr);

    xcb_put_image(m_connection, XCB_IMAGE_FORMAT_Z_PIXMAP, pix, gc, img.width(), img.height(), 0, 0, 0, 32, img.byteCount(), img.constBits());

    XRenderPicture pic(pix, 32);
    xcb_render_create_cursor(m_connection, cid, pic, hotspot.x(), hotspot.y());
    for (auto it = m_windows.constBegin(); it != m_windows.constEnd(); ++it) {
        xcb_change_window_attributes(m_connection, (*it).window, XCB_CW_CURSOR, &cid);
    }

    xcb_free_pixmap(m_connection, pix);
    xcb_free_gc(m_connection, gc);
    if (m_cursor) {
        xcb_free_cursor(m_connection, m_cursor);
    }
    m_cursor = cid;
    xcb_flush(m_connection);
    markCursorAsRendered();
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
    return new BasicScreens(this, parent);
}

OpenGLBackend *X11WindowedBackend::createOpenGLBackend()
{
    return  new EglX11Backend(this);
}

QPainterBackend *X11WindowedBackend::createQPainterBackend()
{
    return new X11WindowedQPainterBackend(this);
}

void X11WindowedBackend::warpPointer(const QPointF &globalPos)
{
    const xcb_window_t w = m_windows.at(0).window;
    xcb_warp_pointer(m_connection, w, w, 0, 0, 0, 0, globalPos.x(), globalPos.y());
    xcb_flush(m_connection);
}

xcb_window_t X11WindowedBackend::windowForScreen(int screen) const
{
    if (screen > m_windows.count()) {
        return XCB_WINDOW_NONE;
    }
    return m_windows.at(screen).window;
}

QVector<QRect> X11WindowedBackend::screenGeometries() const
{
    QVector<QRect> ret;
    for (auto it = m_windows.constBegin(); it != m_windows.constEnd(); ++it) {
        ret << QRect((*it).internalPosition, (*it).size / (*it).scale);
    }
    return ret;
}

QVector<qreal> X11WindowedBackend::screenScales() const
{
    QVector<qreal> ret;
    for (auto it = m_windows.constBegin(); it != m_windows.constEnd(); ++it) {
        ret << (*it).scale;
    }
    return ret;
}

}
