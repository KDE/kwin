/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "tablet_input.h"
#include "decorations/decoratedclient.h"
#include "input_event.h"
#include "input_event_spy.h"
#include "pointer_input.h"
#include "wayland/seat_interface.h"
#include "wayland/surface_interface.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"
// KDecoration
#include <KDecoration2/Decoration>
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

    connect(workspace(), &QObject::destroyed, this, [this] {
        setInited(false);
    });
    connect(waylandServer(), &QObject::destroyed, this, [this] {
        setInited(false);
    });
}

void TabletInputRedirection::tabletToolEvent(KWin::InputRedirection::TabletEventType type, const QPointF &pos,
                                             qreal pressure, int xTilt, int yTilt, qreal rotation, bool tipDown,
                                             bool tipNear, const TabletToolId &tabletToolId,
                                             std::chrono::microseconds time)
{
    if (!inited()) {
        return;
    }
    input()->setLastInputHandler(this);
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
    workspace()->setActiveCursorOutput(pos);

    const auto button = m_tipDown ? Qt::LeftButton : Qt::NoButton;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // TODO: Not correct, but it should work fine. In long term, we need to stop using QTabletEvent.
    const QPointingDevice *dev = QPointingDevice::primaryPointingDevice();
    TabletEvent ev(t, dev, pos, pos, pressure,
                   xTilt, yTilt,
                   0, // tangentialPressure
                   rotation,
                   0, // z
                   Qt::NoModifier, button, button, tabletToolId);
#else
    TabletEvent ev(t, pos, pos, QTabletEvent::Stylus, QTabletEvent::Pen, pressure,
                   xTilt, yTilt,
                   0, // tangentialPressure
                   rotation,
                   0, // z
                   Qt::NoModifier, tabletToolId.m_uniqueId, button, button, tabletToolId);
#endif

    ev.setTimestamp(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
    input()->processSpies(std::bind(&InputEventSpy::tabletToolEvent, std::placeholders::_1, &ev));
    input()->processFilters(
        std::bind(&InputEventFilter::tabletToolEvent, std::placeholders::_1, &ev));

    m_tipDown = tipDown;
    m_tipNear = tipNear;
}

void KWin::TabletInputRedirection::tabletToolButtonEvent(uint button, bool isPressed,
                                                         const TabletToolId &tabletToolId, std::chrono::microseconds time)
{
    input()->processSpies(std::bind(&InputEventSpy::tabletToolButtonEvent,
                                    std::placeholders::_1, button, isPressed, tabletToolId, time));
    input()->processFilters(std::bind(&InputEventFilter::tabletToolButtonEvent,
                                      std::placeholders::_1, button, isPressed, tabletToolId, time));
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletPadButtonEvent(uint button, bool isPressed,
                                                        const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    input()->processSpies(std::bind(&InputEventSpy::tabletPadButtonEvent,
                                    std::placeholders::_1, button, isPressed, tabletPadId, time));
    input()->processFilters(std::bind(&InputEventFilter::tabletPadButtonEvent,
                                      std::placeholders::_1, button, isPressed, tabletPadId, time));
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletPadStripEvent(int number, int position, bool isFinger,
                                                       const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    input()->processSpies(std::bind(&InputEventSpy::tabletPadStripEvent,
                                    std::placeholders::_1, number, position, isFinger, tabletPadId, time));
    input()->processFilters(std::bind(&InputEventFilter::tabletPadStripEvent,
                                      std::placeholders::_1, number, position, isFinger, tabletPadId, time));
    input()->setLastInputHandler(this);
}

void KWin::TabletInputRedirection::tabletPadRingEvent(int number, int position, bool isFinger,
                                                      const TabletPadId &tabletPadId, std::chrono::microseconds time)
{
    input()->processSpies(std::bind(&InputEventSpy::tabletPadRingEvent,
                                    std::placeholders::_1, number, position, isFinger, tabletPadId, time));
    input()->processFilters(std::bind(&InputEventFilter::tabletPadRingEvent,
                                      std::placeholders::_1, number, position, isFinger, tabletPadId, time));
    input()->setLastInputHandler(this);
}

bool TabletInputRedirection::focusUpdatesBlocked()
{
    return input()->isSelectingWindow();
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

    const auto pos = m_lastPosition - now->window()->pos();
    QHoverEvent event(QEvent::HoverEnter, pos, pos);
    QCoreApplication::instance()->sendEvent(now->decoration(), &event);
    now->window()->processDecorationMove(pos, m_lastPosition);

    m_decorationGeometryConnection = connect(
        decoration()->window(), &Window::frameGeometryChanged, this, [this]() {
            // ensure maximize button gets the leave event when maximizing/restore a window, see BUG 385140
            const auto oldDeco = decoration();
            update();
            if (oldDeco && oldDeco == decoration() && !decoration()->window()->isInteractiveMove() && !decoration()->window()->isInteractiveResize()) {
                // position of window did not change, we need to send HoverMotion manually
                const QPointF p = m_lastPosition - decoration()->window()->pos();
                QHoverEvent event(QEvent::HoverMove, p, p);
                QCoreApplication::instance()->sendEvent(decoration()->decoration(), &event);
            }
        },
        Qt::QueuedConnection);

    // if our decoration gets destroyed whilst it has focus, we pass focus on to the same client
    m_decorationDestroyedConnection = connect(now, &QObject::destroyed, this, &TabletInputRedirection::update, Qt::QueuedConnection);
}

void TabletInputRedirection::focusUpdate(Window *focusOld, Window *focusNow)
{
    // This method is left blank intentionally.
}

}
