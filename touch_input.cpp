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
#include "touch_input.h"
#include "abstract_client.h"
#include "input.h"
#include "toplevel.h"
#include "wayland_server.h"
#include "workspace.h"
#include "decorations/decoratedclient.h"
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

TouchInputRedirection::TouchInputRedirection(InputRedirection *parent)
    : InputDeviceHandler(parent)
{
}

TouchInputRedirection::~TouchInputRedirection() = default;

void TouchInputRedirection::init()
{
    Q_ASSERT(!m_inited);
    m_inited = true;

    if (waylandServer()->hasScreenLockerIntegration()) {
        connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this,
            [this] {
                cancel();
                // position doesn't matter
                update();
            }
        );
    }
    connect(workspace(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(waylandServer(), &QObject::destroyed, this, [this] { m_inited = false; });
}

void TouchInputRedirection::update(const QPointF &pos)
{
    if (!m_inited) {
        return;
    }
    if (m_windowUpdatedInCycle) {
        return;
    }
    m_windowUpdatedInCycle = true;
    // TODO: handle pointer grab aka popups
    Toplevel *t = m_input->findToplevel(pos.toPoint());
    auto oldWindow = m_window;
    updateInternalWindow(pos);
    if (!m_internalWindow) {
        updateDecoration(t, pos);
    } else {
        // TODO: send hover leave to decoration
        if (m_decoration) {
            m_decoration->client()->leaveEvent();
        }
        m_decoration.clear();
    }
    if (m_decoration || m_internalWindow) {
        t = nullptr;
    } else if (!m_decoration) {
        m_decorationId = -1;
    } else if (!m_internalWindow) {
        m_internalId = -1;
    }
    if (!oldWindow.isNull() && t == oldWindow.data()) {
        return;
    }
    auto seat = waylandServer()->seat();
    // disconnect old surface
    if (oldWindow) {
        disconnect(m_windowGeometryConnection);
        m_windowGeometryConnection = QMetaObject::Connection();
    }
    if (t && t->surface()) {
        seat->setFocusedTouchSurface(t->surface(), t->pos());
        m_windowGeometryConnection = connect(t, &Toplevel::geometryChanged, this,
            [this] {
                if (m_window.isNull()) {
                    return;
                }
                auto seat = waylandServer()->seat();
                if (m_window.data()->surface() != seat->focusedTouchSurface()) {
                    return;
                }
                seat->setFocusedTouchSurfacePosition(m_window.data()->pos());
            }
        );
    } else {
        seat->setFocusedTouchSurface(nullptr);
        t = nullptr;
    }
    if (!t) {
        m_window.clear();
        return;
    }
    m_window = QPointer<Toplevel>(t);
}

void TouchInputRedirection::insertId(quint32 internalId, qint32 kwaylandId)
{
    m_idMapper.insert(internalId, kwaylandId);
}

qint32 TouchInputRedirection::mappedId(quint32 internalId)
{
    auto it = m_idMapper.constFind(internalId);
    if (it != m_idMapper.constEnd()) {
        return it.value();
    }
    return -1;
}

void TouchInputRedirection::removeId(quint32 internalId)
{
    m_idMapper.remove(internalId);
}

void TouchInputRedirection::processDown(qint32 id, const QPointF &pos, quint32 time, LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!m_inited) {
        return;
    }
    m_windowUpdatedInCycle = false;
    m_input->processFilters(std::bind(&InputEventFilter::touchDown, std::placeholders::_1, id, pos, time));
    m_windowUpdatedInCycle = false;
}

void TouchInputRedirection::processUp(qint32 id, quint32 time, LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!m_inited) {
        return;
    }
    m_windowUpdatedInCycle = false;
    m_input->processFilters(std::bind(&InputEventFilter::touchUp, std::placeholders::_1, id, time));
    m_windowUpdatedInCycle = false;
}

void TouchInputRedirection::processMotion(qint32 id, const QPointF &pos, quint32 time, LibInput::Device *device)
{
    Q_UNUSED(device)
    if (!m_inited) {
        return;
    }
    m_windowUpdatedInCycle = false;
    m_input->processFilters(std::bind(&InputEventFilter::touchMotion, std::placeholders::_1, id, pos, time));
    m_windowUpdatedInCycle = false;
}

void TouchInputRedirection::cancel()
{
    if (!m_inited) {
        return;
    }
    waylandServer()->seat()->cancelTouchSequence();
    m_idMapper.clear();
}

void TouchInputRedirection::frame()
{
    if (!m_inited) {
        return;
    }
    waylandServer()->seat()->touchFrame();
}

}
