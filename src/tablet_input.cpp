/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "tablet_input.h"
#include "abstract_client.h"
#include "decorations/decoratedclient.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "pointer_input.h"
#include "toplevel.h"
#include "wayland_server.h"
#include "workspace.h"
// KDecoration
#include <KDecoration2/Decoration>
// KWayland
#include <KWaylandServer/seat_interface.h>
#include <KWaylandServer/surface_interface.h>
// screenlocker
#include <KScreenLocker/KsldApp>
// Qt
#include <QHoverEvent>
#include <QWindow>

namespace KWin
{
TabletInputRedirection::TabletInputRedirection(InputRedirection *parent)
    : InputDeviceHandler(parent)
{
}

TabletInputRedirection::~TabletInputRedirection() = default;

void TabletInputRedirection::init()
{
    Q_ASSERT(!inited());
    setInited(true);
    InputDeviceHandler::init();

    connect(workspace(), &QObject::destroyed, this, [this] { setInited(false); });
    connect(waylandServer(), &QObject::destroyed, this, [this] { setInited(false); });
}

void TabletInputRedirection::tabletToolEvent(KWin::InputRedirection::TabletEventType type, const QPointF &pos,
                                             qreal pressure, int xTilt, int yTilt, qreal rotation, bool tipDown,
                                             bool tipNear, const TabletToolId &tabletToolId,
                                             quint32 time)
{
    if (!inited()) {
        return;
    }
    m_lastEventTime = time;
    m_lastPosition = pos;

    QEvent::Type t;
    switch (type) {
    case InputRedirection::Axis:
        t = QEvent::TabletMove;
        break;
    case InputRedirection::Tip:
        t = tipDown ? QEvent::TabletPress : QEvent::TabletRelease;
        break;
    case InputRedirection::Proximity:
        t = tipNear ? QEvent::TabletEnterProximity : QEvent::TabletLeaveProximity;
        break;
    }

    update();

    const auto button = m_tipDown ? Qt::LeftButton : Qt::NoButton;
    TabletEvent ev(t, pos, pos, QTabletEvent::Stylus, QTabletEvent::Pen, pressure,
                    xTilt, yTilt,
                    0, // tangentialPressure
                    rotation,
                    0, // z
                    Qt::NoModifier, tabletToolId.m_uniqueId, button, button, tabletToolId);

    ev.setTimestamp(time);
    input()->processSpies(std::bind(&InputEventSpy::tabletToolEvent, std::placeholders::_1, &ev));
    input()->processFilters(
        std::bind(&InputEventFilter::tabletToolEvent, std::placeholders::_1, &ev));

    m_tipDown = tipDown;
    m_tipNear = tipNear;
}

void KWin::TabletInputRedirection::tabletToolButtonEvent(uint button, bool isPressed,
                                                         const TabletToolId &tabletToolId)
{
    input()->processSpies(std::bind(&InputEventSpy::tabletToolButtonEvent,
                                    std::placeholders::_1, button, isPressed, tabletToolId));
    input()->processFilters(std::bind( &InputEventFilter::tabletToolButtonEvent,
                                      std::placeholders::_1, button, isPressed, tabletToolId));
}

void KWin::TabletInputRedirection::tabletPadButtonEvent(uint button, bool isPressed,
                                                        const TabletPadId &tabletPadId)
{
    input()->processSpies(std::bind( &InputEventSpy::tabletPadButtonEvent,
                                     std::placeholders::_1, button, isPressed, tabletPadId));
    input()->processFilters(std::bind( &InputEventFilter::tabletPadButtonEvent,
                                       std::placeholders::_1, button, isPressed, tabletPadId));
}

void KWin::TabletInputRedirection::tabletPadStripEvent(int number, int position, bool isFinger,
                                                       const TabletPadId &tabletPadId)
{
    input()->processSpies(std::bind( &InputEventSpy::tabletPadStripEvent,
                                     std::placeholders::_1, number, position, isFinger, tabletPadId));
    input()->processFilters(std::bind( &InputEventFilter::tabletPadStripEvent,
                                       std::placeholders::_1, number, position, isFinger, tabletPadId));
}

void KWin::TabletInputRedirection::tabletPadRingEvent(int number, int position, bool isFinger,
                                                      const TabletPadId &tabletPadId)
{
    input()->processSpies(std::bind( &InputEventSpy::tabletPadRingEvent,
                                     std::placeholders::_1, number, position, isFinger, tabletPadId));
    input()->processFilters(std::bind( &InputEventFilter::tabletPadRingEvent,
                                       std::placeholders::_1, number, position, isFinger, tabletPadId));
}

bool TabletInputRedirection::focusUpdatesBlocked()
{
    if (waylandServer()->seat()->isDragPointer()) {
        // ignore during drag and drop
        return true;
    }
    if (waylandServer()->seat()->isTouchSequence()) {
        // ignore during touch operations
        return true;
    }
    if (input()->isSelectingWindow()) {
        return true;
    }
    return false;
}

void TabletInputRedirection::cleanupDecoration(Decoration::DecoratedClientImpl *old,
                                               Decoration::DecoratedClientImpl *now)
{
    disconnect(m_decorationGeometryConnection);
    m_decorationGeometryConnection = QMetaObject::Connection();

    disconnect(m_decorationDestroyedConnection);
    m_decorationDestroyedConnection = QMetaObject::Connection();

    if (old) {
        // send leave event to old decoration
        QHoverEvent event(QEvent::HoverLeave, QPointF(), QPointF());
        QCoreApplication::instance()->sendEvent(old->decoration(), &event);
    }
    if (!now) {
        // left decoration
        return;
    }

    waylandServer()->seat()->setFocusedPointerSurface(nullptr);

    const auto pos = m_lastPosition - now->client()->pos();
    QHoverEvent event(QEvent::HoverEnter, pos, pos);
    QCoreApplication::instance()->sendEvent(now->decoration(), &event);
    now->client()->processDecorationMove(pos.toPoint(), m_lastPosition.toPoint());

    m_decorationGeometryConnection = connect(decoration()->client(), &AbstractClient::frameGeometryChanged, this,
        [this] {
            // ensure maximize button gets the leave event when maximizing/restore a window, see BUG 385140
            const auto oldDeco = decoration();
            update();
            if (oldDeco &&
                oldDeco == decoration() &&
                !decoration()->client()->isInteractiveMove() &&
                !decoration()->client()->isInteractiveResize()) {
                // position of window did not change, we need to send HoverMotion manually
                const QPointF p = m_lastPosition - decoration()->client()->pos();
                QHoverEvent event(QEvent::HoverMove, p, p);
                QCoreApplication::instance()->sendEvent(decoration()->decoration(), &event);
            }
        }, Qt::QueuedConnection);

    // if our decoration gets destroyed whilst it has focus, we pass focus on to the same client
    m_decorationDestroyedConnection = connect(now, &QObject::destroyed, this, &TabletInputRedirection::update, Qt::QueuedConnection);
}

void TabletInputRedirection::cleanupInternalWindow(QWindow *old, QWindow *now)
{
    disconnect(m_internalWindowConnection);
    m_internalWindowConnection = QMetaObject::Connection();

    if (old) {
        // leave internal window
        QEvent leaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(old, &leaveEvent);
    }

    if (now) {
        m_internalWindowConnection = connect(internalWindow(), &QWindow::visibleChanged, this,
            [this] (bool visible) {
                if (!visible) {
                    update();
                }
            }
        );
    }
}

void TabletInputRedirection::focusUpdate(Toplevel *focusOld, Toplevel *focusNow)
{
    if (AbstractClient *ac = qobject_cast<AbstractClient*>(focusOld)) {
        ac->leaveEvent();
    }
    disconnect(m_focusGeometryConnection);
    m_focusGeometryConnection = QMetaObject::Connection();

    if (AbstractClient *ac = qobject_cast<AbstractClient*>(focusNow)) {
        ac->enterEvent(m_lastPosition.toPoint());
    }

    if (internalWindow()) {
        // enter internal window
        const auto pos = at()->pos();
        QEnterEvent enterEvent(pos, pos, m_lastPosition);
        QCoreApplication::sendEvent(internalWindow(), &enterEvent);
    }

    auto seat = waylandServer()->seat();
    if (!focusNow || !focusNow->surface() || decoration()) {
        // Clean up focused pointer surface if there's no client to take focus,
        // or the pointer is on a client without surface or on a decoration.
        warpXcbOnSurfaceLeft(nullptr);
        seat->setFocusedPointerSurface(nullptr);
        return;
    }

    // TODO: add convenient API to update global pos together with updating focused surface
    warpXcbOnSurfaceLeft(focusNow->surface());

    seat->notifyPointerMotion(m_lastPosition.toPoint());
    seat->setFocusedPointerSurface(focusNow->surface(), focusNow->inputTransformation());

    m_focusGeometryConnection = connect(focusNow, &Toplevel::inputTransformationChanged, this,
        [this] {
            // TODO: why no assert possible?
            if (!focus()) {
                return;
            }
            // TODO: can we check on the client instead?
            if (workspace()->moveResizeClient()) {
                // don't update while moving
                return;
            }
            auto seat = waylandServer()->seat();
            if (focus()->surface() != seat->focusedPointerSurface()) {
                return;
            }
            seat->setFocusedPointerSurfaceTransformation(focus()->inputTransformation());
        }
    );
}

void TabletInputRedirection::warpXcbOnSurfaceLeft(KWaylandServer::SurfaceInterface *newSurface)
{
    auto xc = waylandServer()->xWaylandConnection();
    if (!xc) {
        // No XWayland, no point in warping the x cursor
        return;
    }
    const auto c = kwinApp()->x11Connection();
    if (!c) {
        return;
    }
    static bool s_hasXWayland119 = xcb_get_setup(c)->release_number >= 11900000;
    if (s_hasXWayland119) {
        return;
    }
    if (newSurface && newSurface->client() == xc) {
        // new window is an X window
        return;
    }
    auto s = waylandServer()->seat()->focusedPointerSurface();
    if (!s || s->client() != xc) {
        // pointer was not on an X window
        return;
    }
    // warp pointer to 0/0 to trigger leave events on previously focused X window
    xcb_warp_pointer(c, XCB_WINDOW_NONE, kwinApp()->x11RootWindow(), 0, 0, 0, 0, 0, 0),
    xcb_flush(c);
}

}
