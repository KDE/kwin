/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2013, 2016 Martin Gräßlin <mgraesslin@kde.org>

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
#include "pointer_input.h"
#include "abstract_backend.h"
#include "screens.h"
#include "shell_client.h"
#include "wayland_server.h"
#include "workspace.h"
#include "decorations/decoratedclient.h"
// KDecoration
#include <KDecoration2/Decoration>
// KWayland
#include <KWayland/Server/seat_interface.h>
// screenlocker
#include <KScreenLocker/KsldApp>

#include <QHoverEvent>
#include <QWindow>

#include <linux/input.h>

namespace KWin
{

static Qt::MouseButton buttonToQtMouseButton(uint32_t button)
{
    switch (button) {
    case BTN_LEFT:
        return Qt::LeftButton;
    case BTN_MIDDLE:
        return Qt::MiddleButton;
    case BTN_RIGHT:
        return Qt::RightButton;
    case BTN_BACK:
        return Qt::XButton1;
    case BTN_FORWARD:
        return Qt::XButton2;
    }
    return Qt::NoButton;
}

static bool screenContainsPos(const QPointF &pos)
{
    for (int i = 0; i < screens()->count(); ++i) {
        if (screens()->geometry(i).contains(pos.toPoint())) {
            return true;
        }
    }
    return false;
}

PointerInputRedirection::PointerInputRedirection(InputRedirection* parent)
    : QObject(parent)
    , m_input(parent)
    , m_supportsWarping(Application::usesLibinput())
{
}

PointerInputRedirection::~PointerInputRedirection() = default;

void PointerInputRedirection::init()
{
    Q_ASSERT(!m_inited);
    m_inited = true;
    connect(workspace(), &Workspace::stackingOrderChanged, this, &PointerInputRedirection::update);
    connect(screens(), &Screens::changed, this, &PointerInputRedirection::updateAfterScreenChange);
    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this, &PointerInputRedirection::update);
    connect(workspace(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(waylandServer(), &QObject::destroyed, this, [this] { m_inited = false; });

    // warp the cursor to center of screen
    warp(screens()->geometry().center());
    updateAfterScreenChange();
}

void PointerInputRedirection::processMotion(const QPointF &pos, uint32_t time)
{
    if (!m_inited) {
        return;
    }
    updatePosition(pos);
    QMouseEvent event(QEvent::MouseMove, m_pos.toPoint(), m_pos.toPoint(),
                      Qt::NoButton, m_qtButtons, m_input->keyboardModifiers());
    event.setTimestamp(time);

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->pointerEvent(&event, 0)) {
            return;
        }
    }
}

void PointerInputRedirection::processButton(uint32_t button, InputRedirection::PointerButtonState state, uint32_t time)
{
    if (!m_inited) {
        return;
    }
    updateButton(button, state);

    QEvent::Type type;
    switch (state) {
    case InputRedirection::PointerButtonReleased:
        type = QEvent::MouseButtonRelease;
        break;
    case InputRedirection::PointerButtonPressed:
        type = QEvent::MouseButtonPress;
        break;
    default:
        Q_UNREACHABLE();
        return;
    }

    QMouseEvent event(type, m_pos.toPoint(), m_pos.toPoint(),
                      buttonToQtMouseButton(button), m_qtButtons, m_input->keyboardModifiers());
    event.setTimestamp(time);

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->pointerEvent(&event, button)) {
            return;
        }
    }
}

void PointerInputRedirection::processAxis(InputRedirection::PointerAxis axis, qreal delta, uint32_t time)
{
    if (!m_inited) {
        return;
    }
    if (delta == 0) {
        return;
    }

    emit m_input->pointerAxisChanged(axis, delta);

    QWheelEvent wheelEvent(m_pos, m_pos, QPoint(),
                           (axis == InputRedirection::PointerAxisHorizontal) ? QPoint(delta, 0) : QPoint(0, delta),
                           delta,
                           (axis == InputRedirection::PointerAxisHorizontal) ? Qt::Horizontal : Qt::Vertical,
                           m_qtButtons,
                           m_input->keyboardModifiers());
    wheelEvent.setTimestamp(time);

    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->wheelEvent(&wheelEvent)) {
            return;
        }
    }
}

