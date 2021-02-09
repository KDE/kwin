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
#include "libinput/device.h"
#include "pointer_input.h"
#include "toplevel.h"
#include "wayland_server.h"
#include "workspace.h"
// KDecoration
#include <KDecoration2/Decoration>
// KWayland
#include <KWaylandServer/seat_interface.h>
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

void TabletInputRedirection::cleanupDecoration(Decoration::DecoratedClientImpl *old,
                                               Decoration::DecoratedClientImpl *now)
{
    Q_UNUSED(old)
    Q_UNUSED(now)
}

void TabletInputRedirection::cleanupInternalWindow(QWindow *old, QWindow *now)
{
    Q_UNUSED(old)
    Q_UNUSED(now)
}

void TabletInputRedirection::focusUpdate(KWin::Toplevel *old, KWin::Toplevel *now)
{
    Q_UNUSED(old)
    Q_UNUSED(now)
}

}
