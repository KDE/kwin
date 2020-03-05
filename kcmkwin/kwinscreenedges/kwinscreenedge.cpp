/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2009 Lucas Murray <lmurray@undefinedfire.com>
Copyright (C) 2020 Cyril Rossi <cyril.rossi@enioka.com>

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

#include "kwinscreenedge.h"

#include "monitor.h"

namespace KWin
{

KWinScreenEdge::KWinScreenEdge(QWidget *parent)
    : QWidget(parent)
{
    QMetaObject::invokeMethod(this, "createConnection", Qt::QueuedConnection);
}

KWinScreenEdge::~KWinScreenEdge()
{
}

void KWinScreenEdge::monitorHideEdge(ElectricBorder border, bool hidden)
{
    const int edge = KWinScreenEdge::electricBorderToMonitorEdge(border);
    monitor()->setEdgeHidden(edge, hidden);
}

void KWinScreenEdge::monitorAddItem(const QString &item)
{
    for (int i = 0; i < 8; i++) {
        monitor()->addEdgeItem(i, item);
    }
}

void KWinScreenEdge::monitorItemSetEnabled(int index, bool enabled)
{
    for (int i = 0; i < 8; i++) {
        monitor()->setEdgeItemEnabled(i, index, enabled);
    }
}

void KWinScreenEdge::monitorChangeEdge(const QList<int> &borderList, int index)
{
    for (int border : borderList) {
        monitorChangeEdge(static_cast<ElectricBorder>(border), index);
    }
}

void KWinScreenEdge::monitorChangeEdge(ElectricBorder border, int index)
{
    if (ELECTRIC_COUNT == border || ElectricNone == border) {
        return;
    }
    m_reference[border] = index;
    monitor()->selectEdgeItem(KWinScreenEdge::electricBorderToMonitorEdge(border), index);
}

QList<int> KWinScreenEdge::monitorCheckEffectHasEdge(int index) const
{
    QList<int> list;
    if (monitor()->selectedEdgeItem(Monitor::Top) == index) {
        list.append(ElectricTop);
    }
    if (monitor()->selectedEdgeItem(Monitor::TopRight) == index) {
        list.append(ElectricTopRight);
    }
    if (monitor()->selectedEdgeItem(Monitor::Right) == index) {
        list.append(ElectricRight);
    }
    if (monitor()->selectedEdgeItem(Monitor::BottomRight) == index) {
        list.append(ElectricBottomRight);
    }
    if (monitor()->selectedEdgeItem(Monitor::Bottom) == index) {
        list.append(ElectricBottom);
    }
    if (monitor()->selectedEdgeItem(Monitor::BottomLeft) == index) {
        list.append(ElectricBottomLeft);
    }
    if (monitor()->selectedEdgeItem(Monitor::Left) == index) {
        list.append(ElectricLeft);
    }
    if (monitor()->selectedEdgeItem(Monitor::TopLeft) == index) {
        list.append(ElectricTopLeft);
    }

    if (list.isEmpty()) {
        list.append(ElectricNone);
    }
    return list;
}

int KWinScreenEdge::selectedEdgeItem(ElectricBorder border) const
{
    return monitor()->selectedEdgeItem(KWinScreenEdge::electricBorderToMonitorEdge(border));
}

void KWinScreenEdge::monitorChangeDefaultEdge(ElectricBorder border, int index)
{
    if (ELECTRIC_COUNT == border || ElectricNone == border) {
        return;
    }
    m_default[border] = index;
}

void KWinScreenEdge::monitorChangeDefaultEdge(const QList<int> &borderList, int index)
{
    for (int border : borderList) {
        monitorChangeDefaultEdge(static_cast<ElectricBorder>(border), index);
    }
}

void KWinScreenEdge::reload()
{
    for (auto it = m_reference.begin(); it != m_reference.cend(); ++it) {
        monitor()->selectEdgeItem(KWinScreenEdge::electricBorderToMonitorEdge(it.key()), it.value());
    }
    onChanged();
}

void KWinScreenEdge::setDefaults()
{
    for (auto it = m_default.begin(); it != m_default.cend(); ++it) {
        monitor()->selectEdgeItem(KWinScreenEdge::electricBorderToMonitorEdge(it.key()), it.value());
    }
    onChanged();
}

int KWinScreenEdge::electricBorderToMonitorEdge(ElectricBorder border)
{
    switch(border) {
    case ElectricTop:
        return Monitor::Top;
    case ElectricTopRight:
        return Monitor::TopRight;
    case ElectricRight:
        return Monitor::Right;
    case ElectricBottomRight:
        return Monitor::BottomRight;
    case ElectricBottom:
        return Monitor::Bottom;
    case ElectricBottomLeft:
        return Monitor::BottomLeft;
    case ElectricLeft:
        return Monitor::Left;
    case ElectricTopLeft:
        return Monitor::TopLeft;
    default: // ELECTRIC_COUNT and ElectricNone
        Q_ASSERT(false);
    }
}

ElectricBorder KWinScreenEdge::monitorEdgeToElectricBorder(int edge)
{
    const Monitor::Edges monitorEdge = static_cast<Monitor::Edges>(edge);
    switch (monitorEdge) {
    case Monitor::Left:
        return ElectricLeft;
    case Monitor::Right:
        return ElectricRight;
    case Monitor::Top:
        return ElectricTop;
    case Monitor::Bottom:
        return ElectricBottom;
    case Monitor::TopLeft:
        return ElectricTopLeft;
    case Monitor::TopRight:
        return ElectricTopRight;
    case Monitor::BottomLeft:
        return ElectricBottomLeft;
    case Monitor::BottomRight:
        return ElectricBottomRight;
    default:
        return ElectricNone;
    }
}

void KWinScreenEdge::onChanged()
{
    bool needSave = false;
    for (auto it = m_reference.begin(); it != m_reference.cend(); ++it) {
        needSave |= it.value() != monitor()->selectedEdgeItem(KWinScreenEdge::electricBorderToMonitorEdge(it.key()));
    }
    emit saveNeededChanged(needSave);

    bool isDefault = true;
    for (auto it = m_default.begin(); it != m_default.cend(); ++it) {
        isDefault &= it.value() == monitor()->selectedEdgeItem(KWinScreenEdge::electricBorderToMonitorEdge(it.key()));
    }
    emit defaultChanged(isDefault);
}

void KWinScreenEdge::createConnection()
{
        connect(monitor(),
                &Monitor::changed,
                this,
                &KWinScreenEdge::onChanged);
}

} // namespace
