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
#include "input.h"
#include "toplevel.h"
#include "wayland_server.h"
#include "workspace.h"
// KWayland
#include <KWayland/Server/seat_interface.h>
// screenlocker
#include <KScreenLocker/KsldApp>

namespace KWin
{

TouchInputRedirection::TouchInputRedirection(InputRedirection *parent)
    : QObject(parent)
    , m_input(parent)
{
}

TouchInputRedirection::~TouchInputRedirection() = default;

void TouchInputRedirection::init()
{
    Q_ASSERT(!m_inited);
    m_inited = true;

    connect(ScreenLocker::KSldApp::self(), &ScreenLocker::KSldApp::lockStateChanged, this,
        [this] {
            cancel();
            // position doesn't matter
            update();
        }
    );
    connect(workspace(), &QObject::destroyed, this, [this] { m_inited = false; });
    connect(waylandServer(), &QObject::destroyed, this, [this] { m_inited = false; });
}

void TouchInputRedirection::update(const QPointF &pos)
{
    if (!m_inited) {
        return;
    }
    // TODO: handle pointer grab aka popups
    Toplevel *t = m_input->findToplevel(pos.toPoint());
    auto oldWindow = m_window;
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

void TouchInputRedirection::processDown(qint32 id, const QPointF &pos, quint32 time)
{
    if (!m_inited) {
        return;
    }
    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->touchDown(id, pos, time)) {
            return;
        }
    }
}

void TouchInputRedirection::processUp(qint32 id, quint32 time)
{
    if (!m_inited) {
        return;
    }
    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->touchUp(id, time)) {
            return;
        }
    }
}

void TouchInputRedirection::processMotion(qint32 id, const QPointF &pos, quint32 time)
{
    if (!m_inited) {
        return;
    }
    const auto &filters = m_input->filters();
    for (auto it = filters.begin(), end = filters.end(); it != end; it++) {
        if ((*it)->touchMotion(id, pos, time)) {
            return;
        }
    }
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