void PointerInputRedirection::update()
{
    if (!m_inited) {
        return;
    }
    // TODO: handle pointer grab aka popups
    Toplevel *t = m_input->findToplevel(m_pos.toPoint());
    updateInternalWindow();
    if (!m_internalWindow) {
        updateDecoration(t);
    } else {
        // TODO: send hover leave to decoration
        m_decoration.clear();
    }
    if (m_decoration || m_internalWindow) {
        t = nullptr;
    }
    auto oldWindow = m_window;
    if (!oldWindow.isNull() && t == m_window.data()) {
        return;
    }
    auto seat = waylandServer()->seat();
    // disconnect old surface
    if (oldWindow) {
        disconnect(m_windowGeometryConnection);
        m_windowGeometryConnection = QMetaObject::Connection();
        if (auto p = seat->focusedPointer()) {
            if (auto c = p->cursor()) {
                disconnect(c, &KWayland::Server::Cursor::changed, waylandServer()->backend(), &AbstractBackend::installCursorFromServer);
            }
        }
    }
    if (t && t->surface()) {
        seat->setFocusedPointerSurface(t->surface(), t->inputTransformation());
        m_windowGeometryConnection = connect(t, &Toplevel::geometryChanged, this,
            [this] {
                if (m_window.isNull()) {
                    return;
                }
                // TODO: can we check on the client instead?
                if (workspace()->getMovingClient()) {
                    // don't update while moving
                    return;
                }
                auto seat = waylandServer()->seat();
                if (m_window.data()->surface() != seat->focusedPointerSurface()) {
                    return;
                }
                seat->setFocusedPointerSurfaceTransformation(m_window.data()->inputTransformation());
            }
        );
        waylandServer()->backend()->installCursorFromServer();
        if (auto p = seat->focusedPointer()) {
            if (auto c = p->cursor()) {
                connect(c, &KWayland::Server::Cursor::changed, waylandServer()->backend(), &AbstractBackend::installCursorFromServer);
            }
        }
    } else {
        seat->setFocusedPointerSurface(nullptr);
        t = nullptr;
    }
    if (!t) {
        m_window.clear();
        return;
    }
    m_window = QPointer<Toplevel>(t);
}

void PointerInputRedirection::updateInternalWindow()
{
    const auto oldInternalWindow = m_internalWindow;
    bool found = false;
    // TODO: screen locked check without going through wayland server
    bool needsReset = waylandServer()->isScreenLocked();
    const auto &internalClients = waylandServer()->internalClients();
    const bool change = m_internalWindow.isNull() || !(m_internalWindow->flags().testFlag(Qt::Popup) && m_internalWindow->isVisible());
    if (!internalClients.isEmpty() && change) {
        auto it = internalClients.end();
        do {
            it--;
            if (QWindow *w = (*it)->internalWindow()) {
                if (!w->isVisible()) {
                    continue;
                }
                if (w->geometry().contains(m_pos.toPoint())) {
                    m_internalWindow = QPointer<QWindow>(w);
                    found = true;
                    break;
                }
            }
        } while (it != internalClients.begin());
        if (!found) {
            needsReset = true;
        }
    }
    if (needsReset) {
        m_internalWindow.clear();
    }
    if (oldInternalWindow != m_internalWindow) {
        // changed
        if (oldInternalWindow) {
            disconnect(m_internalWindowConnection);
            m_internalWindowConnection = QMetaObject::Connection();
            QEvent event(QEvent::Leave);
            QCoreApplication::sendEvent(oldInternalWindow.data(), &event);
        }
        if (m_internalWindow) {
            m_internalWindowConnection = connect(m_internalWindow.data(), &QWindow::visibleChanged, this,
                [this] (bool visible) {
                    if (!visible) {
                        update();
                    }
                });
            QEnterEvent event(m_pos - m_internalWindow->position(),
                              m_pos - m_internalWindow->position(),
                              m_pos);
            QCoreApplication::sendEvent(m_internalWindow.data(), &event);
            return;
        }
    }
}

