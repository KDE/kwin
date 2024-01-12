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

qreal OutputLayer::scale() const
{
    return m_scale;
}

void OutputLayer::setScale(qreal scale)
{
    m_scale = scale;
}

QPointF OutputLayer::hotspot() const
{
    return m_hotspot;
}

void OutputLayer::setHotspot(const QPointF &hotspot)
{
    m_hotspot = hotspot;
}

QSizeF OutputLayer::size() const
{
    return m_size;
}

void OutputLayer::setSize(const QSizeF &size)
{
    m_size = size;
}

std::optional<QSize> OutputLayer::fixedSize() const
{
    return std::nullopt;
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

bool OutputLayer::needsRepaint() const
{
    return !m_repaints.isEmpty();
}

bool OutputLayer::scanout(SurfaceItem *surfaceItem)
{
    return false;
}

void OutputLayer::setPosition(const QPointF &position)
{
    m_position = position;
}

QPointF OutputLayer::position() const
{
    return m_position;
}

void OutputLayer::setEnabled(bool enable)
{
    m_enabled = enable;
}

bool OutputLayer::isEnabled() const
{
    return m_enabled;
}

} // namespace KWin

#include "moc_outputlayer.cpp"
