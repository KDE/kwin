/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2019 Aleix Pol Gonzalez <aleixpol@kde.org>

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
#include "tablet_input.h"
#include "abstract_client.h"
#include "decorations/decoratedclient.h"
#include "input.h"
#include "input_event_spy.h"
#include "libinput/device.h"
#include "pointer_input.h"
#include "toplevel.h"
#include "wayland_server.h"
#include "workspace.h"
// KDecoration
#include <KDecoration2/Decoration>
// KWayland
#include <KWayland/Server/seat_interface.h>
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

void TabletInputRedirection::tabletToolEvent(KWin::InputRedirection::TabletEventType type,
                                             const QPointF &pos, qreal pressure,
                                             int xTilt, int yTilt, qreal rotation,
                                             bool tipDown, bool tipNear, quint64 serialId,
                                             quint64 toolId, LibInput::Device *device)
{
    Q_UNUSED(device)
    Q_UNUSED(toolId)
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
    QTabletEvent ev(t, pos, pos, QTabletEvent::Stylus, QTabletEvent::Pen, pressure,
                    xTilt, yTilt,
                    0, // tangentialPressure
                    rotation,
                    0, // z
                    Qt::NoModifier, serialId, button, button);

    input()->processSpies(std::bind(&InputEventSpy::tabletToolEvent, std::placeholders::_1, &ev));
    input()->processFilters(
        std::bind(&InputEventFilter::tabletToolEvent, std::placeholders::_1, &ev));

    m_tipDown = tipDown;
    m_tipNear = tipNear;
}

void KWin::TabletInputRedirection::tabletToolButtonEvent(uint button, bool isPressed)
{
    if (isPressed)
        m_toolPressedButtons.insert(button);
    else
        m_toolPressedButtons.remove(button);

    input()->processSpies(std::bind(&InputEventSpy::tabletToolButtonEvent,
                                    std::placeholders::_1, m_toolPressedButtons));
    input()->processFilters(std::bind( &InputEventFilter::tabletToolButtonEvent,
                                      std::placeholders::_1, m_toolPressedButtons));
}

void KWin::TabletInputRedirection::tabletPadButtonEvent(uint button, bool isPressed)
{
    if (isPressed) {
        m_padPressedButtons.insert(button);
    } else {
        m_padPressedButtons.remove(button);
    }

    input()->processSpies(std::bind( &InputEventSpy::tabletPadButtonEvent,
                                     std::placeholders::_1, m_padPressedButtons));
    input()->processFilters(std::bind( &InputEventFilter::tabletPadButtonEvent,
                                       std::placeholders::_1, m_padPressedButtons));
}

void KWin::TabletInputRedirection::tabletPadStripEvent(int number, int position, bool isFinger)
{
    input()->processSpies(std::bind( &InputEventSpy::tabletPadStripEvent,
                                     std::placeholders::_1, number, position, isFinger));
    input()->processFilters(std::bind( &InputEventFilter::tabletPadStripEvent,
                                       std::placeholders::_1, number, position, isFinger));
}

void KWin::TabletInputRedirection::tabletPadRingEvent(int number, int position, bool isFinger)
{
    input()->processSpies(std::bind( &InputEventSpy::tabletPadRingEvent,
                                     std::placeholders::_1, number, position, isFinger));
    input()->processFilters(std::bind( &InputEventFilter::tabletPadRingEvent,
                                       std::placeholders::_1, number, position, isFinger));
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