void PointerInputRedirection::updateDecoration(Toplevel *t)
{
    const auto oldDeco = m_decoration;
    bool needsReset = waylandServer()->isScreenLocked();
    if (AbstractClient *c = dynamic_cast<AbstractClient*>(t)) {
        // check whether it's on a Decoration
        if (c->decoratedClient()) {
            const QRect clientRect = QRect(c->clientPos(), c->clientSize()).translated(c->pos());
            if (!clientRect.contains(m_pos.toPoint())) {
                m_decoration = c->decoratedClient();
            } else {
                needsReset = true;
            }
        } else {
            needsReset = true;
        }
    } else {
        needsReset = true;
    }
    if (needsReset) {
        m_decoration.clear();
    }

    if (oldDeco && oldDeco != m_decoration) {
        // send leave
        QHoverEvent event(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::instance()->sendEvent(oldDeco->decoration(), &event);
        if (!m_decoration) {
            waylandServer()->backend()->installCursorImage(Qt::ArrowCursor);
        }
    }
    if (m_decoration) {
        const QPointF p = m_pos - t->pos();
        QHoverEvent event(QEvent::HoverMove, p, p);
        QCoreApplication::instance()->sendEvent(m_decoration->decoration(), &event);
        m_decoration->client()->processDecorationMove();
        installCursorFromDecoration();
    }
}

void PointerInputRedirection::updatePosition(const QPointF &pos)
{
    // verify that at least one screen contains the pointer position
    QPointF p = pos;
    if (!screenContainsPos(p)) {
        // allow either x or y to pass
        p = QPointF(m_pos.x(), pos.y());
        if (!screenContainsPos(p)) {
            p = QPointF(pos.x(), m_pos.y());
            if (!screenContainsPos(p)) {
                return;
            }
        }
    }
    m_pos = p;
    emit m_input->globalPointerChanged(m_pos);
}

void PointerInputRedirection::updateButton(uint32_t button, InputRedirection::PointerButtonState state)
{
    m_buttons[button] = state;

    // update Qt buttons
    m_qtButtons = Qt::NoButton;
    for (auto it = m_buttons.constBegin(); it != m_buttons.constEnd(); ++it) {
        if (it.value() == InputRedirection::PointerButtonReleased) {
            continue;
        }
        Qt::MouseButton button = buttonToQtMouseButton(it.key());
        // TODO: we need to map all buttons, otherwise checks for are buttons pressed fail
        if (button != Qt::NoButton) {
            m_qtButtons |= button;
        }
    }

    emit m_input->pointerButtonStateChanged(button, state);
}

void PointerInputRedirection::warp(const QPointF &pos)
{
    if (supportsWarping()) {
        waylandServer()->backend()->warpPointer(pos);
        processMotion(pos, waylandServer()->seat()->timestamp());
    }
}

bool PointerInputRedirection::supportsWarping() const
{
    if (!m_inited) {
        return false;
    }
    if (m_supportsWarping) {
        return true;
    }
    if (waylandServer()->backend()->supportsPointerWarping()) {
        return true;
    }
    return false;
}

void PointerInputRedirection::installCursorFromDecoration()
{
    if (!m_inited || !m_decoration) {
        return;
    }
    waylandServer()->backend()->installCursorImage(m_decoration->client()->cursor());
}

void PointerInputRedirection::updateAfterScreenChange()
{
    if (!m_inited) {
        return;
    }
    if (screenContainsPos(m_pos)) {
        // pointer still on a screen
        return;
    }
    // pointer no longer on a screen, reposition to closes screen
    const QPointF pos = screens()->geometry(screens()->number(m_pos.toPoint())).center();
    // TODO: better way to get timestamps
    processMotion(pos, waylandServer()->seat()->timestamp());
}

}
