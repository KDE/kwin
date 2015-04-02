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
#include "abstract_backend.h"
#include <config-kwin.h>
#include "composite.h"
#include "cursor.h"
#include "wayland_server.h"
#if HAVE_WAYLAND_CURSOR
#include "wayland_backend.h"
#endif
// KWayland
#include <KWayland/Client/buffer.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/clientconnection.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/surface_interface.h>
// Wayland
#if HAVE_WAYLAND_CURSOR
#include <wayland-cursor.h>
#endif

namespace KWin
{

AbstractBackend::AbstractBackend(QObject *parent)
    : QObject(parent)
{
    WaylandServer::self()->installBackend(this);
}

AbstractBackend::~AbstractBackend()
{
    WaylandServer::self()->uninstallBackend(this);
}

void AbstractBackend::installCursorFromServer()
{
    if (!m_softWareCursor) {
        return;
    }
    if (!waylandServer() || !waylandServer()->seat()->focusedPointer()) {
        return;
    }
    auto c = waylandServer()->seat()->focusedPointer()->cursor();
    if (!c) {
        return;
    }
    auto cursorSurface = c->surface();
    if (cursorSurface.isNull()) {
        return;
    }
    auto buffer = cursorSurface.data()->buffer();
    if (!buffer) {
        return;
    }
    triggerCursorRepaint();
    m_cursor.hotspot = c->hotspot();
    m_cursor.image = buffer->data().copy();
}

void AbstractBackend::installCursorImage(Qt::CursorShape shape)
{
    if (!m_softWareCursor) {
        return;
    }
#if HAVE_WAYLAND_CURSOR
    if (!m_cursorTheme) {
        // check whether we can create it
        if (waylandServer() && waylandServer()->internalShmPool()) {
            m_cursorTheme = new Wayland::WaylandCursorTheme(waylandServer()->internalShmPool(), this);
        }
    }
    if (!m_cursorTheme) {
        return;
    }
    wl_cursor_image *cursor = m_cursorTheme->get(shape);
    if (!cursor) {
        return;
    }
    wl_buffer *b = wl_cursor_image_get_buffer(cursor);
    if (!b) {
        return;
    }
    waylandServer()->internalClientConection()->flush();
    QMetaObject::invokeMethod(this,
                              "installThemeCursor",
                              Qt::QueuedConnection,
                              Q_ARG(quint32, KWayland::Client::Buffer::getId(b)),
                              Q_ARG(QPoint, QPoint(cursor->hotspot_x, cursor->hotspot_y)));
#else
    Q_UNUSED(shape)
#endif
}

void AbstractBackend::installThemeCursor(quint32 id, const QPoint &hotspot)
{
    auto buffer = KWayland::Server::BufferInterface::get(waylandServer()->internalConnection()->getResource(id));
    if (!buffer) {
        return;
    }
    triggerCursorRepaint();
    m_cursor.hotspot = hotspot;
    m_cursor.image = buffer->data().copy();
}

Screens *AbstractBackend::createScreens(QObject *parent)
{
    Q_UNUSED(parent)
    return nullptr;
}

OpenGLBackend *AbstractBackend::createOpenGLBackend()
{
    return nullptr;
}

QPainterBackend *AbstractBackend::createQPainterBackend()
{
    return nullptr;
}

void AbstractBackend::setSoftWareCursor(bool set)
{
    if (m_softWareCursor == set) {
        return;
    }
    m_softWareCursor = set;
    if (m_softWareCursor) {
        connect(Cursor::self(), &Cursor::posChanged, this, &AbstractBackend::triggerCursorRepaint);
    } else {
        disconnect(Cursor::self(), &Cursor::posChanged, this, &AbstractBackend::triggerCursorRepaint);
    }
}

void AbstractBackend::triggerCursorRepaint()
{
    if (!Compositor::self() || m_cursor.image.isNull()) {
        return;
    }
    Compositor::self()->addRepaint(m_cursor.lastRenderedPosition.x() - m_cursor.hotspot.x(),
                                   m_cursor.lastRenderedPosition.y() - m_cursor.hotspot.y(),
                                   m_cursor.image.width(), m_cursor.image.height());
}

void AbstractBackend::markCursorAsRendered()
{
    m_cursor.lastRenderedPosition = Cursor::pos();
}

}
