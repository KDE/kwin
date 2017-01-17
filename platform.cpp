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
#include "platform.h"
#include <config-kwin.h>
#include "abstract_egl_backend.h"
#include "composite.h"
#include "cursor.h"
#include "input.h"
#include "pointer_input.h"
#include "scene_opengl.h"
#include "screenedge.h"
#include "wayland_server.h"

namespace KWin
{

Platform::Platform(QObject *parent)
    : QObject(parent)
    , m_eglDisplay(EGL_NO_DISPLAY)
{
}

Platform::~Platform()
{
    if (m_eglDisplay != EGL_NO_DISPLAY) {
        eglTerminate(m_eglDisplay);
    }
}

QImage Platform::softwareCursor() const
{
    return input()->pointer()->cursorImage();
}

QPoint Platform::softwareCursorHotspot() const
{
    return input()->pointer()->cursorHotSpot();
}

PlatformCursorImage Platform::cursorImage() const
{
    return PlatformCursorImage(softwareCursor(), softwareCursorHotspot());
}

void Platform::hideCursor()
{
    m_hideCursorCounter++;
    if (m_hideCursorCounter == 1) {
        doHideCursor();
    }
}

void Platform::doHideCursor()
{
}

void Platform::showCursor()
{
    m_hideCursorCounter--;
    if (m_hideCursorCounter == 0) {
        doShowCursor();
    }
}

void Platform::doShowCursor()
{
}

Screens *Platform::createScreens(QObject *parent)
{
    Q_UNUSED(parent)
    return nullptr;
}

OpenGLBackend *Platform::createOpenGLBackend()
{
    return nullptr;
}

QPainterBackend *Platform::createQPainterBackend()
{
    return nullptr;
}

Edge *Platform::createScreenEdge(ScreenEdges *edges)
{
    return new Edge(edges);
}

void Platform::createPlatformCursor(QObject *parent)
{
    new InputRedirectionCursor(parent);
}

void Platform::configurationChangeRequested(KWayland::Server::OutputConfigurationInterface *config)
{
    Q_UNUSED(config)
    qCWarning(KWIN_CORE) << "This backend does not support configuration changes.";
}

void Platform::setSoftWareCursor(bool set)
{
    if (m_softWareCursor == set) {
        return;
    }
    m_softWareCursor = set;
    if (m_softWareCursor) {
        connect(Cursor::self(), &Cursor::posChanged, this, &Platform::triggerCursorRepaint);
        connect(this, &Platform::cursorChanged, this, &Platform::triggerCursorRepaint);
    } else {
        disconnect(Cursor::self(), &Cursor::posChanged, this, &Platform::triggerCursorRepaint);
        disconnect(this, &Platform::cursorChanged, this, &Platform::triggerCursorRepaint);
    }
}

void Platform::triggerCursorRepaint()
{
    if (!Compositor::self()) {
        return;
    }
    Compositor::self()->addRepaint(m_cursor.lastRenderedGeometry);
    Compositor::self()->addRepaint(QRect(Cursor::pos() - softwareCursorHotspot(), softwareCursor().size()));
}

void Platform::markCursorAsRendered()
{
    if (m_softWareCursor) {
        m_cursor.lastRenderedGeometry = QRect(Cursor::pos() - softwareCursorHotspot(), softwareCursor().size());
    }
    if (input()->pointer()) {
        input()->pointer()->markCursorAsRendered();
    }
}

void Platform::keyboardKeyPressed(quint32 key, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processKeyboardKey(key, InputRedirection::KeyboardKeyPressed, time);
}

void Platform::keyboardKeyReleased(quint32 key, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processKeyboardKey(key, InputRedirection::KeyboardKeyReleased, time);
}

void Platform::keyboardModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group)
{
    if (!input()) {
        return;
    }
    input()->processKeyboardModifiers(modsDepressed, modsLatched, modsLocked, group);
}

void Platform::keymapChange(int fd, uint32_t size)
{
    if (!input()) {
        return;
    }
    input()->processKeymapChange(fd, size);
}

void Platform::pointerAxisHorizontal(qreal delta, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerAxis(InputRedirection::PointerAxisHorizontal, delta, time);
}

