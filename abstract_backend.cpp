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
#include "abstract_egl_backend.h"
#include "composite.h"
#include "cursor.h"
#include "input.h"
#include "scene_opengl.h"
#include "wayland_server.h"
#include "wayland_cursor_theme.h"
// KWayland
#include <KWayland/Client/buffer.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Server/buffer_interface.h>
#include <KWayland/Server/clientconnection.h>
#include <KWayland/Server/seat_interface.h>
#include <KWayland/Server/surface_interface.h>
// Wayland
#include <wayland-cursor.h>

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
    triggerCursorRepaint();
    updateCursorFromServer();
}

void AbstractBackend::updateCursorFromServer()
{
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
    m_cursor.hotspot = c->hotspot();
    m_cursor.image = buffer->data().copy();
    emit cursorChanged();
}

void AbstractBackend::installCursorImage(Qt::CursorShape shape)
{
    if (!m_softWareCursor) {
        return;
    }
    updateCursorImage(shape);
}

void AbstractBackend::updateCursorImage(Qt::CursorShape shape)
{
    if (!m_cursorTheme) {
        // check whether we can create it
        if (waylandServer() && waylandServer()->internalShmPool()) {
            m_cursorTheme = new WaylandCursorTheme(waylandServer()->internalShmPool(), this);
            connect(waylandServer(), &WaylandServer::terminatingInternalClientConnection, this,
                [this] {
                    delete m_cursorTheme;
                    m_cursorTheme = nullptr;
                }
            );
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
    waylandServer()->dispatch();
    installThemeCursor(KWayland::Client::Buffer::getId(b), QPoint(cursor->hotspot_x, cursor->hotspot_y));
}

void AbstractBackend::installThemeCursor(quint32 id, const QPoint &hotspot)
{
    auto buffer = KWayland::Server::BufferInterface::get(waylandServer()->internalConnection()->getResource(id));
    if (!buffer) {
        return;
    }
    if (m_softWareCursor) {
        triggerCursorRepaint();
    }
    m_cursor.hotspot = hotspot;
    m_cursor.image = buffer->data().copy();
    emit cursorChanged();
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

void AbstractBackend::keyboardKeyPressed(quint32 key, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processKeyboardKey(key, InputRedirection::KeyboardKeyPressed, time);
}

void AbstractBackend::keyboardKeyReleased(quint32 key, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processKeyboardKey(key, InputRedirection::KeyboardKeyReleased, time);
}

void AbstractBackend::keyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!input()) {
        return;
    }
    input()->processKeyboardModifiers(modsDepressed, modsLatched, modsLocked, group);
}

void AbstractBackend::keymapChange(int fd, uint32_t size)
{
    if (!input()) {
        return;
    }
    input()->processKeymapChange(fd, size);
}

void AbstractBackend::pointerAxisHorizontal(qreal delta, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerAxis(InputRedirection::PointerAxisHorizontal, delta, time);
}

void AbstractBackend::pointerAxisVertical(qreal delta, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerAxis(InputRedirection::PointerAxisVertical, delta, time);
}

void AbstractBackend::pointerButtonPressed(quint32 button, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerButton(button, InputRedirection::PointerButtonPressed, time);
}

void AbstractBackend::pointerButtonReleased(quint32 button, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerButton(button, InputRedirection::PointerButtonReleased, time);
}

void AbstractBackend::pointerMotion(const QPointF &position, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerMotion(position, time);
}

void AbstractBackend::touchCancel()
{
    if (!input()) {
        return;
    }
    input()->cancelTouch();
}

void AbstractBackend::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processTouchDown(id, pos, time);
}

void AbstractBackend::touchFrame()
{
    if (!input()) {
        return;
    }
    input()->touchFrame();
}

void AbstractBackend::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processTouchMotion(id, pos, time);
}

void AbstractBackend::touchUp(qint32 id, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processTouchUp(id, time);
}

void AbstractBackend::repaint(const QRect &rect)
{
    if (!Compositor::self()) {
        return;
    }
    Compositor::self()->addRepaint(rect);
}

void AbstractBackend::setReady(bool ready)
{
    if (m_ready == ready) {
        return;
    }
    m_ready = ready;
    emit readyChanged(m_ready);
}

void AbstractBackend::warpPointer(const QPointF &globalPos)
{
    Q_UNUSED(globalPos)
}

bool AbstractBackend::supportsQpaContext() const
{
    return hasGLExtension(QByteArrayLiteral("EGL_KHR_surfaceless_context"));
}

EGLDisplay AbstractBackend::sceneEglDisplay() const
{
    if (Compositor *c = Compositor::self()) {
        if (SceneOpenGL *s = dynamic_cast<SceneOpenGL*>(c->scene())) {
            return static_cast<AbstractEglBackend*>(s->backend())->eglDisplay();
        }
    }
    return EGL_NO_DISPLAY;
}

EGLContext AbstractBackend::sceneEglContext() const
{
    if (Compositor *c = Compositor::self()) {
        if (SceneOpenGL *s = dynamic_cast<SceneOpenGL*>(c->scene())) {
            return static_cast<AbstractEglBackend*>(s->backend())->context();
        }
    }
    return EGL_NO_CONTEXT;
}

QSize AbstractBackend::screenSize() const
{
    return QSize();
}

QVector<QRect> AbstractBackend::screenGeometries() const
{
    return QVector<QRect>({QRect(QPoint(0, 0), screenSize())});
}

}
