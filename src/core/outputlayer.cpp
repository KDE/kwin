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

} // namespace KWin