void Platform::pointerAxisVertical(qreal delta, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerAxis(InputRedirection::PointerAxisVertical, delta, time);
}

void Platform::pointerButtonPressed(quint32 button, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerButton(button, InputRedirection::PointerButtonPressed, time);
}

void Platform::pointerButtonReleased(quint32 button, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerButton(button, InputRedirection::PointerButtonReleased, time);
}

void Platform::pointerMotion(const QPointF &position, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processPointerMotion(position, time);
}

void Platform::touchCancel()
{
    if (!input()) {
        return;
    }
    input()->cancelTouch();
}

void Platform::touchDown(qint32 id, const QPointF &pos, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processTouchDown(id, pos, time);
}

void Platform::touchFrame()
{
    if (!input()) {
        return;
    }
    input()->touchFrame();
}

void Platform::touchMotion(qint32 id, const QPointF &pos, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processTouchMotion(id, pos, time);
}

void Platform::touchUp(qint32 id, quint32 time)
{
    if (!input()) {
        return;
    }
    input()->processTouchUp(id, time);
}

void Platform::repaint(const QRect &rect)
{
    if (!Compositor::self()) {
        return;
    }
    Compositor::self()->addRepaint(rect);
}

void Platform::setReady(bool ready)
{
    if (m_ready == ready) {
        return;
    }
    m_ready = ready;
    emit readyChanged(m_ready);
}

void Platform::warpPointer(const QPointF &globalPos)
{
    Q_UNUSED(globalPos)
}

bool Platform::supportsQpaContext() const
{
    if (Compositor *c = Compositor::self()) {
        if (SceneOpenGL *s = dynamic_cast<SceneOpenGL*>(c->scene())) {
            return s->backend()->hasExtension(QByteArrayLiteral("EGL_KHR_surfaceless_context"));
        }
    }
    return false;
}

EGLDisplay KWin::Platform::sceneEglDisplay() const
{
    return m_eglDisplay;
}

void Platform::setSceneEglDisplay(EGLDisplay display)
{
    m_eglDisplay = display;
}

EGLContext Platform::sceneEglContext() const
{
    if (Compositor *c = Compositor::self()) {
        if (SceneOpenGL *s = dynamic_cast<SceneOpenGL*>(c->scene())) {
            return static_cast<AbstractEglBackend*>(s->backend())->context();
        }
    }
    return EGL_NO_CONTEXT;
}

EGLSurface Platform::sceneEglSurface() const
{
    if (Compositor *c = Compositor::self()) {
        if (SceneOpenGL *s = dynamic_cast<SceneOpenGL*>(c->scene())) {
            return static_cast<AbstractEglBackend*>(s->backend())->surface();
        }
    }
    return EGL_NO_SURFACE;
}

EGLConfig Platform::sceneEglConfig() const
{
    if (Compositor *c = Compositor::self()) {
        if (SceneOpenGL *s = dynamic_cast<SceneOpenGL*>(c->scene())) {
            return static_cast<AbstractEglBackend*>(s->backend())->config();
        }
    }
    return nullptr;
}

QSize Platform::screenSize() const
{
    return QSize();
}

QVector<QRect> Platform::screenGeometries() const
{
    return QVector<QRect>({QRect(QPoint(0, 0), screenSize())});
}

bool Platform::requiresCompositing() const
{
    return true;
}

bool Platform::compositingPossible() const
{
    return true;
}

QString Platform::compositingNotPossibleReason() const
{
    return QString();
}

bool Platform::openGLCompositingIsBroken() const
{
    return false;
}

void Platform::createOpenGLSafePoint(OpenGLSafePoint safePoint)
{
    Q_UNUSED(safePoint)
}

void Platform::startInteractiveWindowSelection(std::function<void(KWin::Toplevel*)> callback, const QByteArray &cursorName)
{
    if (!input()) {
        callback(nullptr);
        return;
    }
    input()->startInteractiveWindowSelection(callback, cursorName);
}

void Platform::startInteractivePositionSelection(std::function<void(const QPoint &)> callback)
{
    if (!input()) {
        callback(QPoint(-1, -1));
        return;
    }
    input()->startInteractivePositionSelection(callback);
}

void Platform::setupActionForGlobalAccel(QAction *action)
{
    Q_UNUSED(action)
}

}
