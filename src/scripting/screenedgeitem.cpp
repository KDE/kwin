/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "screenedgeitem.h"

#include <config-kwin.h>

#include "screenedge.h"
#include "workspace.h"

#include <QAction>

namespace KWin
{

ScreenEdgeItem::ScreenEdgeItem(QObject *parent)
    : QObject(parent)
    , m_enabled(true)
    , m_edge(NoEdge)
    , m_action(new QAction(this))
{
    connect(m_action, &QAction::triggered, this, &ScreenEdgeItem::activated);
}

ScreenEdgeItem::~ScreenEdgeItem()
{
}

void ScreenEdgeItem::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    disableEdge();
    m_enabled = enabled;
    enableEdge();
    Q_EMIT enabledChanged();
}

void ScreenEdgeItem::setEdge(Edge edge)
{
    if (m_edge == edge) {
        return;
    }
    disableEdge();
    m_edge = edge;
    enableEdge();
    Q_EMIT edgeChanged();
}

void ScreenEdgeItem::enableEdge()
{
    if (!m_enabled || m_edge == NoEdge) {
        return;
    }
    switch (m_mode) {
    case Mode::Pointer:
        workspace()->screenEdges()->reserve(static_cast<ElectricBorder>(m_edge), this, "borderActivated");
        break;
    case Mode::Touch:
        workspace()->screenEdges()->reserveTouch(static_cast<ElectricBorder>(m_edge), m_action);
        break;
    default:
        Q_UNREACHABLE();
    }
}

void ScreenEdgeItem::disableEdge()
{
    if (!m_enabled || m_edge == NoEdge) {
        return;
    }
    switch (m_mode) {
    case Mode::Pointer:
        workspace()->screenEdges()->unreserve(static_cast<ElectricBorder>(m_edge), this);
        break;
    case Mode::Touch:
        workspace()->screenEdges()->unreserveTouch(static_cast<ElectricBorder>(m_edge), m_action);
        break;
    default:
        Q_UNREACHABLE();
    }
}

bool ScreenEdgeItem::borderActivated(ElectricBorder edge)
{
    if (edge != static_cast<ElectricBorder>(m_edge) || !m_enabled) {
        return false;
    }
    Q_EMIT activated();
    return true;
}

void ScreenEdgeItem::setMode(Mode mode)
{
    if (m_mode == mode) {
        return;
    }
    disableEdge();
    m_mode = mode;
    enableEdge();
    Q_EMIT modeChanged();
}

} // namespace
