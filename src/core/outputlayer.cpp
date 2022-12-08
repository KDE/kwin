/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "outputlayer.h"

namespace KWin
{

OutputLayer::OutputLayer(QObject *parent)
    : QObject(parent)
{
}

bool OutputLayer::isAccepted() const
{
    return m_accepted;
}

void OutputLayer::setAccepted(bool accepted)
{
    m_accepted = accepted;
}

QRegion OutputLayer::repaints() const
{
    return m_repaints;
}

void OutputLayer::addRepaint(const QRegion &region)
{
    m_repaints += region;
}

void OutputLayer::resetRepaints()
{
    m_repaints = QRegion();
}

void OutputLayer::aboutToStartPainting(const QRegion &damage)
{
}

bool OutputLayer::scanout(SurfaceItem *surfaceItem)
{
    return false;
}

qreal OutputLayer::scale() const
{
    return m_scale;
}

void OutputLayer::setScale(qreal scale)
{
    m_scale = scale;
}

QPoint OutputLayer::position() const
{
    return m_position;
}

void OutputLayer::setPosition(const QPoint &pos)
{
    m_position = pos;
}

QSize OutputLayer::size() const
{
    return m_size;
}

void OutputLayer::setSize(const QSize &size)
{
    m_size = size;
}

bool OutputLayer::isVisible() const
{
    return m_visible;
}

void OutputLayer::setVisible(bool visible)
{
    m_visible = visible;
}

QPoint OutputLayer::origin() const
{
    return m_origin;
}

void OutputLayer::setOrigin(const QPoint &origin)
{
    m_origin = origin;
}

} // namespace KWin
